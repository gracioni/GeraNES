#include "EmscriptenUtil.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

void emcriptenFileDialog(intptr_t handler) {

    EM_ASM({

        function openFileDialog() {

            var handler = Number($0);

            var input = document.getElementById('__geranes_open_rom_input');
            if (!input) {
                input = document.createElement('input');
                input.id = '__geranes_open_rom_input';
                input.type = 'file';
                input.accept = '.nes,.fds,.nsf,.zip,.7z';
                input.style.display = 'none';
                document.body.appendChild(input);
            }

            input.addEventListener('change', function () {
                if (!input.files || input.files.length === 0) return;

                var file = input.files[0];
                var reader = new FileReader();

                reader.onload = function(e) {
                    var fileContent = new Uint8Array(e.target.result);
                    var fileName = file.name;
                    var fileSize = fileContent.length;

                    var moduleObj = (typeof Module !== 'undefined') ? Module : null;
                    var mallocFn = (typeof _malloc === 'function') ? _malloc : (moduleObj && typeof moduleObj._malloc === 'function' ? moduleObj._malloc.bind(moduleObj) : null);
                    var freeFn = (typeof _free === 'function') ? _free : (moduleObj && typeof moduleObj._free === 'function' ? moduleObj._free.bind(moduleObj) : null);
                    var heapU8 = (typeof HEAPU8 !== 'undefined' && HEAPU8) ? HEAPU8 : (moduleObj ? moduleObj.HEAPU8 : null);
                    var ccallFn = (typeof ccall === 'function') ? ccall : (moduleObj && typeof moduleObj.ccall === 'function' ? moduleObj.ccall.bind(moduleObj) : null);

                    if (!mallocFn || !freeFn || !heapU8 || !ccallFn) {
                        console.error('Emscripten runtime symbols are unavailable for file upload');
                        return;
                    }

                    var buffer = mallocFn(fileSize);
                    heapU8.set(fileContent, buffer);
                    ccallFn('processUploadedFile', null, ['number', 'string', 'number', 'number'], [handler, fileName, fileSize, buffer]);
                    freeFn(buffer);
                };

                reader.onerror = function(err) {
                    console.error('Failed to read selected file:', err);
                };

                reader.readAsArrayBuffer(file);
            }, { once: true });

            input.value = "";
            try {
                if (typeof input.showPicker === 'function') input.showPicker();
                else input.click();
            } catch (e) {
                console.error('openFileDialog failed to show picker, falling back to click:', e);
                try { input.click(); } catch (_) {}
            }
        }
        openFileDialog();

    }, handler);
}

void emcriptenRegisterAudioReset(intptr_t handler)
{
    EM_ASM({
        var handler = Number($0);
        function resolveCcall() {
            if (typeof ccall === 'function') return ccall;
            if (typeof Module !== 'undefined' && Module && typeof Module.ccall === 'function') return Module.ccall.bind(Module);
            return null;
        }
        function restartAudioIfNeeded() {
            var ccallFn = resolveCcall();
            if (!ccallFn) return;
            try {
                ccallFn(
                    'restartAudioModule',
                    null,
                    ['number'],
                    [handler]
                );
                console.log("Audio restarted");
            } catch (e) {
                console.error("Failed to call restartAudioModule:", e);
            }
        }

        // Register only once
        if (!window.__geranes_audio_reset_registered) {
            window.__geranes_audio_reset_registered = true;

            document.addEventListener('visibilitychange', function () {
                if (document.visibilityState === 'visible') {
                    restartAudioIfNeeded();
                }
            });

            window.addEventListener('focus', function () {
                restartAudioIfNeeded();
            });

            console.log("Audio auto-reset listeners installed for handler:", handler);
        }

    }, handler);
}

void emcriptenImportSession(intptr_t handler) {

    EM_ASM({
        (async () => {

            const handler = Number($0);
            const ccallFn = (typeof ccall === 'function') ? ccall :
                ((typeof Module !== 'undefined' && Module && typeof Module.ccall === 'function') ? Module.ccall.bind(Module) : null);

            try {
                if (typeof importEntireFSFromZip !== 'function') {
                    console.error("importEntireFSFromZip not found");
                    return;
                }

                await importEntireFSFromZip();

                if (!ccallFn) {
                    console.error("ccall not available for onSessionImportComplete");
                    return;
                }

                ccallFn('onSessionImportComplete', null, ['number'], [handler]);

            } catch (err) {
                console.error("Import failed:", err);
            }

        })();
    }, handler);
}

void emcriptenExportSession() {
    EM_ASM({
        try {
            if (typeof exportEntireFS === 'function') {
                exportEntireFS();
            } else {
                console.error('exportEntireFS not found on window/Module.');
            }
        } catch (e) {
            console.error('emcriptenExportSession error:', e);
        }
    });
}

#endif
