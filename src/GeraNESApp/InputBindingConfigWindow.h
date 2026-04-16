#pragma once

#include <string>
#include <vector>

#include "signal/signal.h"

#include "GeraNESApp/InputBindingInfo.h"

class InputBindingConfigWindow
{
private:
    static constexpr float CAPTURE_TIME = 3.0f;

    std::vector<int> m_selected;
    enum {NONE, WAIT_EMPTY, WAIT_BUTTON} m_captureState = NONE;

    bool m_show = false;
    bool m_lastShow = false;
    InputBindingInfo* m_inputInfo = nullptr;
    std::string m_windowTitle = "";
    int m_captureIndex = -1;
    float m_lastTime = 0.0f;
    float m_captureTime = 0.0f;

    void startCapture(int index);
    void stopCapture();

public:
    SigSlot::Signal<> signalShow;
    SigSlot::Signal<> signalClose;

    void show(const std::string& windowTitle, InputBindingInfo& input);
    void hide();
    void update();
};
