#include "PhysicsWorld.h"
#include "Thread.h"
#include "PhysicsAutoLock.h"
#include "DataDef.h"
#include "MemorySystem.h"
#include "Profiler.h"
#include "Resource.h"
#include "Log.h"
#include "MathDefs.h"
#include "DebugDraw.h"
#include "Resource.h"
#include "id_array.h"
#include "config.h"
#include "EngineAssert.h"
#include "Event.h"
//===========================================
//          COMPONENTS
#include "PhysicsInstance.h"
#include "ProxyInstance.h"
#include "Actor.h"
//===========================================

#include <tinystl/allocator.h>
#include <tinystl/unordered_map.h>

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

static int  m_status = 0;
static void check_status()
{
    ENGINE_ASSERT((m_status != kTickProcessing),  "PhysicsSystem Status is Processing!!!");
}
static void set_status(int newStatus)
{
    m_status = newStatus;
}

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
        evt.m_objects[0] = objectA->m_actor;
        evt.m_objects[1] = objectB->m_actor;
        transform_vec3(evt.m_position, event.m_contactPoint->getPosition());
        transform_vec3(evt.m_normal, event.m_contactPoint->getNormal());
        evt.m_velocity = event.getSeparatingVelocity();
        
        uint64_t key = (uint64_t)dataA << 32 | (uint64_t)dataB;
        g_physicsWorld.add_collision_event(key, evt);
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

PhysicsWorld                g_physicsWorld;
typedef tinystl::unordered_map<uint64_t, CollisionEvent*> CollisionEventMap;
static CollisionEventMap                        g_collisionEvtMap;
static hkCriticalSection                        g_eventCS;
static IdArray<MAX_PHYSICS, PhysicsInstance>    m_objects;
static IdArray<MAX_PROXY, ProxyInstance>        m_proxies;

#define MAX_RAYCAST_PERFRAME    (1000)

void PhysicsWorld::init()
{
    m_config = 0;
    m_world = 0;
    m_numCollisionEvents = 0;
    m_numRaycasts = 0;
    m_raycastSem = new hkSemaphoreBusyWait(0, 1000);
    hkpRayCastQueryJobQueueUtils::registerWithJobQueue(g_threadMgr.get_jobqueue());
}

void PhysicsWorld::quit()
{
    delete m_raycastSem;
    destroy_world();
}

void PhysicsWorld::frame_start()
{
    set_status(kTickFrameStart);
    m_numCollisionEvents = 0;
    m_numRaycasts = 0;
    g_collisionEvtMap.clear();
    m_collisionEvents = FRAME_ALLOC(CollisionEvent*, id_array::size(m_objects));
    m_raycasts = FRAME_ALLOC(RaycastJob, MAX_RAYCAST_PERFRAME);
}


void PhysicsWorld::clear_world()
{
    if(!m_world)
        return;
    PHYSICS_LOCKWRITE(m_world);
    m_world->removeAll();
    SAFE_DELETE(m_contactListener);
}

void PhysicsWorld::create_world(PhysicsConfig* config)
{
    if(m_world) return;
    m_config = config;
    // The world cinfo contains global simulation parameters, including gravity, solver settings etc.
    hkpWorldCinfo info;
    // Set the simulation type of the world to multi-threaded.
    info.m_simulationType = hkpWorldCinfo::SIMULATION_TYPE_MULTITHREADED;
    // Flag objects that fall "out of the world" to be automatically removed - just necessary for this physics scene
    info.m_broadPhaseBorderBehaviour = hkpWorldCinfo::BROADPHASE_BORDER_DO_NOTHING;
    transform_vec3(info.m_gravity, config->m_gravity);
    info.setBroadPhaseWorldSize(config->m_worldSize);
    //info.m_collisionTolerance = 0.01f;
    m_world = new hkpWorld(info);

    PHYSICS_LOCKWRITE(m_world);
    hkpAgentRegisterUtil::registerAllAgents( m_world->getCollisionDispatcher() );
    // We need to register all modules we will be running multi-threaded with the job queue
    m_world->registerWithJobQueue( g_threadMgr.get_jobqueue() );
    g_threadMgr.vdb_add_world(m_world);

    hkpGroupFilter* pGroupFilter = new hkpGroupFilter();
    // first disable collisions between all groups (group 0 should not be disabled, see docs)
    pGroupFilter->disableCollisionsUsingBitfield(0xfffffffe, 0xfffffffe);
    if(m_config)
    {
        uint32_t num = m_config->m_numFilterLayers;
        CollisionFilter* head = m_config->m_filters;
        for (uint32_t i=1;i<num;++i)
        {
            pGroupFilter->enableCollisionsUsingBitfield(1 << i, head[i].m_mask);
        }
    }
    m_world->setCollisionFilter(pGroupFilter);
    pGroupFilter->removeReference();

    m_contactListener = new HavokContactListener();
    m_world->addContactListener( m_contactListener );
    m_world->addEntityListener( m_contactListener );
}

