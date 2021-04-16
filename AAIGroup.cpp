// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "AAIGroup.h"

#include "AAI.h"
#include "AAIBuildTable.h"
#include "AAIAttack.h"
#include "AAIExecute.h"
#include "AAIAttackManager.h"
#include "AAIAirForceManager.h"
#include "AAIUnitTable.h"
#include "AAIConfig.h"
#include "AAIMap.h"
#include "AAISector.h"
#include "AAIBrain.h"


#include "LegacyCpp/UnitDef.h"
using namespace springLegacyAI;


AAIGroup::AAIGroup(AAI *ai, UnitDefId unitDefId, int continentId) :
	m_groupDefId(unitDefId),
	m_task(GROUP_IDLE),
	m_urgencyOfCurrentTask(0.0f),
	m_attack(nullptr),
	m_targetPosition(ZeroVector),
	m_targetSector(nullptr),
	m_rallyPoint(ZeroVector),
	m_continentId(continentId)
{
	this->ai = ai;

	m_groupType = ai->s_buildTree.GetUnitType(m_groupDefId); 
	m_category  = ai->s_buildTree.GetUnitCategory(unitDefId);
	
	// set movement type of group (filter out add. movement info like underwater, floater, etc.)
	m_moveType = ai->s_buildTree.GetMovementType( unitDefId );

	//determine max group size
	if(m_groupType.IsAntiAir() && !m_groupType.IsAntiSurface())
		m_maxSize = cfg->MAX_ANTI_AIR_GROUP_SIZE;
	else
	{
		if(m_category.IsMobileArtillery())
			m_maxSize = cfg->MAX_ARTY_GROUP_SIZE;
		else if(m_category.IsAirCombat())
			m_maxSize = cfg->MAX_AIR_GROUP_SIZE;
		else if(m_category.IsSeaCombat())
			m_maxSize = cfg->MAX_NAVAL_GROUP_SIZE;
		else if(m_category.IsSubmarineCombat())
			m_maxSize = cfg->MAX_SUBMARINE_GROUP_SIZE;
		else
			m_maxSize = cfg->MAX_GROUP_SIZE;
	}

	lastCommand = Command(CMD_STOP);
	lastCommandFrame = 0;

	// get a rally point
	UpdateRallyPoint();

	ai->Log("Creating new group - max size: %i   unit type: %s   continent: %i\n", m_maxSize, ai->s_buildTree.GetUnitTypeProperties(m_groupDefId).m_name.c_str(), m_continentId);
}

AAIGroup::~AAIGroup(void)
{
	if(m_attack)
	{
		m_attack->RemoveGroup(this);
		m_attack = nullptr;
	}

	m_units.clear();
}

bool AAIGroup::AddUnit(UnitId unitId, UnitDefId unitDefId, int continentId)
{
	if(    (m_continentId == continentId) // for continent bound units: check if unit is on the same continent as the group
		&& (m_groupDefId  == unitDefId) 
		&& (GetCurrentSize() < m_maxSize)
		&& (m_attack == nullptr)
		&& (m_task != GROUP_ATTACKING) && (m_task != GROUP_BOMBING))
	{
		m_units.push_back(unitId);

		// send unit to rally point of the group
		if(m_rallyPoint.x > 0.0f)
		{
			Command c(CMD_MOVE);
			c.PushPos(m_rallyPoint);

			if(m_category.IsAirCombat() )
				c.SetOpts(c.GetOpts() | SHIFT_KEY);

			ai->Execute()->GiveOrder(&c, unitId.id, "Group::AddUnit");
		}

		return true;
	}
	else
		return false;
}

