// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_CONSTRUCTOR_H
#define AAI_CONSTRUCTOR_H

#include "aidef.h"
#include "AAITypes.h"
#include "AAIBuildTable.h"

class AAI;
class AAIBuildTask;

#include <list>
using namespace std;

//! Possible tasks of a constructor
enum class EConstructorActivity : uint32_t
{
	UNKNOWN                 = 0x00u, //! Unknown task (default value)
	IDLE                    = 0x01u, //! Idle, i.e. not doing anything
	CONSTRUCTING           	= 0x02u, //! Currently constructing a unit/building
	ASSISTING              	= 0x04u, //! Currently assisting in construction
	REPAIRING               = 0x08u, //! Currently repairing a damaged unit
	RECLAIMING              = 0x10u, //! Currently reclaiming wreckage
	RESSURECTING            = 0x20u, //! Currently resurrecting wreckage
	HEADING_TO_BUILDSITE    = 0x40u, //! Currently moving to buildsite (i.e. construction not started yet)
	DESTROYED               = 0x80u  //! Constructor has been destroyed
};

class AAIConstructorActivity{
public: 
	AAIConstructorActivity() : m_activity(EConstructorActivity::UNKNOWN) {};

	AAIConstructorActivity(EConstructorActivity activity) : m_activity(activity) {};

	void SetActivity(EConstructorActivity activity) { m_activity = activity; };

	bool IsDestroyed() const { return (m_activity == EConstructorActivity::DESTROYED); };

	bool IsIdle() const { return (m_activity == EConstructorActivity::IDLE); };

	bool IsAssisting() const { return (m_activity == EConstructorActivity::ASSISTING); };

	bool IsReclaiming() const { return (m_activity == EConstructorActivity::RECLAIMING); };

	bool IsConstructing() const { return (m_activity == EConstructorActivity::CONSTRUCTING); };

	bool IsHeadingToBuildsite() const { return (m_activity == EConstructorActivity::HEADING_TO_BUILDSITE);};

	//! @brief Return whether the contsructor is currently constructing or preparing to do so
	bool IsCarryingOutConstructionOrder() const 
	{ 
		uint32_t constructingBitmask = static_cast<uint32_t>(EConstructorActivity::CONSTRUCTING) + static_cast<uint32_t>(EConstructorActivity::HEADING_TO_BUILDSITE);
		return static_cast<bool>(static_cast<uint32_t>(m_activity) & constructingBitmask);
	};

private:
	EConstructorActivity m_activity;
};

class AAIConstructor
{
public:
	AAIConstructor(AAI *ai, UnitId unitId, UnitDefId defId, bool factory, bool builder, bool assistant, std::list<UnitDefId>* buildqueue);

	~AAIConstructor(void);

	void Update();

	void Idle();

	//! @brief Returns whether constructor is busy (i.e. list of current commands is not empty )
	bool isBusy() const;

	//! @brief Returns whether constructor is currently idle (-> not building anything or assisting) @todo: check for heading to buildsite/reclaiming
	bool IsIdle() const { return m_activity.IsIdle(); }; //  (assistance < 0) && (m_constructedUnitId.isValid() == false); };

	//! @brief Returns true if builder is currently heading to buildsite
	bool IsHeadingToBuildsite() const { return m_activity.IsHeadingToBuildsite(); };

	//! @brief A constructor is considered as available if idle/occupied with lower priority tasks suchs as assisting/reclaiming
	bool IsAvailableForConstruction() const { return (m_activity.IsCarryingOutConstructionOrder() == false); };

	//! @brief Checks if an active construction order has failed; if this is the case update internal data
	void CheckIfConstructionFailed();

	//! @brief Returns whether nano turret is desired to support unit production (always false for builders)
	bool IsAssitanceByNanoTurretDesired() const;

	// checks if assisting builders needed
	void CheckAssistance();

	// stops this unit from assisting another builder/factory
	void StopAssisting();

	//! @brief Assigns unit id of cosntructed unit and sets activity to CONSTRUCTING
	void ConstructionStarted(UnitId unitId, AAIBuildTask *buildTask);

	//! @brief Set constructor to idle and invalidate all data associated with constructing a unit/building (construction ids, build pos, ...)
	void ConstructionFinished();
	
	//! @brief Issues a construction order for given building at position and sets all internal variables of the construction unit accordingly
	void GiveConstructionOrder(UnitDefId building, const float3& pos);

	//! @brief Assist given contructor (factories will be guarded, constructed units/buildings will be repaired)
	void AssistConstruction(UnitId constructorUnitId, bool factory = false);

	// continue with construction after original builder has been killed
	void TakeOverConstruction(AAIBuildTask *build_task);

	//! Let constructor reclaim the given unit
	void GiveReclaimOrder(UnitId unitId);

	void Killed();

	//! @brief Retreats mobile constructors to safe sectors (do not in rertreat in own base when attacked by scouts or air)
	void CheckRetreatFromAttackBy(const AAIUnitCategory& attackedByCategory);

	//! @brief Return the position where a building has been placed
	const float3& GetBuildPos() const { return m_buildPos; };

	//! @brief Returns the catgegory of the unit that is currently being constructed (unknown if none)
	const AAIUnitCategory& GetCategoryOfConstructedUnit() const { return ai->s_buildTree.GetUnitCategory(m_constructedDefId); };

	//! Unit id of the construction unit
	UnitId    m_myUnitId;
	
	//! Unit definition id of the construction unit
	UnitDefId m_myDefId;

	//! Unit id of the constructed unit (-1 if none)
	UnitId    m_constructedUnitId;
	
	//! Unit definition id of the constructed unit  (0 if none)
	UnitDefId m_constructedDefId;

	// assistant builders
	set<int> assistants;

	// pointer to possible buildtask
	AAIBuildTask *build_task;

private:
	//! @brief Returns true if constructor needs assistance
	bool DoesFactoryNeedAssistance() const;

	//! @brief Returns the time the constructor would need to build the given unit
	float GetBuildtimeOfUnit(UnitDefId constructedUnitDefId) const;

    //! @brief Construction has failed (e.g. unit/building has been destroyed before being finished)
    void ConstructionFailed();

	// removes an assisting con unit
	void RemoveAssitant(int unit_id);

    // stops all assisters from assisting this unit
	void ReleaseAllAssistants();

	AAI *ai;

	// specify type of construction units (several values may be true, e.g. builders that may build certain combat units as well)
	
	//! Constructor can build units
	bool m_isFactory;	

	//! Constructor can build buildings
	bool m_isBuilder;

	//! Constructor can assist construction of other units/buildings (nanotowers, fark, etc.)
	bool m_isAssistant;

	//	bool resurrect;		// can resurrect

    //! Position zero vector if none
	float3 m_buildPos;

	//! Unit id of the unit, the constructor currently assists (-1 if none)
	UnitId m_assistUnitId;

    //! Current task (idle, building, assisting)
	AAIConstructorActivity m_activity;

	//! Pointer to buildqueue (if it is a factory or constructor)
	std::list<UnitDefId> *m_buildqueue;
};

#endif

