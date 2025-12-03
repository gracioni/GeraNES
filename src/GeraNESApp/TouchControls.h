#ifndef TOUCH_CONTROLS_H
#define TOUCH_CONTROLS_H

#include <map>

#include <SDL.h>
#include <glm/vec2.hpp>

#include "GeraNESApp/InputManager.h"
#include "InputInfo.h"

#include "yoga_raii.hpp"
#include "json2yoga.hpp"

template <typename Map, typename Key>
static auto get_or_null(Map& map, const Key& key)
{
    auto it = map.find(key);
    return (it != map.end()) ? it->second : nullptr;
}

class TouchControls {

private:

    struct Rect {
        glm::vec2 min;
        glm::vec2 max;
    };

    int thumbIndex = -1;
    glm::vec2 thumbCenter;

    int aFingerId = -1;
    int bFingerId = -1;
    int selectFingerId = -1;
    int startFingerId = -1;
    int rewindFingerId = -1;

    InputInfo& m_controller1;

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
    };

    Buttons m_buttons;

    int m_screenWidth;
    int m_screenHeight;
    glm::vec2 m_dpi;

    std::map<std::string, Rect> touchButtons = {
        {"select", Rect{{0,0},{0,0}}}
    };

    yoga_raii::Node::Ptr m_root = nullptr;

    //yoga_raii::Node::Ptr m_m = nullptr;

    glm::vec4 m_margin = {0,0,0,0};

    json_yoga::IdMap idMap;

