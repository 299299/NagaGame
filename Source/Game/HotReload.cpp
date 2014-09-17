#include "Resource.h"
#include "Log.h"
//===========================================
#include "AnimationSystem.h"
#include "Scene.h"
#include "EntityManager.h"
#include "PhysicsWorld.h"
#include "PhysicsAutoLock.h"
//===========================================
#include "ShadingEnviroment.h"
#include "AnimRig.h"
#include "Animation.h"
#include "ComponentManager.h"
#include "Graphics.h"
#include "Model.h"
#include "Shader.h"
#include "Material.h"
#include "Light.h"
#include "Mesh.h"       
#include "IK.h"
#include "Texture.h"
#include "Ragdoll.h"
#include "PhysicsInstance.h"
#include "ProxyInstance.h"
#include "Entity.h"
#include "AnimFSM.h"
#include "GameFSM.h"
#include "GameState.h"
#include "Level.h"
#include <bx/bx.h>
//==========================================================================================
#include <Animation/Animation/Animation/hkaAnimation.h>
#include <Animation/Animation/Animation/Mirrored/hkaMirroredSkeleton.h>
#include <Animation/Animation/Animation/Mirrored/hkaMirroredAnimation.h>
//==========================================================================================

static void* g_tmpResourceArray[1024*10];
const uint32_t resourceMax = BX_COUNTOF(g_tmpResourceArray);
static ResourceInfo** result = (ResourceInfo**)g_tmpResourceArray;

//===================================================================================================
template <typename T, typename U> void reload_component_resource(void* oldResource, void* newResource)
{
    T* oldCompResource = (T*)oldResource;
    T* newCompResource = (T*)newResource;
    uint32_t componentNum = g_componentMgr.numComponents(T::getType());
    U* components = (U*)g_componentMgr.listComponents(T::getType());
    LOGI("component %s instance num = %d", T::getName(), componentNum);
    for(size_t i=0; i<componentNum; ++i)
    {
        if(components[i].m_resource == oldCompResource)
            components[i].init(newCompResource); //---> no destroy?? may memleak
    }
}
template <typename T, typename U> void register_component_resource_reload_callback()
{
    g_resourceMgr.registerReloadCallback(T::getType(), reload_component_resource<T, U>);
}
//===================================================================================================


//===============================================================================
void reload_animation_resource(void* oldResource, void* newResource)
{
    Animation* oldAnimation = (Animation*)oldResource;
    Animation* newAnimation = (Animation*)newResource;

    uint32_t numOfFSM = g_resourceMgr.findResourcesTypeOf(AnimFSM::getType(), result, resourceMax);
    LOGD("total num of anim fsm resources = %d", numOfFSM);
    //holy shit, deep loop!
    for (uint32_t i=0; i<numOfFSM; ++i)
    {
        AnimFSM* fsm = (AnimFSM*)result[i]->m_ptr;
        for (uint32_t j=0; j<fsm->m_numLayers; ++j)
        {
            AnimFSMLayer& layer = fsm->m_layers[j];
            for(uint32_t k=0; k<layer.m_numStates; ++k)
            {
                State& state = layer.m_states[k];
                for (uint32_t m=0; m<state.m_numAnimations; ++m)
                {
                    if(state.m_animations[m] == oldAnimation)
                        state.m_animations[m] = newAnimation;
                }
            }
        }
        //just tell anim fsm to reload.
        reload_component_resource<AnimFSM, AnimFSMInstance>(fsm, fsm);
    }

    uint32_t numOfAnimations = g_resourceMgr.findResourcesTypeOf(Animation::getType(), result, resourceMax);
    LOGD("total num of animation resources = %d", numOfAnimations);
    for (uint32_t i = 0; i < numOfAnimations; ++i)
    {
        Animation* anim = (Animation*)result[i]->m_ptr;
        if(anim->m_mirroredFrom.isZero()) continue;
        hkaMirroredAnimation* mirrorAnim = (hkaMirroredAnimation*)anim->m_animation;
        const hkaAnimationBinding* binding = mirrorAnim->getOriginalBinding();
        if(oldAnimation->m_binding == binding)
        {
            anim->destroy();
            anim->createMirrorAnimation(newAnimation);
        }
    }
}

void reload_anim_rig_resource(void* oldResource, void* newResource)
{

}

//===============================================================================

void reload_level_resource(void* oldResource, void* newResource)
{
    Level* oldLevel = (Level*)oldResource;
    Level* newLevel = (Level*)newResource;
    oldLevel->unload();
    newLevel->load(-1);
    newLevel->flush();
}