bool AAIGroup::RemoveUnit(UnitId unitId, UnitId attackerUnitId)
{
	// look for unit with that id
	for(auto unit = m_units.begin(); unit != m_units.end(); ++unit)
	{
		if( *unit == unitId)
		{
			const int newGroupSize = GetCurrentSize() - 1;

			m_units.erase(unit);

			if(newGroupSize == 0)
			{
				m_task = GROUP_IDLE;

				if(m_attack)
				{
					m_attack->RemoveGroup(this);
					m_attack = nullptr;		
				}
			}

			if(attackerUnitId.IsValid() && (newGroupSize > 0) )
			{
				const UnitDefId attackerDefId = ai->GetUnitDefId(attackerUnitId);

				if(attackerDefId.IsValid())
				{
					const AAIUnitCategory&  category    = ai->s_buildTree.GetUnitCategory(attackerDefId);
					const TargetTypeValues& combatPower = ai->s_buildTree.GetCombatPower(attackerDefId);
					const AAITargetType&    targetType  = ai->s_buildTree.GetTargetType(attackerDefId);

					if(     category.IsStaticDefence()
						|| (category.IsGroundCombat() && (combatPower.GetValue(ETargetType::SURFACE) > cfg->MIN_AIR_SUPPORT_EFFICIENCY) )
						|| (category.IsSeaCombat()    && (combatPower.GetValue(ETargetType::FLOATER) > cfg->MIN_AIR_SUPPORT_EFFICIENCY) )
						|| (category.IsHoverCombat()  && (combatPower.GetValue(ETargetType::SURFACE) > cfg->MIN_AIR_SUPPORT_EFFICIENCY) ) )
					{
						ai->AirForceMgr()->CheckTarget( attackerUnitId, targetType, ai->s_buildTree.GetHealth(attackerDefId));
					}
				}
			}

			return true;
		}
	}

	// unit not found
	const UnitDefId unitDefId = ai->GetUnitDefId(unitId);

	if(unitDefId.IsValid())
		ai->Log("Error: Failed to remove unit %s from group of %s!\n", ai->s_buildTree.GetUnitTypeProperties(unitDefId).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(m_groupDefId).m_name.c_str() );
	else
		ai->Log("Error: Failed to remove unit with unknown unit type from group of %s!\n", ai->s_buildTree.GetUnitTypeProperties(m_groupDefId).m_name.c_str() );
	return false;
}

void AAIGroup::GiveOrderToGroup(Command *c, float importance, UnitTask task, const char *owner)
{
	lastCommandFrame = ai->GetAICallback()->GetCurrentFrame();

	m_urgencyOfCurrentTask = importance;

	for(auto unit : m_units)
	{
		ai->Execute()->GiveOrder(c, unit.id, owner);
		ai->UnitTable()->SetUnitStatus( unit.id, task);
	}
}

void AAIGroup::Update()
{
	m_urgencyOfCurrentTask *= 0.98f;

	// attacking groups recheck target
	/*if(task == GROUP_ATTACKING && m_targetSector)
	{
		if(m_targetSector->GetNumberOfEnemyBuildings() <= 0)
		{
			task = GROUP_IDLE;
			m_targetSector = nullptr;
		}
	}*/

	// check fall back of long range units
	if(m_task == GROUP_ATTACKING)
	{
		float range;
		float3 pos;
		Command c(CMD_MOVE);

		/*for(list<int2>::iterator unit = units.begin(); unit != units.end(); ++unit)
		{
			range = ai->s_buildTree.GetMaxRange(UnitDefId(unit->y));

			if(range > cfg->MIN_FALLBACK_RANGE)
			{
				ai->Getexecute()->GetFallBackPos(&pos, unit->x, range);

				if(pos.x > 0)
				{
					c = Command(CMD_MOVE);
					c.PushParam(pos.x);
					c.PushParam(ai->GetAICallback()->GetElevation(pos.x, pos.z));
					c.PushParam(pos.z);

					//ai->Getcb()->GiveOrder(unit->x, &c);
					ai->Getexecute()->GiveOrder(&c, unit->x, "GroupFallBack");
				}
			}
		}*/
	}
}

