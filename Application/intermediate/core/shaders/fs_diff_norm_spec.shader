$input v_wpos, v_normal, v_tangent, v_texcoord0, v_view, v_shadowcoord, v_binormal
//[def=intermediate/core/shaders/ubershader.def]
#define DIFFUSE_MAPPING
#define NORMAL_MAPPING
#define SPECULAR_MAPPING
#define SHADOW
#include "fs_ubershader.sh"