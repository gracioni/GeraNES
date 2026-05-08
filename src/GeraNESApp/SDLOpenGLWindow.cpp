#include "GeraNESApp/SDLOpenGLWindow.h"

#include "logger/logger.h"

#include <algorithm>

#ifdef __EMSCRIPTEN__
SDLOpenGLWindow* instance = NULL;
#endif

namespace {
SDL_HitTestResult borderlessWindowHitTest(SDL_Window* window, const SDL_Point* area, void* data)
{
    auto* self = reinterpret_cast<SDLOpenGLWindow*>(data);
    if(window == nullptr || area == nullptr || self == nullptr) return SDL_HITTEST_NORMAL;
    if(self->isFullScreen() || self->isMaximized()) return SDL_HITTEST_NORMAL;
    if((SDL_GetWindowFlags(window) & SDL_WINDOW_BORDERLESS) == 0) return SDL_HITTEST_NORMAL;

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    if(width <= 0 || height <= 0) return SDL_HITTEST_NORMAL;

    constexpr int border = 6;
    const bool left = area->x < border;
    const bool right = area->x >= width - border;
    const bool top = area->y < border;
    const bool bottom = area->y >= height - border;

    if(top && left) return SDL_HITTEST_RESIZE_TOPLEFT;
    if(top && right) return SDL_HITTEST_RESIZE_TOPRIGHT;
    if(bottom && left) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if(bottom && right) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    if(left) return SDL_HITTEST_RESIZE_LEFT;
    if(right) return SDL_HITTEST_RESIZE_RIGHT;
    if(top) return SDL_HITTEST_RESIZE_TOP;
    if(bottom) return SDL_HITTEST_RESIZE_BOTTOM;
    return SDL_HITTEST_NORMAL;
}
}

void SDLOpenGLWindow::swapBuffers()
{
    SDL_GL_SwapWindow(m_window);
}

void SDLOpenGLWindow::syncDrawableSize(bool emitResizeEvent)
{
    if(m_window == NULL) return;

    int drawableW = 0;
    int drawableH = 0;
    SDL_GL_GetDrawableSize(m_window, &drawableW, &drawableH);
    if(drawableW <= 0 || drawableH <= 0) return;

    if(drawableW == m_lastDrawableW && drawableH == m_lastDrawableH) return;

    m_lastDrawableW = drawableW;
    m_lastDrawableH = drawableH;

    glViewport(0, 0, drawableW, drawableH);

    if(!emitResizeEvent) return;

    int windowW = 0;
    int windowH = 0;
    SDL_GetWindowSize(m_window, &windowW, &windowH);

    SDL_Event resizeEvent;
    SDL_zero(resizeEvent);
    resizeEvent.type = SDL_WINDOWEVENT;
    resizeEvent.window.windowID = SDL_GetWindowID(m_window);
    resizeEvent.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    resizeEvent.window.data1 = windowW;
    resizeEvent.window.data2 = windowH;
    SDL_PushEvent(&resizeEvent);
}

void SDLOpenGLWindow::syncDisplayIndex()
{
    if(m_window == NULL) return;

    const int displayIndex = SDL_GetWindowDisplayIndex(m_window);
    if(displayIndex < 0) return;
    if(displayIndex == m_lastDisplayIndex) return;

    m_lastDisplayIndex = displayIndex;
    onWindowDisplayChanged(displayIndex);
}

void SDLOpenGLWindow::notifyWindowsTitleBarInteractionChanged()
{
#ifdef _WIN32
    onWindowsTitleBarInteractionChanged(m_inWindowsCaptionInteraction || m_inNativeMoveSizeLoop);
#endif
}

