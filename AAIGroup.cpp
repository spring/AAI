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
	m_targetPosition(ZeroVector),
	m_targetSector(nullptr),
	m_rallyPoint(ZeroVector),
	m_continentId(continentId)
{
	this->ai = ai;

	attack = nullptr;

	m_groupType = ai->s_buildTree.GetUnitType(m_groupDefId); 
	m_category  = ai->s_buildTree.GetUnitCategory(unitDefId);
	
	// set movement type of group (filter out add. movement info like underwater, floater, etc.)
	m_moveType = ai->s_buildTree.GetMovementType( unitDefId );

	// now we know type and category, determine max group size
	if(cfg->AIR_ONLY_MOD)
	{
		m_maxSize = cfg->MAX_AIR_GROUP_SIZE;
	}
	else if(m_groupType.IsAntiAir() && !m_groupType.IsAntiSurface())
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

	task_importance = 0;
	task = GROUP_IDLE;

	lastCommand = Command(CMD_STOP);
	lastCommandFrame = 0;

	// get a rally point
	GetNewRallyPoint();

	ai->Log("Creating new group - max size: %i   unit type: %s   continent: %i\n", m_maxSize, ai->s_buildTree.GetUnitTypeProperties(m_groupDefId).m_name.c_str(), m_continentId);
}

AAIGroup::~AAIGroup(void)
{
	if(attack)
	{
		attack->RemoveGroup(this);
		attack = nullptr;
	}

	m_units.clear();
}

