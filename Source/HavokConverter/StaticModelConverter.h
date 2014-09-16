#pragma once
#include "ActorConverter.h"

class StaticModelConverter : public ActorConverter
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(HK_MEMORY_CLASS_USER);
    StaticModelConverter();
    ~StaticModelConverter();
    virtual void process(void* pData);
    void process(hkxNode* node);

private:
    void process(hkxScene* scene);
private:
    hkxScene*                                   m_scene;
};