void reload_shading_enviroment(void* oldResource, void* newResource)
{
    ShadingEnviroment* oldShading = (ShadingEnviroment*)oldResource;
    ShadingEnviroment* newShading = (ShadingEnviroment*)newResource;
    for(int i=0; i<g_gameFSM.m_numStates; ++i)
    {
        GameState* state = g_gameFSM.m_states[i];
        if(state->m_shading == oldShading)
            state->m_shading = newShading;
    }
}
void reload_material_resource(void* oldResource, void* newResource)
{
    Material* oldMat = (Material*)oldResource;
    Material* newMat = (Material*)newResource;

    uint32_t numOfModels = g_resourceMgr.findResourcesTypeOf(ModelResource::getType(), result, resourceMax);
    LOGD("total num of model resources = %d", numOfModels);
    for(uint32_t i=0; i<numOfModels; ++i)
    {
        ModelResource* model = (ModelResource*)result[i]->m_ptr;
        for(uint32_t j=0; j<model->m_numMaterials; ++j)
        {
            if(model->m_materials[j] == oldMat)
            {
                model->m_materials[j] = newMat;
            }
        }
    }

    ComponentFactory* fac = g_componentMgr.findFactory(ModelResource::getType());
    numOfModels = fac->numComponents();
    ModelInstance* models = (ModelInstance*)fac->listComponents();
    LOGD("total num of model instances = %d", numOfModels);
    for(uint32_t i=0; i<numOfModels; ++i)
    {
        ModelInstance& model = models[i];
        for(uint32_t j=0; j<model.m_numMaterials; ++j)
        {
            if(model.m_materials[j] == oldMat)
            {
                model.m_materials[j] = newMat;
            }
        }
    }
}
void reload_texture_resource(void* oldResource, void* newResource)
{   
    Texture* oldTex = (Texture*)oldResource;
    Texture* newTex = (Texture*)newResource;

    uint32_t numOfMaterials = g_resourceMgr.findResourcesTypeOf(Material::getType(), result, resourceMax);
    LOGD("total num of materials = %d", numOfMaterials);
    for(uint32_t i=0; i<numOfMaterials; ++i)
    {
        Material* mat = (Material*)result[i]->m_ptr;
        for(uint32_t j=0; j<mat->m_numSamplers; ++j)
        {
            MatSampler& sampler = mat->m_samplers[j];
            if(sampler.m_texture == oldTex)
            {
                sampler.m_texture = newTex;
            }
        }
        mat->bringIn();
    }
}
void reload_texture_3d_resource(void* oldResource, void* newResource)
{
    Raw3DTexture* oldTex = (Raw3DTexture*)oldResource;
    Raw3DTexture* newTex = (Raw3DTexture*)newResource;

    uint32_t numOfEnv = g_resourceMgr.findResourcesTypeOf(ShadingEnviroment::getType(), result, resourceMax);
    LOGD("total num of shading enviroment = %d", numOfEnv);
    for(uint32_t i=0; i<numOfEnv; ++i)
    {
        ShadingEnviroment* env = (ShadingEnviroment*)result[i]->m_ptr;
        for (uint32_t j = 0; j < env->m_numColorgradingTextures; ++j)
        {
            if(env->m_colorGradingTextures[j] == oldTex) env->m_colorGradingTextures[j] = newTex;
        }
    }
}
void reload_mesh_resource(void* oldResource, void* newResource)
{
    Mesh* oldMesh = (Mesh*)oldResource;
    Mesh* newMesh = (Mesh*)newResource;

    uint32_t numOfModels = g_resourceMgr.findResourcesTypeOf(ModelResource::getType(), result, resourceMax);
    LOGD("total num of model resources = %d", numOfModels);
    for(uint32_t i=0; i<numOfModels; ++i)
    {
        ModelResource* model = (ModelResource*)result[i]->m_ptr;
        if(model->m_mesh == oldMesh)
            model->m_mesh = newMesh;
    }

    ComponentFactory* fac = g_componentMgr.findFactory(ModelResource::getType());
    numOfModels = fac->numComponents();
    ModelInstance* models = (ModelInstance*)fac->listComponents();
    LOGD("total num of model instances = %d", numOfModels);
    for(uint32_t i=0; i<numOfModels; ++i)
    {
        ModelInstance& model = models[i];
        if(model.m_mesh == oldMesh)
            model.m_mesh = newMesh;
    }
}
void reload_program_resource(void* oldResource, void* newResource)
{
#define CHECK_SHADER_HANDLE(shader)  if(shader.idx == oldHandle.idx) shader.idx = newHandle.idx;
    
    ShaderProgram* oldProgram = (ShaderProgram*)oldResource;
    ShaderProgram* newProgram = (ShaderProgram*)newResource;

    bgfx::ProgramHandle oldHandle = oldProgram->m_handle;
    bgfx::ProgramHandle newHandle = newProgram->m_handle;

    extern PostProcess          g_postProcess;
    CHECK_SHADER_HANDLE(g_postProcess.m_brightShader);
    CHECK_SHADER_HANDLE(g_postProcess.m_blurShader);
    CHECK_SHADER_HANDLE(g_postProcess.m_combineShader);
    
     uint32_t numOfMaterials = g_resourceMgr.findResourcesTypeOf(Material::getType(), result, resourceMax);
    LOGD("total num of materials = %d", numOfMaterials);
    for(uint32_t i=0; i<numOfMaterials; ++i)
    {
        Material* mat = (Material*)result[i]->m_ptr;
        if(mat->m_shader == oldProgram)
            mat->m_shader = newProgram;
        if(mat->m_shadowShader == oldProgram)
            mat->m_shadowShader = newProgram;
    }
}