void AAIGroup::AddGroupCombatPower(TargetTypeValues& combatPower) const
{
	const float numberOfUnits = static_cast<float>(m_units.size());
	combatPower.AddValues(ai->s_buildTree.GetCombatPower(m_groupDefId), numberOfUnits);
}

float AAIGroup::GetCombatPowerVsTargetType(const AAITargetType& targetType) const
{
	const float combatPower = ai->s_buildTree.GetCombatPower(m_groupDefId).GetValue(targetType);
	return static_cast<float>(m_units.size()) * combatPower;
}

const AAITargetType& AAIGroup::GetTargetType() const
{
	return ai->s_buildTree.GetTargetType(m_groupDefId);
}

float3 AAIGroup::GetGroupPos() const
{
	if(!m_units.empty())
	{
		std::list<UnitId>::const_iterator unit = std::prev(m_units.end());
		return ai->GetAICallback()->GetUnitPos( (*unit).id );
	}
	else
		return ZeroVector;
}

bool AAIGroup::IsEntireGroupAtRallyPoint() const
{
	const float3 position = GetGroupPos();

	float dx = position.x - m_rallyPoint.x;
	float dy = position.z - m_rallyPoint.z;

	return (dx*dx+dy*dy) < AAIConstants::maxSquaredDistToRallyPoint;
}

float AAIGroup::GetDefenceRating(const AAITargetType& attackerTargetType, const float3& position, float importance, int continentId) const
{
	if( (m_continentId == -1) || (m_continentId == continentId) )
	{
		const bool matchingType  = m_groupType.CanFightTargetType(attackerTargetType);
		const bool groupAvailable = (m_task == GROUP_IDLE) || (m_urgencyOfCurrentTask < importance);

		if(matchingType && groupAvailable)
		{
			const float3& groupPosition = GetGroupPos();

			const float speed = ai->s_buildTree.GetMaxSpeed(m_groupDefId);

			const float dx = position.x - groupPosition.x;
			const float dy = position.z - groupPosition.z;

			return speed / ( 1.0f + fastmath::apxsqrt(dx * dx  +  dy * dy));
		}
	}

	return 0.0f;
}


void AAIGroup::TargetUnitKilled()
{
	// air groups retreat to rally point
	if(m_category.IsAirCombat())
	{
		GiveMoveOrderToGroup(CMD_MOVE, UnitTask::HEADING_TO_RALLYPOINT, m_rallyPoint, AAIConstants::distanceBetweenUnitsInGroup);

		m_urgencyOfCurrentTask = 0.0f;
		m_task                 = GROUP_RETREATING;
		//m_targetPosition = m_rallyPoint;
		//m_targetSector   = ai->Map()->GetSectorOfPos(m_rallyPoint);
	}
}

void AAIGroup::AttackPositionInSector(const float3& position, const AAISector* sector, float urgency)
{
	const float3 attackDirection = DetermineDirectionToPosition(position);

	const float  distanceToTarget = static_cast<float>(8 * SQUARE_SIZE);
	const float3 attackPositionCenter( 	position.x - distanceToTarget * attackDirection.x, 
										position.y, 
										position.z - distanceToTarget * attackDirection.z);

	const int commandId = m_groupType.IsMeleeCombatUnit() ? CMD_MOVE : CMD_FIGHT;

	GiveMoveOrderToGroup(commandId, UnitTask::UNIT_ATTACKING, attackPositionCenter, AAIConstants::distanceBetweenUnitsInGroup);

	m_urgencyOfCurrentTask = urgency;
	m_task                 = GROUP_ATTACKING;
	m_targetPosition       = position;
	m_targetSector         = sector;
}

