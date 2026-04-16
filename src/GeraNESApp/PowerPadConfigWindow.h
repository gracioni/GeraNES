#pragma once

#include <string>

#include "signal/signal.h"

#include "GeraNESApp/PowerPadInfo.h"

class PowerPadConfigWindow
{
private:
    static constexpr float CAPTURE_TIME = 3.0f;
    static constexpr int GRID_COLUMNS = 4;
    static constexpr int GRID_ROWS = 3;
    static constexpr float WINDOW_WIDTH = 620.0f;
    static constexpr float BUTTON_HEIGHT = 34.0f;

    enum CaptureState { NONE, WAIT_EMPTY, WAIT_BUTTON };

    bool m_show = false;
    bool m_lastShow = false;
    CaptureState m_captureState = NONE;
    int m_captureIndex = -1;
    float m_lastTime = 0.0f;
    float m_captureTime = 0.0f;
    std::string m_windowTitle;
    PowerPadInfo* m_info = nullptr;

    void startCapture(int index);
    void stopCapture();

public:
    SigSlot::Signal<> signalShow;
    SigSlot::Signal<> signalClose;

    void show(const std::string& windowTitle, PowerPadInfo& info);
    void hide();
    void update();
};
