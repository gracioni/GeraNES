#pragma once

#include <map>

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

class TouchControls {

private:

    int thumbIndex = -1;
    glm::vec2 thumbCenter;
    glm::vec2 fakeThumCenter;

    int aFingerId = -1;
    int bFingerId = -1;
    int selectFingerId = -1;
    int startFingerId = -1;
    int rewindFingerId = -1;

    ControllerInfo& m_controller1;

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

    std::map<std::string, Rect> touchButtons = {
        {"select", Rect{{0,0},{0,0}}}
    };

    yoga_raii::Node::Ptr m_root = nullptr;

    glm::vec4 m_margin = {0,0,0,0};

    std::shared_ptr<GLTexture> m_rewindTexture;
    std::shared_ptr<GLTexture> m_digitalPagTexture;
    std::shared_ptr<GLTexture> m_midButtonTexture;
    std::shared_ptr<GLTexture> m_midButtonPressedTexture;
    std::shared_ptr<GLTexture> m_rightButtonTexture;
    std::shared_ptr<GLTexture> m_rightButtonPressedTexture;

    void testDownButton(std::string_view id, glm::vec2 point, const std::function<void()>& callback) {
        auto node = m_root->getById(std::string(id));
            if(node) {
                glm::vec2 min, max;
                node->getAbsoluteRect(min, max);        
                if(pointInRect(point, Rect(min, max))) {
                    callback();
                }
            }
    }

    void updateDigitalPad(int eventFingerIndex, glm::vec2 point ) {

        float digitalPadThreshold = 0;
        
        if(glm::vec2 pixels; metersToPixels(0.005f, pixels, m_dpi)) {
            digitalPadThreshold = pixels.x;
        }
        else return;

        if (thumbIndex >= 0 && eventFingerIndex == thumbIndex) {

            glm::vec2 dir = point - thumbCenter;              

            if(glm::length(dir) > digitalPadThreshold) {

                glm::vec2 norm = glm::normalize(dir);

                float threshold = 0.5;
                            
                m_buttons.up = glm::dot(glm::vec2{0,-1}, norm) > threshold;

                m_buttons.right = glm::dot(glm::vec2{1,0}, norm) > threshold;

                m_buttons.down = glm::dot(glm::vec2{0,1}, norm) > threshold;

                m_buttons.left = glm::dot(glm::vec2{-1,0}, norm) > threshold;

                if(AppSettings::instance().data.input.touchControls.digitalPadMode == DigitaPadMode::Relative) {
                    thumbCenter = point; //update center
                }               
            }
            else {

                if(AppSettings::instance().data.input.touchControls.digitalPadMode != DigitaPadMode::Relative) {
                    m_buttons.up = false;
                    m_buttons.right = false;
                    m_buttons.down = false;
                    m_buttons.left = false;
                }
            }        
        
        }

    }

public:

    TouchControls(ControllerInfo& controller1, int screenWidth, int screenHeight, glm::vec2 dpi) : m_controller1(controller1){
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;
        m_dpi = dpi;        

        createLayout();

        auto fs2 = cmrc::resources::get_filesystem();

        {
            auto file = fs2.open("resources/rewind.png");
            m_rewindTexture = loadImageFromMemory(reinterpret_cast<const unsigned char*>(file.begin()), file.size());
        }
        {
            auto file = fs2.open("resources/digital-pad.png");
            m_digitalPagTexture = loadImageFromMemory(reinterpret_cast<const unsigned char*>(file.begin()), file.size());
        }
        {
            auto file = fs2.open("resources/mid-button.png");
            m_midButtonTexture = loadImageFromMemory(reinterpret_cast<const unsigned char*>(file.begin()), file.size());
        }
        {
            auto file = fs2.open("resources/mid-button-pressed.png");
            m_midButtonPressedTexture = loadImageFromMemory(reinterpret_cast<const unsigned char*>(file.begin()), file.size());
        }
        {
            auto file = fs2.open("resources/right-button.png");
            m_rightButtonTexture = loadImageFromMemory(reinterpret_cast<const unsigned char*>(file.begin()), file.size());
        }
        {
            auto file = fs2.open("resources/right-button-pressed.png");
            m_rightButtonPressedTexture = loadImageFromMemory(reinterpret_cast<const unsigned char*>(file.begin()), file.size());
        }
    }

    void setMargin(glm::vec4 margin) {
        m_margin = margin;
        if(m_root) {
            m_root->setPadding(margin);
            m_root->calculateLayout();  
        }       
    }

    void setTopMargin(float value) {
        m_margin[0] = value;      

        if(m_root) {
            m_root->setPadding(YGEdgeTop, value);        
            m_root->calculateLayout();           
        }
    }