void SDLOpenGLWindow::mainLoop()
{
    SDL_Event event;

    while(SDL_PollEvent(&event)) {
        onEvent(event);

        switch(event.type) {
            case SDL_QUIT:
                quit();
                break;

            case SDL_WINDOWEVENT:
                switch(event.window.event) {
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        syncDrawableSize(false);
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }

    if(m_quit) return;

    // Fullscreen transitions don't always arrive as SIZE_CHANGED in SDL web.
    syncDrawableSize(true);
    syncDisplayIndex();

    if(paintGL()) {
        swapBuffers();
    }
}

#ifdef _WIN32
LRESULT CALLBACK SDLOpenGLWindow::windowsSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<SDLOpenGLWindow*>(GetPropW(hwnd, WINDOW_PROP_NAME));
    if(self == nullptr || self->m_prevWndProc == nullptr) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->handleWindowsMessage(hwnd, msg, wParam, lParam);
}

LRESULT SDLOpenGLWindow::handleWindowsMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if(!m_windowsNativePumpEnabled) {
        return CallWindowProcW(m_prevWndProc, hwnd, msg, wParam, lParam);
    }

    switch(msg) {
        case WM_NCLBUTTONDOWN:
            if(wParam == HTCAPTION) {
                m_inWindowsCaptionInteraction = true;
                notifyWindowsTitleBarInteractionChanged();
            }
            break;

        case WM_NCLBUTTONUP:
        case WM_CAPTURECHANGED:
            m_inWindowsCaptionInteraction = false;
            notifyWindowsTitleBarInteractionChanged();
            break;

        case WM_ENTERSIZEMOVE:
            m_inNativeMoveSizeLoop = true;
            notifyWindowsTitleBarInteractionChanged();
            m_lastNativeMoveSizePumpTick = 0;
            SetTimer(hwnd, NATIVE_MOVE_SIZE_TIMER_ID, static_cast<UINT>(NATIVE_MOVE_SIZE_PUMP_INTERVAL_MS), nullptr);
            nativeMoveSizeLoopStep(true);
            break;

        case WM_EXITSIZEMOVE:
            m_inNativeMoveSizeLoop = false;
            m_inWindowsCaptionInteraction = false;
            notifyWindowsTitleBarInteractionChanged();
            m_lastNativeMoveSizePumpTick = 0;
            KillTimer(hwnd, NATIVE_MOVE_SIZE_TIMER_ID);
            nativeMoveSizeLoopStep(true);
            break;

        case WM_MOVING:
        case WM_SIZING:
        case WM_PAINT:
            if(m_inNativeMoveSizeLoop) {
                nativeMoveSizeLoopStep(false);
            }
            break;

        case WM_TIMER:
            if(m_inNativeMoveSizeLoop && wParam == NATIVE_MOVE_SIZE_TIMER_ID) {
                nativeMoveSizeLoopStep(false);
                return 0;
            }
            break;

        case WM_CLOSE:
        case WM_DESTROY:
        case WM_NCDESTROY:
            m_inNativeMoveSizeLoop = false;
            m_inWindowsCaptionInteraction = false;
            notifyWindowsTitleBarInteractionChanged();
            m_lastNativeMoveSizePumpTick = 0;
            KillTimer(hwnd, NATIVE_MOVE_SIZE_TIMER_ID);
            break;
    }

    return CallWindowProcW(m_prevWndProc, hwnd, msg, wParam, lParam);
}

void SDLOpenGLWindow::nativeMoveSizeLoopStep(bool force)
{
    if(m_window == NULL || m_context == NULL) return;

    const Uint64 now = SDL_GetTicks64();
    if(!force && m_lastNativeMoveSizePumpTick != 0 &&
       (now - m_lastNativeMoveSizePumpTick) < NATIVE_MOVE_SIZE_PUMP_INTERVAL_MS) {
        return;
    }

    m_lastNativeMoveSizePumpTick = now;
    SDL_GL_MakeCurrent(m_window, m_context);
    syncDrawableSize(true);
    syncDisplayIndex();
    if(paintGL()) {
        swapBuffers();
    }
}

void SDLOpenGLWindow::installWindowsSubclass()
{
    if(m_window == NULL) return;

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if(!SDL_GetWindowWMInfo(m_window, &wmInfo)) return;
    if(wmInfo.subsystem != SDL_SYSWM_WINDOWS) return;

    m_hwnd = wmInfo.info.win.window;
    if(m_hwnd == nullptr) return;

    SetPropW(m_hwnd, WINDOW_PROP_NAME, this);
    m_prevWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&windowsSubclassProc))
    );
}

