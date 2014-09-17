#include "Shader.h"
#include "Resource.h"
#include "Log.h"

void Shader::bringIn()
{
    if(bgfx::isValid(m_handle)) return;
    m_handle = bgfx::createShader(bgfx::makeRef(m_blob, m_size));
    if(!bgfx::isValid(m_handle)) LOGE("Shader bringin error!");
}

void Shader::bringOut()
{
    if(!bgfx::isValid(m_handle)) return;
    bgfx::destroyShader(m_handle);
    m_handle.idx = bgfx::invalidHandle;
}

void ShaderProgram::bringIn()
{
    if(bgfx::isValid(m_handle)) return;
    if(!m_vs || !m_ps) return;
    m_handle = bgfx::createProgram(m_vs->m_handle, m_ps->m_handle, false);
    if(!bgfx::isValid(m_handle)) LOGE("ShaderProgram bringin error!");
}

void ShaderProgram::bringOut()
{
    if(!bgfx::isValid(m_handle))
        return;
    bgfx::destroyProgram(m_handle);
    m_handle.idx = bgfx::invalidHandle;
}

void ShaderProgram::lookup()
{
    m_vs = FIND_RESOURCE(Shader, m_vsName);
    m_ps = FIND_RESOURCE(Shader, m_psName);
    if(!m_vs) LOGE("vs[%s] lookup error", stringid_lookup(m_vsName));
    if(!m_ps) LOGE("ps[%s] lookup error", stringid_lookup(m_psName));
}

void* load_resource_shader(const char* data, uint32_t size)
{
    Shader* shader = (Shader*)data;
    shader->m_blob = (char*)(data + sizeof(Shader));
    shader->m_handle.idx = bgfx::invalidHandle;
    return shader;
}

void bringin_resource_shader(void* resource)
{
    Shader* shader = (Shader*)resource;
    shader->bringIn();
}

void bringout_resource_shader(void* resource)
{
    Shader* shader = (Shader*)resource;
    shader->bringOut();
}

void lookup_resource_shader_program(void* resource)
{
    ShaderProgram* program = (ShaderProgram*)resource;
    program->lookup();
}

void bringin_resource_shader_program(void* resource)
{
    ShaderProgram* program = (ShaderProgram*)resource;
    program->bringIn();
}

void bringout_resource_shader_program(void* resource)
{
    ShaderProgram* program = (ShaderProgram*)resource;
    program->bringOut();
}

ShaderProgram* findShader( const char* name )
{
    char buf[256];
    sprintf_s(buf, PROGRAM_PATH"%s", name);
    return FIND_RESOURCE(ShaderProgram, StringId(buf));
}