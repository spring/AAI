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

class AAI;
class AAIBuildTable;
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
	AAIConstructor(AAI *ai, UnitId unitId, UnitDefId defId, bool factory, bool builder, bool assistant, std::list<int>* buildque);

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

	// checks if assisting builders needed
	void CheckAssistance();

	// stops this unit from assisting another builder/factory
	void StopAssisting();

	//! @brief Assigns unit id of cosntructed unit and sets activity to CONSTRUCTING
	void ConstructionStarted(UnitId unitId, AAIBuildTask *buildTask);

	//! @brief Set constructor to idle and invalidate all data associated with constructing a unit/building (construction ids, build pos, ...)
	void ConstructionFinished();
	
	void GiveConstructionOrder(int id_building, float3 pos, bool water);

	//! @brief Assist given contructor
	void AssistConstruction(UnitId constructorUnitId);

	// continue with construction after original builder has been killed
	void TakeOverConstruction(AAIBuildTask *build_task);

	void GiveReclaimOrder(int unit_id);

	void Killed();

	// moves mobile constructors to safe sectors
	void Retreat(UnitCategory attacked_by);

	//! @brief Return the position where a building has been placed
	const float3& GetBuildPos() const { return m_buildPos; };

	int buildspeed;

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
	std::list<int> *m_buildqueue;
};

#endif