bool AAIGroup::AddUnit(UnitId unitId, UnitDefId unitDefId, int continentId)
{
	if(    (m_continentId == continentId) // for continent bound units: check if unit is on the same continent as the group
		&& (m_groupDefId  == unitDefId) 
		&& (GetCurrentSize() < m_maxSize)
		&& (attack == nullptr)
		&& (task != GROUP_ATTACKING) && (task != GROUP_BOMBING))
	{
		m_units.push_back(unitId);

		// send unit to rally point of the group
		if(m_rallyPoint.x > 0.0f)
		{
			Command c(CMD_MOVE);
			c.PushPos(m_rallyPoint);

			if(m_category.IsAirCombat() )
				c.SetOpts(c.GetOpts() | SHIFT_KEY);

			//ai->Getcb()->GiveOrder(unit_id, &c);
			ai->Getexecute()->GiveOrder(&c, unitId.id, "Group::AddUnit");
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
				task   = GROUP_IDLE;

				if(attack)
				{
					attack->RemoveGroup(this);
					attack = nullptr;		
				}
			}

			if(attackerUnitId.IsValid() && (newGroupSize > 0) )
			{
				const UnitDefId attackerDefId = ai->GetUnitDefId(attackerUnitId);

				if(attackerDefId.IsValid() && !cfg->AIR_ONLY_MOD)
				{
					const AAIUnitCategory& category    = ai->s_buildTree.GetUnitCategory(attackerDefId);
					const TargetTypeValues&  combatPower = ai->s_buildTree.GetCombatPower(attackerDefId);

					if(category.IsStaticDefence())
						ai->Getaf()->CheckTarget( attackerUnitId, category, ai->s_buildTree.GetHealth(attackerDefId));
					else if( category.IsGroundCombat() && (combatPower.GetValue(ETargetType::SURFACE) > cfg->MIN_AIR_SUPPORT_EFFICIENCY) )
						ai->Getaf()->CheckTarget( attackerUnitId, category, ai->s_buildTree.GetHealth(attackerDefId));
					else if( category.IsSeaCombat()    && (combatPower.GetValue(ETargetType::FLOATER) > cfg->MIN_AIR_SUPPORT_EFFICIENCY) )
						ai->Getaf()->CheckTarget( attackerUnitId, category, ai->s_buildTree.GetHealth(attackerDefId));
					else if( category.IsHoverCombat()  && (combatPower.GetValue(ETargetType::SURFACE) > cfg->MIN_AIR_SUPPORT_EFFICIENCY) )
						ai->Getaf()->CheckTarget( attackerUnitId, category, ai->s_buildTree.GetHealth(attackerDefId));
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

	task_importance = importance;

	for(auto unit = m_units.begin(); unit != m_units.end(); ++unit)
	{
		//ai->Getcb()->GiveOrder(i->x, c);
		ai->Getexecute()->GiveOrder(c, (*unit).id, owner);
		ai->Getut()->SetUnitStatus( (*unit).id, task);
	}
}

void AAIGroup::Update()
{
	task_importance *= 0.97f;

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
	if(task == GROUP_ATTACKING)
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

float AAIGroup::GetCombatPowerVsTargetType(const AAITargetType& targetType) const
{
	const float combatPower = ai->s_buildTree.GetCombatPower(m_groupDefId).GetValue(targetType);
	return static_cast<float>(m_units.size()) * combatPower;
}

void AAIGroup::AddGroupCombatPower(TargetTypeValues& combatPower) const
{
	const float numberOfUnits = static_cast<float>(m_units.size());

	combatPower.AddValues(ai->s_buildTree.GetCombatPower(m_groupDefId), numberOfUnits);
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
		const bool groupAvailble = (task == GROUP_IDLE) || (task_importance < importance); //!(*group)->attack

		if(matchingType && groupAvailble)
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
	// behaviour of normal mods
	if(!cfg->AIR_ONLY_MOD)
	{
		// air groups retreat to rally point
		if(m_category.IsAirCombat())
		{
			Command c(CMD_MOVE);
			c.PushPos(m_rallyPoint);

			GiveOrderToGroup(&c, 90, MOVING, "Group::TargetUnitKilled");
		}
	}
}

void AAIGroup::AttackSector(const AAISector *sector, float importance)
{
	Command c(CMD_FIGHT);

	const float3 attackPosition = sector->DetermineAttackPosition();
	c.PushPos(attackPosition);

	// move group to that sector
	GiveOrderToGroup(&c, importance + 8.0f, UNIT_ATTACKING, "Group::AttackSector");

	m_targetPosition = attackPosition;
	m_targetSector   = sector;
	task = GROUP_ATTACKING;
}

void AAIGroup::Defend(UnitId unitId, const float3& enemyPosition, int importance)
{
	const bool enemyPositionKnown = (enemyPosition.x > 0.0f);

	Command cmd(enemyPositionKnown ? CMD_FIGHT: CMD_GUARD);

	if(enemyPositionKnown)
	{
		cmd.PushPos(enemyPosition);

		GiveOrderToGroup(&cmd, importance, DEFENDING, "Group::Defend");

		m_targetPosition = enemyPosition;
		m_targetSector   = ai->Getmap()->GetSectorOfPos(enemyPosition);
	}
	else
	{
		cmd.PushParam(unitId.id);

		GiveOrderToGroup(&cmd, importance, GUARDING, "Group::Defend");

		const float3 pos = ai->GetAICallback()->GetUnitPos(unitId.id);

		m_targetPosition = pos;
		m_targetSector   = ai->Getmap()->GetSectorOfPos(pos);
	}

	task = GROUP_DEFENDING;
}

void AAIGroup::Retreat(const float3& pos)
{
	this->task = GROUP_RETREATING;

	Command c(CMD_MOVE);
	c.PushPos(pos);

	GiveOrderToGroup(&c, 105, MOVING, "Group::Retreat");

	// set new dest sector
	m_targetPosition = pos;
	m_targetSector   = ai->Getmap()->GetSectorOfPos(pos);
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

bool AAIGroup::IsAvailableForAttack()
{
	if(!attack && IsEntireGroupAtRallyPoint())
	{
		if( m_groupType.IsAssaultUnit() && SufficientAttackPower())
			return true;
		else if( m_groupType.IsAntiAir() && !m_groupType.IsAssaultUnit() )
			return true;
	}

	return false;
}

void AAIGroup::UnitIdle(int unit)
{
	if(ai->GetAICallback()->GetCurrentFrame() - lastCommandFrame < 10)
		return;

	// special behaviour of aircraft in non air only mods
	if(m_category.IsAirCombat() && (task != GROUP_IDLE) && !cfg->AIR_ONLY_MOD)
	{
		Command c(CMD_MOVE);
		c.PushPos(m_rallyPoint);

		GiveOrderToGroup(&c, 100, MOVING, "Group::Idle_a");

		task = GROUP_IDLE;
	}
	// behaviour of all other categories
	else if(attack)
	{
		//check if idle unit is in target sector
		const float3 pos = ai->GetAICallback()->GetUnitPos(unit);
		const AAISector *sector = ai->Getmap()->GetSectorOfPos(pos);

		if( (sector == m_targetSector) || (m_targetSector == nullptr) )
		{
			// combat groups
			if(ai->s_buildTree.GetUnitType(m_groupDefId).IsAssaultUnit() && attack->HasTargetBeenCleared() )
			{
				ai->Log("Combat group idle - checking for next sector to attack\n");
				ai->Getam()->AttackNextSectorOrAbort(attack);
				return;
			}
			// unit the aa group was guarding has been killed
			else if(ai->s_buildTree.GetUnitType(m_groupDefId).IsAntiAir())
			{
				if(!attack->m_combatUnitGroups.empty())
				{
					UnitId unitId = (*attack->m_combatUnitGroups.begin())->GetRandomUnit();

					if(unitId.IsValid())
					{
						Command c(CMD_GUARD);
						c.PushParam(unitId.id);

						GiveOrderToGroup(&c, 110, GUARDING, "Group::Idle_b");
					}
				}
				else
					attack->StopAttack();
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
				ai->Getexecute()->GiveOrder(&c, unit, "Group::Idle_c");
				ai->Getut()->SetUnitStatus(unit, UNIT_ATTACKING);
			}
		}
	}
	else if(task == GROUP_RETREATING)
	{
		//check if retreating units is in target sector
		float3 pos = ai->GetAICallback()->GetUnitPos(unit);

		AAISector *temp = ai->Getmap()->GetSectorOfPos(pos);

		if(temp == m_targetSector || !m_targetSector)
			task = GROUP_IDLE;
	}
	else if(task == GROUP_DEFENDING)
	{
		//check if retreating units is in target sector
		float3 pos = ai->GetAICallback()->GetUnitPos(unit);

		AAISector *temp = ai->Getmap()->GetSectorOfPos(pos);

		if(temp == m_targetSector || !m_targetSector)
			task = GROUP_IDLE;
	}
}

void AAIGroup::BombTarget(UnitId unitId, const float3& position)
{
	Command c(CMD_ATTACK);
	c.PushPos(position);

	GiveOrderToGroup(&c, 110, UNIT_ATTACKING, "Group::BombTarget");

	ai->Getut()->AssignGroupToEnemy(unitId.id, this);

	task = GROUP_BOMBING;
}

void AAIGroup::DefendAirSpace(float3 *pos)
{
	Command c(CMD_PATROL);
	c.PushPos(*pos);

	GiveOrderToGroup(&c, 110, UNIT_ATTACKING, "Group::DefendAirSpace");

	task = GROUP_PATROLING;
}

void AAIGroup::AirRaidUnit(int unit_id)
{
	Command c(CMD_ATTACK);
	c.PushParam(unit_id);

	GiveOrderToGroup(&c, 110, UNIT_ATTACKING, "Group::AirRaidUnit");

	ai->Getut()->AssignGroupToEnemy(unit_id, this);

	task = GROUP_ATTACKING;
}

void AAIGroup::UpdateRallyPoint()
{
	AAISector *sector = ai->Getmap()->GetSectorOfPos(m_rallyPoint);

	// check if rally point lies within base (e.g. AAI has expanded its base after rally point had been set)
	if(sector->GetDistanceToBase() <= 0)
		GetNewRallyPoint();

	//! @todo check if rally point is blocked by building
}

void AAIGroup::GetNewRallyPoint()
{
	//-----------------------------------------------------------------------------------------------------------------
	// determine rally point in sector close to base
	//-----------------------------------------------------------------------------------------------------------------
	AAISector* bestSector(nullptr);
	AAISector* secondBestSector(nullptr);

	float highestRating(0.0f);

	for(int i = 1; i <= 2; ++i)
	{
		for(auto sector : ai->Getbrain()->m_sectorsInDistToBase[i])
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
		if(task == GROUP_IDLE)
		{
			Command c(CMD_MOVE);
			c.PushPos(m_rallyPoint);

			GiveOrderToGroup(&c, 90, HEADING_TO_RALLYPOINT, "Group::RallyPoint");
		}
	}
	else
	{
		ai->Log("Failed to determine rally point for goup of unit type %s!\n", ai->s_buildTree.GetUnitTypeProperties(m_groupDefId).m_name.c_str());
	}
}
