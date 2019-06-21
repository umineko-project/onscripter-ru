/**
 *  DroidProfile.hpp
 *  ONScripter-RU
 *
 *  Implements basic droid profiler.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <cstddef>

void profileStart(size_t res);
void profileStop();