void PhysicsWorld::create_plane(float size)
{
    ENGINE_ASSERT(m_world, "physics is not created!");
    PHYSICS_LOCKWRITE(m_world);
    hkVector4 planeSize(size, 2.0f, size);
    hkVector4 halfExtents(planeSize(0) * 0.5f, planeSize(1) * 0.5f, planeSize(2) * 0.5f);
    float radius = 0.f;
    hkpShape* cube = new hkpBoxShape(halfExtents, radius );
    hkpRigidBodyCinfo boxInfo;
    boxInfo.m_motionType = hkpMotion::MOTION_FIXED;
    boxInfo.m_rotation.setIdentity();
    boxInfo.m_shape = cube;
    boxInfo.m_position = hkVector4(0,-1,0);
    hkpRigidBody* boxRigidBody = new hkpRigidBody(boxInfo);
    cube->removeReference();
    m_world->addEntity(boxRigidBody);
    boxRigidBody->removeReference();
}

void PhysicsWorld::post_simulation()
{
    PROFILE(Physics_PostCallback);
    PHYSICS_LOCKREAD(m_world);
    const hkArray<hkpSimulationIsland*>& activeIslands = m_world->getActiveSimulationIslands();
    int num = activeIslands.getSize();

    for(int i = 0; i < num; ++i)
    {
        const hkArray<hkpEntity*>& activeEntities = activeIslands[i]->getEntities();
        int num_entity = activeEntities.getSize();
        const hkArray<hkpEntity*>::const_iterator head = activeEntities.begin();

        for(int j = 0; j < num_entity; ++j)
        {
            hkpRigidBody* rigidBody = static_cast<hkpRigidBody*>(head[j]);
            //fix object should not update renderer
            if(rigidBody->isFixed()) continue;
            hkUlong user_data = rigidBody->getUserData();
            if (!user_data) continue;
            PhysicsInstance* phy = (PhysicsInstance*)user_data;
            phy->post_simulation(rigidBody);
        }
    }
}

void PhysicsWorld::destroy_world()
{
    check_status();

    if(!m_world) return;
    m_world->markForWrite();
    g_threadMgr.vdb_remove_world(m_world);
    SAFE_DELETE(m_world);
}

int PhysicsWorld::get_contact_rigidbodies(const hkpRigidBody* body, hkpRigidBody** contactingBodies, int maxLen)
{
    check_status();

    PHYSICS_LOCKREAD(m_world);
    int retNum = 0;
    const hkArray<hkpLinkedCollidable::CollisionEntry>& collisionEntries = \
            body->getLinkedCollidable()->getCollisionEntriesNonDeterministic();
    int num = collisionEntries.getSize();
    const hkpLinkedCollidable::CollisionEntry* head = collisionEntries.begin();

    for(int i = 0; i < num; i++) 
    { 
        const hkpLinkedCollidable::CollisionEntry& entry = head[i]; 
        hkpRigidBody* otherBody = hkpGetRigidBody(entry.m_partner);
        if (otherBody != HK_NULL) 
        { 
            if (entry.m_agentEntry->m_contactMgr->m_type == 
                hkpContactMgr::TYPE_SIMPLE_CONSTRAINT_CONTACT_MGR)
            {
                hkpSimpleConstraintContactMgr* mgr = 
                    static_cast<hkpSimpleConstraintContactMgr*>(entry.m_agentEntry->m_contactMgr);
                if (mgr->m_contactConstraintData.getNumContactPoints() > 0)
                {
                    contactingBodies[retNum ++] = otherBody;
                    if(retNum >= maxLen) return retNum;
                }
            }
        }
    }
    return retNum;
}


