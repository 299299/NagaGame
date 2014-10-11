#include "ProxyInstance.h"
#include "PhysicsWorld.h"
#include "PhysicsAutoLock.h"
#include "Log.h"
#include "Resource.h"
#include "DataDef.h"
#include "Utils.h"
//=============================================================================================
#include <Physics2012/Utilities/CharacterControl/CharacterProxy/hkpCharacterProxy.h>
#include <Physics2012/Collide/Shape/Convex/Capsule/hkpCapsuleShape.h>
#include <Physics2012/Dynamics/Phantom/hkpSimpleShapePhantom.h>
#include <Common/Internal/SimplexSolver/hkSimplexSolver.h>
#include <Physics2012/Collide/Query/Collector/PointCollector/hkpAllCdPointCollector.h>
#include <Common/Visualize/hkDebugDisplay.h>
#include <Physics2012/Collide/Filter/Group/hkpGroupFilter.h>
#include <Physics2012/Utilities/CharacterControl/CharacterProxy/hkpCharacterProxyListener.h>
#include <Common/Base/Types/Physics/hkStepInfo.h>
//=============================================================================================

//--------------------------------------------------------------------------
void* load_resource_proxy( const char* data, uint32_t size)
{
    ProxyResource* proxy = (ProxyResource*)data;
    proxy->createShape();
    return proxy;
}

void destroy_resource_proxy( void * resource )
{
    ProxyResource* proxy = (ProxyResource*)resource;
    proxy->destroyShape();
}
//--------------------------------------------------------------------------

static hkVector4 UP(0,1,0);
struct AnimCharacterListener : public hkReferencedObject, public hkpCharacterProxyListener
{
    HK_DECLARE_CLASS_ALLOCATOR(HK_MEMORY_CLASS_DEMO);
    AnimCharacterListener()
    :m_onMovingSurface(false)
    {};
    // Don't push objects
    virtual void objectInteractionCallback(hkpCharacterProxy* proxy, 
        const hkpCharacterObjectInteractionEvent& input, 
        hkpCharacterObjectInteractionResult& output )
    {
        output.m_objectImpulse.mul4( m_strength );
    }
    // In this demo the motion of the character proxy is controlled explicitly by the animation system.
    // The feedback between the character proxy and the animation system prevents the character proxy from moving
    // unless the animation system has told it to. In this listener we explicitly override this by adding additional
    // velocity for some moving obecjts (the crates in this demo). We add the lateral velocity of the obejct straight in to the controller.
    virtual void processConstraintsCallback( const hkpCharacterProxy* proxy, 
        const hkArray<hkpRootCdPoint>& manifold, 
        hkSimplexSolverInput& input )
    {
        // We go through the manifold and add velocity to the character proxy as required.
        for (int i=0; i < manifold.getSize(); i++)
        {
            hkSurfaceConstraintInfo& surface = input.m_constraints[i];

            hkpRigidBody* body = hkpGetRigidBody(manifold[i].m_rootCollidableB);
            bool movingBody = (body && (body->getMotionType() != hkpMotion::MOTION_FIXED) && (body->getMotionType() != hkpMotion::MOTION_KEYFRAMED));

            if (movingBody)
            {
                hkVector4 surfaceVel;
                surfaceVel = body->getLinearVelocity();

                // Work out surface velocity in plane perpendicular to up
                surfaceVel.addMul4(-surfaceVel.dot3(UP), UP);

                m_onMovingSurface = surface.m_plane.dot3(UP) > 0.95f;
                if (m_onMovingSurface)
                {
                    // Add the velocity to the character directly
                    input.m_velocity.addMul4(m_friction, surfaceVel);
                }
            }
            else
            {
                m_onMovingSurface = false;
            }

        }
    }

    ProxyInstance*          m_proxy;
    float                   m_strength;
    float                   m_friction;
    bool                    m_onMovingSurface;
};


