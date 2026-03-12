#pragma once

#include <cstdint>

void emcriptenFileDialog(intptr_t handler);

void emcriptenRegisterAudioReset(intptr_t handler);

void emcriptenImportSession(intptr_t handler);

void emcriptenExportSession();
