#include "EmscriptenUtil.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

EM_JS(void, emcriptenSyncImGuiTextInputJs, (int wantTextInput), {
    try {
        const want = !!wantTextInput;

        function resolveCcall() {
            if (typeof ccall === 'function') return ccall;
            if (typeof Module !== 'undefined' && Module && typeof Module.ccall === 'function') return Module.ccall.bind(Module);
            return null;
        }

        function ensureBridge() {
            if (window.__geranes_imgui_text_input_bridge) {
                return window.__geranes_imgui_text_input_bridge;
            }

            const bridge = {};
            bridge.wantTextInput = false;
            bridge.lastUserActivation = 0;
            bridge.input = null;
            bridge.lastPointerX = 0;
            bridge.lastPointerY = 0;

            const input = document.createElement('input');
            input.id = '__geranes_imgui_text_input';
            input.type = 'text';
            input.autocomplete = 'off';
            input.autocapitalize = 'off';
            input.autocorrect = 'off';
            input.spellcheck = false;
            input.style.position = 'fixed';
            input.style.left = '0px';
            input.style.top = '0px';
            input.style.width = '16px';
            input.style.height = '16px';
            input.style.opacity = '0';
            input.style.border = '0';
            input.style.padding = '0';
            input.style.margin = '0';
            input.style.pointerEvents = 'none';
            input.style.background = 'transparent';
            input.style.color = 'transparent';
            input.style.zIndex = '1';

            bridge.input = input;
            document.body.appendChild(input);

            function callNative(name, types, values) {
                const ccallFn = resolveCcall();
                if (!ccallFn) return;

                try {
                    ccallFn(name, null, types, values);
                } catch (e) {
                    console.error(name + ' failed:', e);
                }
            }

            function clearInputValue() {
                input.value = "";
            }

            function getVirtualKeyboard() {
                return (typeof navigator !== 'undefined' && navigator.virtualKeyboard) ? navigator.virtualKeyboard : null;
            }

            function updateInputPosition(x, y) {
                const px = isFinite(x) ? x : 0;
                const py = isFinite(y) ? y : 0;
                input.style.left = Math.max(0, Math.round(px - 8)) + 'px';
                input.style.top = Math.max(0, Math.round(py - 8)) + 'px';
            }
            bridge.updateInputPosition = updateInputPosition;

            function showVirtualKeyboard() {
                const vk = getVirtualKeyboard();
                if (!vk || typeof vk.show !== 'function') {
                    return;
                }

                try {
                    vk.show();
                } catch (_) {}
            }
            bridge.showVirtualKeyboard = showVirtualKeyboard;

            function hideVirtualKeyboard() {
                const vk = getVirtualKeyboard();
                if (!vk || typeof vk.hide !== 'function') {
                    return;
                }

                try {
                    vk.hide();
                } catch (_) {}
            }
            bridge.hideVirtualKeyboard = hideVirtualKeyboard;

            function focusInput() {
                try {
                    input.focus({ preventScroll: true });
                } catch (_) {
                    try { input.focus(); } catch (_) {}
                }

                try {
                    input.setSelectionRange(input.value.length, input.value.length);
                } catch (_) {}

                showVirtualKeyboard();
            }

            function sendPendingText(text) {
                if (!text) return;
                callNative('injectWebTextUtf8', ['string'], [text]);
            }

            function rememberUserActivation(event) {
                bridge.lastUserActivation = Date.now();
                if (event) {
                    if (event.touches && event.touches.length > 0) {
                        bridge.lastPointerX = event.touches[0].clientX;
                        bridge.lastPointerY = event.touches[0].clientY;
                    } else if (event.changedTouches && event.changedTouches.length > 0) {
                        bridge.lastPointerX = event.changedTouches[0].clientX;
                        bridge.lastPointerY = event.changedTouches[0].clientY;
                    } else if (typeof event.clientX === 'number' && typeof event.clientY === 'number') {
                        bridge.lastPointerX = event.clientX;
                        bridge.lastPointerY = event.clientY;
                    }
                    updateInputPosition(bridge.lastPointerX, bridge.lastPointerY);
                }
                if (bridge.wantTextInput) {
                    focusInput();
                }
            }

            function isManagedKey(key) {
                return key === 'Backspace' ||
                    key === 'Tab' ||
                    key === 'Enter' ||
                    key === 'Escape' ||
                    key === 'Delete' ||
                    key === 'Insert' ||
                    key === 'Home' ||
                    key === 'End' ||
                    key === 'PageUp' ||
                    key === 'PageDown' ||
                    key === 'ArrowLeft' ||
                    key === 'ArrowRight' ||
                    key === 'ArrowUp' ||
                    key === 'ArrowDown';
            }

            input.addEventListener('keydown', function(event) {
                callNative(
                    'injectWebKeyEvent',
                    ['string', 'number', 'number', 'number', 'number', 'number'],
                    [event.key || "", 1, event.ctrlKey ? 1 : 0, event.shiftKey ? 1 : 0, event.altKey ? 1 : 0, event.metaKey ? 1 : 0]
                );

                if (isManagedKey(event.key)) {
                    event.preventDefault();
                }
            });

            input.addEventListener('keyup', function(event) {
                callNative(
                    'injectWebKeyEvent',
                    ['string', 'number', 'number', 'number', 'number', 'number'],
                    [event.key || "", 0, event.ctrlKey ? 1 : 0, event.shiftKey ? 1 : 0, event.altKey ? 1 : 0, event.metaKey ? 1 : 0]
                );

                if (isManagedKey(event.key)) {
                    event.preventDefault();
                }
            });

            input.addEventListener('input', function() {
                if (input.value.length > 0) {
                    sendPendingText(input.value);
                    clearInputValue();
                }
            });

            input.addEventListener('compositionend', function(event) {
                sendPendingText((event && event.data) ? event.data : input.value);
                clearInputValue();
            });

            input.addEventListener('blur', function() {
                if (bridge.wantTextInput) {
                    setTimeout(function() {
                        if (bridge.wantTextInput && document.activeElement !== input) {
                            focusInput();
                        }
                    }, 0);
                }
            });

            const canvas = (typeof Module !== 'undefined' && Module && Module.canvas) ? Module.canvas : document.getElementById('canvas');
            if (canvas) {
                ['pointerdown', 'pointerup', 'touchstart', 'touchend', 'mousedown', 'mouseup', 'click'].forEach(function(eventName) {
                    canvas.addEventListener(eventName, rememberUserActivation, true);
                });
            }

            updateInputPosition(bridge.lastPointerX, bridge.lastPointerY);
            clearInputValue();
            window.__geranes_imgui_text_input_bridge = bridge;
            return bridge;
        }

        const bridge = window.__geranes_imgui_text_input_bridge;
        if (!want && !bridge) {
            return;
        }

        const activeBridge = bridge || ensureBridge();
        activeBridge.wantTextInput = want;

        if (want) {
            const now = Date.now();
            const recentlyActivated = (now - activeBridge.lastUserActivation) < 1500;
            activeBridge.input.setAttribute('inputmode', 'text');
            activeBridge.updateInputPosition(activeBridge.lastPointerX, activeBridge.lastPointerY);
            if (recentlyActivated || document.activeElement !== activeBridge.input) {
                try {
                    activeBridge.input.focus({ preventScroll: true });
                } catch (_) {
                    try { activeBridge.input.focus(); } catch (_) {}
                }
            }
            activeBridge.showVirtualKeyboard();
            return;
        }

        if (document.activeElement === activeBridge.input) {
            activeBridge.input.blur();
        }

        activeBridge.input.value = "";
        activeBridge.input.setAttribute('inputmode', 'none');
        activeBridge.hideVirtualKeyboard();

        const canvas = (typeof Module !== 'undefined' && Module && Module.canvas) ? Module.canvas : document.getElementById('canvas');
        if (canvas) {
            try { canvas.focus(); } catch (_) {}
        }
    } catch (e) {
        console.error('emcriptenSyncImGuiTextInputJs failed:', e);
    }
});

