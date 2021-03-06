#pragma once
#include "BaseTypes.h"


/// Base class for memory allocators.
///
/// @ingroup Memory
class Allocator
{
public:

    Allocator() {}
    virtual ~Allocator() {}

    /// Allocates @a size bytes of memory aligned to the specified
    /// @a align byte and returns a pointer to the first allocated byte.
    virtual void* allocate(size_t size, size_t align = DEFAULT_ALIGN) = 0;

    /// Deallocates a previously allocated block of memory pointed by @a data.
    virtual void deallocate(void* data) = 0;

    /// Returns the total number of bytes allocated.
    virtual size_t allocated_size() = 0;

    /// Default memory alignment in bytes.
    static const size_t DEFAULT_ALIGN = 16;

private:

    // Disable copying
    Allocator(const Allocator&);
    Allocator& operator=(const Allocator&);
};
