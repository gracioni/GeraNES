#pragma once

#include <cstddef>
#include <cstdint>

void emcriptenFileDialog(intptr_t handler);
void emcriptenReplayFileDialog(intptr_t handler);

void emcriptenRegisterVisibilityHandler(intptr_t handler);

void emcriptenRegisterUnloadHandler(intptr_t handler);

void emcriptenImportSession(intptr_t handler);

void emcriptenExportSession();
void emcriptenDownloadBinaryFile(const uint8_t* data, size_t size, const char* fileName, const char* mimeType);

void emcriptenSyncImGuiTextInput(bool wantTextInput);

void emcriptenInstallImGuiClipboardBackend();
void emcriptenSyncImGuiClipboardSelection();
void emcriptenCopyTextToClipboardExact(const char* text);