    void createLayout() {

        auto fs2 = cmrc::resources::get_filesystem();
        auto file = fs2.open("resources/touch-layout.json");
        std::string touchLayoutJsonStr(file.begin(), file.end());

        auto touchLayoutJson = json::parse(touchLayoutJsonStr);

        m_root = json_yoga::buildTree(touchLayoutJson, m_dpi);
    
        if(m_root) {
            m_root->setWidth(m_screenWidth);
            m_root->setHeight(m_screenHeight);
            m_root->calculateLayout();
        }
    }

    void onResize(int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        if(m_root) {
            m_root->setWidth(m_screenWidth);
            m_root->setHeight(m_screenHeight);
            m_root->calculateLayout();
        }
    }

    void onEvent(SDL_Event& ev) {        

        const bool emulateTouch = false;

        if(!AppSettings::instance().data.input.touchControls.enabled) {
            m_buttons.reset();
            return;
        }

        int index = 0;
        bool evtDown = false;
        bool evtUp = false;
        bool evtDrag = false;
        glm::vec2 point = {0,0};

        if(emulateTouch && ev.type == SDL_MOUSEBUTTONDOWN) {
            evtDown = true;
            point.x = ev.button.x;
            point.y = ev.button.y;
        }
        else if(emulateTouch && ev.type == SDL_MOUSEBUTTONUP) {
            evtUp = true;
            point.x = ev.button.x;
            point.y = ev.button.y;
        }
        else if(emulateTouch && ev.type == SDL_MOUSEMOTION) {
            evtDrag = true;
            point.x = ev.button.x;
            point.y = ev.button.y;
        }
        else if(ev.type == SDL_FINGERDOWN) {
            evtDown = true;
            index = ev.tfinger.fingerId;
            point.x = ev.tfinger.x * m_screenWidth;
            point.y = ev.tfinger.y * m_screenHeight;
        }
        else if(ev.type == SDL_FINGERUP) {
            evtUp = true;
            index = ev.tfinger.fingerId;
            point.x = ev.tfinger.x * m_screenWidth;
            point.y = ev.tfinger.y * m_screenHeight;
        }
        else if(ev.type == SDL_FINGERMOTION) {
            evtDrag = true;
            index = ev.tfinger.fingerId;
            point.x = ev.tfinger.x * m_screenWidth;
            point.y = ev.tfinger.y * m_screenHeight;
        }

        if(evtDown) {
                        
            testDownButton("main/top/rewind", point, [&]() {
                m_buttons.rewind = true;
                rewindFingerId = index;
            });

            bool digitalPadAbsoluteMode = AppSettings::instance().data.input.touchControls.digitalPadMode == DigitaPadMode::Absolute;

            testDownButton(std::string("main/bottom/digital-pad") + (digitalPadAbsoluteMode ? "/draw" : ""), point, [&]() {
                thumbIndex = index;

                auto digitalPadMode = AppSettings::instance().data.input.touchControls.digitalPadMode;

                if(digitalPadMode == DigitaPadMode::CentralizeOnTouch || digitalPadMode == DigitaPadMode::Relative) {
                    thumbCenter = point;
                    fakeThumCenter = point;
                }
                else {
                    thumbCenter = m_root->getById("main/bottom/digital-pad/draw")->getAbsoluteCenter();
                }

                if(digitalPadAbsoluteMode) updateDigitalPad(index, point);
            });

            bool buttonsAbsoluteMode = AppSettings::instance().data.input.touchControls.buttonsMode == ButtonsMode::Absolute;

            testDownButton(std::string("main/bottom/mid/select") + (buttonsAbsoluteMode ? "/draw" : ""), point, [&]() {
                m_buttons.select = true;
                selectFingerId = index;
            });

            testDownButton(std::string("main/bottom/mid/start") + (buttonsAbsoluteMode ? "/draw" : ""), point, [&]() {
                m_buttons.start = true;
                startFingerId = index;
            });

            testDownButton(std::string("main/bottom/right/b") + (buttonsAbsoluteMode ? "/draw" : ""), point, [&]() {
                m_buttons.b = true;
                bFingerId = index;
            });

            testDownButton(std::string("main/bottom/right/a") + (buttonsAbsoluteMode ? "/draw" : ""), point, [&]() {
                m_buttons.a = true;
                aFingerId = index;
            });       
        }
        else if(evtDrag) {            
            updateDigitalPad(index, point);       
        }
        else if(evtUp) {

            if(index == thumbIndex) {
                thumbIndex = -1;
                m_buttons.up = false;
                m_buttons.right = false;
                m_buttons.down = false;
                m_buttons.left = false;
            }
            else if(index == selectFingerId) {
                selectFingerId = -1;
                m_buttons.select = false;
            }
            else if(index == startFingerId) {
                startFingerId = -1;
                m_buttons.start = false;
            }
            else if(index == aFingerId) {
                aFingerId = -1;
                m_buttons.a = false;
            }
            else if(index == bFingerId) {
                bFingerId = -1;
                m_buttons.b = false;
            }
            else if(index == rewindFingerId) {
                rewindFingerId = -1;
                m_buttons.rewind = false;
            }
        }      
        

    }    

