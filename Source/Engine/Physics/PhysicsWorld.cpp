#include "PhysicsWorld.h"
#include "Thread.h"
#include "PhysicsAutoLock.h"
#include "DataDef.h"
#include "MemoryPool.h"
#include "Memory.h"
#include "Profiler.h"
#include "Resource.h"
#include "Log.h"
#include "MathDefs.h"
#include "DebugDraw.h"
#include "Component.h"
//===========================================
//          COMPONENTS
#include "PhysicsInstance.h"
#include "ProxyInstance.h"
//===========================================

#include <bx/tinystl/allocator.h>
#include <bx/tinystl/unordered_map.h>

//========================================================================================
#include <Physics2012/Dynamics/World/Listener/hkpWorldPostSimulationListener.h>
#include <Physics2012/Collide/Query/CastUtil/hkpLinearCastInput.h>
#include <Physics2012/Dynamics/World/hkpSimulationIsland.h>
#include <Physics2012/Dynamics/World/hkpWorld.h>
#include <Physics2012/Dynamics/Entity/hkpRigidBody.h>
#include <Physics2012/Collide/Filter/Group/hkpGroupFilter.h>
#include <Physics2012/Utilities/Serialize/hkpPhysicsData.h>
#include <Physics2012/Dynamics/World/BroadPhaseBorder/hkpBroadPhaseBorder.h>
#include <Physics2012/Collide/Dispatch/hkpAgentRegisterUtil.h>
#include <Physics2012/Collide/Shape/Convex/Box/hkpBoxShape.h>
#include <Physics2012/Collide/Query/CastUtil/hkpWorldRayCastInput.h>
#include <Physics2012/Collide/Query/CastUtil/hkpWorldRayCastOutput.h>
#include <Physics2012/Collide/Agent3/Machine/Nn/hkpAgentNnTrack.h>
#include <Physics2012/Dynamics/Collide/hkpSimpleConstraintContactMgr.h>
#include <Physics2012/Utilities/Dynamics/Inertia/hkpInertiaTensorComputer.h>
#include <Physics2012/Dynamics/Phantom/hkpAabbPhantom.h>
#include <Physics2012/Dynamics/Phantom/hkpSimpleShapePhantom.h>
#include <Physics2012/Dynamics/Collide/ContactListener/hkpContactListener.h>
#include <Common/Base/Thread/CriticalSection/hkCriticalSection.h>
#include <Physics2012/Collide/Query/Multithreaded/RayCastQuery/hkpRayCastQueryJobs.h>
#include <Physics2012/Collide/Query/Multithreaded/RayCastQuery/hkpRayCastQueryJobQueueUtils.h>
//========================================================================================

class PhysicsPostSimlator : public hkpWorldPostSimulationListener
{
public:
    PhysicsPostSimlator(PhysicsWorld* world): m_world(world) {}
    virtual void postSimulationCallback( hkpWorld* world ){ m_world->postSimulationCallback(); }
private:
    PhysicsWorld*   m_world;
};


/// The Havok Physics contact listener is added to the Havok Physics world to provide collision information. 
/// It responds to collision callbacks, collects the collision information and sends messages to the 
/// collider objects. 
/// 
class HavokContactListener : public hkpContactListener, public hkpEntityListener
{
    //called in havok thread.
    virtual void contactPointCallback(const hkpContactPointEvent& event)
    {   
        hkUlong dataA = event.getBody(0)->getUserData();
        hkUlong dataB = event.getBody(1)->getUserData();
        PhysicsInstance* objectA = (PhysicsInstance*)dataA;
        PhysicsInstance* objectB = (PhysicsInstance*)dataB;
        if(!objectA || !objectB) return;
        
        CollisionEvent evt;
        evt.m_objects[0] = objectA;
        evt.m_objects[1] = objectB;
        transform_vec3(evt.m_position, event.m_contactPoint->getPosition());
        transform_vec3(evt.m_normal, event.m_contactPoint->getNormal());
        evt.m_velocity = event.getSeparatingVelocity();
        
        uint64_t key = (uint64_t)dataA << 32 | (uint64_t)dataB;
        g_physicsWorld.addCollisionEvent(key, evt);
    }
    virtual void collisionAddedCallback(const hkpCollisionEvent& event)
    {
        //TODO
    }
    virtual void collisionRemovedCallback(const hkpCollisionEvent& event)
    {
        //TODO
    }
};

