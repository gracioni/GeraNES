#pragma once

#include <cstdint>

void emcriptenFileDialog(intptr_t handler);

void emcriptenRegisterVisibilityHandler(intptr_t handler);

void emcriptenRegisterUnloadHandler(intptr_t handler);

void emcriptenImportSession(intptr_t handler);

void emcriptenExportSession();

void emcriptenSyncImGuiTextInput(bool wantTextInput);