void PhysicsWorld::kick_in_jobs( float timeStep )
{
    if(!m_world) return;
    PROFILE(Physics_KickInJobs);
    set_status(kTickProcessing);
    kickin_raycast_jobs();
    m_world->initMtStep( g_threadMgr.get_jobqueue(),timeStep );
}

void PhysicsWorld::tick_finished_jobs()
{
    if(!m_world) return;
    PROFILE(Physics_TickFinishJobs);
    set_status(kTickFinishedJobs);
    m_world->finishMtStep(g_threadMgr.get_jobqueue(), g_threadMgr.get_threadpool());
    if(m_numRaycasts) m_raycastSem->acquire();
    post_simulation();
}


void PhysicsWorld::add_to_world(PhysicsInstance* instance)
{
    ENGINE_ASSERT(m_world, "physics is not created!");

    check_status();
    PHYSICS_LOCKWRITE(m_world);

    switch(instance->m_systemType)
    {
    case kSystemRigidBody: m_world->addEntity(instance->m_rigidBody); break;
    case kSystemRagdoll:
    case kSystemComplex:
        m_world->addPhysicsSystem(instance->m_system);
        break;
    default:
        break;
    }
    
}

void PhysicsWorld::remove_from_world(PhysicsInstance* instance)
{
    ENGINE_ASSERT(m_world, "physics is not created!");

    check_status();
    PHYSICS_LOCKWRITE(m_world);
    switch(instance->m_systemType)
    {
    case kSystemRigidBody: m_world->removeEntity(instance->m_rigidBody); break;
    case kSystemRagdoll:
    case kSystemComplex:
        m_world->removePhysicsSystem(instance->m_system);
        break;
    default:
        break;
    }
}

void PhysicsWorld::add_collision_event(uint64_t key, const CollisionEvent& evt)
{
    PROFILE(Physics_addCollisionEvent);
    if(g_collisionEvtMap.find(key) != g_collisionEvtMap.end()) return;
    CollisionEvent* newEvt = FRAME_ALLOC(CollisionEvent, 1);
    *newEvt = evt;
    m_collisionEvents[m_numCollisionEvents++] = newEvt;
    g_collisionEvtMap[key] = newEvt;
}


int PhysicsWorld::add_raycast_job(const float* from, const float* to, int32_t filterInfo)
{
    check_status();
    if(!m_raycasts) return -1;
    int retHandle = m_numRaycasts;
    RaycastJob* job = m_raycasts + (m_numRaycasts++);
    ENGINE_ASSERT(m_numRaycasts < MAX_RAYCAST_PERFRAME, "raycast overflow");
    bx::vec3Move(job->m_from, from);
    bx::vec3Move(job->m_to, to);
    job->m_filterInfo = filterInfo;
    return retHandle;
}

RaycastJob* PhysicsWorld::get_raycast_job( int handle ) const
{
    if(handle < 0) return 0;
    return &m_raycasts[handle];
}

