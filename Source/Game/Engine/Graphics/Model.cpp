#include "Model.h"
#include "Material.h"
#include "Mesh.h"
#include "Resource.h"
#include "Profiler.h"
#include "MemorySystem.h"
#include "Log.h"
#include "DataDef.h"
#include "MathDefs.h"
#include "Graphics.h"
#include "IdArray.h"
#include "Shader.h"

void Model::submit()
{
    const Mesh* mesh = m_mesh;
    const SubMesh* submeshes = (const SubMesh*)((char*)mesh + mesh->m_submesh_offset);
    bool bSkinning = mesh->m_num_joints > 0;
    float* t = bSkinning ? m_skinMatrix : m_transform;
    int num = bSkinning ? mesh->m_num_joints : 1;
    uint8_t viewId = m_viewId;

    uint32_t mat_num = m_numMaterials;
    Material** head = m_materials;

    for (uint32_t i=0; i<mat_num; ++i)
    {
        bgfx::setTransform(t, num);
        if (!submit_material(head[i]))
            continue;
        submit_submesh(submeshes + i);
        bgfx::submit(viewId, head[i]->m_shader->m_handle);
    }
}

void Model::submit_shadow()
{
    const Mesh* mesh = m_mesh;
    const SubMesh* submeshes = (const SubMesh*)((char*)mesh + mesh->m_submesh_offset);
    bool bSkinning = mesh->m_num_joints > 0;
    float* t = bSkinning ? m_skinMatrix : m_transform;
    int num = bSkinning ? mesh->m_num_joints : 1;

    uint32_t mat_num = m_numMaterials;
    Material** head = m_materials;

    for (uint32_t i=0; i<mat_num; ++i)
    {
        bgfx::setTransform(t, num);
        if (!submit_material_shadow(head[i]))
            continue;
        submit_submesh(submeshes + i);
        bgfx::submit(kShadowViewId, head[i]->m_shadow_shader->m_handle);
    }
}

void Model::update()
{
    if(!HAS_BITS(m_flag, kNodeTransformDirty))
        return;
    const Mesh* mesh = m_mesh;
    const Aabb& modelAabb = mesh->m_aabb;
    transform_aabb(m_aabb.m_min, m_aabb.m_max, m_transform, modelAabb.m_min, modelAabb.m_max);
    REMOVE_BITS(m_flag, kNodeTransformDirty);
}

float* Model::alloc_skinning_mat()
{
    return m_skinMatrix = FRAME_ALLOC(float, m_mesh->m_num_joints * 16);
}

bool Model::check_intersection(
    const float* rayOrig,
    const float* rayDir,
    float* intsPos,
    float* outNormal ) const
{
    if(!ray_aabb_intersection(rayOrig, rayDir, m_aabb.m_min, m_aabb.m_max))
        return false;

    const Mesh* mesh = m_mesh;
    if(!mesh) return false;

    const bgfx::VertexDecl& decl = mesh->m_decl;

    // Transform ray to local space
    float invm[16];
    bx::mtxInverse(invm, m_transform);
    float orig[3], dir[3], od[3], dir1[3];
    bx::vec3Add(od, orig, dir);
    bx::vec3MulMtx(orig, rayOrig, invm);
    bx::vec3MulMtx(dir1, od, invm);
    bx::vec3Sub(dir, dir1, orig);
    float nearestIntsPos[3] = { MAX_FLOAT, MAX_FLOAT, MAX_FLOAT };
    bool intersection = false;

    float tmpPos[3];
    float tmpNormal[3];
    float vert0[3], vert1[3], vert2[3];

    uint16_t posOffset = decl.getOffset(bgfx::Attrib::Position);
    uint32_t posSize = sizeof(float) * 3;

    uint32_t num = mesh->m_num_submeshes;

    for(uint32_t i=0; i<num; ++i)
    {
        const char* vertData = (const char*)get_mesh_vertex_data(mesh, i);
        uint32_t vertNum = get_mesh_vertex_num(mesh, i);
        const uint16_t* indexData = get_mesh_index_data(mesh, i);
        uint32_t indexNum = get_mesh_index_num(mesh, i);
        for( uint32_t j = 0; j < indexNum; j += 3 )
        {
            uint16_t index0 = indexData[j + 0];
            uint16_t index1 = indexData[j + 1];
            uint16_t index2 = indexData[j + 2];
            memcpy(vert0, vertData + decl.getStride() * index0 + posOffset, sizeof(vert0));
            memcpy(vert1, vertData + decl.getStride() * index1 + posOffset, sizeof(vert1));
            memcpy(vert2, vertData + decl.getStride() * index2 + posOffset, sizeof(vert2));
            if(ray_triangle_intersection( orig, dir, vert0, vert1, vert2, tmpPos))
            {
                intersection = true;
                float sub1[3], sub2[3];
                bx::vec3Sub(sub1, tmpPos, orig);
                bx::vec3Sub(sub2, tmpPos, orig);
                if(bx::vec3Length(sub1) < bx::vec3Length(sub2))
                {
                    bx::vec3Move(nearestIntsPos, tmpPos);
                    float plane[4];
                    bx::calcPlane(plane, vert0, vert1, vert2);
                    bx::vec3Move(tmpNormal, plane);
                }
            }
        }
    }

    bx::vec3MulMtx(intsPos, nearestIntsPos, m_transform);
    bx::vec3MulMtx(outNormal, tmpNormal, m_transform);
    return intersection;
}

