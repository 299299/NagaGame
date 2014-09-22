#include "Script.h"
#include "Log.h"
#include "id_array.h"
#include "config.h"
#include "EngineAssert.h"
#include <Windows.h>
#include <gamemonkey/gmThread.h>
#include <gamemonkey/gmStreamBuffer.h>

#define GC_TIME (2.0f)

static IdArray<MAX_SCRIPT_OBJECT, ScriptInstance> g_scriptObjects;

void* load_resource_script(const char* data, uint32_t size)
{
    ScriptResource* script = (ScriptResource*)data;
    script->m_code = (char*)data + sizeof(ScriptResource);
    return script;
}

void bringin_resource_script(void* resource)
{
    ScriptResource* script = (ScriptResource*)resource;
    gmStreamBufferStatic stream(script->m_code, script->m_codeSize);
    script->m_rootFunction = g_script.m_vm->BindLibToFunction(stream);
    if(!script->m_rootFunction) g_script.print_error();
}

void bringout_resource_script(void* resource)
{
    ScriptResource* script = (ScriptResource*)resource;
    //script->m_rootFunction->Destruct(g_script.m_vm);
}


void ScriptInstance::init( const void* resource )
{
    m_threadId = -1;
    m_resource = (const ScriptResource*)resource;
    bool bOK = g_script.m_vm->ExecuteFunction(m_resource->m_rootFunction, &m_threadId, true);
    if(!bOK) g_script.print_error();
}

void ScriptInstance::destroy()
{
    if(m_threadId < 0) return;
    g_script.m_vm->KillThread(m_threadId);
    m_threadId = -1;
}

ScriptSystem g_script;
static bool GM_CDECL machine_callback(gmMachine * a_machine, gmMachineCommand a_command, const void * a_context)
{
    if(a_command == MC_THREAD_EXCEPTION)
    {
        LOGE("[GM] Execption!");
    }
    return true;
}
static void GM_CDECL print_callback(gmMachine * a_machine, const char * a_string)
{
    LOGI("[GM] print %s.", a_string);
    OutputDebugString(a_string);
    OutputDebugString("\n");
}

void ScriptSystem::init()
{
    memset(this, 0x00, sizeof(ScriptSystem));
    m_vm = new gmMachine();
    m_vm->SetDebugMode(true);
    gmMachine::s_machineCallback = machine_callback;
    gmMachine::s_printCallback = print_callback;
    extern void register_script_api(gmMachine* machine);
    register_script_api(m_vm);
    print_error();
}

void ScriptSystem::quit()
{
    delete m_vm;
}

void ScriptSystem::ready()
{
    m_coreTable = m_vm->GetGlobals()->Get(m_vm, "g_core").GetTableObjectSafe();
    m_update_func = m_coreTable->Get(m_vm, "update").GetFunctionObjectSafe();
    m_pre_step_func = m_coreTable->Get(m_vm, "pre_step").GetFunctionObjectSafe();
    m_post_step_func = m_coreTable->Get(m_vm, "post_step").GetFunctionObjectSafe();
    m_render_func = m_coreTable->Get(m_vm, "render").GetFunctionObjectSafe();
    print_error();
}

void ScriptSystem::print_error()
{
    bool firstErr = true;

    gmLog & compileLog = m_vm->GetLog();
    const char *msg = compileLog.GetEntry(firstErr);

    if ( msg )
    {
        LOGE("#############################\n[GameMonkey Run-time Error]:");
    }

    while(msg)	
    {
        LOGE(msg);
        msg = compileLog.GetEntry(firstErr);
    }

    compileLog.Reset();
}

void ScriptSystem::call_function(const char* a_obj_name, const char * a_func_name, gmVariable *a_param)
{
    gmTableObject* table = m_vm->GetGlobals()->Get(m_vm, a_obj_name).GetTableObjectSafe();
    if(table) {
        LOGE("not find table %s", a_obj_name);
        return;
    }
    gmCall call;
    call.BeginTableFunction(m_vm, a_func_name, table, gmVariable(table));
    if ( a_param ) call.AddParam( *a_param );
    call.End();
    print_error();
}

void ScriptSystem::call_global_function(const char* a_func_name, gmVariable* a_param)
{
    gmCall call;
    call.BeginGlobalFunction(m_vm, a_func_name);
    if(a_param) call.AddParam(*a_param);
    call.End();
    print_error();
}

#define CALL_STEP_FUNC(func_obj)\
        if(!func_obj) return; \
        call.BeginFunction(m_vm, func_obj, gmVariable(m_coreTable));\
        gmVariable dtParam(dt);\
        call.AddParam(dtParam);\
        call.End();\
        print_error();

void ScriptSystem::pre_step(float dt)
{
    CALL_STEP_FUNC(m_pre_step_func);
}

void ScriptSystem::update(float dt)
{
    CALL_STEP_FUNC(m_update_func);
    int nThreadCount = m_vm->Execute(16);
    m_time += dt;
    if(m_time > GC_TIME)
    {
        m_time -= GC_TIME;
        m_vm->CollectGarbage();
    }
    print_error();
}

void ScriptSystem::post_step(float dt)
{
    CALL_STEP_FUNC(m_post_step_func);
}

void ScriptSystem::render()
{
    CALL_STEP_FUNC(m_render_func);
}

//-----------------------------------------------------------------------
Id create_script_object(const void* resource)
{
    ScriptInstance inst;
    inst.init(resource);
    return id_array::create(g_scriptObjects, inst);
}
void destroy_script_object(Id id)
{
    if(!id_array::has(g_scriptObjects, id)) return;
    id_array::destroy(g_scriptObjects, id);
}
void* get_script_object(Id id)
{
    if(!id_array::has(g_scriptObjects, id)) return 0;
    return &id_array::get(g_scriptObjects, id);
}
uint32_t num_script_objects()
{
    return id_array::size(g_scriptObjects);
}
void* get_script_objects()
{
    return id_array::begin(g_scriptObjects);
}
//-----------------------------------------------------------------------