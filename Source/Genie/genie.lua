

function print_table(tab, name)
    print("print table "..name)
    for i,v in pairs(tab) do
        if type(v) == "table" then
        print("table",i,"{")
        printTab(v)
        print("}")
        else
            print(i.."="..v)
        end
    end
end


solution "game"
    configurations {
        "Debug",
        "Release",
    }

    platforms {
        "x32",
        "x64",
    }

    language "C++"
    startproject "game"

NAGA_SRC_DIR = (path.getabsolute("..") .. "/")
local NAGA_BUILD_DIR = (NAGA_SRC_DIR .. ".build/")

print_table(_ARGS, "_ARGS")
print_table(_OPTIONS, "_OPTIONS")

OS = _OPTIONS["os"]
if OS == nil then
    print("os must be specified!")
    return
end

if OS == "linux" then
BX_DIR = ("/home/lixin/work/bx/")
BGFX_DIR = ("/home/lixin/work/bgfx/")
else
BX_DIR = ("C:/Project/bx/")
BGFX_DIR = ("C:/Project/bgfx/")
end

HAVOK_DIR = ("C:/Project/hk2014_1_0_r1/Source/")
GAME_SRC_DIR = (NAGA_SRC_DIR .. "Game/")
TARGET_DIR = (NAGA_SRC_DIR .. "../Application")

defines {
    "HK_PLATFORM_SIM",
    "HK_CONFIG_SIMD=1"
}

dofile ("toolchain.lua")
if not toolchain(NAGA_BUILD_DIR, "", BX_DIR, TARGET_DIR) then
    return -- no action specified
end

function exeProject(_name, _incdirs, _files)

    project (_name)
        uuid (os.uuid(_name))
        kind "WindowedApp"

    configuration {}

    includedirs {
        _incdirs
    }
    files {
        _files
    }

    configuration { "vs*" }
        linkoptions {
            "/ignore:4199", -- LNK4199: /DELAYLOAD:*.dll ignored; no imports found from *.dll
        }
        links { -- this is needed only for testing with GLES2/3 on Windows with VS2008
            "DelayImp",
        }
        defines {
            "HAVOK_COMPILE"
        }
        removeflags {
            "StaticRuntime"
        }

    configuration { "vs201*" }
        linkoptions { -- this is needed only for testing with GLES2/3 on Windows with VS201x
            "/DELAYLOAD:\"libEGL.dll\"",
            "/DELAYLOAD:\"libGLESv2.dll\"",
        }

    configuration { "mingw*" }
        targetextension ".exe"

    configuration { "vs20* or mingw*" }
        links {
            "gdi32",
            "psapi",
        }

    configuration { "linux-*" }
        links {
            "X11",
            "GL",
            "pthread",
        }

    configuration {"Debug"}
        defines {
            "_DEBUG",
            "BGFX_CONFIG_DEBUG"
        }

    configuration {"Release"}
        defines {
            "NDEBUG",
        }


    configuration {}

    if(_ACTION == "vs2010") then
        print("vs2010 force toolset to v100")
        premake.vstudio.toolset = "v100"
    end
end

game_inc_dirs = {
    BX_DIR .. "include",
    BGFX_DIR .. "include",
    BGFX_DIR .. "src",
    BGFX_DIR .. "examples/common",
    BGFX_DIR .. "3rdparty",
    HAVOK_DIR,
    GAME_SRC_DIR .. "**"
}

bgfx_files = {
    BGFX_DIR .. "src/bgfx.cpp",
    BGFX_DIR .. "src/image.cpp",
    BGFX_DIR .. "src/ovr.cpp",
    BGFX_DIR .. "src/renderdoc.cpp",
    BGFX_DIR .. "src/renderer_d3d11.cpp",
    BGFX_DIR .. "src/renderer_null.cpp",
    BGFX_DIR .. "src/vertexdecl.cpp",
    BGFX_DIR .. "examples/common/imgui/imgui.cpp",
    BGFX_DIR .. "examples/common/nanovg/nanovg.cpp",
    BGFX_DIR .. "examples/common/nanovg/nanovg_bgfx.cpp",
}

game_files = {
    GAME_SRC_DIR .. "Core/*",
    GAME_SRC_DIR .. "Animation/*",
    GAME_SRC_DIR .. "Engine/*",
    GAME_SRC_DIR .. "Game/*",
    GAME_SRC_DIR .. "Graphics/*",
    GAME_SRC_DIR .. "Physics/*",
    bgfx_files,
    GAME_SRC_DIR .. "linkopt.cpp",
}
exeProject("game", game_inc_dirs, game_files)