void SDLOpenGLWindow::removeWindowsSubclass()
{
    if(m_hwnd != nullptr) {
        KillTimer(m_hwnd, NATIVE_MOVE_SIZE_TIMER_ID);
        if(m_prevWndProc != nullptr) {
            SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_prevWndProc));
        }
        RemovePropW(m_hwnd, WINDOW_PROP_NAME);
    }

    m_hwnd = nullptr;
    m_prevWndProc = nullptr;
    m_inNativeMoveSizeLoop = false;
    m_inWindowsCaptionInteraction = false;
    m_lastNativeMoveSizePumpTick = 0;
}

void SDLOpenGLWindow::stopWindowsNativePump()
{
    if(m_hwnd != nullptr) {
        KillTimer(m_hwnd, NATIVE_MOVE_SIZE_TIMER_ID);
    }
    m_inNativeMoveSizeLoop = false;
    m_inWindowsCaptionInteraction = false;
    m_lastNativeMoveSizePumpTick = 0;
}
#endif

SDLOpenGLWindow::SDLOpenGLWindow()
{
#ifdef __EMSCRIPTEN__
    instance = this;
#endif
}

bool SDLOpenGLWindow::create(const std::string& title, int x, int y, int w, int h, Uint32 flags)
{
    Logger::instance().log("Initializing SDL window...", Logger::Type::INFO);

    m_window = SDL_CreateWindow(title.c_str(), x, y, w, h, flags | SDL_WINDOW_OPENGL);

    if(m_window == NULL) {
        Logger::instance().log(std::string("SDL_CreateWindow error: ") + SDL_GetError(), Logger::Type::ERROR);
        return false;
    }

    Logger::instance().log("Initializing GL context...", Logger::Type::INFO);

    m_context = SDL_GL_CreateContext(m_window);

    if(m_context == NULL) {
        Logger::instance().log(std::string("SDL_GL_CreateContext error: ") + SDL_GetError(), Logger::Type::ERROR);
        return false;
    }

    SDL_GL_MakeCurrent(m_window, m_context);

    syncDrawableSize(false);
    m_lastDisplayIndex = SDL_GetWindowDisplayIndex(m_window);
    SDL_SetWindowHitTest(m_window, borderlessWindowHitTest, this);

    initGL();

#ifdef _WIN32
    installWindowsSubclass();
#endif

    return true;
}

void SDLOpenGLWindow::setTitle(const std::string& title)
{
    if(m_window == NULL) return;
    SDL_SetWindowTitle(m_window, title.c_str());
}

std::string SDLOpenGLWindow::title() const
{
    if(m_window == NULL) return {};
    const char* currentTitle = SDL_GetWindowTitle(m_window);
    return currentTitle != nullptr ? std::string(currentTitle) : std::string();
}

bool SDLOpenGLWindow::initGL()
{
    return true;
}

int SDLOpenGLWindow::width()
{
    if(m_window == NULL) return 0;

    int w = 0;
    int h = 0;
    SDL_GetWindowSize(m_window, &w, &h);
    return w;
}

int SDLOpenGLWindow::height()
{
    if(m_window == NULL) return 0;

    int w = 0;
    int h = 0;
    SDL_GetWindowSize(m_window, &w, &h);
    return h;
}

void SDLOpenGLWindow::run()
{
    if(m_window == NULL) return;

#ifdef __EMSCRIPTEN__
    // 0 fps means to use requestAnimationFrame; non-0 means to use setTimeout.
    emscripten_set_main_loop([]() {
        instance->mainLoop();
    }, 0, 1);
#else
    while(!m_quit) {
        mainLoop();
    }
#endif
}

bool SDLOpenGLWindow::onEvent(SDL_Event& e)
{
    (void)e;
    return false;
}

void SDLOpenGLWindow::onWindowsTitleBarInteractionChanged(bool active)
{
    (void)active;
}

void SDLOpenGLWindow::onWindowDisplayChanged(int displayIndex)
{
    (void)displayIndex;
}

bool SDLOpenGLWindow::paintGL()
{
    return true;
}

