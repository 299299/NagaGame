#pragma once
#include "EntityConverter.h"

class ModelConverter;
class LightConverter;

class LevelConverter : public EntityConverter
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(HK_MEMORY_CLASS_USER);
    LevelConverter();
    ~LevelConverter();
    virtual void process(void* pData);
    virtual jsonxx::Object serializeToJson() const;
    virtual jsonxx::Object serializeToJsonSplit() const;
private:
    void process(hkxScene* scene);
private:
    hkxScene*                                   m_scene;
    std::vector<hkxNode*>                       m_sceneNodes;
    std::vector<hkxNode*>                       m_meshNodes;
    std::vector<hkxNode*>                       m_lightNodes;
    std::vector<ModelConverter*>                m_models;
    std::vector<LightConverter*>                m_lights;
};
