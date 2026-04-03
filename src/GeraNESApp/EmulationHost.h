#pragma once

#include "GeraNESApp/IEmulationHost.h"

#ifdef __EMSCRIPTEN__
#include "GeraNESApp/SingleThreadEmulationHost.h"
using EmulationHost = SingleThreadEmulationHost;
#else
#include "GeraNESApp/ThreadedEmulationHost.h"
using EmulationHost = ThreadedEmulationHost;
#endif