void AAIGroup::DefendUnit(UnitId unitId, const float3& enemyPosition, float urgency)
{
	const bool enemyPositionKnown = (enemyPosition.x > 0.0f);

	Command cmd(enemyPositionKnown ? CMD_FIGHT: CMD_GUARD);

	if(enemyPositionKnown)
	{
		cmd.PushPos(enemyPosition);

		GiveOrderToGroup(&cmd, urgency, DEFENDING, "Group::Defend");

		m_targetPosition = enemyPosition;
		m_targetSector   = ai->Map()->GetSectorOfPos(enemyPosition);
	}
	else
	{
		cmd.PushParam(unitId.id);

		GiveOrderToGroup(&cmd, urgency, GUARDING, "Group::Defend");

		const float3 defendedUnitPosition = ai->GetAICallback()->GetUnitPos(unitId.id);

		m_targetPosition = defendedUnitPosition;
		m_targetSector   = ai->Map()->GetSectorOfPos(defendedUnitPosition);
	}

	m_task = GROUP_DEFENDING;
}

void AAIGroup::GuardUnit(UnitId unitId)
{
	Command c(CMD_GUARD);
	c.PushParam(unitId.id);

	GiveOrderToGroup(&c, AAIConstants::defendUnitsUrgency, GUARDING, "Group::AttackSector");
}

void AAIGroup::RetreatToRallyPoint()
{
	GiveMoveOrderToGroup(CMD_MOVE, UnitTask::MOVING, m_rallyPoint, AAIConstants::rallyDistanceBetweenUnitsInGroup);

	m_attack               = nullptr;
	m_urgencyOfCurrentTask = 0.0f;
	m_task                 = GROUP_RETREATING;
	m_targetPosition       = m_rallyPoint;
	m_targetSector         = ai->Map()->GetSectorOfPos(m_rallyPoint);
}

UnitId AAIGroup::GetRandomUnit() const
{
	if(m_units.empty())
		return UnitId();
	else
	{
		const int selectedUnitId = rand()%static_cast<int>(m_units.size());

		auto unit = m_units.begin();

		for(int i = 0; i < selectedUnitId; ++i)
			++unit;

		return *unit;		
	}
}

void AAIGroup::GiveMoveOrderToGroup(int commandId, UnitTask task, const float3& targetPositionCenter, float distanceBetweenUnits)
{
	lastCommandFrame = ai->GetAICallback()->GetCurrentFrame();

	const float3 moveDirection = DetermineDirectionToPosition(targetPositionCenter);

	const float3 distanceBetweenUnitsVector(  distanceBetweenUnits * moveDirection.z,
											  0.0f,
											- distanceBetweenUnits * moveDirection.x);

	float3 nextPosition( targetPositionCenter.x - distanceBetweenUnitsVector.x * 0.5f * static_cast<float>(GetCurrentSize()-1),
						 targetPositionCenter.y,
						 targetPositionCenter.z - distanceBetweenUnitsVector.z * 0.5f * static_cast<float>(GetCurrentSize()-1));

	for(auto unit : m_units)
	{
		Command c(commandId);
		c.PushPos(nextPosition);

		ai->Execute()->GiveOrder(&c, unit.id, "Group::MoveFight");
		ai->UnitTable()->SetUnitStatus( unit.id, task);

		nextPosition.x += distanceBetweenUnitsVector.x;
		nextPosition.z += distanceBetweenUnitsVector.z;
	}
}

float3 AAIGroup::DetermineDirectionToPosition(const float3& position) const
{
	const float3 groupPosition = GetGroupPos();
	const float dx = position.x - groupPosition.x;
	const float dz = position.z - groupPosition.z;
	const float invNorm = fastmath::isqrt_nosse(dx*dx+dz*dz);

	return float3(invNorm * dx, 0.0f, invNorm * dz);
}

