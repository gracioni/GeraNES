#pragma once

#include <vector>
#include <string>

#include "imgui_include.h"
#include "util/sdl_util.h"

#include "signal/signal.h"

#include "GeraNESApp/ControllerInfo.h"
#include "GeraNESApp/InputManager.h"
#include "GeraNESApp/yoga_raii.hpp"

class ControllerConfigWindow {

private:

    const static constexpr size_t N_BUTTONS = ControllerInfo::BUTTONS.size();
    const float CAPTURE_TIME = 3.0f;
    
    std::vector<int> selected = std::vector<int>(N_BUTTONS, 0);   

    enum {NONE, WAIT_EMPTY, WAIT_BUTTON} m_captureState = NONE;

    bool m_show = false;
    bool m_lastShow = false;

    ControllerInfo* m_inputInfo = nullptr;

    std::string m_windowTitle = "";

    int m_captureIndex = -1;
    float m_lastTime = 0.0f;
    float m_captureTime = 0.0f;

    void startCapture(int index) {
        m_captureIndex = index;
        m_captureState = WAIT_EMPTY;
        m_captureTime = CAPTURE_TIME;
        m_lastTime = getTime();

        ImGuiIO& io = ImGui::GetIO();
        // Desativa o input de teclado
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    }

    void stopCapture() {
        m_captureState = NONE;
        m_captureIndex = -1;

        ImGuiIO& io = ImGui::GetIO();
        // Desativa o input de teclado
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    }

    void NESControllerDraw() {

        const ImVec2 GRID_SIZE = ImVec2(11,5);
        const ImVec2 GRID_PADDING = ImVec2(10,10);
        const ImU32 WHITE = IM_COL32(255, 255, 255, 255);    
  
        ImDrawList* drawList = ImGui::GetWindowDrawList(); 

        ImVec2 vMin = ImGui::GetWindowContentRegionMin();
        ImVec2 vMax = ImGui::GetWindowContentRegionMax();

        vMax.y = 128;

        vMin += ImGui::GetWindowPos();
        vMax += ImGui::GetWindowPos();
        
        ImVec2 unitSize =  ImVec2((vMax-vMin)/GRID_SIZE);

        ImVec2 cursorOffset = ImGui::GetCursorScreenPos();     

        ImGui::Dummy(unitSize * GRID_SIZE); 

        // Draw Up button
        drawList->AddRectFilled(cursorOffset + unitSize * ImVec2(2, 1) + GRID_PADDING/2,
        cursorOffset + unitSize * ImVec2(2, 1) + GRID_PADDING/2 + unitSize - GRID_PADDING,
        WHITE);       
                
        // Draw Left button
        drawList->AddRectFilled(cursorOffset + unitSize * ImVec2(1, 2) + GRID_PADDING/2,
        cursorOffset + unitSize * ImVec2(1, 2) + GRID_PADDING/2 + unitSize - GRID_PADDING,
        WHITE);  
  
        // Draw Right button
        drawList->AddRectFilled(cursorOffset + unitSize * ImVec2(3, 2) + GRID_PADDING/2,
        cursorOffset + unitSize * ImVec2(3, 2) + GRID_PADDING/2 + unitSize - GRID_PADDING,
        WHITE);
 

        // Draw Down button
        drawList->AddRectFilled(cursorOffset + unitSize * ImVec2(2, 3) + GRID_PADDING/2,
        cursorOffset + unitSize * ImVec2(2, 3) + GRID_PADDING/2 + unitSize - GRID_PADDING,
        WHITE);
 

        drawList->AddRectFilled(cursorOffset + unitSize * ImVec2(5, 3) + GRID_PADDING/2,
        cursorOffset + unitSize * ImVec2(5, 3) + GRID_PADDING/2 + unitSize - GRID_PADDING,
        WHITE);
   

        drawList->AddRectFilled(cursorOffset + unitSize * ImVec2(6, 3) + GRID_PADDING/2,
        cursorOffset + unitSize * ImVec2(6, 3) + GRID_PADDING/2 + unitSize - GRID_PADDING,
        WHITE);
 

        drawList->AddRectFilled(cursorOffset + unitSize * ImVec2(8, 3) + GRID_PADDING/2,
        cursorOffset + unitSize * ImVec2(8, 3) + GRID_PADDING/2 + unitSize - GRID_PADDING,
        WHITE);


        drawList->AddRectFilled(cursorOffset + unitSize * ImVec2(9, 3) + GRID_PADDING/2,
        cursorOffset + unitSize * ImVec2(9, 3) + GRID_PADDING/2 + unitSize - GRID_PADDING,
        WHITE);



}


public:

