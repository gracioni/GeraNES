#include "GeraNESApp/PowerPadConfigWindow.h"

#include <algorithm>
#include <cstdio>

#include "imgui_include.h"
#include "util/sdl_util.h"

#include "GeraNESApp/InputManager.h"

void PowerPadConfigWindow::startCapture(int index)
{
    m_captureIndex = index;
    m_captureState = WAIT_EMPTY;
    m_captureTime = CAPTURE_TIME;
    m_lastTime = static_cast<float>(getTime());

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
}

void PowerPadConfigWindow::stopCapture()
{
    m_captureState = NONE;
    m_captureIndex = -1;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
}

void PowerPadConfigWindow::show(const std::string& windowTitle, PowerPadInfo& info)
{
    if(m_info != nullptr) {
        stopCapture();
    }

    m_windowTitle = windowTitle;
    m_info = &info;
    m_show = true;
}

void PowerPadConfigWindow::hide()
{
    m_show = false;
    m_windowTitle.clear();
    m_info = nullptr;
    stopCapture();
}

void PowerPadConfigWindow::update()
{
    if(m_show != m_lastShow) {
        if(m_show) signalShow();
        else signalClose();
        m_lastShow = m_show;
    }

    if(!m_show || m_info == nullptr) return;

    if(m_captureState != NONE) {
        const float currentTime = static_cast<float>(getTime());
        m_captureTime -= currentTime - m_lastTime;
        m_lastTime = currentTime;

        if(m_captureTime <= 0.0f) {
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
                    m_info->setBinding(m_captureIndex, capture[0] == "Escape" ? "" : capture[0]);
                    if(m_captureIndex + 1 < static_cast<int>(m_info->bindingCount())) {
                        startCapture(m_captureIndex + 1);
                    } else {
                        stopCapture();
                    }
                }
                break;

            default:
                break;
        }
    }

    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(ImGui::Begin(m_windowTitle.c_str(), &m_show, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("Click a pad and press the desired key. Escape clears the current binding.");
        ImGui::Separator();

        if(ImGui::BeginTable("PowerPadGrid", GRID_COLUMNS, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
            for(int row = 0; row < GRID_ROWS; ++row) {
                ImGui::TableNextRow();
                for(int col = 0; col < GRID_COLUMNS; ++col) {
                    const int index = row * GRID_COLUMNS + col;
                    ImGui::TableSetColumnIndex(col);

                    const bool capturingThis = (m_captureState != NONE && m_captureIndex == index);
                    const std::string& binding = m_info->getBinding(index);
                    const std::string bindingText = capturingThis ? "Press a key..." : (binding.empty() ? "-" : binding);

                    ImGui::PushID(index);
                    ImGui::BeginGroup();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextDisabled("Pad %s", m_info->bindingLabel(index));

                    if(capturingThis) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                    }

                    if(ImGui::Button(bindingText.c_str(), ImVec2(-1.0f, BUTTON_HEIGHT)) && m_captureState == NONE) {
                        startCapture(index);
                    }

                    if(capturingThis) {
                        ImGui::PopStyleColor(3);
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        char aux[128];
        if(m_captureState != NONE && m_captureIndex >= 0) {
            std::snprintf(aux,
                          sizeof(aux),
                          "Waiting input for pad %s... (%.1fs)",
                          m_info->bindingLabel(static_cast<size_t>(m_captureIndex)),
                          std::max(0.0f, m_captureTime));
        } else {
            std::snprintf(aux, sizeof(aux), "%s", "");
        }
        ImGui::Text("%s", aux);
    }
    ImGui::End();
}