void emcriptenFileDialog(intptr_t handler) {
    constexpr const char* kAcceptedExtensions =
#ifdef ENABLE_NSF_PLAYER
        ".nes,.fds,.nsf,.zip,.7z";
#else
        ".nes,.fds,.zip,.7z";
#endif

    EM_ASM({
        var handler = Number(arguments[0]);
        var accepted = UTF8ToString(arguments[1]);

        function openFileDialog() {

            var input = document.getElementById('__geranes_open_rom_input');
            if (!input) {
                input = document.createElement('input');
                input.id = '__geranes_open_rom_input';
                input.type = 'file';
                input.accept = accepted;
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

    }, handler, kAcceptedExtensions);
}

void emcriptenRegisterVisibilityHandler(intptr_t handler)
{
    EM_ASM({
        var handler = Number(arguments[0]);
        function resolveCcall() {
            if (typeof ccall === 'function') return ccall;
            if (typeof Module !== 'undefined' && Module && typeof Module.ccall === 'function') return Module.ccall.bind(Module);
            return null;
        }
        function notifyVisibility(visible) {
            var ccallFn = resolveCcall();
            if (!ccallFn) return;
            try {
                ccallFn(
                    'onWebVisibilityChanged',
                    null,
                    ['number', 'number'],
                    [handler, visible ? 1 : 0]
                );
            } catch (e) {
                console.error("Failed to call onWebVisibilityChanged:", e);
            }
        }

        // Register only once
        if (!window.__geranes_visibility_handler_registered) {
            window.__geranes_visibility_handler_registered = true;

            document.addEventListener('visibilitychange', function () {
                notifyVisibility(document.visibilityState === 'visible');
            });

            console.log("Web visibility listeners installed for handler:", handler);
        }

    }, handler);
}

void emcriptenRegisterUnloadHandler(intptr_t handler)
{
    EM_ASM({
        var handler = Number(arguments[0]);

        function resolveCcall() {
            if (typeof ccall === 'function') return ccall;
            if (typeof Module !== 'undefined' && Module && typeof Module.ccall === 'function') return Module.ccall.bind(Module);
            return null;
        }

        function notifyUnload() {
            var ccallFn = resolveCcall();
            if (!ccallFn) return;
            try {
                ccallFn(
                    'onWebAppUnload',
                    null,
                    ['number'],
                    [handler]
                );
            } catch (e) {
                console.error("Failed to call onWebAppUnload:", e);
            }
        }

        if (!window.__geranes_unload_handler_registered) {
            window.__geranes_unload_handler_registered = true;

            window.addEventListener('pagehide', function (event) {
                if (event && event.persisted) return;
                notifyUnload();
            });

            window.addEventListener('beforeunload', function () {
                notifyUnload();
            });

            console.log("Web unload listeners installed for handler:", handler);
        }
    }, handler);
}

void emcriptenImportSession(intptr_t handler) {

    EM_ASM({
        const handler = Number(arguments[0]);
        (async () => {

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

void emcriptenSyncImGuiTextInput(bool wantTextInput)
{
    emcriptenSyncImGuiTextInputJs(wantTextInput ? 1 : 0);
}

#endif