bool AAIGroup::SufficientAttackPower() const
{
	//! @todo Check if this criteria are really sensible
	if(m_units.size() >= 3)
		return true;

	if(m_groupType.IsAntiAir())
	{
		if(ai->s_buildTree.GetCombatPower(m_groupDefId).GetValue(ETargetType::AIR) > AAIConstants::minCombatPowerForSoloAttack)
			return true;
	}
	else 
	{
		const AAITargetType& targetType = GetTargetType();

		if(targetType.IsSurface())
		{
			if(ai->s_buildTree.GetCombatPower(m_groupDefId).GetValue(ETargetType::SURFACE) > AAIConstants::minCombatPowerForSoloAttack)
				return true;
		}
		else if(targetType.IsFloater())
		{
			if(ai->s_buildTree.GetCombatPower(m_groupDefId).GetValue(ETargetType::FLOATER) > AAIConstants::minCombatPowerForSoloAttack)
				return true;
		}
		else if(targetType.IsSubmerged())
		{
			if(ai->s_buildTree.GetCombatPower(m_groupDefId).GetValue(ETargetType::SUBMERGED) > AAIConstants::minCombatPowerForSoloAttack)
				return true;
		}
	}
	
	return false;
}

bool AAIGroup::IsAvailableForAttack() const
{
	if(!m_attack && IsEntireGroupAtRallyPoint())
	{
		if( m_groupType.IsAssaultUnit() && SufficientAttackPower())
			return true;
		else if( m_groupType.IsAntiAir() && !m_groupType.IsAssaultUnit() )
			return true;
	}

	return false;
}

void AAIGroup::UnitIdle(UnitId unitId, AAIAttackManager* attackManager)
{
	if(ai->GetAICallback()->GetCurrentFrame() - lastCommandFrame < 10)
		return;

	// special behaviour of aircraft in non air only mods
	/*if(m_category.IsAirCombat() && (m_task != GROUP_IDLE))
	{
		Command c(CMD_MOVE);
		c.PushPos(m_rallyPoint);

		GiveOrderToGroup(&c, 100, MOVING, "Group::Idle_a");

		m_task = GROUP_IDLE;
	}*/
	// behaviour of all other categories
	if(m_attack)
	{
		//check if idle unit is in target sector
		const float3 pos = ai->GetAICallback()->GetUnitPos(unitId.id);
		const AAISector *sector = ai->Map()->GetSectorOfPos(pos);

		if( (sector == m_targetSector) || (m_targetSector == nullptr) )
		{
			// combat groups
			if(ai->s_buildTree.GetUnitType(m_groupDefId).IsAssaultUnit() && m_attack->HasTargetBeenCleared() )
			{
				ai->Log("Combat group idle - checking for next sector to attack\n");
				attackManager->AttackNextSectorOrAbort(m_attack);
				return;
			}
			// unit the aa group was guarding has been killed
			else if(ai->s_buildTree.GetUnitType(m_groupDefId).IsAntiAir())
			{
				if(!m_attack->m_combatUnitGroups.empty())
				{
					const UnitId guardedUnitId = (*m_attack->m_combatUnitGroups.begin())->GetRandomUnit();

					if(guardedUnitId.IsValid())
					{
						Command c(CMD_GUARD);
						c.PushParam(guardedUnitId.id);

						GiveOrderToGroup(&c, AAIConstants::defendUnitsUrgency, GUARDING, "Group::Idle_b");
					}
				}
				else
					m_attack->StopAttack();
			}
		}
		else
		{
			// idle assault units are ordered to attack the current target sector
			if(ai->s_buildTree.GetUnitType(m_groupDefId).IsAssaultUnit())
			{
				Command c(CMD_FIGHT);

				const float3 attackPosition = m_targetSector->DetermineAttackPosition();
				c.PushPos(attackPosition);

				// move group to that sector
				ai->Execute()->GiveOrder(&c, unitId.id, "Group::Idle_c");
				ai->UnitTable()->SetUnitStatus(unitId.id, UNIT_ATTACKING);
			}
		}
	}
	else if( (m_task == GROUP_RETREATING) || (m_task == GROUP_DEFENDING) ) 
	{
		//check if retreating units is in target sector
		const float3 pos = ai->GetAICallback()->GetUnitPos(unitId.id);

		const AAISector* temp = ai->Map()->GetSectorOfPos(pos);

		if(temp == m_targetSector || !m_targetSector)
			m_task = GROUP_IDLE;
	}
}

