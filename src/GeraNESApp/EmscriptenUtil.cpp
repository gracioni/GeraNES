#include "EmscriptenUtil.h"

#include "imgui.h"

#include <cstring>
#include <string>

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <imgui_internal.h>

namespace {

std::string g_imguiClipboardText;
std::string g_imguiSelectionText;

const char* resolveInputTextSelectionSource(const ImGuiInputTextState& state, int& outLength)
{
    outLength = 0;

    const int selectionStart = ImMin(state.GetSelectionStart(), state.GetSelectionEnd());
    const int selectionEnd = ImMax(state.GetSelectionStart(), state.GetSelectionEnd());
    if(selectionEnd <= selectionStart) {
        return nullptr;
    }

    outLength = selectionEnd - selectionStart;

    if(state.TextSrc != nullptr) {
        return state.TextSrc + selectionStart;
    }

    if(state.TextA.Data != nullptr && state.TextA.Size > 0) {
        return state.TextA.Data + selectionStart;
    }

    if(state.TextToRevertTo.Data != nullptr && state.TextToRevertTo.Size > selectionEnd) {
        return state.TextToRevertTo.Data + selectionStart;
    }

    outLength = 0;
    return nullptr;
}

void refreshImGuiSelectionTextCache()
{
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if(ctx == nullptr) {
        g_imguiSelectionText.clear();
        return;
    }

    ImGuiInputTextState& state = ctx->InputTextState;
    if(state.ID == 0 || !state.HasSelection()) {
        g_imguiSelectionText.clear();
        return;
    }

    int selectionLength = 0;
    const char* selectionPtr = resolveInputTextSelectionSource(state, selectionLength);
    if(selectionPtr != nullptr && selectionLength > 0) {
        g_imguiSelectionText.assign(selectionPtr, static_cast<size_t>(selectionLength));
    } else {
        g_imguiSelectionText.clear();
    }
}

extern "C" {
EMSCRIPTEN_KEEPALIVE const char* emscriptenGetClipboardTextForBrowserCopy()
{
    refreshImGuiSelectionTextCache();
    if(!g_imguiSelectionText.empty()) {
        return g_imguiSelectionText.c_str();
    }
    return g_imguiClipboardText.c_str();
}
}

void imguiSetClipboardText(void*, const char* text)
{
    g_imguiClipboardText = (text != nullptr) ? text : "";
    EM_ASM({
        const text = UTF8ToString($0);
        window.__geranes_imgui_clipboard_text = text;
        window.__geranes_imgui_clipboard_updated_at = Date.now();

        if (typeof window.__geranes_request_clipboard_copy === 'function') {
            window.__geranes_request_clipboard_copy(text);
        } else if (typeof window.__geranes_copy_text_to_clipboard === 'function') {
            window.__geranes_copy_text_to_clipboard(text, true);
        }
    }, g_imguiClipboardText.c_str());
}

const char* imguiGetClipboardText(void*)
{
    return g_imguiClipboardText.c_str();
}

} // namespace

void emcriptenCopyTextToClipboardExact(const char* text)
{
    g_imguiClipboardText = (text != nullptr) ? text : "";
    EM_ASM({
        const text = UTF8ToString($0);
        window.__geranes_imgui_clipboard_text = text;
        window.__geranes_imgui_clipboard_updated_at = Date.now();
        window.__geranes_force_clipboard_text = text;

        if (typeof window.__geranes_copy_text_to_clipboard === 'function') {
            window.__geranes_copy_text_to_clipboard(text, true);
        }
    }, g_imguiClipboardText.c_str());
}

