#pragma once

#include "GeraNESApp/IEmulationHost.h"

#if defined(__EMSCRIPTEN__) && defined(GERANES_WEB_PTHREADS)
#include "GeraNESApp/ThreadedEmulationHost.h"
using EmulationHost = ThreadedEmulationHost;
static_assert(GERANES_WEB_PTHREADS_INITIAL_MEMORY_MB >= 1, "GERANES_WEB_PTHREADS_INITIAL_MEMORY_MB must be positive");
#elif defined(__EMSCRIPTEN__)
#include "GeraNESApp/SingleThreadEmulationHost.h"
using EmulationHost = SingleThreadEmulationHost;
#else
#include "GeraNESApp/ThreadedEmulationHost.h"
using EmulationHost = ThreadedEmulationHost;
#endif
