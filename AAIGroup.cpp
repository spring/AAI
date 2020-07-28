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

	category = ai->Getbt()->s_buildTree.GetUnitCategory(unitDefId);
	combat_category = ai->Getbt()->GetIDOfAssaultCategory(category);

	// set unit type of group
	group_unit_type = ai->Getbt()->GetUnitType(unitDefId.id);

	// set movement type of group (filter out add. movement info like underwater, floater, etc.)
	m_moveType = ai->Getbt()->s_buildTree.GetMovementType( unitDefId );

	// now we know type and category, determine max group size
	if(cfg->AIR_ONLY_MOD)
	{
		maxSize = cfg->MAX_AIR_GROUP_SIZE;
	}
	else if(group_unit_type == ANTI_AIR_UNIT)
			maxSize = cfg->MAX_ANTI_AIR_GROUP_SIZE;
	else
	{
		if(category.isMobileArtillery())
			maxSize = cfg->MAX_ARTY_GROUP_SIZE;
		else if(category.isAirCombat())
			maxSize = cfg->MAX_AIR_GROUP_SIZE;
		else if(category.isSeaCombat())
			maxSize = cfg->MAX_NAVAL_GROUP_SIZE;
		else if(category.isSubmarineCombat())
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

	ai->Log("Creating new group - max size: %i   unit type: %s   continent: %i\n", maxSize, ai->Getbt()->s_buildTree.GetUnitTypeProperties(m_groupDefId).m_name.c_str(), m_continentId);
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

				if(category.isAirCombat() == false)
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
	for(list<int2>::iterator i = units.begin(); i != units.end(); ++i)
	{
		if(i->x == unit)
		{
			units.erase(i);
			--size;

			// remove from list of attacks groups if too less units
			if(attack)
			{
				if(group_unit_type == ASSAULT_UNIT)
				{
					// todo: improve criteria
					if(size < 2)
					{
						attack->RemoveGroup(this);
						attack = 0;
					}
				}
				else if(group_unit_type == ANTI_AIR_UNIT)
				{
					if(size < 1)
					{
						attack->RemoveGroup(this);
						attack = 0;
					}
				}
			}

			// check if attacking still sensible
			if(attack)
				ai->Getam()->CheckAttack(attack);

			if(attacker)
			{
				const springLegacyAI::UnitDef *def = ai->Getcb()->GetUnitDef(attacker);

				if(def && !cfg->AIR_ONLY_MOD)
				{
					UnitDefId attackerUnitDefId(def->id);
					const AAIUnitCategory& category = ai->Getbt()->s_buildTree.GetUnitCategory(attackerUnitDefId);

					if(category.isStaticDefence() == true)
						ai->Getaf()->CheckTarget( UnitId(attacker), category, def->health);
					else if( (category.isGroundCombat() == true) && (ai->Getbt()->units_static[def->id].efficiency[0] > cfg->MIN_AIR_SUPPORT_EFFICIENCY) )
						ai->Getaf()->CheckTarget( UnitId(attacker), category, def->health);
					else if( (category.isSeaCombat() == true)    && (ai->Getbt()->units_static[def->id].efficiency[3] > cfg->MIN_AIR_SUPPORT_EFFICIENCY) )
						ai->Getaf()->CheckTarget( UnitId(attacker), category, def->health);
					else if( (category.isHoverCombat() == true)  && (ai->Getbt()->units_static[def->id].efficiency[2] > cfg->MIN_AIR_SUPPORT_EFFICIENCY) )
						ai->Getaf()->CheckTarget( UnitId(attacker), category, def->health);
					else if(category.isAirCombat() == true)
					{
						float3 enemy_pos = ai->Getcb()->GetUnitPos(attacker);

						// get a random unit of the group
						int unit = GetRandomUnit();

						if(unit)
							ai->Getexecute()->DefendUnitVS(unit, ai->Getbt()->s_buildTree.GetMovementType(UnitDefId(def->id)), &enemy_pos, 100);
					}
				}
			}

			return true;
		}
	}

	// unit not found
	return false;
}

