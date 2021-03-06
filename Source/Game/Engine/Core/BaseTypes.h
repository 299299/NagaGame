#pragma once
#include <stdint.h>
#include <stddef.h>
#include <bx/macros.h>

#define SAFE_DELETE(ptr)                            delete (ptr); (ptr) = 0;
#define SAFE_DELETE_ARRAY(ptr)                      delete[] (ptr); (ptr) = 0;
#define SAFE_REMOVEREF(ptr)                         if(ptr) { (ptr)->removeReference(); (ptr) = 0;}
//no template
#define SAFE_DESTRUCTOR(ptr, class_name)            if(ptr) { (ptr)->~class_name(); (ptr) = 0;}

#define INVALID_U8          (0xFF)
#define INVALID_U16         (0xFFFF)
#define INVALID_U32         (0xFFFFFFFF)

#define LOAD_OBJECT(container, objectClassName)\
                 (objectClassName*)container->findObjectByType( objectClassName##Class.getName() );


#define RGBA(r,g,b,a) (((unsigned)r) | ((unsigned)g << 8) | ((unsigned)b << 16) | ((unsigned)a << 24))
#define RGBCOLOR(r,g,b) RGBA(r,g,b,255)
#define RGBA_TO_FLOAT(rgba, color)\
    color[0] = (rgba & 0x000000FF) / 255.0f;\
    color[1] = ((rgba & 0x0000FF00) >> 8) / 255.0f;\
    color[2] = ((rgba & 0x00FF0000) >> 16) / 255.0f;\
    color[3] = ((rgba & 0xFF000000) >> 24) / 255.0f;

#define ADD_BITS(value, bit)                (value |= bit)
#define REMOVE_BITS(value, bit)             (value &= ~bit)
#define HAS_BITS(value, bit)                (value & bit)
#define NOT_HAS_BITS(value, bit)            (value & bit == 0)

#define ENGINE_NATIVE_ALIGN BX_ALIGN_DECL_16

#ifndef _RETAIL
#define RESOURCE_RELOAD
#endif

typedef uint32_t StringId32;
typedef uint64_t StringId64;
typedef uint32_t ActorId32;

#define INVALID_ID 65535

struct Id
{
    uint16_t id;
    uint16_t index;

    void decode(uint32_t id_and_index)
    {
        id = (id_and_index & 0xFFFF0000) >> 16;
        index = id_and_index & 0xFFFF;
    }

    uint32_t encode() const
    {
        return (uint32_t(id) << 16) | uint32_t(index);
    }

    bool operator==(const Id& other)
    {
        return id == other.id && index == other.index;
    }

    bool operator!=(const Id& other)
    {
        return id != other.id || index != other.index;
    }

    bool is_valid() const
    {
        return id != INVALID_ID;
    }
};


#define SIZE_MB(size)  1024*1024*size
#define SIZE_KB(size)  1024*size

/// Note that ALIGNMENT must be a power of two for this to work.
/// Note: to use this macro you must cast your pointer to a byte pointer or to an integer value.
#define NEXT_MULTIPLE_OF(ALIGNMENT, VALUE)  ( ((VALUE) + ((ALIGNMENT)-1)) & (~((ALIGNMENT)+(VALUE)*0-1)) )

#define FIND_IN_ARRAY(_array, _len, _find, _index)\
        for(int i=0; i<_len; ++i)\
        {\
            if(_array[i] == _find)\
            {\
                _index = i;\
                break;\
            } \
       }

#define FIND_IN_ARRAY_RET(_array, _len, _find)\
        for(int i=0; i<(int)_len; ++i)\
        {\
            if(_array[i] == _find)\
            {\
                return i;\
            } \
        }\
        return -1;


#define NATIVE_ALIGN_VALUE  16
#define NATIVE_ALGIN_SIZE(size) NEXT_MULTIPLE_OF(NATIVE_ALIGN_VALUE, (size))