ModelWorld g_modelWorld;
static IdArray<Model>      m_models;

void ModelWorld::init(int max_model)
{
    m_models.init(max_model, g_memoryMgr.get_allocator(kMemoryCategoryCommon));
    reset();
}

void ModelWorld::shutdown()
{
    m_models.destroy();
}

void ModelWorld::reset()
{
    m_modelsToDraw = 0;
    m_numModels = 0;
    m_shadowsToDraw = 0;
    m_numShadows = 0;
}

void ModelWorld::update(float dt)
{
    int num = m_models.size();
    Model* begin = m_models.begin();
    for(int i=0; i<num; ++i)
    {
        begin[i].update();
    }
}

void ModelWorld::submit_models()
{
    int num = m_numModels;
    Model** head = m_modelsToDraw;

    for(int i=0; i<num; ++i)
    {
        head[i]->submit();
    }
}

void ModelWorld::submit_shadows()
{
    uint32_t num = m_numShadows;
    Model** head = m_shadowsToDraw;

    for(uint32_t i=0; i<num; ++i)
    {
        head[i]->submit_shadow();
    }
}

void ModelWorld::cull_models(const Frustum& frust)
{
    int numModels = m_models.size();
    m_modelsToDraw = FRAME_ALLOC(Model*, numModels);

    Model* begin = m_models.begin();
    Model** head = m_modelsToDraw;
    int num = 0;

    for(int i=0; i<numModels; ++i)
    {
        Model* model = begin + i;
        if(model->m_flag & kNodeInvisible) continue;
        bool bVisible = !frust.cull_box(model->m_aabb.m_min, model->m_aabb.m_max);
        model->m_visibleThisFrame = bVisible;
        if(!bVisible) continue;
        head[num++] = model;
    }
    m_numModels = num;
}

void ModelWorld::cull_shadows(const Frustum& lightFrust)
{
    int numModels = m_models.size();
    m_shadowsToDraw = FRAME_ALLOC(Model*, numModels);

    Model* begin = m_models.begin();
    Model** head = m_shadowsToDraw;
    int num = 0;

    for(int i=0; i<numModels; ++i)
    {
        Model* model = begin + i;
        uint32_t flag = model->m_flag;
        if((flag & kNodeInvisible) || (flag & kNodeNoShadow))
            continue;
        bool bVisible = !lightFrust.cull_box(model->m_aabb.m_min, model->m_aabb.m_max);
        if(!bVisible) continue;
        head[num++] = model;
    }

    m_numShadows = num;
}

//-----------------------------------------------------------------
//
//-----------------------------------------------------------------
Id create_model(const void* data, ActorId32)
{
    Model* inst;
    Id modelId = m_models.create(&inst);
    //inst->init(modelResource);
    memcpy(inst, data, sizeof(Model));
    return modelId;
}

void destroy_model(Id id)
{
    if(!m_models.has(id)) return;
    m_models.destroy(id);
}

void* get_model(Id id)
{
    if(!m_models.has(id)) return 0;
    return m_models.get(id);
}

int num_all_model()
{
    return m_models.size();
}

void* get_all_model()
{
    return m_models.begin();
}

void transform_model(Id id, const hkQsTransform& t)
{
    if(!m_models.has(id))
        return;
    Model* model = m_models.get(id);
#ifdef HAVOK_COMPILE
    t.get4x4ColumnMajor(model->m_transform);
    ADD_BITS(model->m_flag, kNodeTransformDirty);
#endif
}

void lookup_model_instance_data( void * resource )
{
    Model* model = (Model*)resource;
    model->m_mesh = FIND_RESOURCE(Mesh, EngineTypes::MESH, model->m_meshName);
    uint32_t num = model->m_numMaterials;
    Material** head = model->m_materials;
    StringId* matNames = model->m_materialNames;
    for(uint32_t i=0; i<num;++i)
    {
        head[i] = FIND_RESOURCE(Material, EngineTypes::MATERIAL, matNames[i]);
    }
}


#ifndef _RETAIL
//-----------------------------------------------------------------
//
//-----------------------------------------------------------------
#include "DebugDraw.h"
void debug_draw_models()
{
    PROFILE(debug_draw_models);
    int num = m_models.size();
    Model* models = m_models.begin();
    hkVector4 min, max;
    for (int i=0; i<num; ++i)
    {
        const Aabb& aabb = models[i].m_aabb;
        transform_vec3(min, aabb.m_min);
        transform_vec3(max, aabb.m_max);
        g_debugDrawMgr.add_aabb(min, max, RGBA(0,255,0,255), true);
    }
}
//-----------------------------------------------------------------
//
//-----------------------------------------------------------------
#endif