public:

    TouchControls(InputInfo& controller1, int screenWidth, int screenHeight, glm::vec2 dpi) : m_controller1(controller1){
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;
        m_dpi = dpi;        

        createLayout();
    }

    void setMargin(glm::vec4 margin) {
        m_margin = margin;
        // if(!m_m) return;
        // m_m->setMargin(YGEdgeTop, margin[0]);
        // m_m->setMargin(YGEdgeRight, margin[1]);
        // m_m->setMargin(YGEdgeBottom, margin[2]);
        // m_m->setMargin(YGEdgeLeft, margin[3]);
        // m_root->calculateLayout();      
    }

    void setTopMargin(float value) {
        m_margin[0] = value;
        
        // if(!m_m) return;
        // m_m->setMargin(YGEdgeTop, value);
        // m_root->calculateLayout();

        auto mainNode = get_or_null(idMap, "main");
        if(mainNode) {
            mainNode->setMargin(YGEdgeTop, value);            
        }

        auto root = get_or_null(idMap, "root");
        if(root) {
            root->calculateLayout();           
        }
    }

    void createLayout() {

        auto fs2 = cmrc::resources::get_filesystem();
        auto file = fs2.open("resources/touch-layout.json");
        std::string touchLayoutJsonStr(file.begin(), file.end());

        auto touchLayoutJson = json::parse(touchLayoutJsonStr);

        m_root = json_yoga::buildTree(touchLayoutJson, idMap);

        printf("buildTree returned root=%p\n", (void*)m_root.get());
for (auto &p : idMap) {
    printf("idMap['%s'] -> node=%p, raw=%p, children=%zu\n",
           p.first.c_str(), (void*)p.second.get(), (void*)p.second->raw(), p.second->childrenCount());
}

        auto root = get_or_null(idMap, "root");
        if(root) {
            root->setWidth(m_screenWidth);
            root->setHeight(m_screenHeight);
            root->calculateLayout();
        }

        // m_root = yoga_raii::Node::create();
        // m_root->setWidth(m_screenWidth);
        // m_root->setHeight(m_screenHeight);
        // m_root->setFlexDirection(YGFlexDirectionRow);
        // m_root->setJustifyContent(YGJustifyCenter);
        // m_root->setAlignItems(YGAlignCenter);

        // m_m = yoga_raii::Node::create();
        // m_m->setWidth(80);
        // m_m->setHeight(60);
        // setMargin(m_margin);
        
        // m_root->addChild(m_m);

        // m_root->calculateLayout();
    }

    void onResize(int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        // m_root->setWidth(m_screenWidth);
        // m_root->setHeight(m_screenHeight);
        // m_root->calculateLayout();

        auto root = get_or_null(idMap, "root");
        if(root) {
            root->setWidth(m_screenWidth);
            root->setHeight(m_screenHeight);
            root->calculateLayout();
        }
    }

    void onEvent(SDL_Event& ev) {

        const float digitalPadThreshold = 16;

        const bool emulateTouch = true;

        int index = 0;
        bool evtDown = false;
        bool evtUp = false;
        bool evtDrag = false;
        float x = 0;
        float y = 0;

        if(emulateTouch && ev.type == SDL_MOUSEBUTTONDOWN) {
            evtDown = true;
            x = ev.button.x;
            y = ev.button.y;
        }
        else if(emulateTouch && ev.type == SDL_MOUSEBUTTONUP) {
            evtUp = true;
            x = ev.button.x;
            y = ev.button.y;
        }
        else if(emulateTouch && ev.type == SDL_MOUSEMOTION) {
            evtDrag = true;
            x = ev.button.x;
            y = ev.button.y;
        }
        else if(ev.type == SDL_FINGERDOWN) {
            evtDown = true;
            index = ev.tfinger.fingerId;
            x = ev.tfinger.x * m_screenWidth;
            y = ev.tfinger.y * m_screenHeight;
        }
        else if(ev.type == SDL_FINGERUP) {
            evtUp = true;
            index = ev.tfinger.fingerId;
            x = ev.tfinger.x * m_screenWidth;
            y = ev.tfinger.y * m_screenHeight;
        }
        else if(ev.type == SDL_FINGERMOTION) {
            evtDrag = true;
            index = ev.tfinger.fingerId;
            x = ev.tfinger.x * m_screenWidth;
            y = ev.tfinger.y * m_screenHeight;
        }

        if(evtDown) {   

            if(y < m_screenHeight/5) {
                m_buttons.rewind = true;
                rewindFingerId = index;
            }
            else {

                if(x < 3*m_screenWidth/7 ) {
                    if(thumbIndex < 0) {
                        thumbIndex = index;
                        thumbCenter = glm::vec2{x, y};
                    }
                }
                else if(x < 4*m_screenWidth/7) {
                    m_buttons.select = true;
                    selectFingerId = index;
                }
                else if(x < 5*m_screenWidth/7) {
                    m_buttons.start = true;
                    startFingerId = index;
                }
                else if(x < 6*m_screenWidth/7) {
                    m_buttons.b = true;
                    bFingerId = index;
                }
                else if(x < 7*m_screenWidth/7) {
                    m_buttons.a = true;
                    aFingerId = index;
                }
            }            
        }
        else if(evtDrag) {          
            
            if (thumbIndex >= 0 && index == thumbIndex) {

                glm::vec2 dir = glm::vec2{x, y} - thumbCenter;              

                if(glm::length(dir) > digitalPadThreshold) {

                    glm::vec2 norm = glm::normalize(dir);

                    float threshold = 0.5;
                   				
                    m_buttons.up = glm::dot(glm::vec2{0,-1}, norm) > threshold;

                    m_buttons.right = glm::dot(glm::vec2{1,0}, norm) > threshold;

                    m_buttons.down = glm::dot(glm::vec2{0,1}, norm) > threshold;

                    m_buttons.left = glm::dot(glm::vec2{-1,0}, norm) > threshold;

                    thumbCenter = glm::vec2(x,y); //update center                 
                }            
            
            }
            
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

    Buttons& buttons() {
        return m_buttons;
    }

    void draw(ImDrawList* drawList)
    {
        // ImVec2 min = {m_m->getLayoutLeft(), m_m->getLayoutTop()};
        // ImVec2 max = {m_m->getLayoutRight(), m_m->getLayoutBottom()};
    
        // drawList->AddRect(min, max, IM_COL32(255, 0, 0, 255));

        auto bottomNode = get_or_null(idMap, "bottom");

        if(bottomNode) {
            ImVec2 min, max;
            bottomNode->getAbsoluteRect(min, max);        
            drawList->AddRect(min, max, IM_COL32(255, 255, 255, 255));
        }

        auto rewindNode = get_or_null(idMap, "rewind");

        if(rewindNode) {
            ImVec2 min, max;
            rewindNode->getAbsoluteRect(min, max);        
            drawList->AddRect(min, max, IM_COL32(0, 0, 255, 255));
        }

        auto selectNode = get_or_null(idMap, "select");

        if(selectNode) {
            ImVec2 min, max;
            selectNode->getAbsoluteRect(min, max);        
            drawList->AddRect(min, max, IM_COL32(255, 0, 0, 255));
        }

        auto digitalPadNode = get_or_null(idMap, "digital-pad");

        if(digitalPadNode) {
            ImVec2 min, max;
            digitalPadNode->getAbsoluteRect(min, max);        
            drawList->AddRect(min, max, IM_COL32(0, 255, 0, 255));
        }

        
    }

    

};

#endif