void reload_shader_resource(void* oldResource, void* newResource)
{
#define CHECK_PROGRAM_HANDLE(shader)  if(shader.idx == oldHandle.idx) shader.idx = newHandle.idx;

    extern PostProcess          g_postProcess;
    Shader* oldShader = (Shader*)oldResource;
    Shader* newShader = (Shader*)newResource;
    
    uint32_t numOfPrograms = g_resourceMgr.findResourcesTypeOf(ShaderProgram::getType(), result, resourceMax);
    LOGD("total num of programs = %d", numOfPrograms);
    for(uint32_t i=0; i<numOfPrograms; ++i)
    {
        ShaderProgram* program = (ShaderProgram*)result[i]->m_ptr;
        if(program->m_vs == oldShader || 
           program->m_ps == oldShader)
        {
            bgfx::ProgramHandle oldHandle = program->m_handle;
            program->bringOut();
            if(program->m_vs == oldShader) program->m_vs = newShader;
            if(program->m_ps == oldShader) program->m_ps = newShader;
            program->bringIn();
            bgfx::ProgramHandle newHandle = program->m_handle;
            
            CHECK_PROGRAM_HANDLE(g_postProcess.m_brightShader);
            CHECK_PROGRAM_HANDLE(g_postProcess.m_blurShader);
            CHECK_PROGRAM_HANDLE(g_postProcess.m_combineShader);
        }
    }
}


void reload_entity_resource(void* oldResource, void* newResource)
{
    EntityResource* oldEntity = (EntityResource*)oldResource;
    EntityResource* newEntity = (EntityResource*)newResource;
    EntityInstance** tmphead = (EntityInstance**)(g_tmpResourceArray);

    for(uint32_t i=0; i<kEntityClassNum; ++i)
    {
        EntityBucket* bucket = g_entityMgr.getBucket(i);
        uint32_t num = bucket->list_entities(tmphead, resourceMax);
        for(uint32_t j=0; j<num; ++j)
        {
            EntityInstance* inst = tmphead[j];
            if(inst->m_resource == oldEntity)
            {
                hkQsTransform t = inst->m_transform;
                uint32_t oldId = inst->m_id;
                bucket->remove_entity(inst->m_id);
                uint32_t newId = bucket->add_entity(newEntity, t);
                LOGI("entity %d --> %d", oldId, newId);
            }
        }
    }
}
//===================================================================================================


void resource_hot_reload_init()
{
    g_resourceMgr.registerReloadCallback(ShadingEnviroment::getType(), reload_shading_enviroment);
    g_resourceMgr.registerReloadCallback(Texture::getType(), reload_texture_resource);
    g_resourceMgr.registerReloadCallback(Raw3DTexture::getType(), reload_texture_3d_resource);
    g_resourceMgr.registerReloadCallback(Mesh::getType(), reload_mesh_resource);
    g_resourceMgr.registerReloadCallback(EntityResource::getType(), reload_entity_resource);
    g_resourceMgr.registerReloadCallback(Material::getType(), reload_material_resource);
    g_resourceMgr.registerReloadCallback(Shader::getType(), reload_shader_resource);
    g_resourceMgr.registerReloadCallback(ShaderProgram::getType(), reload_program_resource);
    g_resourceMgr.registerReloadCallback(Level::getType(), reload_level_resource);
    g_resourceMgr.registerReloadCallback(Animation::getType(), reload_animation_resource);
    g_resourceMgr.registerReloadCallback(AnimRig::getType(), reload_anim_rig_resource);

    register_component_resource_reload_callback<ModelResource, ModelInstance>();
    register_component_resource_reload_callback<LightResource, LightInstance>();
    register_component_resource_reload_callback<RagdollResource,RagdollInstance>();
    register_component_resource_reload_callback<AnimRig, AnimRigInstance>();
    register_component_resource_reload_callback<LookAtResource, LookAtInstance>();
    register_component_resource_reload_callback<ReachResource, ReachInstance>();
    register_component_resource_reload_callback<FootResource, FootInstance>();
    register_component_resource_reload_callback<PhysicsResource, PhysicsInstance>();
    register_component_resource_reload_callback<ProxyResource, ProxyInstance>();
    register_component_resource_reload_callback<AnimFSM, AnimFSMInstance>();
}

