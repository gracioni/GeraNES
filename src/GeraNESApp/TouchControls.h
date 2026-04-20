#pragma once

#include <functional>
#include <map>
#include <string_view>

#include <SDL.h>
#include <glm/glm.hpp>

#include "AppSettings.h"

#include "GeraNESApp/InputManager.h"
#include "ControllerInfo.h"

#include "util/geometry.h"
#include "util/sdl_util.h"
#include "util/glm_util.h"

#include "yoga_raii.hpp"
#include "json2yoga.hpp"

struct ImDrawList;

class TouchControls {

private:

    static constexpr SDL_FingerID kInvalidFingerId = static_cast<SDL_FingerID>(-1);

    SDL_FingerID thumbIndex = kInvalidFingerId;
    glm::vec2 thumbCenter;
    SDL_FingerID aFingerId = kInvalidFingerId;
    SDL_FingerID bFingerId = kInvalidFingerId;
    SDL_FingerID selectFingerId = kInvalidFingerId;
    SDL_FingerID startFingerId = kInvalidFingerId;
    SDL_FingerID rewindFingerId = kInvalidFingerId;

    struct Buttons {
        bool up = false;
        bool right = false;
        bool down = false;
        bool left = false;
        bool select = false;
        bool start = false;
        bool b = false;
        bool a = false;
        bool rewind = false;
        bool saveState = false;
        bool loadState = false;

        void reset() {
            up = false;
            right = false;
            down = false;
            left = false;
            select = false;
            start = false;
            b = false;
            a = false;
            rewind = false;
            saveState = false;
            loadState = false;
        }

        bool anyPressed() const {
            if(up || right || down || left ||
            select || start || b || a ||
            rewind || saveState || loadState) return true;

            return false;
        }
    };

    Buttons m_buttons;

    int m_screenWidth;
    int m_screenHeight;
    glm::vec2 m_dpi;

    yoga_raii::Node::Ptr m_root = nullptr;

    glm::vec4 m_margin = {0,0,0,0};

    std::shared_ptr<GLTexture> m_rewindTexture;
    std::shared_ptr<GLTexture> m_digitalPagTexture;
    std::shared_ptr<GLTexture> m_midButtonTexture;
    std::shared_ptr<GLTexture> m_midButtonPressedTexture;
    std::shared_ptr<GLTexture> m_rightButtonTexture;
    std::shared_ptr<GLTexture> m_rightButtonPressedTexture;

    void testDownButton(std::string_view id, glm::vec2 point, const std::function<void()>& callback);
    void updateDigitalPad(SDL_FingerID eventFingerIndex, glm::vec2 point );

public:

    TouchControls(int screenWidth, int screenHeight, glm::vec2 dpi);
    void setMargin(glm::vec4 margin);
    void setTopMargin(float value);
    void createLayout();
    void onResize(int screenWidth, int screenHeight);
    void onEvent(SDL_Event& ev);
    const Buttons& buttons();
    void draw(ImDrawList* drawList);
    void update(Uint64 dt);

};
