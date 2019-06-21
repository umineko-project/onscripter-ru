/**
 *  Resources.hpp
 *  ONScripter-RU
 *
 *  A system for storing files within the executable.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <cstdint>
#include <cstddef>

struct InternalResource {
    const char *filename;
    const uint8_t *buffer;
    size_t size;
	InternalResource *glesVariant; /* Used for shaders */
};

extern const InternalResource *getResource(const char *filename, bool mobile=false);
extern const InternalResource *getResourceList();