typedef tinystl::unordered_map<uint64_t, CollisionEvent*> CollisionEventMap;
CollisionEventMap           g_collisionEvtMap;
PhysicsWorld                g_physicsWorld;
static hkCriticalSection    g_eventCS;
static DynamicObjectComponentFactory<PhysicsInstance> g_physicsInstances(PhysicsResource::getName(), 1000);
static DynamicObjectComponentFactory<ProxyInstance>   g_proxyInstances(ProxyResource::getName(), 30);

#define MAX_RAYCAST_PERFRAME    (1000)

void PhysicsWorld::init()
{
    m_update = true;
    m_world = 0;
    m_postCallback = 0;
    m_status = 0;
    m_numCollisionEvents = 0;
    m_numRaycasts = 0;
    m_raycastSem = new hkSemaphoreBusyWait(0, 1000);
    g_componentMgr.registerFactory(&g_physicsInstances);
    g_componentMgr.registerFactory(&g_proxyInstances);
    hkpRayCastQueryJobQueueUtils::registerWithJobQueue(g_threadMgr.getJobQueue());
}

void PhysicsWorld::quit()
{
    delete m_raycastSem;
    destroyWorld();
}

void PhysicsWorld::frameStart()
{
    m_status = kTickFrameStart;
    m_numCollisionEvents = 0;
    m_numRaycasts = 0;
    g_collisionEvtMap.clear();
    if(!m_update) return;
    m_collisionEvents = FRAME_ALLOC(CollisionEvent*, g_physicsInstances.m_objects.getSize());
    m_raycasts = FRAME_ALLOC(RaycastJob, MAX_RAYCAST_PERFRAME);
}


void PhysicsWorld::clearWorld()
{
    if(!m_world)
        return;
    PHYSICS_LOCKWRITE(m_world);
    m_world->removeAll();
    SAFE_DELETE(m_contactListener);
}

void PhysicsWorld::createWorld(float worldSize,const hkVector4& gravity, bool bPlane)
{
    if(m_world)
        return;
    // The world cinfo contains global simulation parameters, including gravity, solver settings etc.
    hkpWorldCinfo info;
    // Set the simulation type of the world to multi-threaded.
    info.m_simulationType = hkpWorldCinfo::SIMULATION_TYPE_MULTITHREADED;
    // Flag objects that fall "out of the world" to be automatically removed - just necessary for this physics scene
    info.m_broadPhaseBorderBehaviour = hkpWorldCinfo::BROADPHASE_BORDER_DO_NOTHING;
    info.m_gravity = gravity;
    info.setBroadPhaseWorldSize(worldSize);
    //info.m_collisionTolerance = 0.01f;

    m_world = new hkpWorld(info);

    PHYSICS_LOCKWRITE(m_world);
    hkpAgentRegisterUtil::registerAllAgents( m_world->getCollisionDispatcher() );
    // We need to register all modules we will be running multi-threaded with the job queue
    m_world->registerWithJobQueue( g_threadMgr.getJobQueue() );
    m_postCallback = new PhysicsPostSimlator(this);
    m_world->addWorldPostSimulationListener(m_postCallback);
    g_threadMgr.vdbAddWorld(m_world);

    hkpGroupFilter* filter = new hkpGroupFilter();
    setupGroupFilter(filter);
    m_world->setCollisionFilter(filter);
    filter->removeReference();

    m_contactListener = new HavokContactListener();
    m_world->addContactListener( m_contactListener );
    m_world->addEntityListener( m_contactListener );

    if(bPlane)
    {   
        hkVector4 planeSize(worldSize - 10.0f, 2.0f, worldSize - 10.0f);
        hkVector4 halfExtents(planeSize(0) * 0.5f, planeSize(1) * 0.5f, planeSize(2) * 0.5f);
        float radius = 0.f;
        hkpShape* cube = new hkpBoxShape(halfExtents, radius );
        hkpRigidBodyCinfo boxInfo;
        boxInfo.m_motionType = hkpMotion::MOTION_FIXED;
        boxInfo.m_rotation.setIdentity();
        boxInfo.m_shape = cube;
        boxInfo.m_position = hkVector4(0,-1,0);
        //boxInfo.m_collisionFilterInfo = hkpGroupFilter::calcFilterInfo( LAYER_LANDSCAPE,  0);
        hkpRigidBody* boxRigidBody = new hkpRigidBody(boxInfo);
        cube->removeReference();
        m_world->addEntity(boxRigidBody);
        boxRigidBody->removeReference();
    }
}

