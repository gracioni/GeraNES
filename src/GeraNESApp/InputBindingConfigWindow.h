#pragma once

#include <string>
#include <vector>

#include "imgui_include.h"
#include "util/sdl_util.h"
#include "signal/signal.h"

#include "GeraNESApp/InputManager.h"
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

    void startCapture(int index)
    {
        m_captureIndex = index;
        m_captureState = WAIT_EMPTY;
        m_captureTime = CAPTURE_TIME;
        m_lastTime = getTime();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    }

    void stopCapture()
    {
        m_captureState = NONE;
        m_captureIndex = -1;

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    }

public:
    SigSlot::Signal<> signalShow;
    SigSlot::Signal<> signalClose;

    void show(const std::string& windowTitle, InputBindingInfo& input)
    {
        m_windowTitle = windowTitle;

        if(m_inputInfo != nullptr) {
            stopCapture();
        }

        m_inputInfo = &input;
        m_selected.assign(input.bindingCount(), 0);
        m_show = true;
        m_captureIndex = -1;
    }

    void hide()
    {
        m_windowTitle = "";
        m_show = false;
        stopCapture();
        m_inputInfo = nullptr;
        m_selected.clear();
    }

    void update()
    {
        if(m_show != m_lastShow) {
            if(m_show) signalShow();
            else signalClose();

            m_lastShow = m_show;
        }

        if(!m_show || m_inputInfo == nullptr) return;

        if(m_captureState != NONE) {
            double time = getTime();
            double dt = time - m_lastTime;
            m_captureTime -= static_cast<float>(dt);
            m_lastTime = static_cast<float>(time);

            if(m_captureTime <= 0) {
                stopCapture();
            }

            InputManager::instance().updateInputs();
            auto capture = InputManager::instance().capture();

            switch(m_captureState) {
                case WAIT_EMPTY:
                    if(capture.empty()) m_captureState = WAIT_BUTTON;
                    break;

                case WAIT_BUTTON:
                    if(!capture.empty()) {
                        if(capture[0] == "Escape") m_inputInfo->setBinding(m_captureIndex, "");
                        else m_inputInfo->setBinding(m_captureIndex, capture[0]);

                        if(m_captureIndex + 1 < static_cast<int>(m_inputInfo->bindingCount())) startCapture(m_captureIndex + 1);
                        else stopCapture();
                    }
                    break;

                default:
                    break;
            }
        }

        ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::Begin(m_windowTitle.c_str(), &m_show)) {
            if(ImGui::BeginTable("InputBindingTable", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Button");
                ImGui::TableSetupColumn("Input");
                ImGui::TableHeadersRow();

                for(int i = 0; i < static_cast<int>(m_inputInfo->bindingCount()); i++) {
                    ImGui::TableNextRow();

                    auto style = ImGui::GetStyle();
                    auto color = style.Colors[ImGuiCol_TabHovered];
                    if(m_captureState != NONE && i == m_captureIndex) {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(color));
                    }

                    ImGui::TableNextColumn();
                    if(m_captureState == NONE) ImGui::Selectable(m_inputInfo->bindingLabel(i), m_selected[i], ImGuiSelectableFlags_SpanAllColumns);
                    else ImGui::Text("%s", m_inputInfo->bindingLabel(i));

                    if(m_captureState == NONE && ImGui::IsItemActive()) {
                        startCapture(i);
                    }

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", m_inputInfo->getBinding(i).c_str());
                }

                ImGui::EndTable();
            }

            char aux[128];
            if(m_captureState != NONE) {
                sprintf(aux, "Waiting input for button '%s'... (%0.1fs)", m_inputInfo->bindingLabel(m_captureIndex), std::max(0.0f, m_captureTime));
            }
            else {
                sprintf(aux, "%s", "");
            }

            ImGui::Text("%s", aux);
            ImGui::SetWindowFocus(m_windowTitle.c_str());
        }
        ImGui::End();
    }
};