void PhysicsWorld::kickin_raycast_jobs()
{
    uint32_t num = m_numRaycasts;
    if(!num) return;

    hkpCollisionQueryJobHeader* jobHeader = FRAME_ALLOC(hkpCollisionQueryJobHeader, num);
    hkpWorldRayCastCommand* commands = FRAME_ALLOC(hkpWorldRayCastCommand, num);
    hkpWorldRayCastOutput* outputs = FRAME_ALLOC(hkpWorldRayCastOutput, num);
    RaycastJob* head = m_raycasts;

    for (uint32_t i = 0; i < num; ++i)
    {
        RaycastJob& job = head[i];
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

    g_threadMgr.get_jobqueue()->addJob(*worldRayCastJob, hkJobQueue::JOB_LOW_PRIORITY);
}


int PhysicsWorld::get_layer(const StringId& name) const
{
    //if not found default filter is 0.
    if(!m_config) return 0;
    uint32_t num = m_config->m_numFilterLayers;
    const CollisionFilter* head = m_config->m_filters;
    for(uint32_t i=0; i<num; ++i)
    {
        if(head[i].m_name == name)
            return i;
    }
    return 0;
}

void PhysicsWorld::sync_rigidbody_actors( struct Actor* actors, uint32_t num )
{
    PROFILE(sync_rigidbody_actors);
    check_status();
    hkQsTransform t;
    hkTransform t1;
    for (uint32_t i=0; i<num; ++i)
    {
        Actor& actor = actors[i];
        PhysicsInstance* physics_object = (PhysicsInstance*)actor.get_component(kComponentPhysics);
        if(!physics_object->m_dirty) continue;
        physics_object->fetch_transform(0, t1);
        t.setFromTransformNoScale(t1);
        actor.transform_renders(t);
        physics_object->m_dirty = false;
    }
}

void PhysicsWorld::sync_proxy_actors( Actor* actors, uint32_t num )
{
    PROFILE(sync_proxy_actors);
    check_status();
    float pos[3];
    hkQsTransform t;
    t.setIdentity();
    for (uint32_t i=0; i<num; ++i)
    {
        Actor& actor = actors[i];
        ProxyInstance* proxy = (ProxyInstance*)actor.get_component(kComponentProxy);
        //ModelInstance* model = (ModelInstance*)actor.get_component(kComponentModel);
        if(!proxy) continue;
        transform_vec3(pos, proxy->m_transform.m_translation);
        pos[1] += proxy->m_resource->m_offset;
        transform_vec3(t.m_translation, pos);
        actor.transform_renders(t);
    }
}

void PhysicsWorld::update_character_proxies(float timeStep)
{
    PROFILE(update_character_proxies);
    check_status();
    uint32_t num = id_array::size(m_proxies);
    ProxyInstance* proxies = id_array::begin(m_proxies);
    PHYSICS_LOCK(m_world);
    for (uint32_t i=0; i<num; ++i)
    {
        proxies[i].update(timeStep);
    }
}

//-----------------------------------------------------------------
//
//-----------------------------------------------------------------
Id create_physics_object(const void* resource, ActorId32 id)
{
    check_status();
    PhysicsInstance inst;
    memset(&inst, 0x00, sizeof(inst));
    inst.init(resource);
    inst.m_actor = id;
    return id_array::create(m_objects, inst);
}

void destroy_physics_object(Id id)
{
    check_status();
    if(!id_array::has(m_objects, id)) return;
    PhysicsInstance& inst = id_array::get(m_objects, id);
    inst.destroy();
    id_array::destroy(m_objects, id);
}

void* get_physics_object(Id id)
{
    if(!id_array::has(m_objects, id))
        return 0;
    return &id_array::get(m_objects, id);
}

uint32_t num_physics_objects()
{
    return id_array::size(m_objects);
}

void* get_physics_objects()
{
    return id_array::begin(m_objects);
}

Id create_physics_proxy(const void* resource, ActorId32 )
{
    check_status();
    ProxyInstance inst;
    memset(&inst, 0x00, sizeof(inst));
    inst.init(resource);
    return id_array::create(m_proxies, inst);
}

void destroy_physics_proxy(Id id)
{
    check_status();
    if(!id_array::has(m_proxies, id)) return;
    ProxyInstance& inst = id_array::get(m_proxies, id);
    inst.destroy();
    id_array::destroy(m_proxies, id);
}

void* get_physics_proxy(Id id)
{
    if(!id_array::has(m_proxies, id)) return 0;
    return &id_array::get(m_proxies, id);
}

uint32_t num_physics_proxies()
{
    return id_array::size(m_proxies);
}

void* get_physics_proxies()
{
    return id_array::begin(m_proxies);
}
//-----------------------------------------------------------------
//
//-----------------------------------------------------------------