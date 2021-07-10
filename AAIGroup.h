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

class AAI;
class AAIBuildTable;
class AAIAttack;
class AAISector;
class AAIAttackManager;

class AAIGroup
{
public:
	AAIGroup(AAI *ai, UnitDefId unitDefId, int continentId);
	~AAIGroup(void);

	//! @brief Tries to add the given unit to the group
	bool AddUnit(UnitId unitId, UnitDefId unitDefId, int continentId);

	//! @brief Removes the given unit from the group and checks if air support to defend group shall be requested
	bool RemoveUnit(UnitId unitId, UnitId attackerUnitId);

	//! @brief Returns the number of units in the group
	int GetCurrentSize() const { return static_cast<int>(m_units.size()); }

	void GiveOrderToGroup(Command *c, float importance, UnitTask task, const char *owner);

	//! @brief Orders unit to move/patrol/fight to given position where given distance between individual target positions is maintained
	void GiveMoveOrderToGroup(int commandId, UnitTask unitTask, const float3& targetPositionCenter, const float3& distanceBetweenUnitsVector);

	//! @brief Determines the position of an enemy building in the given sector and orders all units to attack it
	void AttackSector(const AAISector *sector, float importance);

	//! @brief Defend unit vs enemy (enemyPosition equals ZeroVector if enemy unknown -> guard unit instead)
	void Defend(UnitId unitId, const float3& enemyPosition, int importance);

	//! @brief Retreat to rally point
	void RetreatToRallyPoint()  { Retreat(m_rallyPoint); }

	//! @brief Retreat units in group to given position
	void Retreat(const float3& pos);

	//! @brief Orders units to attack given target (either directly attack the position for bombers or fight command for gun ships)
	void AirRaidTarget(UnitId unitId, const float3& position, float importance);

	//! @brief Orders fighters to defend air space (patrol to given position)
	void DefendAirSpace(const float3& pos, float importance);

	//! @brief Orders the units of the (air) group to attack the given enemy unit
	void AirRaidUnit(UnitId unitId, float importance);

	//! @brief Returns a random unit from the group (or invalid unitId if group is empty)
	UnitId GetRandomUnit() const;

	void Update();

	void TargetUnitKilled();

	//! @brief Checks the current rally point and chooses new one if necessary
	void UpdateRallyPoint();

	//! @brief Determines a new rally point and orders units to get there
	void GetNewRallyPoint();

	//! @brief 
	void UnitIdle(UnitId unitId, AAIAttackManager* attackManager);

	//! @brief Returns combat power of the group vs given target type
	float GetCombatPowerVsTargetType(const AAITargetType& targetType) const;

	//! @brief Adds the combat power of the units in this group to the given values
	void AddGroupCombatPower(TargetTypeValues& combatPower) const;

	//! @brief Return the id of the continent the units of this group are stationed on (-1 for non-continent bound movement types)
	int GetContinentId() const { return m_continentId; }

	//! @brief Returns the unitDefId of the units in the group 
	const UnitDefId& GetUnitDefIdOfGroup() const { return m_groupDefId; }

	//! @brief Returns the combat unit type of the units in the group 
	const AAIUnitType& GetUnitTypeOfGroup() const { return m_groupType; }

	//! @brief Returns the combat unit type of the units in the group 
	const AAIUnitCategory& GetUnitCategoryOfGroup() const { return m_category; }

	//! @brief Returns the movement type of the units in the group
	const AAIMovementType& GetMovementType() const { return m_moveType; }

	//! @brief Returns the target type of the units in the group
	const AAITargetType& GetTargetType() const;

	//! @brief Returns the current target position where the units shall move
	const float3& GetTargetPosition() const { return m_targetPosition; }

	//! @brief Returns the position of the group (to save effort, only the position of the last unit added to the group)
	float3 GetGroupPos() const;

	//! @brief Returns true if most recently added unit is close to rally point
	bool IsEntireGroupAtRallyPoint() const;

	//! @brief Returns rating of the group to perform a task (e.g. defend) of given performance at given position 
	float GetDefenceRating(const AAITargetType& attackerTargetType, const float3& position, float importance, int continentId) const;

	//! @brief Checks if the group may participate in an attack (= idle, sufficient combat power, etc.)
	bool IsAvailableForAttack();

	//! importance of current task
	float m_urgencyOfCurrentTask;	

	// attack the group takes part in
	AAIAttack *attack;

private:
	AAI* ai;

	//! @brief Determines direction (i.e. normalized vector) pointing from group to given position
	float3 DetermineDirectionToPosition(const float3& position) const;

	//! @brief Returns whether unit group is considered to be strong enough to attack
	bool SufficientAttackPower() const;

	int lastCommandFrame;
	Command lastCommand;

	//! The maximum number of units the group may consist of
	int               m_maxSize;

	//! The units that belong to this group
	std::list<UnitId> m_units;

	//! The type of units in this group
	UnitDefId         m_groupDefId;

	//! The unit category of the units in this group
	AAIUnitCategory   m_category;

	//! The unit type of the units in this group
	AAIUnitType       m_groupType;

	//! The movement type of the units of the group
	AAIMovementType   m_moveType;

	//! The current task of this group
	GroupTask         m_task;

	//! The current position the group shall move to (or ZeroVector if none)
	float3            m_targetPosition;

	//! The current sector in which the destination to move is located(or nullptr if none)
	const AAISector*  m_targetSector;

	//! Rally point of the group, ZeroVector if none.
	float3            m_rallyPoint;

	//! Id of the continent the units of this group are stationed on (only matters if units of group cannot move to another continent)
	int               m_continentId;
};

#endif

