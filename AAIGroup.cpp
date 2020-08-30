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
		maxSize = cfg->MAX_AIR_GROUP_SIZE;
	}
	else if(ai->s_buildTree.GetUnitType(unitDefId).IsAntiAir() )
			maxSize = cfg->MAX_ANTI_AIR_GROUP_SIZE;
	else
	{
		if(m_category.isMobileArtillery())
			maxSize = cfg->MAX_ARTY_GROUP_SIZE;
		else if(m_category.isAirCombat())
			maxSize = cfg->MAX_AIR_GROUP_SIZE;
		else if(m_category.isSeaCombat())
			maxSize = cfg->MAX_NAVAL_GROUP_SIZE;
		else if(m_category.isSubmarineCombat())
			maxSize = cfg->MAX_SUBMARINE_GROUP_SIZE;
		else
			maxSize = cfg->MAX_GROUP_SIZE;
	}

	size = 0;

	task_importance = 0;
	task = GROUP_IDLE;

	lastCommand = Command(CMD_STOP);
	lastCommandFrame = 0;

	target_sector = 0;

	// get a rally point
	GetNewRallyPoint();

	ai->Log("Creating new group - max size: %i   unit type: %s   continent: %i\n", maxSize, ai->s_buildTree.GetUnitTypeProperties(m_groupDefId).m_name.c_str(), m_continentId);
}

AAIGroup::~AAIGroup(void)
{
	if(attack)
	{
		attack->RemoveGroup(this);
		attack = 0;
	}

	units.clear();

	if(m_rallyPoint.x > 0)
	{
		AAISector *sector = ai->Getmap()->GetSectorOfPos(m_rallyPoint);

		--sector->rally_points;
	}
}

bool AAIGroup::AddUnit(UnitId unitId, UnitDefId unitDefId, int continentId)
{
	// for continent bound units: check if unit is on the same continent as the group
	if(continentId == m_continentId)
	{
		//check if type match && current size
		if( (m_groupDefId.id == unitDefId.id) && (units.size() < maxSize) && !attack && (task != GROUP_ATTACKING) && (task != GROUP_BOMBING))
		{
			units.push_back(int2(unitId.id, unitDefId.id));
			++size;

			// send unit to rally point of the group
			if(m_rallyPoint.x > 0)
			{
				Command c(CMD_MOVE);
				c.PushPos(m_rallyPoint);

				if(m_category.isAirCombat() )
					c.SetOpts(c.GetOpts() | SHIFT_KEY);

				//ai->Getcb()->GiveOrder(unit_id, &c);
				ai->Getexecute()->GiveOrder(&c, unitId.id, "Group::AddUnit");
			}

			return true;
		}
	}

	return false;
}

bool AAIGroup::RemoveUnit(int unit, int attacker)
{
	// look for unit with that id
	for(std::list<int2>::iterator i = units.begin(); i != units.end(); ++i)
	{
		if(i->x == unit)
		{
			units.erase(i);
			--size;

			// remove from list of attacks groups if too less units
			if(attack)
			{	
				if(units.size() == 0)
				{
					attack->RemoveGroup(this);
					attack = nullptr;
				}
				else
					ai->Getam()->CheckAttack(attack);
			}
				
			if(UnitId(attacker).IsValid())
			{
				const springLegacyAI::UnitDef *def = ai->GetAICallback()->GetUnitDef(attacker);

				if(def && !cfg->AIR_ONLY_MOD)
				{
					UnitDefId attackerUnitDefId(def->id);
					const AAIUnitCategory& category    = ai->s_buildTree.GetUnitCategory(attackerUnitDefId);
					const AAICombatPower&  combatPower = ai->s_buildTree.GetCombatPower(attackerUnitDefId);

					if(category.isStaticDefence())
						ai->Getaf()->CheckTarget( UnitId(attacker), category, def->health);
					else if( category.isGroundCombat() && (combatPower.GetCombatPowerVsTargetType(ETargetType::SURFACE) > cfg->MIN_AIR_SUPPORT_EFFICIENCY) )
						ai->Getaf()->CheckTarget( UnitId(attacker), category, def->health);
					else if( category.isSeaCombat()    && (combatPower.GetCombatPowerVsTargetType(ETargetType::FLOATER) > cfg->MIN_AIR_SUPPORT_EFFICIENCY) )
						ai->Getaf()->CheckTarget( UnitId(attacker), category, def->health);
					else if( category.isHoverCombat()  && (combatPower.GetCombatPowerVsTargetType(ETargetType::SURFACE) > cfg->MIN_AIR_SUPPORT_EFFICIENCY) )
						ai->Getaf()->CheckTarget( UnitId(attacker), category, def->health);
				}
			}

			return true;
		}
	}

	// unit not found
	return false;
}

