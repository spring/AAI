// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_GROUP_H
#define AAI_GROUP_H

#include "System/type2.h"
#include "Sim/Units/CommandAI/Command.h"
#include "aidef.h"
#include "AAITypes.h"
#include "AAIUnitTypes.h"

enum GroupTask {GROUP_IDLE, GROUP_ATTACKING, GROUP_DEFENDING, GROUP_PATROLING, GROUP_BOMBING, GROUP_RETREATING};

namespace springLegacyAI {
	struct UnitDef;
}

using namespace springLegacyAI;

#include <vector>
#include <list>
using namespace std;

class AAI;
class AAIBuildTable;
class AAIAttack;
class AAISector;

class AAIGroup
{
public:
	AAIGroup(AAI *ai, UnitDefId unitDefId, int continentId);
	~AAIGroup(void);

	bool AddUnit(UnitId unitId, UnitDefId unitDefId, int continentId);

	bool RemoveUnit(int unit, int attacker);

	//! @brief Returns the number of units in the group
	int GetNumberOfUnits() const { return static_cast<int>(units.size()); }

	void GiveOrderToGroup(Command *c, float importance, UnitTask task, const char *owner);

	void AttackSector(const AAISector *sector, float importance);

	//! @brief Defend unit vs enemy (enemyPosition equals ZeroVector if enemy unknown -> guard unit instead)
	void Defend(UnitId unitId, const float3& enemyPosition, int importance);

	//! @brief Retreat to rally point
	void RetreatToRallyPoint()  { Retreat(m_rallyPoint); }

	//! @brief Retreat units in group to given position
	void Retreat(const float3& pos);

	// bombs target (only for bomber groups)
	void BombTarget(int target_id, float3 *target_pos);

	// orders fighters to defend air space
	void DefendAirSpace(float3 *pos);

	// orders air units to attack
	void AirRaidUnit(int unit_id);

	int GetRandomUnit();

	void Update();

	void TargetUnitKilled();

	// checks current rally point and chooses new one if necessary
	void UpdateRallyPoint();

	// gets a new rally point and orders units to get there
	void GetNewRallyPoint();

	void UnitIdle(int unit);

	float GetCombatPowerVsTargetType(const AAITargetType& targetType) const;

	bool CanFightTargetType(const AAITargetType& targetType) const 
	{
		return m_groupType.CanFightTargetType(targetType);
	}

	//! @brief Return the id of the continent the units of this group are stationed on (-1 for non-continent bound movement types)
	int GetContinentId() const { return m_continentId; }

	//! @brief Returns the combat unit type of the units in the group 
	const AAIUnitType& GetUnitTypeOfGroup() const { return m_groupType; }

	//! @brief Returns the combat unit type of the units in the group 
	const AAIUnitCategory& GetUnitCategoryOfGroup() const { return m_category; }

	//! @brief Returns the movement type of the units in the group
	const AAIMovementType& GetMovementType() const { return m_moveType; }

	//! @brief Returns the target type of the units in the group
	const AAITargetType& GetTargetType() const;

	float3 GetGroupPos();

	// checks if the group may participate in an attack (= idle, sufficient combat power, etc.)
	bool AvailableForAttack();

	int maxSize;
	int size;

	float avg_speed;
	std::list<int2> units;

	float task_importance;	// importance of current task

	GroupTask task;

	// attack the group takes part in
	AAIAttack *attack;

private:
	//! @brief Returns whether unit group is considered to be strong enough to attack
	bool SufficientAttackPower() const;

	int lastCommandFrame;
	Command lastCommand;

	//! The type of units in this group
	UnitDefId m_groupDefId;

	//! The unit category of the units in this group
	AAIUnitCategory m_category;

	//! The unit type of the units in this group
	AAIUnitType m_groupType;

	//! The movement type of the units of the group
	AAIMovementType m_moveType;

	AAI* ai;

	const AAISector *target_sector;

	//! Rally point of the group, ZeroVector if none.
	float3 m_rallyPoint;

	//! Id of the continent the units of this group are stationed on (only matters if units of group cannot move to another continent)
	int m_continentId;
};

#endif