void AAIGroup::GiveOrder(Command *c, float importance, UnitTask task, const char *owner)
{
	lastCommandFrame = ai->Getcb()->GetCurrentFrame();

	task_importance = importance;

	for(list<int2>::iterator i = units.begin(); i != units.end(); ++i)
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
		if(target_sector->enemy_structures <= 0.001f)
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

		for(list<int2>::iterator unit = units.begin(); unit != units.end(); ++unit)
		{
			range = ai->Getbt()->s_buildTree.GetMaxRange(UnitDefId(unit->y));

			if(range > cfg->MIN_FALLBACK_RANGE)
			{
				ai->Getexecute()->GetFallBackPos(&pos, unit->x, range);

				if(pos.x > 0)
				{
					c = Command(CMD_MOVE);
					c.PushParam(pos.x);
					c.PushParam(ai->Getcb()->GetElevation(pos.x, pos.z));
					c.PushParam(pos.z);

					//ai->Getcb()->GiveOrder(unit->x, &c);
					ai->Getexecute()->GiveOrder(&c, unit->x, "GroupFallBack");
				}
			}
		}
	}
}

float AAIGroup::GetCombatPowerVsCategory(int assault_cat_id)
{
	float power = 0;

	for(list<int2>::iterator unit = units.begin(); unit != units.end(); ++unit)
		power += ai->Getbt()->units_static[unit->y].efficiency[assault_cat_id];

	return power;
}

void AAIGroup::GetCombatPower(vector<float> *combat_power)
{
	for(list<int2>::iterator unit = units.begin(); unit != units.end(); ++unit)
	{
		for(int cat = 0; cat < AAIBuildTable::combat_categories; ++cat)
			(*combat_power)[cat] += ai->Getbt()->units_static[unit->y].efficiency[cat];
	}
}

float3 AAIGroup::GetGroupPos()
{
	if(!units.empty())
	{
		return ai->Getcb()->GetUnitPos(units.begin()->x);
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
		if(category.isAirCombat())
		{
			Command c(CMD_MOVE);
			c.PushPos(m_rallyPoint);

			GiveOrder(&c, 90, MOVING, "Group::TargetUnitKilled");
		}
	}
}

void AAIGroup::AttackSector(AAISector *dest, float importance)
{
	float3 pos;
	Command c(CMD_FIGHT);

	// get position of the group
	c.PushPos(pos = GetGroupPos());

	int group_x = pos.x/ai->Getmap()->xSectorSize;
	int group_y = pos.z/ai->Getmap()->ySectorSize;

	//c.SetParam(0, 0.5f * (dest->left   + dest->right));
	//c.SetParam(2, 0.5f * (dest->bottom + dest->top)  );

	// choose location that way that attacking units must cross the entire sector
	if(dest->x > group_x)
		c.SetParam(0, (dest->left + 7.0f * dest->right)/8.0f);
	else if(dest->x < group_x)
		c.SetParam(0, (7 * dest->left + dest->right)/8.0f);
	else
		c.SetParam(0, (dest->left + dest->right)/2.0f);

	if(dest->y > group_y)
		c.SetParam(2, (7.0f * dest->bottom + dest->top)/8.0f);
	else if(dest->y < group_y)
		c.SetParam(2, (dest->bottom + 7.0f * dest->top)/8.0f);
	else
		c.SetParam(2, (dest->bottom + dest->top)/2.0f);

	c.SetParam(1, ai->Getcb()->GetElevation(c.GetParam(0), c.GetParam(2)));

	// move group to that sector
	GiveOrder(&c, importance + 8, UNIT_ATTACKING, "Group::AttackSector");

	target_sector = dest;
	task = GROUP_ATTACKING;
}

void AAIGroup::Defend(int unit, float3 *enemy_pos, int importance)
{
	Command cmd((enemy_pos != nullptr)? CMD_FIGHT: CMD_GUARD);

	if(enemy_pos)
	{
		cmd.PushPos(*enemy_pos);

		GiveOrder(&cmd, importance, DEFENDING, "Group::Defend");

		target_sector = ai->Getmap()->GetSectorOfPos(*enemy_pos);
	}
	else
	{
		cmd.PushParam(unit);

		GiveOrder(&cmd, importance, GUARDING, "Group::Defend");

		float3 pos = ai->Getcb()->GetUnitPos(unit);

		target_sector = ai->Getmap()->GetSectorOfPos(pos);
	}

	task = GROUP_DEFENDING;
}