void AAIGroup::GiveOrderToGroup(Command *c, float importance, UnitTask task, const char *owner)
{
	lastCommandFrame = ai->GetAICallback()->GetCurrentFrame();

	task_importance = importance;

	for(std::list<int2>::iterator i = units.begin(); i != units.end(); ++i)
	{
		//ai->Getcb()->GiveOrder(i->x, c);
		ai->Getexecute()->GiveOrder(c, i->x, owner);
		ai->Getut()->SetUnitStatus(i->x, task);
	}
}

void AAIGroup::Update()
{
	task_importance *= 0.97f;

	// attacking groups recheck target
	if(task == GROUP_ATTACKING && target_sector)
	{
		if(target_sector->GetNumberOfEnemyBuildings() <= 0)
		{
			task = GROUP_IDLE;
			target_sector = 0;
		}
	}

	// idle empty groups so they can be filled with units again...
	if(units.empty())
	{
		target_sector = 0;
		task = GROUP_IDLE;
	}

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
	const float combatPower = ai->s_buildTree.GetCombatPower(m_groupDefId).GetCombatPowerVsTargetType(targetType);
	return static_cast<float>(units.size()) * combatPower;
}

const AAITargetType& AAIGroup::GetTargetType() const
{
	return ai->s_buildTree.GetTargetType(m_groupDefId);
}

float3 AAIGroup::GetGroupPos()
{
	if(!units.empty())
	{
		return ai->GetAICallback()->GetUnitPos(units.begin()->x);
	}
	else
		return ZeroVector;
}

void AAIGroup::TargetUnitKilled()
{
	// behaviour of normal mods
	if(!cfg->AIR_ONLY_MOD)
	{
		// air groups retreat to rally point
		if(m_category.isAirCombat())
		{
			Command c(CMD_MOVE);
			c.PushPos(m_rallyPoint);

			GiveOrderToGroup(&c, 90, MOVING, "Group::TargetUnitKilled");
		}
	}
}

void AAIGroup::DeterminePositionForAttackOrder(Command& c, const AAISector* targetSector, const float3& currentUnitPos) const
{
	c.PushPos(currentUnitPos);

	const int group_x = currentUnitPos.x/ai->Getmap()->xSectorSize;
	const int group_y = currentUnitPos.z/ai->Getmap()->ySectorSize;

	// choose location that way that attacking units must cross the entire sector
	if(targetSector->x > group_x)
		c.SetParam(0, (targetSector->left + 7.0f * targetSector->right)/8.0f);
	else if(targetSector->x < group_x)
		c.SetParam(0, (7 * targetSector->left + targetSector->right)/8.0f);
	else
		c.SetParam(0, (targetSector->left + targetSector->right)/2.0f);

	if(targetSector->y > group_y)
		c.SetParam(2, (7.0f * targetSector->bottom + targetSector->top)/8.0f);
	else if(targetSector->y < group_y)
		c.SetParam(2, (targetSector->bottom + 7.0f * targetSector->top)/8.0f);
	else
		c.SetParam(2, (targetSector->bottom + targetSector->top)/2.0f);

	c.SetParam(1, ai->GetAICallback()->GetElevation(c.GetParam(0), c.GetParam(2)));
}

void AAIGroup::AttackSector(const AAISector *sector, float importance)
{
	const float3 pos = GetGroupPos();
	Command c(CMD_FIGHT);

	// get position of the group
	DeterminePositionForAttackOrder(c, sector, pos);

	// move group to that sector
	GiveOrderToGroup(&c, importance + 8, UNIT_ATTACKING, "Group::AttackSector");

	target_sector = sector;
	task = GROUP_ATTACKING;
}

