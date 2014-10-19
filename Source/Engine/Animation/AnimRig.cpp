#include "AnimRig.h"
#include "Resource.h"
#include "Log.h"
#include "Animation.h"
#include "Utils.h"
#include "MemorySystem.h"
#include "MathDefs.h"
#include <bx/fpumath.h>
//=======================================================================================
#include <Animation/Animation/Playback/hkaAnimatedSkeleton.h>
#include <Animation/Animation/hkaAnimationContainer.h>
#include <Animation/Animation/Animation/Mirrored/hkaMirroredSkeleton.h>
#include <Animation/Animation/Rig/hkaSkeleton.h>
#include <Animation/Animation/Rig/hkaPose.h>
#include <Animation/Animation/Playback/Control/Default/hkaDefaultAnimationControl.h>
#include <Common/Serialize/Util/hkRootLevelContainer.h>
#include <Animation/Animation/Animation/ReferencePose/hkaReferencePoseAnimation.h>
#include <Animation/Animation/Rig/hkaSkeletonUtils.h>
//=======================================================================================

struct hk_anim_ctrl : public hkaDefaultAnimationControl
{
	HK_DECLARE_CLASS_ALLOCATOR(HK_MEMORY_CLASS_ANIM_CONTROL);
	hk_anim_ctrl(Animation* anim, bool bLoop)
	:hkaDefaultAnimationControl(anim->m_binding, false, bLoop?-1:1)
	,m_animation(anim)
	{

	}
	Animation*				m_animation;

	void ease_in(float time, int type)
	{
		switch(type)
		{
		case kEaseCurveLinear:setEaseInCurve(0, 0.33f, 0.66f, 1);break;
		case kEaseCurveFast:setEaseInCurve(0, 0, 0, 1);break;
		case kEaseCurveSmooth:
		default:
			setEaseInCurve(0, 0, 1, 1);break;
		}
		easeIn(time);
	}

	void ease_out(float time, int type)
	{
		switch(type)
		{
		case kEaseCurveLinear:setEaseInCurve(1, 0.66f, 0.33f, 0);break;
		case kEaseCurveFast:setEaseInCurve(1, 1, 0, 0);break;
		case kEaseCurveSmooth:
		default:
			setEaseInCurve(1, 1, 0, 0);break;
		}
		easeOut(time);
	}

	void set_weight(float fWeight)
	{
		setMasterWeight(fWeight);
	}

	float get_peroid() const
	{
		return m_binding->m_animation->m_duration;
	}
};


int AnimRig::find_joint_index(const StringId& jointName)
{
    for(int i=0; i<m_jointNum; ++i)
    {
        if(m_jointNames[i] == jointName)
            return i;
    }
    return -1;
}

void AnimRig::create_mirrored_skeleton()
{
    hkArray<hkStringPtr> ltag;
    hkArray<hkStringPtr> rtag;
    ltag.pushBack( "EyeL" ); rtag.pushBack( "EyeR" );
    ltag.pushBack( " L " ); rtag.pushBack( " R " );
    ltag.pushBack( "L_" ); rtag.pushBack( "R_" );
    ltag.pushBack( "_L" ); rtag.pushBack( "_R" );
    ltag.pushBack( "-L-" ); rtag.pushBack( "-R-" );
    ltag.pushBack( "-LUp" ); rtag.pushBack( "-RUp" );
    ltag.pushBack( "Left" ); rtag.pushBack( "Right" );

    m_mirroredSkeleton = new hkaMirroredSkeleton( m_skeleton );
    m_mirroredSkeleton->computeBonePairingFromNames( ltag, rtag );
    // Mirror this character about the X axis
    hkQuaternion v_mir( -1.0f, 0.0f, 0.0f, 0.0f );
    m_mirroredSkeleton->setAllBoneInvariantsFromReferencePose( v_mir, 0.0f );
}

void AnimRigInstance::destroy()
{
    SAFE_DELETE(m_pose);
    SAFE_REMOVEREF(m_skeleton);
}

