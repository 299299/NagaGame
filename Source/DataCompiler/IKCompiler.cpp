#include "IKCompiler.h"

static const char* raycast_type_names[] =
{
    "physics", "graphics", 0
};
static const char* left_right_names[] = 
{
    "left", "right", 0
};

bool LookIKCompiler::readJSON( const JsonValue& root )
{
    __super::readJSON(root);
    LookAtResource lookat;
    memset(&lookat, 0x00, sizeof(lookat));

    vec3Make(lookat.m_fwdLS, 0, 0, 1);
    JSON_GetFloats(root.GetValue("forward-ls"), lookat.m_fwdLS, 3);

    lookat.m_lookAtLimit = JSON_GetFloat(root.GetValue("lookat-limit"), 3.1415926f / 4.0f);
    lookat.m_gain = JSON_GetFloat(root.GetValue("gain"), 0.05f);
    lookat.m_targetGain = JSON_GetFloat(root.GetValue("target-gain"),  0.2f);

    std::string rigFile = JSON_GetString(root.GetValue("rig"));
    lookat.m_rigName = StringId(rigFile.c_str());
    addDependency("rig", name_to_file_path(rigFile, AnimRig::getName()));

    if(!write_file(m_output, &lookat, sizeof(lookat)))
    {
        return false;
    }

#ifdef COMPILER_LOAD_TEST
    char* buf = 0;
    size_t fileLen = read_file(m_output, &buf);  
    HK_ASSERT(0, fileLen == sizeof(LookAtResource));
    LookAtResource* lookat2 = (LookAtResource*)buf;
    HK_ASSERT(0, lookat2->m_rigName == lookat.m_rigName);
    HK_ASSERT(0, lookat2->m_targetGain == lookat.m_targetGain);
    free(buf);
#endif
    return true;
}

bool ReachIKCompiler::readJSON( const JsonValue& root )
{
    __super::readJSON(root);
    ReachResource reach;
    memset(&reach, 0x00, sizeof(reach));

    vec3Make(reach.m_elbowAxis, 0, 1, 0);
    JSON_GetFloats(root.GetValue("elbow-axis"), reach.m_elbowAxis, 3);

    reach.m_hingeLimitAngle[0] = 0;
    reach.m_hingeLimitAngle[1] = 180;

    JSON_GetFloats(root.GetValue("hinge-limit-angle"), reach.m_hingeLimitAngle, 2);

    reach.m_reachGain = JSON_GetFloat(root.GetValue("reach-gain"),  0.3f);
    reach.m_leaveGain = JSON_GetFloat(root.GetValue("leave-gain"), 0.19f);
    reach.m_moveGain = JSON_GetFloat(root.GetValue("move-gain"), 0.085f);
    reach.m_targetGain = JSON_GetFloat(root.GetValue("target-gain"),  0.2f);
    reach.m_index = JSON_GetEnum(root.GetValue("hand"), left_right_names);

    std::string rigFile = JSON_GetString(root.GetValue("rig"));
    reach.m_rigName = StringId(rigFile.c_str());
    addDependency("rig", name_to_file_path(rigFile, AnimRig::getName()));

    if(!write_file(m_output, &reach, sizeof(reach)))
    {
        return false;
    }

#ifdef COMPILER_LOAD_TEST
    char* buf = 0;
    size_t fileLen = read_file(m_output, &buf);  
    HK_ASSERT(0, fileLen == sizeof(ReachResource));
    ReachResource* reach2 = (ReachResource*)buf;
    HK_ASSERT(0, reach2->m_rigName == reach.m_rigName);
    HK_ASSERT(0, reach2->m_index == reach.m_index);
    free(buf);
#endif
    return true;
}

bool FootIKCompiler::readJSON( const JsonValue& root )
{
    __super::readJSON(root);
    FootResource foot;
    memset(&foot, 0x00, sizeof(foot));

    vec3Make(foot.m_leftKneeAxisLS, 0, 0, 1);
    vec3Make(foot.m_rightKneeAxisLS, 0, 0, 1);
    vec3Make(foot.m_footEndLS, 0, 0, 0.2f);

    JSON_GetFloats(root.GetValue("left-knee-axis"), foot.m_leftKneeAxisLS, 3);
    JSON_GetFloats(root.GetValue("right-knee-axis"), foot.m_rightKneeAxisLS, 3);
    JSON_GetFloats(root.GetValue("foot-end-ls"), foot.m_footEndLS, 3);

    foot.m_orignalGroundHeightMS = JSON_GetFloat(root.GetValue("orginal-ground-height-ms"));
    foot.m_minAnkleHeightMS = JSON_GetFloat(root.GetValue("min-ankle-height-ms"));
    foot.m_maxAnkleHeightMS = JSON_GetFloat(root.GetValue("max-ankle-height-ms"));
    foot.m_footPlantedAnkleHeightMS = JSON_GetFloat(root.GetValue("foot-planted-ankle-height-ms"));
    foot.m_footRaisedAnkleHeightMS = JSON_GetFloat(root.GetValue("foot-raised-ankle-height-ms"));
    foot.m_cosineMaxKneeAngle = JSON_GetFloat(root.GetValue("max-consine-knee-angle"), 180);
    foot.m_cosineMinKneeAngle = JSON_GetFloat(root.GetValue("min-consine-knee-angle"), 0);
    foot.m_raycastDistanceUp = JSON_GetFloat(root.GetValue("raycast-dis-up"), 0.5f);
    foot.m_raycastDistanceDown = JSON_GetFloat(root.GetValue("raycast-dis-down"), 0.8f);
    foot.m_raycastCollisionLayer = JSON_GetInt(root.GetValue("raycast-layer"), -1);
    foot.m_groundAscendingGain = JSON_GetFloat(root.GetValue("ground-ascending-gain"), 0.35f);
    foot.m_groundDescendingGain = JSON_GetFloat(root.GetValue("ground-descending-gain"), 0.6f);
    foot.m_standAscendingGain = JSON_GetFloat(root.GetValue("ground-ascending-gain"), 0.6f);
    foot.m_footPlantedGain = JSON_GetFloat(root.GetValue("foot-planted-gain"), 1.0f);
    foot.m_footRaisedGain = JSON_GetFloat(root.GetValue("foot-raised-gain"), 0.85f);
    foot.m_footOnOffGain = JSON_GetFloat(root.GetValue("foot-onoff-gain"), 0.2f);
    foot.m_footUnLockGain = JSON_GetFloat(root.GetValue("foot-unlock-gain"), 0.85f);
    foot.m_pelvisFeedback = JSON_GetFloat(root.GetValue("pelvis-feedback"), 0.1f);
    foot.m_pelvisUpDownBias = JSON_GetFloat(root.GetValue("pelvis-updown-bias"), 0.95f);
    foot.m_raycastType = JSON_GetEnum(root.GetValue("raycast-type"), raycast_type_names);

    std::string rigFile = JSON_GetString(root.GetValue("rig"));
    foot.m_rigName = StringId(rigFile.c_str());
    addDependency("rig", name_to_file_path(rigFile, AnimRig::getName()));

    if(!write_file(m_output, &foot, sizeof(foot)))
    {
        return false;
    }

#ifdef COMPILER_LOAD_TEST
    char* buf = 0;
    size_t fileLen = read_file(m_output, &buf);  
    HK_ASSERT(0, fileLen == sizeof(FootResource));
    FootResource* foot2 = (FootResource*)buf;
    HK_ASSERT(0, foot2->m_rigName == foot.m_rigName);
    HK_ASSERT(0, foot2->m_raycastType == foot.m_raycastType);
    free(buf);
#endif
    return true;
}