void PhysicsWorld::setupGroupFilter( hkpGroupFilter* groupFilter )
{
    // We disable collisions between different layers to determine what behaviour we want
    // For example, by disabling collisions between RAYCAST (used by foot ik) and DEBRIS, we make
    // sure the feet are never placed on top of debris objects.
    groupFilter->disableCollisionsBetween(kLayerLandScape, kLayerRagdollKeyframed);
    groupFilter->disableCollisionsBetween(kLayerCharacterProxy, kLayerDebris);
    groupFilter->disableCollisionsBetween(kLayerCharacterProxy, kLayerRagdollKeyframed);
    groupFilter->disableCollisionsBetween(kLayerCharacterProxy, kLayerRaycast);
    groupFilter->disableCollisionsBetween(kLayerCharacterProxy, kLayerRagdollDyanmic);
    groupFilter->disableCollisionsBetween(kLayerDebris, kLayerRaycast);
    groupFilter->disableCollisionsBetween(kLayerMovableEnviroment, kLayerRagdollKeyframed);
    groupFilter->disableCollisionsBetween(kLayerRagdollKeyframed, kLayerRaycast);
    groupFilter->disableCollisionsBetween(kLayerRaycast, kLayerRagdollDyanmic);
    groupFilter->disableCollisionsBetween(kLayerPicking, kLayerLandScape);
    groupFilter->disableCollisionsBetween(kLayerPicking, kLayerCharacterProxy);
    groupFilter->disableCollisionsBetween(kLayerPicking, kLayerRagdollKeyframed);
}

void PhysicsWorld::postSimulationCallback()
{
    PROFILE(Physics_PostCallback);
    PHYSICS_LOCKREAD(m_world);
    const hkArray<hkpSimulationIsland*>& activeIslands = m_world->getActiveSimulationIslands();
    for(int i = 0; i < activeIslands.getSize(); ++i)
    {
        const hkArray<hkpEntity*>& activeEntities = activeIslands[i]->getEntities();
        for(int j = 0; j < activeEntities.getSize(); ++j)
        {
            hkpRigidBody* rigidBody = static_cast<hkpRigidBody*>(activeEntities[j]);
            //fix object should not update renderer
            if(rigidBody->isFixed())
                continue;
            hkUlong user_data = rigidBody->getUserData();
            if (!user_data)
                continue;
            PhysicsInstance* phy = (PhysicsInstance*)user_data;
            phy->postSimulation(rigidBody);
        }
    }
}

void PhysicsWorld::destroyWorld()
{
    checkStatus();

    if(!m_world)
        return;
    m_world->markForWrite();
    g_threadMgr.vdbRemoveWorld(m_world);
    SAFE_DELETE(m_world);
    SAFE_DELETE(m_postCallback);
}

int PhysicsWorld::getContactingRigidBodies(const hkpRigidBody* body, hkpRigidBody** contactingBodies, int maxLen)
{
    checkStatus();

    PHYSICS_LOCKREAD(m_world);
    int retNum = 0;
    const hkArray<hkpLinkedCollidable::CollisionEntry>& collisionEntries = body->getLinkedCollidable()->getCollisionEntriesNonDeterministic();
    for(int i = 0; i < collisionEntries.getSize(); i++) 
    { 
        const hkpLinkedCollidable::CollisionEntry& entry = collisionEntries[i]; 
        hkpRigidBody* otherBody = hkpGetRigidBody(entry.m_partner);
        if (otherBody != HK_NULL) 
        { 
            if (entry.m_agentEntry->m_contactMgr->m_type == 
                hkpContactMgr::TYPE_SIMPLE_CONSTRAINT_CONTACT_MGR)
            {
                hkpSimpleConstraintContactMgr* mgr = 
                    static_cast<hkpSimpleConstraintContactMgr*>(
                    collisionEntries[i].m_agentEntry->m_contactMgr);
                if (mgr->m_contactConstraintData.getNumContactPoints() > 0)
                {
                    contactingBodies[retNum ++] = otherBody;

                    if(retNum >= maxLen)
                        return retNum;
                }
            }
        }
    }
    return retNum;
}

void PhysicsWorld::checkStatus()
{
    HK_ASSERT2(0, (m_status != kTickProcessing),  "PhysicsSystem Status is Processing!!!");
}


void PhysicsWorld::kickInJobs( float timeStep )
{
    if(!m_update) return;
    PROFILE(Physics_KickInJobs);
    m_status = kTickProcessing;
    kickInRaycastJob();
    m_world->initMtStep( g_threadMgr.getJobQueue(),timeStep );
}

void PhysicsWorld::tickFinishJobs( float timeStep )
{
    if(!m_update) return;
    PROFILE(Physics_TickFinishJobs);
    m_status = kTickFinishedJobs;
    m_world->finishMtStep(g_threadMgr.getJobQueue(), g_threadMgr.getThreadPool());
    if(m_numRaycasts) m_raycastSem->acquire();
}


