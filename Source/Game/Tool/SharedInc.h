#pragma once
#include "Prerequisites.h"
#include "Log.h"
#include "MemorySystem.h"
#include "MathDefs.h"

#include <stdio.h>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>

#include <bgfx.h>
#include <bx/fpumath.h>
#include <bx/bx.h>
#include <bx/platform.h>
#include <bx/commandline.h>
#include <bx/mutex.h>
#include <bx/readerwriter.h>
#include <tinystl/allocator.h>
#include <tinystl/unordered_map.h>

#include "jsonxx.h"
#include "DataDef.h"
#include "ShaderInc.h"
#include "Utils.h"
#include "Profiler.h"

#ifdef HAVOK_COMPILE
#include <Common/Base/hkBase.h>
#include <Common/Base/Ext/hkBaseExt.h>
#include <Common/Base/Math/hkMath.h>
#include <Common/Base/System/hkBaseSystem.h>
#include <Common/Base/System/Error/hkError.h>
#include <Common/Base/Object/hkSingleton.h>
#include <Common/Base/Container/String/hkStringPtr.h>
#include <Common/Base/Container/StringMap/hkStringMap.h>
#include <Common/Base/Container/String/hkStringBuf.h>
#include <Common/Base/Memory/System/hkMemorySystem.h>
#include <Common/Base/Memory/System/Util/hkMemoryInitUtil.h>
#include <Common/Base/Memory/Allocator/Malloc/hkMallocAllocator.h>
#include <Common/Base/Memory/Allocator/Thread/hkThreadMemory.h>
#include <Common/Base/System/Io/OStream/hkOStream.h>
#include <Common/Serialize/Util/hkSerializeUtil.h>
#include <Common/Serialize/Packfile/hkPackfileData.h>
#include <Common/Base/Types/Geometry/Aabb/hkAabb.h>
#include <Common/Base/Types/Geometry/hkGeometry.h>
#include <Common/Base/System/hkBaseSystem.h>
#include <Common/Base/System/Error/hkDefaultError.h>
#include <Common/Base/System/Io/IStream/hkIStream.h>
#include <Common/Base/Memory/System/Util/hkMemoryInitUtil.h>
#include <Common/Base/Monitor/hkMonitorStream.h>
#include <Common/Base/Memory/System/hkMemorySystem.h>
#include <Common/Base/Container/LocalArray/hkLocalBuffer.h>
#include <Common/Base/Thread/Pool/hkCpuThreadPool.h>
#include <Common/Base/Thread/JobQueue/hkJobQueue.h>
#include <Common/Serialize/Util/hkLoader.h>
#include <Common/Serialize/Util/hkRootLevelContainer.h>
#include <Common/Base/Types/hkRefPtr.h>
#include <Common/Visualize/hkVisualDebugger.h>
#include <Common/Visualize/hkDebugDisplay.h>
#include <Common/Visualize/Shape/hkDisplayGeometry.h>
#include <Common/Base/System/Io/OArchive/hkOArchive.h>
#include <Common/Base/System/Io/IArchive/hkIArchive.h>
#include <Common/Base/Container/String/hkStringBuf.h>
#include <Common/Base/System/Io/Reader/Memory/hkMemoryStreamReader.h>
#include <Animation/Animation/hkaAnimationContainer.h>
#include <Animation/Animation/Rig/hkaSkeleton.h>
#include <Animation/Animation/Rig/hkaSkeletonUtils.h>
#include <Animation/Animation/Rig/hkaPose.h>
#include <Animation/Animation/Playback/hkaAnimatedSkeleton.h>
#include <Animation/Animation/Playback/Control/Default/hkaDefaultAnimationControl.h>
#include <Animation/Animation/Playback/Control/Default/hkaDefaultAnimationControlListener.h>
#include <Animation/Animation/Animation/hkaAnimationBinding.h>
#include <Animation/Animation/Mapper/hkaSkeletonMapper.h>
#include <Animation/Animation/Deform/Skinning/hkaMeshBinding.h>
#include <Animation/Animation/Animation/Mirrored/hkaMirroredAnimation.h>
#include <Animation/Animation/Animation/Mirrored/hkaMirroredSkeleton.h>
#include <Animation/Animation/Ik/LookAt/hkaLookAtIkSolver.h>
#include <Animation/Animation/Ik/TwoJoints/hkaTwoJointsIkSolver.h>
#include <Animation/Animation/Ik/FootPlacement/hkaFootPlacementIkSolver.h>
#include <Animation/Animation/Ik/Ccd/hkaCcdIkSolver.h>
#include <Common/Visualize/hkDebugDisplay.h>
#include <Common/Visualize/hkDebugDisplayHandler.h>
#include <Common/Visualize/hkProcessFactory.h>
#include <Common/Visualize/hkVisualDebugger.h>
#include <Physics2012/Dynamics/World/Listener/hkpWorldPostSimulationListener.h>
#include <Physics2012/Utilities/VisualDebugger/hkpPhysicsContext.h>
#include <Physics2012/Utilities/VisualDebugger/Viewer/Collide/hkpShapeDisplayViewer.h>
#include <Physics2012/Utilities/VisualDebugger/Viewer/Collide/hkpBroadphaseViewer.h>
#include <Physics2012/Utilities/VisualDebugger/Viewer/Collide/hkpActiveContactPointViewer.h>
#include <Physics2012/Utilities/VisualDebugger/Viewer/Dynamics/hkpPhantomDisplayViewer.h>
#include <Physics2012/Collide/Query/CastUtil/hkpLinearCastInput.h>
#include <Physics2012/Dynamics/World/hkpSimulationIsland.h>
#include <Physics2012/Dynamics/World/hkpWorld.h>
#include <Physics2012/Dynamics/Entity/hkpRigidBody.h>
#include <Physics2012/Collide/Filter/Group/hkpGroupFilter.h>
#include <Physics2012/Utilities/Serialize/hkpPhysicsData.h>
#include <Physics2012/Dynamics/World/BroadPhaseBorder/hkpBroadPhaseBorder.h>
#include <Physics2012/Collide/Dispatch/hkpAgentRegisterUtil.h>
#include <Common/Base/Thread/Pool/hkCpuThreadPool.h>
#include <Physics2012/Collide/Shape/Convex/Box/hkpBoxShape.h>
#include <Physics2012/Utilities/CharacterControl/CharacterRigidBody/hkpCharacterRigidBody.h>
#include <Physics2012/Collide/Shape/Convex/Capsule/hkpCapsuleShape.h>
#include <Common/Serialize/Util/hkNativePackfileUtils.h>
#include <Physics2012/Collide/Query/CastUtil/hkpWorldRayCastInput.h>
#include <Physics2012/Collide/Query/CastUtil/hkpWorldRayCastOutput.h>
#include <Physics2012/Utilities/CharacterControl/CharacterProxy/hkpCharacterProxy.h>
#include <Animation/Animation/Playback/SampleAndBlend/hkaSampleBlendJobQueueUtils.h>
#include <Animation/Animation/Playback/SampleAndBlend/hkaSampleBlendJob.h>
#include <Animation/Animation/Playback/Multithreaded/hkaMultithreadedAnimationUtils.h>
#include <Physics2012/Collide/Agent3/Machine/Nn/hkpAgentNnTrack.h>
#include <Physics2012/Dynamics/Collide/hkpSimpleConstraintContactMgr.h>
#include <Physics2012/Utilities/Dynamics/Inertia/hkpInertiaTensorComputer.h>
#include <Physics2012/Dynamics/Phantom/hkpAabbPhantom.h>
#include <Physics2012/Dynamics/Phantom/hkpSimpleShapePhantom.h>
#include <Physics2012/Collide/Shape/Convex/ConvexVertices/hkpConvexVerticesShape.h>
#include <Animation/Physics2012Bridge/Instance/hkaRagdollInstance.h>
#include <Animation/Physics2012Bridge/Controller/PoweredConstraint/hkaRagdollPoweredConstraintController.h>
#include <Animation/Physics2012Bridge/Controller/RigidBody/hkaRagdollRigidBodyController.h>
#include <Animation/Physics2012Bridge/Utils/hkaRagdollUtils.h>
#include <Physics/Constraint/Data/LimitedHinge/hkpLimitedHingeConstraintData.h>
#include <Physics/Constraint/Data/Ragdoll/hkpRagdollConstraintData.h>
#include <Physics/Constraint/Motor/Position/hkpPositionConstraintMotor.h>
#include <Physics2012/Utilities/Constraint/Bilateral/hkpConstraintUtils.h>
#include <Common/Base/Container/LocalArray/hkLocalBuffer.h>
#include <Common/SceneData/Scene/hkxScene.h>
#include <Common/Base/Reflection/hkClass.h>
#include <Physics2012/Dynamics/World/hkpPhysicsSystem.h>
#include <Common/Base/Math/Matrix/hkMatrixDecomposition.h>
#include <Common/SceneData/Skin/hkxSkinBinding.h>
#include <Animation/Animation/hkaAnimationContainer.h>
#include <Physics2012/Utilities/Serialize/hkpPhysicsData.h>
#include <Physics2012/Collide/Shape/Compound/Tree/Mopp/hkpMoppCompilerInput.h>
#include <Physics2012/Internal/Collide/Mopp/Code/hkpMoppCode.h>
#include <Physics2012/Collide/BroadPhase/hkpBroadPhase.h>
#include <Physics2012/Collide/Shape/Compound/Tree/Mopp/hkpMoppUtility.h>
#include <Physics2012/Collide/Shape/Compound/Tree/Mopp/hkpMoppBvTreeShape.h>
#include <Physics2012/Collide/Shape/Compound/Collection/SimpleMesh/hkpSimpleMeshShape.h>
#include <Physics2012/Collide/Shape/Convex/Capsule/hkpCapsuleShape.h>
#include <Physics2012/Collide/Shape/Convex/Cylinder/hkpCylinderShape.h>
#include <Physics2012/Collide/Shape/Convex/ConvexVertices/hkpConvexVerticesShape.h>
#include <Physics2012/Collide/Shape/Convex/Box/hkpBoxShape.h>
#include <Physics2012/Collide/Shape/Convex/Sphere/hkpSphereShape.h>
#include <Physics2012/Collide/Shape/Compound/Collection/StorageExtendedMesh/hkpStorageExtendedMeshShape.h>
#include <Physics2012/Collide/Shape/Deprecated/CompressedMesh/hkpCompressedMeshShapeBuilder.h>
#include <Physics2012/Dynamics/Phantom/hkpPhantomListener.h>
#include <Physics2012/Dynamics/World/Listener/hkpWorldDeletionListener.h>
#include <Physics/Constraint/Data/Ragdoll/hkpRagdollConstraintData.h>
#include <Physics/Constraint/Data/LimitedHinge/hkpLimitedHingeConstraintData.h>
#include <Physics2012/Dynamics/Entity/hkpRigidBody.h>
#include <Physics2012/Dynamics/World/hkpPhysicsSystem.h>
#include <Physics2012/Dynamics/Entity/hkpEntityListener.h>
#include <Physics2012/Utilities/Dynamics/Inertia/hkpInertiaTensorComputer.h>
#include <Physics2012/Utilities/Serialize/hkpHavokSnapshot.h>
#include <Physics2012/Utilities/Serialize/hkpPhysicsData.h>
#include <Physics2012/Collide/Query/Collector/RayCollector/hkpAllRayHitCollector.h>
#include <Physics2012/Collide/Query/Collector/RayCollector/hkpClosestRayHitCollector.h>
#include <Physics2012/Collide/Query/Collector/PointCollector/hkpAllCdPointCollector.h>
#include <Physics2012/Collide/Query/Collector/PointCollector/hkpClosestCdPointCollector.h>
#include <Common/SceneData/Scene/hkxScene.h>
#include <Common/SceneData/SceneDataToGeometryConverter/hkxSceneDataToGeometryConverter.h>
#include <Common/Base/Math/Matrix/hkMatrix3Util.h>
#include <Common/Base/Types/Geometry/hkGeometry.h>
#include <Common/Internal/ConvexHull/hkGeometryUtility.h>
#include <Common/Base/Algorithm/PseudoRandom/hkPseudoRandomGenerator.h>
#include <Common/Internal/SimplexSolver/hkSimplexSolver.h>
#include <Common/Internal/Math/hkVector4UtilInternal.h>
#include <Common/Base/Math/Vector/hkVector4Util.h>
#include <Physics2012/Dynamics/Collide/ContactListener/hkpContactListener.h>
#include <Physics2012/Collide/Query/Collector/PointCollector/hkpClosestCdPointCollector.h>
#include <Animation/Animation/Animation/ReferencePose/hkaReferencePoseAnimation.h>
#include <Animation/Animation/Animation/Util/hkaPartitionedAnimationUtility.h>
#include <Common/Visualize/hkDebugDisplay.h>
#include <Common/SceneData/Material/hkxMaterial.h>
#include <Common/SceneData/Mesh/hkxMesh.h>
#include <Common/SceneData/Scene/hkxScene.h>
#include <Common/SceneData/Scene/hkxSceneUtils.h>
#include <Common/SceneData/SceneDataToGeometryConverter/hkxSceneDataToGeometryConverter.h>
#include <Common/SceneData/AlignScene/hkAlignSceneToNodeOptions.h>
#include <Common/SceneData/Camera/hkxCamera.h>
#include <Common/SceneData/Light/hkxLight.h>
#include <Common/SceneData/Spline/hkxSpline.h>
#include <Common/SceneData/Environment/hkxEnvironment.h>
#include <Common/Base/Container/Array/hkArray.h>
#include <Common/Base/Algorithm/Sort/hkSort.h>
#include <Common/Serialize/Packfile/Binary/hkBinaryPackfileWriter.h>
#include <Common/Serialize/Util/hkNativePackfileUtils.h>
#include <Common/Base/Math/Matrix/hkMatrixDecomposition.h>
#include <Common/Base/System/Io/Writer/Buffered/hkBufferedStreamWriter.h>
#endif