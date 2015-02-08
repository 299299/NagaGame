#pragma once
#include "BaseTypes.h"

class hkaSkeleton;
class hkaAnimatedSkeleton;
class hkaPose;
class hkLoader;
class hkaMirroredSkeleton;
class hkaAnimation;
class hkaMirroredAnimation;
class hkaAnimationBinding;

struct AnimRig;
struct AnimRigInstance;
struct Actor;
struct AnimationEvent;
class hkaSampleBlendJob;

struct AnimationConfig
{
    int    max_rigs;
    int    max_state_layers;
    int    max_anim_events;
};

struct AnimationSystem
{
    void    init(const AnimationConfig& cfg);
    void    shutdown();

    void    frame_start();
    void    kick_in_jobs();
    void    tick_finished_jobs();
    void    update_attachment(Actor* actors, uint32_t num);
    void    skin_actors(Actor* actors, uint32_t num);
    void    update_animations(float dt);

    uint32_t                m_numAnimEvts;
    AnimationEvent*         m_events;
};
extern AnimationSystem g_animMgr;



