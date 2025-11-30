#ifndef TOUCH_CONTROLS_H
#define TOUCH_CONTROLS_H

#include <map>

#include <SDL.h>
#include <glm/vec2.hpp>

#include "GeraNESApp/InputManager.h"
#include "InputInfo.h"

class TouchControls {

private:

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

public:

    TouchControls(InputInfo& controller1, int screenWidth, int screenHeight) : m_controller1(controller1){
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;
    }

    void onResize(int screenWidth, int screenHeight) {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;
    }

    void onEvent(SDL_Event& ev) {

        const float digitalPadThreshold = 16;

        const bool emulateTouch = false;

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

    

};

#endif