void PhysicsWorld::addToWorld(PhysicsInstance* instance)
{
    checkStatus();
    PHYSICS_LOCKWRITE(m_world);

    switch(instance->m_systemType)
    {
    case kSystemRBOnly:
        {
            for(int i=0; i<instance->m_numData; ++i)
            {
                m_world->addEntity((hkpRigidBody*)instance->m_data[i]);
            }
        }
        break;
    case kSystemRagdoll:
    case kSystemComplex:
        {
            for(int i=0; i<instance->m_numData; ++i)
            {
                m_world->addPhysicsSystem((hkpPhysicsSystem*)instance->m_data[i]);
            }
        }
        break;
    default:
        break;
    }
    
}

void PhysicsWorld::removeFromWorld(PhysicsInstance* instance)
{
    checkStatus();
    PHYSICS_LOCKWRITE(m_world);
    switch(instance->m_systemType)
    {
    case kSystemRBOnly:
        {
            for(int i=0; i<instance->m_numData; ++i)
            {
                m_world->removeEntity((hkpRigidBody*)instance->m_data[i]);
            }
        }
        break;
    case kSystemRagdoll:
    case kSystemComplex:
        {
            for(int i=0; i<instance->m_numData; ++i)
            {
                m_world->removePhysicsSystem((hkpPhysicsSystem*)instance->m_data[i]);
            }
        }
        break;
    default:
        break;
    }
}

void PhysicsWorld::addCollisionEvent(uint64_t key, const CollisionEvent& evt)
{
    PROFILE(Physics_addCollisionEvent);
    if(g_collisionEvtMap.find(key) != g_collisionEvtMap.end())
        return;
    CollisionEvent* newEvt = FRAME_ALLOC(CollisionEvent, 1);
    *newEvt = evt;
    m_collisionEvents[m_numCollisionEvents++] = newEvt;
    g_collisionEvtMap[key] = newEvt;
}


int PhysicsWorld::addRaycastJob(const float* from, const float* to, int32_t filterInfo)
{
    if(!m_raycasts) return -1;
    int retHandle = m_numRaycasts;
    RaycastJob* job = m_raycasts + (m_numRaycasts++);
    HK_ASSERT(0, m_numRaycasts < MAX_RAYCAST_PERFRAME);
    bx::vec3Move(job->m_from, from);
    bx::vec3Move(job->m_to, to);
    job->m_filterInfo = filterInfo;
    return retHandle;
}

RaycastJob* PhysicsWorld::getRaycastJob( int handle ) const
{
    if(handle < 0) return 0;
    return &m_raycasts[handle];
}

void PhysicsWorld::kickInRaycastJob()
{
    if(!m_numRaycasts) return;

    hkpCollisionQueryJobHeader* jobHeader = FRAME_ALLOC(hkpCollisionQueryJobHeader, m_numRaycasts);
    hkpWorldRayCastCommand* commands = FRAME_ALLOC(hkpWorldRayCastCommand, m_numRaycasts);
    hkpWorldRayCastOutput* outputs = FRAME_ALLOC(hkpWorldRayCastOutput, m_numRaycasts);

    for (uint32_t i = 0; i < m_numRaycasts; ++i)
    {
        RaycastJob& job = m_raycasts[i];
        hkpWorldRayCastCommand* command = commands + i;
        hkpWorldRayCastOutput* output = outputs + i;
        hkpWorldRayCastInput& rayInput = command->m_rayInput;
        job.m_command = command;
        job.m_output = output;
        transform_vec3(rayInput.m_from, job.m_from);
        transform_vec3(rayInput.m_to, job.m_to);
        command->m_results = output;
        command->m_resultsCapacity = 1;
        command->m_numResultsOut = 0;
        int filterInfo = job.m_filterInfo;
        if(filterInfo >= 0)
        {
            rayInput.m_enableShapeCollectionFilter = true;
            rayInput.m_filterInfo = hkpGroupFilter::calcFilterInfo(filterInfo);
        }
        else
        {
            rayInput.m_enableShapeCollectionFilter = false;
        }
    }

    m_world->markForRead();
    size_t memSize = sizeof(hkpWorldRayCastJob);
    char* p = FRAME_ALLOC(char, memSize);
    hkpWorldRayCastJob* worldRayCastJob = new (p) hkpWorldRayCastJob(
        m_world->getCollisionInput(), jobHeader, commands, 
        m_numRaycasts, m_world->m_broadPhase, m_raycastSem);
    m_world->unmarkForRead();

    g_threadMgr.getJobQueue()->addJob(*worldRayCastJob, hkJobQueue::JOB_LOW_PRIORITY);
}