void AAIGroup::AirRaidTarget(UnitId unitId, const float3& position, float importance)
{
	int commandId;

	if(m_groupType.IsAntiStatic() )
	{
		commandId = CMD_ATTACK;
		m_task = GROUP_BOMBING;
	}
	else
	{
		commandId = CMD_FIGHT;
		m_task = GROUP_ATTACKING;
	}

	Command c(commandId);
	c.PushPos(position);
	GiveOrderToGroup(&c, importance, UNIT_ATTACKING, "Group::AirRaidTarget");
	ai->UnitTable()->SetEnemyUnitAsTargetOfGroup(unitId, this);
}

void AAIGroup::DefendAirSpace(const float3& position, float importance)
{
	Command c(CMD_PATROL);
	c.PushPos(position);

	GiveOrderToGroup(&c, importance, UNIT_ATTACKING, "Group::DefendAirSpace");

	m_task = GROUP_PATROLING;
}

void AAIGroup::AirRaidUnit(UnitId unitId, float importance)
{
	Command c(CMD_ATTACK);
	c.PushParam(unitId.id);

	GiveOrderToGroup(&c, importance, UNIT_ATTACKING, "Group::AirRaidUnit");

	ai->UnitTable()->SetEnemyUnitAsTargetOfGroup(unitId, this);

	m_task = GROUP_ATTACKING;
}

void AAIGroup::CheckUpdateOfRallyPoint()
{
	const AAISector *sector = ai->Map()->GetSectorOfPos(m_rallyPoint);

	// check if rally point lies within base (e.g. AAI has expanded its base after rally point had been set)
	if(sector->GetDistanceToBase() <= 0)
		UpdateRallyPoint();

	//! @todo check if rally point is blocked by building
}

void AAIGroup::UpdateRallyPoint()
{
	//-----------------------------------------------------------------------------------------------------------------
	// determine rally point in sector close to base
	//-----------------------------------------------------------------------------------------------------------------
	AAISector* bestSector(nullptr);
	AAISector* secondBestSector(nullptr);

	float highestRating(0.0f);

	for(int i = 1; i <= 2; ++i)
	{
		for(auto sector : ai->Brain()->m_sectorsInDistToBase[i])
		{
			const float rating = sector->GetRatingForRallyPoint(m_moveType, m_continentId);
			
			if(rating > highestRating)
			{
				highestRating    = rating;
				secondBestSector = bestSector;
				bestSector       = sector;
			}
		}
	}

	// continent bound units must get a rally point on their current continent
	const int useContinentID = m_moveType.CannotMoveToOtherContinents() ? m_continentId : AAIMap::ignoreContinentID;

	if(bestSector)
	{
		m_rallyPoint = bestSector->DetermineUnitMovePos(m_moveType, useContinentID);

		if((m_rallyPoint.x == 0.0f) && secondBestSector)
			m_rallyPoint = secondBestSector->DetermineUnitMovePos(m_moveType, useContinentID);
	}

	//-----------------------------------------------------------------------------------------------------------------
	// send units to new rally point (if one has been found)
	//-----------------------------------------------------------------------------------------------------------------
	if(m_rallyPoint.x > 0.0f)
	{
		// send idle groups to new rally point
		if(m_task == GROUP_IDLE)
		{
			GiveMoveOrderToGroup(CMD_MOVE, UnitTask::HEADING_TO_RALLYPOINT, m_rallyPoint, AAIConstants::rallyDistanceBetweenUnitsInGroup);
			m_urgencyOfCurrentTask = 0.0f;
		}
	}
	else
	{
		ai->Log("Failed to determine rally point for goup of unit type %s!\n", ai->s_buildTree.GetUnitTypeProperties(m_groupDefId).m_name.c_str());
	}
}