void ProxyResource::createShape()
{
    const hkReal totalHeight = m_standHeight;
    const hkReal radius = m_radius;
    const hkReal capsulePoint = totalHeight*0.5f - radius;
    hkVector4 vertexA(0, capsulePoint * 2 + radius, 0);
    hkVector4 vertexB(0, radius, 0);
    m_standShape = new hkpCapsuleShape(vertexA, vertexB, radius);
}

void ProxyResource::destroyShape()
{
    SAFE_REMOVEREF(m_standShape);
}


//======================================================
//                INSTANCE
//======================================================  
void ProxyInstance::init(const void* resource)
{
    m_horizontalDisplacement.setZero4();
    m_verticalDisplacement = 0;
    m_targetVelocity.setZero4();
    m_renderTranslation.setZero4();

    m_resource = (const ProxyResource*)resource;
    hkpWorld* world = g_physicsWorld.world();
    int layerId = g_physicsWorld.get_layer(m_resource->m_layerName);
    hkpShapePhantom* phantom = new hkpSimpleShapePhantom(m_resource->m_standShape, 
                                                         hkTransform::getIdentity(), 
                                                         hkpGroupFilter::calcFilterInfo(layerId,0));
    hkpCharacterProxyCinfo cpci;
    cpci.m_staticFriction = 0.0f;
    cpci.m_dynamicFriction = 1.0f;
    cpci.m_up = UP;
    cpci.m_userPlanes = 4;
    cpci.m_maxSlope = HK_REAL_PI / 3;
    cpci.m_shapePhantom = phantom;
    m_proxy = new hkpCharacterProxy( cpci );
    phantom->removeReference();

    m_listener = new AnimCharacterListener;
    m_listener->m_proxy = this;
    m_listener->m_friction = m_resource->m_friction;
    m_listener->m_strength = m_resource->m_strength;
    m_proxy->addCharacterProxyListener(m_listener);
    addToSimulation();
}

void ProxyInstance::setTransform(const hkQsTransform& t)
{
    hkTransform tm;
    t.copyToTransformNoScale(tm);
    setTransform(tm);
}

void ProxyInstance::setEnabled(bool bEnable)
{
    if(bEnable) addToSimulation();
    else removeFromSimulation();
}

void ProxyInstance::destroy()
{
    if(!m_proxy) return;
    hkpPhantom* phantom = m_proxy->getShapePhantom();
    hkpWorld* world = phantom->getWorld();
    if(world) 
    {
        world->markForWrite();
        world->removePhantom(phantom);
    }
    SAFE_REMOVEREF(m_listener);
    SAFE_REMOVEREF(m_proxy);
    if(world) world->unmarkForWrite();
}

void ProxyInstance::addToSimulation()
{
    if(!m_proxy) return;
    hkpPhantom* phantom = m_proxy->getShapePhantom();
    if(phantom->getWorld()) return;
    hkpWorld* world = g_physicsWorld.world();
    PHYSICS_LOCKWRITE(world);
    world->addPhantom(phantom);
}

void ProxyInstance::removeFromSimulation()
{
    if(!m_proxy) return;
    hkpPhantom* phantom = m_proxy->getShapePhantom();
    hkpWorld* world = phantom->getWorld();
    if(!world) return;
    PHYSICS_LOCKWRITE(world);
    world->removePhantom(phantom);
}

bool ProxyInstance::checkSupport()
{
    hkpSurfaceInfo ground;
    hkVector4 down; down.setNeg4( UP );
    m_proxy->checkSupport(down, ground);
    return ground.m_supportedState == hkpSurfaceInfo::SUPPORTED;
}

const hkVector4& ProxyInstance::getLinearVelocity() const
{
    return m_proxy->getLinearVelocity();
}

const hkVector4& ProxyInstance::getPosition() const
{
    return m_proxy->getPosition();
}

void ProxyInstance::setLinearVelocity( const hkVector4& vel )
{
    m_proxy->setLinearVelocity(vel);
}