EM_JS(void, emcriptenSyncImGuiTextInputJs, (int wantTextInput), {
    try {
        const want = !!wantTextInput;

        function resolveCcall() {
            if (typeof ccall === 'function') return ccall;
            if (typeof Module !== 'undefined' && Module && typeof Module.ccall === 'function') return Module.ccall.bind(Module);
            return null;
        }

        function resolveClipboardTextForCopyEvent() {
            const ccallFn = resolveCcall();
            if (ccallFn) {
                try {
                    const clipboardText = ccallFn('emscriptenGetClipboardTextForBrowserCopy', 'string', [], []);
                    if (typeof clipboardText === 'string' && clipboardText.length > 0) {
                        return clipboardText;
                    }
                } catch (e) {
                    console.error('emscriptenGetClipboardTextForBrowserCopy failed:', e);
                }
            }
            return "";
        }

        function getPreferredClipboardText() {
            if (typeof window.__geranes_force_clipboard_text === 'string') {
                return window.__geranes_force_clipboard_text;
            }
            const liveClipboardText = resolveClipboardTextForCopyEvent();
            if (liveClipboardText.length > 0) {
                return liveClipboardText;
            }
            const clipboardUpdatedAt =
                typeof window.__geranes_imgui_clipboard_updated_at === 'number'
                    ? window.__geranes_imgui_clipboard_updated_at
                    : 0;
            const selectionUpdatedAt =
                typeof window.__geranes_imgui_selection_updated_at === 'number'
                    ? window.__geranes_imgui_selection_updated_at
                    : 0;
            if (clipboardUpdatedAt >= selectionUpdatedAt &&
                typeof window.__geranes_imgui_clipboard_text === 'string') {
                return window.__geranes_imgui_clipboard_text;
            }
            if (typeof window.__geranes_imgui_selection_text === 'string' &&
                window.__geranes_imgui_selection_text.length > 0) {
                return window.__geranes_imgui_selection_text;
            }
            if (typeof window.__geranes_imgui_clipboard_text === 'string') {
                return window.__geranes_imgui_clipboard_text;
            }
            return "";
        }

        function ensureBridge() {
            if (window.__geranes_imgui_text_input_bridge) {
                return window.__geranes_imgui_text_input_bridge;
            }

            function copyTextToClipboard(text, preferDomCopy) {
                const resolvedText = typeof text === 'string' ? text : "";
                window.__geranes_force_clipboard_text = resolvedText;
                window.__geranes_imgui_clipboard_text = resolvedText;
                window.__geranes_imgui_clipboard_updated_at = Date.now();

                function releaseForcedClipboardTextSoon(expectedText) {
                    setTimeout(function() {
                        if (window.__geranes_force_clipboard_text === expectedText) {
                            delete window.__geranes_force_clipboard_text;
                        }
                    }, 50);
                }

                function fallbackCopy() {
                    let copied = false;
                    const activeElement = document.activeElement;
                    const helper = document.createElement('textarea');
                    helper.value = resolvedText;
                    helper.setAttribute('readonly', '');
                    helper.style.position = 'fixed';
                    helper.style.left = '-9999px';
                    helper.style.top = '0';
                    helper.style.opacity = '0';
                    helper.style.pointerEvents = 'none';

                    try {
                        document.body.appendChild(helper);
                        helper.focus({ preventScroll: true });
                        helper.select();
                        helper.setSelectionRange(0, helper.value.length);
                        copied = document.execCommand('copy');
                    } catch (err) {
                        console.warn('document.execCommand(copy) failed:', err);
                    } finally {
                        if (helper.parentNode) {
                            helper.parentNode.removeChild(helper);
                        }
                        if (activeElement && typeof activeElement.focus === 'function') {
                            try {
                                activeElement.focus({ preventScroll: true });
                            } catch (_) {
                                try { activeElement.focus(); } catch (_) {}
                            }
                        }
                    }
                    releaseForcedClipboardTextSoon(resolvedText);
                    return copied;
                }

                if (preferDomCopy) {
                    if (fallbackCopy()) {
                        return true;
                    }
                }

                if (typeof navigator !== 'undefined' &&
                    navigator.clipboard &&
                    typeof navigator.clipboard.writeText === 'function') {
                    navigator.clipboard.writeText(resolvedText)
                        .then(function() {
                            releaseForcedClipboardTextSoon(resolvedText);
                        })
                        .catch(function(err) {
                            console.warn('navigator.clipboard.writeText failed:', err);
                            fallbackCopy();
                        });
                    return true;
                }

                const copied = fallbackCopy();
                return copied;
            }

            if (typeof window.__geranes_copy_text_to_clipboard !== 'function') {
                window.__geranes_copy_text_to_clipboard = copyTextToClipboard;
            }

            function consumePendingClipboardCopy() {
                if (typeof window.__geranes_pending_clipboard_copy_text !== 'string') {
                    return false;
                }

                const pendingText = window.__geranes_pending_clipboard_copy_text;
                delete window.__geranes_pending_clipboard_copy_text;
                copyTextToClipboard(pendingText, true);
                return true;
            }

            if (typeof window.__geranes_request_clipboard_copy !== 'function') {
                window.__geranes_request_clipboard_copy = function(text) {
                    const resolvedText = typeof text === 'string' ? text : "";
                    window.__geranes_pending_clipboard_copy_text = resolvedText;
                    window.__geranes_imgui_clipboard_text = resolvedText;
                    window.__geranes_imgui_clipboard_updated_at = Date.now();
                };
            }

            const bridge = {};
            bridge.wantTextInput = false;
            bridge.lastUserActivation = 0;
            bridge.input = null;
            bridge.lastPointerX = 0;
            bridge.lastPointerY = 0;
            bridge.compositionText = "";
            bridge.syncedValue = "";
            bridge.globalKeyHandlersInstalled = false;

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

            function resetEditorBuffer() {
                bridge.compositionText = "";
                bridge.syncedValue = "";
                clearInputValue();
            }
            bridge.resetEditorBuffer = resetEditorBuffer;

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

            function sendBackspace() {
                callNative(
                    'injectWebKeyEvent',
                    ['string', 'number', 'number', 'number', 'number', 'number'],
                    ['Backspace', 1, 0, 0, 0, 0]
                );
                callNative(
                    'injectWebKeyEvent',
                    ['string', 'number', 'number', 'number', 'number', 'number'],
                    ['Backspace', 0, 0, 0, 0, 0]
                );
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
                resetEditorBuffer();
                if (bridge.wantTextInput) {
                    focusInput();
                }
            }

            function countCodepoints(text) {
                return Array.from(text || "").length;
            }

            function syncEditorDelta(nextValue) {
                const previousValue = bridge.syncedValue || "";
                const currentValue = nextValue || "";

                let commonPrefix = 0;
                const commonLimit = Math.min(previousValue.length, currentValue.length);
                while (commonPrefix < commonLimit &&
                       previousValue.charCodeAt(commonPrefix) === currentValue.charCodeAt(commonPrefix)) {
                    commonPrefix++;
                }

                const removedSuffix = previousValue.slice(commonPrefix);
                for (let i = 0; i < countCodepoints(removedSuffix); i++) {
                    sendBackspace();
                }

                const addedSuffix = currentValue.slice(commonPrefix);
                if (addedSuffix) {
                    sendPendingText(addedSuffix);
                }

                bridge.syncedValue = currentValue;
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

            function isCopyShortcut(event) {
                const key = event && typeof event.key === 'string' ? event.key.toLowerCase() : "";
                return key === 'c' && !!event && (event.ctrlKey || event.metaKey);
            }

            function isEditableDomTarget(target) {
                if (!target) {
                    return false;
                }

                if (target === input) {
                    return true;
                }

                const tagName = typeof target.tagName === 'string' ? target.tagName.toUpperCase() : "";
                return tagName === 'INPUT' ||
                    tagName === 'TEXTAREA' ||
                    tagName === 'SELECT' ||
                    !!target.isContentEditable;
            }

            function forwardKeyEvent(event, down) {
                callNative(
                    'injectWebKeyEvent',
                    ['string', 'number', 'number', 'number', 'number', 'number'],
                    [event.key || "", down ? 1 : 0, event.ctrlKey ? 1 : 0, event.shiftKey ? 1 : 0, event.altKey ? 1 : 0, event.metaKey ? 1 : 0]
                );
            }

            function handleCopyShortcut(event) {
                copyTextToClipboard(getPreferredClipboardText(), true);
                event.preventDefault();
            }

            input.addEventListener('keydown', function(event) {
                forwardKeyEvent(event, true);

                if (isCopyShortcut(event)) {
                    handleCopyShortcut(event);
                }

                if (isManagedKey(event.key)) {
                    event.preventDefault();
                }
            });

            input.addEventListener('keyup', function(event) {
                forwardKeyEvent(event, false);

                if (isManagedKey(event.key)) {
                    event.preventDefault();
                }
            });

            input.addEventListener('compositionstart', function() {
                bridge.compositionText = "";
            });

            input.addEventListener('copy', function(event) {
                const clipboardText = getPreferredClipboardText();
                if (event && event.clipboardData) {
                    event.clipboardData.setData('text/plain', clipboardText);
                    event.preventDefault();
                }
            });

            input.addEventListener('compositionupdate', function(event) {
                bridge.compositionText = (event && typeof event.data === 'string') ? event.data : "";
            });

            input.addEventListener('input', function(event) {
                const inputType = event && typeof event.inputType === 'string' ? event.inputType : "";

                if (inputType === 'deleteContentForward') {
                    callNative(
                        'injectWebKeyEvent',
                        ['string', 'number', 'number', 'number', 'number', 'number'],
                        ['Delete', 1, 0, 0, 0, 0]
                    );
                    callNative(
                        'injectWebKeyEvent',
                        ['string', 'number', 'number', 'number', 'number', 'number'],
                        ['Delete', 0, 0, 0, 0, 0]
                    );
                } else {
                    const nextValue = input.value.length > 0 ? input.value : bridge.compositionText;
                    syncEditorDelta(nextValue);
                }

                bridge.compositionText = "";
            });

            input.addEventListener('compositionend', function() {
                bridge.compositionText = "";
                syncEditorDelta(input.value);
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
                ['pointerup', 'mouseup', 'touchend', 'click'].forEach(function(eventName) {
                    canvas.addEventListener(eventName, function() {
                        consumePendingClipboardCopy();
                    }, false);
                });
            }

            if (!bridge.globalKeyHandlersInstalled) {
                bridge.globalKeyHandlersInstalled = true;

                document.addEventListener('keydown', function(event) {
                    if (document.activeElement === input || isEditableDomTarget(event.target)) {
                        return;
                    }

                    forwardKeyEvent(event, true);

                    if (isCopyShortcut(event)) {
                        handleCopyShortcut(event);
                    }
                }, true);

                document.addEventListener('keyup', function(event) {
                    if (document.activeElement === input || isEditableDomTarget(event.target)) {
                        return;
                    }

                    forwardKeyEvent(event, false);
                }, true);

                document.addEventListener('copy', function(event) {
                    const clipboardText = getPreferredClipboardText();
                    if (event && event.clipboardData) {
                        event.clipboardData.setData('text/plain', clipboardText);
                        event.preventDefault();
                    }
                }, true);

                ['pointerup', 'mouseup', 'touchend', 'click'].forEach(function(eventName) {
                    document.addEventListener(eventName, function() {
                        consumePendingClipboardCopy();
                    }, false);
                });
            }

            updateInputPosition(bridge.lastPointerX, bridge.lastPointerY);
            clearInputValue();
            window.__geranes_imgui_text_input_bridge = bridge;
            return bridge;
        }

        const bridge = window.__geranes_imgui_text_input_bridge;
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

        activeBridge.resetEditorBuffer();
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

void emcriptenFileDialog(intptr_t handler, const char* acceptedExtensions) {
    constexpr const char* kDefaultAcceptedExtensions =
#ifdef ENABLE_NSF_PLAYER
        ".nes,.fds,.nsf,.zip,.7z";
#else
        ".nes,.fds,.zip,.7z";
#endif
    const char* acceptList = acceptedExtensions != nullptr ? acceptedExtensions : kDefaultAcceptedExtensions;

    EM_ASM({
        var handler = Number(arguments[0]);
        var accepted = UTF8ToString(arguments[1]);

        function openFileDialog() {

            var input = document.getElementById('__geranes_file_input');
            if (!input) {
                input = document.createElement('input');
                input.id = '__geranes_file_input';
                input.type = 'file';
                input.style.display = 'none';
                document.body.appendChild(input);
            }
            input.accept = accepted;

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

    }, handler, acceptList);
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

void emcriptenImportModArchive(intptr_t handler) {

    EM_ASM({
        const handler = Number(arguments[0]);
        (async () => {

            const ccallFn = (typeof ccall === 'function') ? ccall :
                ((typeof Module !== 'undefined' && Module && typeof Module.ccall === 'function') ? Module.ccall.bind(Module) : null);

            try {
                if (typeof importModDirectoryFromZip !== 'function') {
                    console.error("importModDirectoryFromZip not found");
                    return;
                }

                const extractedPath = await importModDirectoryFromZip();
                if (!extractedPath) {
                    return;
                }

                if (!ccallFn) {
                    console.error("ccall not available for onSessionImportComplete");
                    return;
                }

                ccallFn('onSessionImportComplete', null, ['number'], [handler]);

            } catch (err) {
                console.error("Mod import failed:", err);
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

void emcriptenInstallImGuiClipboardBackend()
{
    ImGuiIO& io = ImGui::GetIO();
    io.SetClipboardTextFn = imguiSetClipboardText;
    io.GetClipboardTextFn = imguiGetClipboardText;
    io.ClipboardUserData = nullptr;
    emcriptenSyncImGuiTextInputJs(0);
}

void emcriptenSyncImGuiClipboardSelection()
{
    refreshImGuiSelectionTextCache();

    EM_ASM({
        const text = UTF8ToString($0, $1);
        window.__geranes_imgui_selection_text = text;
        window.__geranes_imgui_selection_updated_at = Date.now();
    }, g_imguiSelectionText.data(), static_cast<int>(g_imguiSelectionText.size()));
}

#endif