    SigSlot::Signal<> signalShow;
    SigSlot::Signal<> signalClose;
    

    void show(const std::string windowTitle, ControllerInfo& input) {

        m_windowTitle = windowTitle;

        if(m_inputInfo != nullptr) {
            stopCapture();
        }   

        m_inputInfo = &input;
        m_show = true;
        m_captureIndex = -1;
    }

    void hide() {
        m_windowTitle = "";
        m_show = false;
        stopCapture();
        m_inputInfo = nullptr;
    }

    void update() {    

        if(m_show != m_lastShow) {
            if(m_show) signalShow();
            else signalClose();

            m_lastShow = m_show;
        }

        if(!m_show) return;        

        if(m_captureState != NONE) {

            double time = getTime();
            double dt = time - m_lastTime;
            m_captureTime -= dt;
            m_lastTime = time;

            if(m_captureTime <= 0) {
                stopCapture();
            }
          
            InputManager::instance().updateInputs();
            auto capture = InputManager::instance().capture();

            switch(m_captureState) {

                case WAIT_EMPTY:
                    if(capture.size() == 0) m_captureState = WAIT_BUTTON;                    
                    break;

                case WAIT_BUTTON:
                    if(capture.size() > 0) {

                        if(capture[0] == "Escape") m_inputInfo->setByButtonIndex(m_captureIndex, "");
                        else m_inputInfo->setByButtonIndex(m_captureIndex, capture[0]);

                        if(m_captureIndex+1 < N_BUTTONS) startCapture(m_captureIndex+1);
                        else stopCapture();                      
                    }
                    break;

                default:
                    break;
            }
            
        }  

        ImGui::SetNextWindowSize(ImVec2(360, 0));
    
        if(ImGui::Begin((m_windowTitle).c_str(), &m_show)) {

            if(ImGui::BeginTable("Tabela", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders)){

                ImGui::TableSetupColumn("Button");
                ImGui::TableSetupColumn("Input");
                ImGui::TableHeadersRow();

                // Adicionar linhas a tabela
                for (int i = 0; i < N_BUTTONS; i++) {

                    ImGui::TableNextRow();

                    auto style = ImGui::GetStyle();
                    auto color = style.Colors[ImGuiCol_TabHovered];

                    if(m_captureState != NONE && i == m_captureIndex) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(color));
                        
                    ImGui::TableNextColumn();

                    // Coluna 1        
                    if(m_captureState == NONE) ImGui::Selectable(ControllerInfo::BUTTONS[i], selected[i], ImGuiSelectableFlags_SpanAllColumns);
                    else ImGui::Text("%s", ControllerInfo::BUTTONS[i]);

                    if (m_captureState == NONE && ImGui::IsItemActive()) {
                        startCapture(i);                    
                    }                    

                    ImGui::TableNextColumn();   
                
                    // Coluna 2
                    ImGui::Text("%s", m_inputInfo->getByButtonName(ControllerInfo::BUTTONS[i]).c_str());
                    
                    
                }


                // Finalizar a janela do ImGui
                ImGui::EndTable();
            }

            //NESControllerDraw();

            char aux[128];

            if (m_captureState != NONE) {            
                sprintf(aux, "Waiting input for button '%s'... (%0.1fs)", ControllerInfo::BUTTONS[m_captureIndex],std::max(0.0f, m_captureTime));
            }
            else sprintf(aux, "%s", "");
            ImGui::Text("%s", aux);     

            ImGui::SetWindowFocus(m_windowTitle.c_str());
        }
        ImGui::End();

    }
    
};