    const Buttons& buttons() {
        return m_buttons;
    }

    void draw(ImDrawList* drawList)
    {
        if(!AppSettings::instance().data.input.touchControls.enabled) {
            return;
        }

        const int transparency = (int)((AppSettings::instance().data.input.touchControls.transparency+0.5f) * 255);

        auto rewindNode = m_root->getById("main/top/rewind");  
        if(rewindNode) {
            glm::vec2 min, max;
            rewindNode->getAbsoluteRect(min, max);        
            //drawList->AddRect(ImVec2{min.x,min.y}, ImVec2{max.x,max.y}, IM_COL32(255, 0, 0, 255));
            drawList->AddImage(m_rewindTexture->id(),
                ImVec2{min.x,min.y}, ImVec2{max.x,max.y}, ImVec2(0,0), ImVec2(1,1),
                IM_COL32(255, 255, 255, transparency));
        }

        auto digitalPadNode = m_root->getById("main/bottom/digital-pad/draw");
        if(digitalPadNode) {
            glm::vec2 min, max;
            digitalPadNode->getAbsoluteRect(min, max);        
            //drawList->AddRect(ImVec2{min.x,min.y}, ImVec2{max.x,max.y}, IM_COL32(0, 255, 0, 255));

            if(thumbIndex >= 0) {
                glm::vec2 center = digitalPadNode->getAbsoluteCenter();
                min -= center;
                max -= center;

                if(AppSettings::instance().data.input.touchControls.digitalPadMode == DigitaPadMode::Relative) {
                    min += fakeThumCenter;
                    max += fakeThumCenter;
                }
                else {
                    min += thumbCenter;
                    max += thumbCenter;
                }
            }

            drawList->AddImage(m_digitalPagTexture->id(),
                ImVec2{min.x,min.y}, ImVec2{max.x,max.y}, ImVec2(0,0), ImVec2(1,1),
                IM_COL32(255, 255, 255, 255-transparency));
        }

        auto selectNode = m_root->getById("main/bottom/mid/select/draw");
        if(selectNode) {
            glm::vec2 min, max;
            selectNode->getAbsoluteRect(min, max);        
            //drawList->AddRect(ImVec2{min.x,min.y}, ImVec2{max.x,max.y}, IM_COL32(255, 0, 0, 255));
            drawList->AddImage(m_buttons.select ? m_midButtonPressedTexture->id() : m_midButtonTexture->id(),
                ImVec2{min.x,min.y}, ImVec2{max.x,max.y}, ImVec2(0,0), ImVec2(1,1),
                IM_COL32(255, 255, 255, transparency));
        }

        auto startNode = m_root->getById("main/bottom/mid/start/draw");
        if(startNode) {
            glm::vec2 min, max;
            startNode->getAbsoluteRect(min, max);        
            //drawList->AddRect(ImVec2{min.x,min.y}, ImVec2{max.x,max.y}, IM_COL32(255, 0, 0, 255));
            drawList->AddImage(m_buttons.start ? m_midButtonPressedTexture->id() : m_midButtonTexture->id(),
                ImVec2{min.x,min.y}, ImVec2{max.x,max.y}, ImVec2(0,0), ImVec2(1,1),
                IM_COL32(255, 255, 255, transparency));
        }

        auto bNode = m_root->getById("main/bottom/right/b/draw");
        if(bNode) {
            glm::vec2 min, max;
            bNode->getAbsoluteRect(min, max);        
            //drawList->AddRect(ImVec2{min.x,min.y}, ImVec2{max.x,max.y}, IM_COL32(255, 0, 0, 255));
            drawList->AddImage(m_buttons.b ? m_rightButtonPressedTexture->id(): m_rightButtonTexture->id(),
                ImVec2{min.x,min.y}, ImVec2{max.x,max.y}, ImVec2(0,0), ImVec2(1,1),
                IM_COL32(255, 255, 255, transparency));
        }

        auto aNode = m_root->getById("main/bottom/right/a/draw");
        if(aNode) {
            glm::vec2 min, max;
            aNode->getAbsoluteRect(min, max);        
            //drawList->AddRect(ImVec2{min.x,min.y}, ImVec2{max.x,max.y}, IM_COL32(255, 0, 0, 255));
            drawList->AddImage(m_buttons.a ? m_rightButtonPressedTexture->id() : m_rightButtonTexture->id(),
                ImVec2{min.x,min.y}, ImVec2{max.x,max.y}, ImVec2(0,0), ImVec2(1,1),
                IM_COL32(255, 255, 255, transparency));
        }
    }

    void update(Uint64 dt) {
        fakeThumCenter = moveTowards(fakeThumCenter, thumbCenter, dt * 0.1f);
    }    

};