void AnimRigInstance::update_local_clock(float dt)
{
	for (int i=0; i<m_skeleton->getNumAnimationControls(); ++i)
	{
		hk_anim_ctrl* ac = (hk_anim_ctrl*)m_skeleton->getAnimationControl(i);
		if(ac->getEaseStatus() == hkaDefaultAnimationControl::EASED_OUT)
			m_skeleton->removeAnimationControl(ac);
	}
    m_skeleton->stepDeltaTime(dt);
}

void AnimRigInstance::init( const void* resource )
{
    m_attachmentTransforms = 0;
    m_resource = (const AnimRig*)resource;
    m_skeleton = new hkaAnimatedSkeleton(m_resource->m_skeleton);
    m_pose = new hkaPose(m_skeleton->getSkeleton());
    m_pose->setToReferencePose();
    m_pose->syncAll();
}

bool AnimRigInstance::is_playing_animation() const
{   
    int numControls = m_skeleton->getNumAnimationControls();
    for(int i=0; i<numControls; ++i)
    {
        hkaDefaultAnimationControl* ac = (hkaDefaultAnimationControl*)m_skeleton->getAnimationControl(i);
        float speed = ac->getPlaybackSpeed();
        if(speed > 0.0f) return true;
    }
    return false;
}

void AnimRigInstance::play_animation( const StringId& anim_name, bool bLoop, float fTime )
{
    Animation* anim = FIND_RESOURCE(Animation, anim_name);
    if(!anim) return;
    int maxCycles = bLoop ? -1 : 1;
	hk_anim_ctrl* ac = new hk_anim_ctrl(anim, bLoop);
    m_skeleton->setReferencePoseWeightThreshold(0.0f);
    m_skeleton->addAnimationControl(ac);
    ac->removeReference();
	ac->ease_in(fTime, kEaseCurveSmooth);
}

void AnimRigInstance::update_attachments( const float* worldFromModel )
{
    uint32_t num = m_resource->m_attachNum;
    const BoneAttachment* attachments = m_resource->m_attachments;
    m_attachmentTransforms = FRAME_ALLOC(float, num*16);
    const hkArray<hkQsTransform>& poseInWorld = m_pose->getSyncedPoseModelSpace();
    hkMatrix4 worldPose; transform_matrix(worldPose, worldFromModel);
    for (uint32_t i=0; i<num; ++i)
    {
        const BoneAttachment& ba = attachments[i];
        hkMatrix4 worldFromBone; worldFromBone.set( poseInWorld [ ba.m_boneIndex ] );
        hkMatrix4 boneFromAttachment; transform_matrix(boneFromAttachment, ba.m_boneFromAttachment);
        hkMatrix4 worldFromAttachment; worldFromAttachment.setMul(worldFromBone, boneFromAttachment);
        hkMatrix4 finalAttachment; finalAttachment.setMul(worldFromAttachment, worldPose);
        transform_matrix(m_attachmentTransforms+i*16, finalAttachment);
    }
}

void* load_resource_anim_rig(const char* data, uint32_t size)
{
    AnimRig* rig = (AnimRig*)data;
    const char* offset = data;
    offset += sizeof(AnimRig);
    rig->m_jointNames = (StringId*)(offset);
    offset += sizeof(StringId) * rig->m_jointNum;
    rig->m_attachments = (BoneAttachment*)offset;
    offset = data + rig->m_havokDataOffset;
    rig->m_skeleton = (hkaSkeleton*)load_havok_inplace((void*)offset, rig->m_havokDataSize);
    if(rig->m_mirrored) rig->create_mirrored_skeleton();
    return rig;
}

void  destroy_resource_anim_rig(void * resource)
{
    AnimRig* rig = (AnimRig*)resource;
    char* p = (char*)resource + rig->m_havokDataOffset;
    SAFE_REMOVEREF(rig->m_mirroredSkeleton);
    unload_havok_inplace(p, rig->m_havokDataSize);
}