void AAIGroup::Retreat(const float3& pos)
{
	this->task = GROUP_RETREATING;

	Command c(CMD_MOVE);
	c.PushPos(pos);

	GiveOrder(&c, 105, MOVING, "Group::Retreat");

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

bool AAIGroup::SufficientAttackPower()
{
	//! @todo Check if this criteria are really sensible
	if(units.size() >= 2) //maxSize - 1)
		return true;

	if(group_unit_type == ASSAULT_UNIT)
	{
		float avg_combat_power = 0;

		if(category.isGroundCombat())
		{
			float ground = 1.0f;
			float hover = 0.2f;

			for(list<int2>::iterator unit = units.begin(); unit != units.end(); ++unit)
				avg_combat_power += ground * ai->Getbt()->units_static[unit->y].efficiency[0] + hover * ai->Getbt()->units_static[unit->y].efficiency[2];

			if( avg_combat_power > (ground * ai->Getbt()->avg_eff[ai->GetSide()-1][0][0] + hover * ai->Getbt()->avg_eff[ai->GetSide()-1][0][2]) * (float)units.size() )
				return true;
		}
		else if(category.isHoverCombat())
		{
			float ground = 1.0f;
			float hover = 0.2f;
			float sea = 1.0f;

			for(list<int2>::iterator unit = units.begin(); unit != units.end(); ++unit)
				avg_combat_power += ground * ai->Getbt()->units_static[unit->y].efficiency[0] + hover * ai->Getbt()->units_static[unit->y].efficiency[2] + sea * ai->Getbt()->units_static[unit->y].efficiency[3];

			if( avg_combat_power > (ground * ai->Getbt()->avg_eff[ai->GetSide()-1][2][0] + hover * ai->Getbt()->avg_eff[ai->GetSide()-1][2][2] + sea * ai->Getbt()->avg_eff[ai->GetSide()-1][2][3]) * (float)units.size() )
				return true;
		}
		else if(category.isSeaCombat())
		{
			float hover = 0.3f;
			float sea = 1.0f;
			float submarine = 0.8f;

			for(list<int2>::iterator unit = units.begin(); unit != units.end(); ++unit)
				avg_combat_power += hover * ai->Getbt()->units_static[unit->y].efficiency[2] + sea * ai->Getbt()->units_static[unit->y].efficiency[3] + submarine * ai->Getbt()->units_static[unit->y].efficiency[4];

			if( avg_combat_power > (hover * ai->Getbt()->avg_eff[ai->GetSide()-1][3][2] + sea * ai->Getbt()->avg_eff[ai->GetSide()-1][3][3] + submarine * ai->Getbt()->avg_eff[ai->GetSide()-1][3][4]) * (float)units.size() )
				return true;
		}
		else if(category.isSubmarineCombat())
		{
			float sea = 1.0f;
			float submarine = 0.8f;

			for(list<int2>::iterator unit = units.begin(); unit != units.end(); ++unit)
				avg_combat_power += sea * ai->Getbt()->units_static[unit->y].efficiency[3] + submarine * ai->Getbt()->units_static[unit->y].efficiency[4];

			if( avg_combat_power > (sea * ai->Getbt()->avg_eff[ai->GetSide()-1][4][3] + submarine * ai->Getbt()->avg_eff[ai->GetSide()-1][4][4]) * (float)units.size() )
				return true;
		}
	}
	else
	{
		float avg_combat_power = 0;

		for(list<int2>::iterator unit = units.begin(); unit != units.end(); ++unit)
			avg_combat_power += ai->Getbt()->units_static[unit->y].efficiency[1];

		//! @todo check this
		//if(avg_combat_power > ai->Getbt()->avg_eff[ai->GetSide()-1][category][1] * (float)units.size())
			return true;
	}

	return false;
}

bool AAIGroup::AvailableForAttack()
{
	if(!attack)
	{
		if(group_unit_type == ASSAULT_UNIT)
		{
			if(SufficientAttackPower())
				return true;
			else
			{
				return false;
			}
		}
		else
			return true;
	}
	else
	{
		return false;
	}
}

void AAIGroup::UnitIdle(int unit)
{
	if(ai->Getcb()->GetCurrentFrame() - lastCommandFrame < 10)
		return;

	// special behaviour of aircraft in non air only mods
	if(category.isAirCombat() && (task != GROUP_IDLE) && !cfg->AIR_ONLY_MOD)
	{
		Command c(CMD_MOVE);
		c.PushPos(m_rallyPoint);

		GiveOrder(&c, 100, MOVING, "Group::Idle_a");

		task = GROUP_IDLE;
	}
	// behaviour of all other categories
	else if(attack)
	{
		//check if idle unit is in target sector
		float3 pos = ai->Getcb()->GetUnitPos(unit);

		const AAISector *temp = ai->Getmap()->GetSectorOfPos(pos);

		if(temp == target_sector || !target_sector)
		{
			// combat groups
			if(group_unit_type == ASSAULT_UNIT && attack->dest->enemy_structures <= 0.0f)
			{
				ai->Getam()->GetNextDest(attack);
				return;
			}
			// unit the aa group was guarding has been killed
			else if(group_unit_type == ANTI_AIR_UNIT)
			{
				if(!attack->combat_groups.empty())
				{
					int unit = (*attack->combat_groups.begin())->GetRandomUnit();

					if(unit >= 0)
					{
						Command c(CMD_GUARD);
						c.PushParam(unit);

						GiveOrder(&c, 110, GUARDING, "Group::Idle_b");
					}
				}
				else
					attack->StopAttack();
			}
		}
		else
		{
			// idle assault units are ordered to attack the current target sector
			if(group_unit_type == ASSAULT_UNIT)
			{
				Command c(CMD_FIGHT);

				// get position of the group
				c.PushPos(pos = ai->Getcb()->GetUnitPos(unit));

				int pos_x = pos.x/ai->Getmap()->xSectorSize;
				int pos_y = pos.z/ai->Getmap()->ySectorSize;

				c.SetParam(0, (target_sector->left + target_sector->right)/2);
				c.SetParam(2, (target_sector->bottom + target_sector->top)/2);

				// choose location that way that attacking units must cross the entire sector
				if(target_sector->x > pos_x)
					c.SetParam(0, (target_sector->left + 7 * target_sector->right)/8);
				else if(target_sector->x < pos_x)
					c.SetParam(0, (7 * target_sector->left + target_sector->right)/8);
				else
					c.SetParam(0, (target_sector->left + target_sector->right)/2);

				if(target_sector->y > pos_y)
					c.SetParam(2, (7 * target_sector->bottom + target_sector->top)/8);
				else if(target_sector->y < pos_y)
					c.SetParam(2, (target_sector->bottom + 7 * target_sector->top)/8);
				else
					c.SetParam(2, (target_sector->bottom + target_sector->top)/2);

				c.SetParam(1, ai->Getcb()->GetElevation(c.GetParam(0), c.GetParam(2)));

				// move group to that sector
				GiveOrder(&c, 110, UNIT_ATTACKING, "Group::Idle_c");
			}
		}
	}
	else if(task == GROUP_RETREATING)
	{
		//check if retreating units is in target sector
		float3 pos = ai->Getcb()->GetUnitPos(unit);

		AAISector *temp = ai->Getmap()->GetSectorOfPos(pos);

		if(temp == target_sector || !target_sector)
			task = GROUP_IDLE;
	}
	else if(task == GROUP_DEFENDING)
	{
		//check if retreating units is in target sector
		float3 pos = ai->Getcb()->GetUnitPos(unit);

		AAISector *temp = ai->Getmap()->GetSectorOfPos(pos);

		if(temp == target_sector || !target_sector)
			task = GROUP_IDLE;
	}
}

void AAIGroup::BombTarget(int target_id, float3 *target_pos)
{
	Command c(CMD_ATTACK);
	c.PushPos(*target_pos);

	GiveOrder(&c, 110, UNIT_ATTACKING, "Group::BombTarget");

	ai->Getut()->AssignGroupToEnemy(target_id, this);

	task = GROUP_BOMBING;
}

void AAIGroup::DefendAirSpace(float3 *pos)
{
	Command c(CMD_PATROL);
	c.PushPos(*pos);

	GiveOrder(&c, 110, UNIT_ATTACKING, "Group::DefendAirSpace");

	task = GROUP_PATROLING;
}

void AAIGroup::AirRaidUnit(int unit_id)
{
	Command c(CMD_ATTACK);
	c.PushParam(unit_id);

	GiveOrder(&c, 110, UNIT_ATTACKING, "Group::AirRaidUnit");

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

			GiveOrder(&c, 90, HEADING_TO_RALLYPOINT, "Group::RallyPoint");
		}
	}
	else
	{
		ai->Log("Failed to determine rally point for goup of unit type %s!\n", ai->Getbt()->s_buildTree.GetUnitTypeProperties(m_groupDefId).m_name.c_str());
	}
}
