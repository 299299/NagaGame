#pragma once
#include "BaseCompiler.h"
#include "Level.h"
#include <bx/tinystl/allocator.h>
#include <bx/tinystl/unordered_map.h>


typedef tinystl::unordered_map<uint32_t, int> ResourceKeyMap;

class LevelCompiler : public BaseCompiler
{
public:
    LevelCompiler();
    ~LevelCompiler();

    virtual std::string getFormatExt() const { return Level::get_name(); };
    virtual bool parseWithJson() const { return true; };
    virtual bool readJSON(const JsonValue& root);
    bool isResourceInLevel(const std::string& resourceName) const;
    virtual bool checkInLevel() const { return false; };

    ResourceKeyMap           m_resourceKeys;
};
