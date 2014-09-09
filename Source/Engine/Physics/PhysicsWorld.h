#pragma once
#include "Prerequisites.h"
#include "StringId.h"

class   hkpEntity;
class   hkpWorld;
class   hkpShape;
class   hkpGroupFilter;
class   hkpPhysicsData;
class   hkpRigidBody;
class   PhysicsPostSimlator;
struct  PhysicsInstance;
struct  PhysicsResource;
class   HavokContactListener;
struct  hkpWorldRayCastCommand;
struct  hkpWorldRayCastOutput;
struct  hkpCollisionQueryJobHeader;
class   hkSemaphore;

struct CollisionEvent
{
    //uint64_t                    m_key;
    PhysicsInstance*            m_objects[2];
    float                       m_normal[3];
    float                       m_position[3];
    float                       m_velocity;
};

struct RaycastJob
{
    float                       m_from[3];
    float                       m_to[3];
    int                         m_filterInfo;
    hkpWorldRayCastOutput*      m_output;
    hkpWorldRayCastCommand*     m_command;
};

struct PhysicsWorld
{
    void init();
    void quit();

    void frameStart();
    void kickInJobs(float timeStep);
    void tickFinishJobs(float timeStep);

    void createWorld(float worldSize, const hkVector4& gravity, bool bPlane);
    void destroyWorld();
    inline hkpWorld* getWorld() const { return m_world;};
    void clearWorld();
    void postSimulationCallback();
    int  getContactingRigidBodies(const hkpRigidBody* body, 
                                  hkpRigidBody** contactingBodies, 
                                  int maxLen = 100);

    void addToWorld(PhysicsInstance* instance);
    void removeFromWorld(PhysicsInstance* instance);
    
    void addCollisionEvent(uint64_t key, const CollisionEvent& evt);
    int addRaycastJob(const float* from, const float* to, int32_t filterInfo = -1);
    RaycastJob* getRaycastJob(int handle) const;

private:
    void setupGroupFilter( hkpGroupFilter* filter );
    void updateVDB(float dt);
    void checkStatus();
    void kickInRaycastJob();
public:
    hkpWorld*                               m_world;;
    PhysicsPostSimlator*                    m_postCallback;
    HavokContactListener*                   m_contactListener;
    CollisionEvent**                        m_collisionEvents;
    RaycastJob*                             m_raycasts;
    hkSemaphore*                            m_raycastSem;
    uint32_t                                m_numCollisionEvents;
    uint32_t                                m_numRaycasts;
    int                                     m_status;
    bool                                    m_update;
};

extern PhysicsWorld g_physicsWorld;