void ProxyInstance::setTransform( const hkTransform& t )
{
    PHYSICS_LOCKWRITE(m_proxy->m_shapePhantom->getWorld());
    m_proxy->getShapePhantom()->setTransform(t);
}

bool ProxyInstance::isInWorld() const
{
    return m_proxy->m_shapePhantom->getWorld() != 0;
}

void ProxyInstance::update(float timeStep)
{
    const ProxyResource* res = m_resource;
    hkVector4 gravity; getGravity(gravity);

    hkVector4 desiredVelocity = m_targetVelocity;

    // Add horizontal displacement (try to make the proxy catch up)
    const hkReal catchUpGain = (m_listener->m_onMovingSurface) ? 0 : res->m_horizontalGain;
    desiredVelocity.addMul4(1.0f/timeStep * catchUpGain , m_horizontalDisplacement);

    // Feed output from state machine into character proxy
    m_proxy->setLinearVelocity(desiredVelocity);

    // Next expected position
    hkVector4 expectedPosition; 
    expectedPosition.setAddMul4(m_proxy->getPosition(), desiredVelocity, timeStep);

    hkStepInfo si;
    si.m_deltaTime = timeStep;
    si.m_invDeltaTime = 1.0f / timeStep;

    //@TODO ?
    //MOVE IT BATCH OR MULTITHREAD
    m_proxy->integrate( si, gravity);

    // Update the horizontal displacement
    const hkVector4& finalPosition = m_proxy->getPosition();
    hkVector4 horizontalFinalPosition; 
    horizontalFinalPosition.setAddMul4(finalPosition, UP, -finalPosition.dot4(UP));
    hkVector4 horizontalExpectedPosition; 
    horizontalExpectedPosition.setAddMul4(expectedPosition, UP, -expectedPosition.dot4(UP));
    m_horizontalDisplacement.setSub4(horizontalExpectedPosition, horizontalFinalPosition);

    {
        // Clamp the error
        const hkReal maxHorizontalDisplacement = res->m_maxHorizontalSeparation;
        const hkReal maxHorizontalDisplacementSqr = maxHorizontalDisplacement * maxHorizontalDisplacement;
        const hkReal errorSqr = m_horizontalDisplacement.lengthSquared3();
        if (errorSqr>maxHorizontalDisplacementSqr)
        {
            m_horizontalDisplacement.mul4(hkMath::sqrt(maxHorizontalDisplacementSqr/errorSqr));
        }
    }

    hkVector4 desiredPosition = m_proxy->getPosition();
    desiredPosition.addMul4(m_verticalDisplacement, UP);
    desiredPosition.add4(m_horizontalDisplacement);

    // Vertical : do gain and clamp
    hkReal newVerticalPosition;
    {
        const hkReal desiredVerticalPosition = desiredPosition.dot3(UP);
        const hkReal currentVerticalPosition = m_renderTranslation.dot3(UP);
        const hkReal diff = desiredVerticalPosition - currentVerticalPosition;

        newVerticalPosition = currentVerticalPosition + diff * res->m_verticalGain;

        // Clamp the vertical separation
        {
            const hkReal resultDiff = newVerticalPosition - desiredVerticalPosition;
            // The maximum we allow the items to separate
            const hkReal maxSeparation = res->m_maxVerticalSeparation;

            if ( resultDiff > maxSeparation ) newVerticalPosition = desiredVerticalPosition + maxSeparation;
            else if ( resultDiff < -maxSeparation ) newVerticalPosition = desiredVerticalPosition - maxSeparation;
        }
    }

    // Horizontal : no need to do gain or clamp (is done somewhere else)
    hkVector4 newHorizontalPosition; 
    newHorizontalPosition.setAddMul4(desiredPosition, UP, -desiredPosition.dot3(UP));
    m_renderTranslation.setAddMul4(newHorizontalPosition, UP, newVerticalPosition);
}