SDLOpenGLWindow::~SDLOpenGLWindow()
{
#ifdef _WIN32
    removeWindowsSubclass();
#endif
    if(m_context != NULL) SDL_GL_DeleteContext(m_context);
    if(m_window != NULL) SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void* SDLOpenGLWindow::glContext()
{
    return m_context;
}

SDL_Window* SDLOpenGLWindow::sdlWindow()
{
    return m_window;
}

void* SDLOpenGLWindow::nativeWindowHandle() const
{
#ifdef _WIN32
    return m_hwnd;
#else
    return nullptr;
#endif
}

void SDLOpenGLWindow::quit()
{
#ifdef _WIN32
    stopWindowsNativePump();
#endif
    m_quit = true;
}

#ifdef _WIN32
void SDLOpenGLWindow::setWindowsNativePumpEnabled(bool enabled)
{
    if(m_windowsNativePumpEnabled == enabled) return;
    m_windowsNativePumpEnabled = enabled;
    if(!enabled) {
        stopWindowsNativePump();
    }
}

bool SDLOpenGLWindow::isWindowsTitleBarInteractionActive() const
{
    return m_inWindowsCaptionInteraction || m_inNativeMoveSizeLoop;
}
#else
void SDLOpenGLWindow::setWindowsNativePumpEnabled(bool enabled)
{
    (void)enabled;
}

bool SDLOpenGLWindow::isWindowsTitleBarInteractionActive() const
{
    return false;
}
#endif

bool SDLOpenGLWindow::isFullScreen()
{
#ifdef __EMSCRIPTEN__
    int isFullscreen = EM_ASM_INT({
        if (typeof window !== 'undefined' && typeof window.geranesIsFullscreen === 'function') {
            return window.geranesIsFullscreen() ? 1 : 0;
        }
        return !!(document.fullscreenElement || document.webkitFullscreenElement || document.msFullscreenElement);
    });
    return isFullscreen != 0;
#else
    auto flags = SDL_GetWindowFlags(m_window);
    return flags & SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
}

bool SDLOpenGLWindow::setFullScreen(bool state, bool exclusive)
{
#ifdef __EMSCRIPTEN__
    (void)exclusive;
    int requested = EM_ASM_INT({
        var desired = !!arguments[0];

        if (typeof window !== 'undefined' && typeof window.geranesSetFullscreen === 'function') {
            try {
                window.geranesSetFullscreen(desired);
                return 1;
            } catch (e) {
                console.error("geranesSetFullscreen failed:", e);
                return 0;
            }
        }

        var canvas = (typeof Module !== 'undefined' && Module.canvas) ? Module.canvas : document.getElementById('canvas');
        if (!canvas) return 0;

        var current = !!(document.fullscreenElement || document.webkitFullscreenElement || document.msFullscreenElement);
        if (desired === current) return 1;

        if (desired) {
            var req = canvas.requestFullscreen || canvas.webkitRequestFullscreen || canvas.msRequestFullscreen;
            if (!req) return 0;
            req.call(canvas);
            return 1;
        }

        var exitFs = document.exitFullscreen || document.webkitExitFullscreen || document.msExitFullscreen;
        if (!exitFs) return 0;
        exitFs.call(document);
        return 1;
    }, state ? 1 : 0);

    return requested != 0;
#else
    if(state) {
        int displayIndex = SDL_GetWindowDisplayIndex(m_window);
        if(displayIndex < 0) displayIndex = 0;

        SDL_Rect displayBounds{};
        if(SDL_GetDisplayBounds(displayIndex, &displayBounds) == 0) {
            int windowW = 0;
            int windowH = 0;
            SDL_GetWindowSize(m_window, &windowW, &windowH);
            SDL_SetWindowPosition(
                m_window,
                displayBounds.x + std::max(0, (displayBounds.w - windowW) / 2),
                displayBounds.y + std::max(0, (displayBounds.h - windowH) / 2)
            );
        }

        if(exclusive) {
            SDL_DisplayMode mode{};
            if(SDL_GetDesktopDisplayMode(displayIndex, &mode) == 0) {
                SDL_SetWindowDisplayMode(m_window, &mode);
            } else {
                Logger::instance().log(std::string("SDL_GetDesktopDisplayMode error: ") + SDL_GetError(), Logger::Type::WARNING);
                SDL_SetWindowDisplayMode(m_window, nullptr);
            }
        }

        return SDL_SetWindowFullscreen(m_window, exclusive ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP) == 0;
    }

    return SDL_SetWindowFullscreen(m_window, 0) == 0;
#endif
}

void SDLOpenGLWindow::setBordered(bool bordered)
{
    if(m_window == NULL) return;
    SDL_SetWindowBordered(m_window, bordered ? SDL_TRUE : SDL_FALSE);
}

void SDLOpenGLWindow::minimizeWindow()
{
    SDL_MinimizeWindow(m_window);
}

void SDLOpenGLWindow::restoreWindow()
{
    SDL_RestoreWindow(m_window);
}

void SDLOpenGLWindow::maximizeWindow()
{
    SDL_MaximizeWindow(m_window);
}

bool SDLOpenGLWindow::isMinimized() const
{
    if(m_window == NULL) return false;
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) != 0;
}