void AAIGroup::Defend(UnitId unitId, const float3& enemyPosition, int importance)
{
	Command cmd((enemyPosition == ZeroVector) ? CMD_FIGHT: CMD_GUARD);

	if(enemyPosition != ZeroVector)
	{
		cmd.PushPos(enemyPosition);

		GiveOrderToGroup(&cmd, importance, DEFENDING, "Group::Defend");

		target_sector = ai->Getmap()->GetSectorOfPos(enemyPosition);
	}
	else
	{
		cmd.PushParam(unitId.id);

		GiveOrderToGroup(&cmd, importance, GUARDING, "Group::Defend");

		const float3 pos = ai->GetAICallback()->GetUnitPos(unitId.id);

		target_sector = ai->Getmap()->GetSectorOfPos(pos);
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
	target_sector = ai->Getmap()->GetSectorOfPos(pos);
}

int AAIGroup::GetRandomUnit()
{
	if(units.empty())
		return -1;
	else
		return units.begin()->x;
}

bool AAIGroup::SufficientAttackPower() const
{
	//! @todo Check if this criteria are really sensible
	if(units.size() >= 3)
		return true;

	if(m_groupType.IsAntiAir())
	{
		if(ai->s_buildTree.GetCombatPower(m_groupDefId).GetCombatPowerVsTargetType(ETargetType::AIR) > AAIConstants::minCombatPowerForSoloAttack)
			return true;
	}
	else 
	{
		const AAITargetType& targetType = GetTargetType();

		if(targetType.IsSurface())
		{
			if(ai->s_buildTree.GetCombatPower(m_groupDefId).GetCombatPowerVsTargetType(ETargetType::SURFACE) > AAIConstants::minCombatPowerForSoloAttack)
				return true;
		}
		else if(targetType.IsFloater())
		{
			if(ai->s_buildTree.GetCombatPower(m_groupDefId).GetCombatPowerVsTargetType(ETargetType::FLOATER) > AAIConstants::minCombatPowerForSoloAttack)
				return true;
		}
		else if(targetType.IsSubmerged())
		{
			if(ai->s_buildTree.GetCombatPower(m_groupDefId).GetCombatPowerVsTargetType(ETargetType::SUBMERGED) > AAIConstants::minCombatPowerForSoloAttack)
				return true;
		}
	}
	
	return false;
}

bool AAIGroup::AvailableForAttack()
{
	if(!attack)
	{
		if( m_groupType.IsAssaultUnit() && SufficientAttackPower())
			return true;
		else if( m_groupType.IsAntiAir() )
			return true;
	}
	
	return false;
}

void AAIGroup::UnitIdle(int unit)
{
	if(ai->GetAICallback()->GetCurrentFrame() - lastCommandFrame < 10)
		return;

	// special behaviour of aircraft in non air only mods
	if(m_category.isAirCombat() && (task != GROUP_IDLE) && !cfg->AIR_ONLY_MOD)
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

		if( (sector == target_sector) || (target_sector == nullptr) )
		{
			// combat groups
			if(ai->s_buildTree.GetUnitType(m_groupDefId).IsAssaultUnit() && (attack->m_attackDestination->GetNumberOfEnemyBuildings() <= 0) )
			{
				ai->Log("Combat group idle - checking for next sector to attack\n");
				ai->Getam()->TryAttackOfNextSector(attack);
				return;
			}
			// unit the aa group was guarding has been killed
			else if(ai->s_buildTree.GetUnitType(m_groupDefId).IsAntiAir())
			{
				if(!attack->combat_groups.empty())
				{
					int unit = (*attack->combat_groups.begin())->GetRandomUnit();

					if(unit >= 0)
					{
						Command c(CMD_GUARD);
						c.PushParam(unit);

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

				// get position of the group
				DeterminePositionForAttackOrder(c, target_sector, pos);

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

		if(temp == target_sector || !target_sector)
			task = GROUP_IDLE;
	}
	else if(task == GROUP_DEFENDING)
	{
		//check if retreating units is in target sector
		float3 pos = ai->GetAICallback()->GetUnitPos(unit);

		AAISector *temp = ai->Getmap()->GetSectorOfPos(pos);

		if(temp == target_sector || !target_sector)
			task = GROUP_IDLE;
	}
}

void AAIGroup::BombTarget(int target_id, float3 *target_pos)
{
	Command c(CMD_ATTACK);
	c.PushPos(*target_pos);

	GiveOrderToGroup(&c, 110, UNIT_ATTACKING, "Group::BombTarget");

	ai->Getut()->AssignGroupToEnemy(target_id, this);

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
	if(sector->distance_to_base <= 0)
		GetNewRallyPoint();

	// check if rally point is blocked by building


}

void AAIGroup::GetNewRallyPoint()
{
	AAISector *sector;

	// delete old rally point (if there is any)
	if(m_rallyPoint.x > 0)
	{
		sector = ai->Getmap()->GetSectorOfPos(m_rallyPoint);

		--sector->rally_points;
	}

	const bool rallyPointFound = ai->Getbrain()->DetermineRallyPoint(m_rallyPoint, m_moveType, m_continentId);

	if(rallyPointFound)
	{
		//add new rally point to sector
		sector = ai->Getmap()->GetSectorOfPos(m_rallyPoint);
		++sector->rally_points;

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