bool SDLOpenGLWindow::isMaximized() const
{
    if(m_window == NULL) return false;
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) != 0;
}

int SDLOpenGLWindow::getVSync() const
{
    return m_context != nullptr ? SDL_GL_GetSwapInterval() : 0;
}

void SDLOpenGLWindow::setVSync(int vsync) const
{
    if(m_context == nullptr) return;

    bool requestedAdaptive = vsync == -1;
    bool requestFailed = false;
    if(SDL_GL_SetSwapInterval(vsync) != 0) {
        requestFailed = true;
    }

    // Check if adaptive vsync was requested but not supported, and fall back
    // to regular vsync if so.
    if(vsync == -1 && SDL_GL_GetSwapInterval() != -1) {
        if(SDL_GL_SetSwapInterval(1) != 0) {
            requestFailed = true;
        }
    }

    const int effectiveSwapInterval = SDL_GL_GetSwapInterval();
    if(vsync == 0) {
        Logger::instance().log("VSync disabled", Logger::Type::INFO);
    } else if(effectiveSwapInterval == -1) {
        Logger::instance().log("Adaptive VSync enabled", Logger::Type::INFO);
    } else if(effectiveSwapInterval == 1) {
        if(requestedAdaptive) {
            Logger::instance().log("Adaptive VSync is not available; using regular VSync", Logger::Type::WARNING);
        } else {
            Logger::instance().log("VSync enabled", Logger::Type::INFO);
        }
    } else if(requestFailed) {
        Logger::instance().log("VSync could not be enabled by the current display driver", Logger::Type::WARNING);
    } else {
        Logger::instance().log("VSync state could not be confirmed", Logger::Type::WARNING);
    }
}

int SDLOpenGLWindow::getDisplayFrameRate()
{
    int displayIndex = 0;
    if(m_window != NULL) {
        displayIndex = SDL_GetWindowDisplayIndex(m_window);
        if(displayIndex < 0) {
            Logger::instance().log(std::string("SDL_GetWindowDisplayIndex error: ") + SDL_GetError(), Logger::Type::WARNING);
            displayIndex = 0;
        }
    }

    SDL_DisplayMode mode;
    if(SDL_GetCurrentDisplayMode(displayIndex, &mode) != 0) {
        Logger::instance().log(std::string("SDL_GetCurrentDisplayMode error: ") + SDL_GetError(), Logger::Type::WARNING);
        if(displayIndex != 0 && SDL_GetCurrentDisplayMode(0, &mode) != 0) {
            Logger::instance().log(std::string("SDL_GetCurrentDisplayMode fallback error: ") + SDL_GetError(), Logger::Type::ERROR);
            return 0;
        }
    }

    return mode.refresh_rate;
}

glm::vec2 SDLOpenGLWindow::GetWindowDPI()
{
    const float baseDpi = 96.0f;

    int winW = 0;
    int winH = 0;
    SDL_GetWindowSize(m_window, &winW, &winH);

    int drawW = 0;
    int drawH = 0;
    SDL_GL_GetDrawableSize(m_window, &drawW, &drawH);

    if(winW == 0 || winH == 0)
        return glm::vec2(baseDpi, baseDpi);

    float scaleX = static_cast<float>(drawW) / static_cast<float>(winW);
    float scaleY = static_cast<float>(drawH) / static_cast<float>(winH);

    float dpiX = scaleX * baseDpi;
    float dpiY = scaleY * baseDpi;

    return glm::vec2(dpiX, dpiY);
}
