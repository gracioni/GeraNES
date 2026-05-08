#include "GeraNESApp/TouchControls.h"

#include <cmrc/cmrc.hpp>

#include "imgui_include.h"

CMRC_DECLARE(resources);

void TouchControls::testDownButton(const yoga_raii::Node::Ptr& node, glm::vec2 point, const std::function<void()>& callback)
{
    if(node) {
        glm::vec2 min, max;
        node->getAbsoluteRect(min, max);
        if(pointInRect(point, Rect(min, max))) {
            callback();
        }
    }
}

void TouchControls::updateDigitalPad(SDL_FingerID eventFingerIndex, glm::vec2 point)
{
    float digitalPadThreshold = 0;

    if(glm::vec2 pixels; metersToPixels(0.005f, pixels, m_dpi)) {
        digitalPadThreshold = pixels.x;
    } else {
        return;
    }

    if(thumbIndex != kInvalidFingerId && eventFingerIndex == thumbIndex) {
        glm::vec2 dir = point - thumbCenter;

        if(glm::length(dir) > digitalPadThreshold) {
            glm::vec2 norm = glm::normalize(dir);
            float threshold = 0.5f;

            m_buttons.up = glm::dot(glm::vec2{0, -1}, norm) > threshold;
            m_buttons.right = glm::dot(glm::vec2{1, 0}, norm) > threshold;
            m_buttons.down = glm::dot(glm::vec2{0, 1}, norm) > threshold;
            m_buttons.left = glm::dot(glm::vec2{-1, 0}, norm) > threshold;
        } else {
            m_buttons.up = false;
            m_buttons.right = false;
            m_buttons.down = false;
            m_buttons.left = false;
        }
    }
}

TouchControls::TouchControls(int screenWidth, int screenHeight, glm::vec2 dpi)
{
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

void TouchControls::setMargin(glm::vec4 margin)
{
    m_margin = margin;
    if(m_root) {
        m_root->setPadding(margin);
        m_root->calculateLayout();
    }
}

void TouchControls::setTopMargin(float value)
{
    m_margin[0] = value;

    if(m_root) {
        m_root->setPadding(YGEdgeTop, value);
        m_root->calculateLayout();
    }
}

void TouchControls::createLayout()
{
    auto fs2 = cmrc::resources::get_filesystem();
    auto file = fs2.open("resources/touch-layout.json");
    std::string touchLayoutJsonStr(file.begin(), file.end());

    auto touchLayoutJson = json::parse(touchLayoutJsonStr);

    m_root = json_yoga::buildTree(touchLayoutJson, m_dpi);

    if(m_root) {
        m_root->setWidth(m_screenWidth);
        m_root->setHeight(m_screenHeight);
        m_root->calculateLayout();
        m_rewindNode = m_root->getById("main/top/rewind");
        m_digitalPadNode = m_root->getById("main/bottom/digital-pad/draw");
        m_selectNode = m_root->getById("main/bottom/mid/select/draw");
        m_startNode = m_root->getById("main/bottom/mid/start/draw");
        m_bNode = m_root->getById("main/bottom/right/b/draw");
        m_aNode = m_root->getById("main/bottom/right/a/draw");
    } else {
        m_rewindNode = nullptr;
        m_digitalPadNode = nullptr;
        m_selectNode = nullptr;
        m_startNode = nullptr;
        m_bNode = nullptr;
        m_aNode = nullptr;
    }
}

void TouchControls::onResize(int screenWidth, int screenHeight)
{
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    if(m_root) {
        m_root->setWidth(m_screenWidth);
        m_root->setHeight(m_screenHeight);
        m_root->calculateLayout();
    }
}

void TouchControls::onEvent(SDL_Event& ev)
{
    const bool emulateTouch = false;

    if(!AppSettings::instance().data.input.touchControls.enabled) {
        m_buttons.reset();
        return;
    }

    SDL_FingerID index = kInvalidFingerId;
    bool evtDown = false;
    bool evtUp = false;
    bool evtDrag = false;
    glm::vec2 point = {0, 0};

    if(emulateTouch && ev.type == SDL_MOUSEBUTTONDOWN) {
        evtDown = true;
        index = 0;
        point.x = ev.button.x;
        point.y = ev.button.y;
    } else if(emulateTouch && ev.type == SDL_MOUSEBUTTONUP) {
        evtUp = true;
        index = 0;
        point.x = ev.button.x;
        point.y = ev.button.y;
    } else if(emulateTouch && ev.type == SDL_MOUSEMOTION) {
        evtDrag = true;
        index = 0;
        point.x = ev.button.x;
        point.y = ev.button.y;
    } else if(ev.type == SDL_FINGERDOWN) {
        evtDown = true;
        index = ev.tfinger.fingerId;
        point.x = ev.tfinger.x * m_screenWidth;
        point.y = ev.tfinger.y * m_screenHeight;
    } else if(ev.type == SDL_FINGERUP) {
        evtUp = true;
        index = ev.tfinger.fingerId;
        point.x = ev.tfinger.x * m_screenWidth;
        point.y = ev.tfinger.y * m_screenHeight;
    } else if(ev.type == SDL_FINGERMOTION) {
        evtDrag = true;
        index = ev.tfinger.fingerId;
        point.x = ev.tfinger.x * m_screenWidth;
        point.y = ev.tfinger.y * m_screenHeight;
    }

    if(evtDown) {
        testDownButton(m_rewindNode, point, [&]() {
            m_buttons.rewind = true;
            rewindFingerId = index;
        });

        testDownButton(m_digitalPadNode, point, [&]() {
            thumbIndex = index;
            thumbCenter = m_digitalPadNode->getAbsoluteCenter();
            updateDigitalPad(index, point);
        });

        testDownButton(m_selectNode, point, [&]() {
            m_buttons.select = true;
            selectFingerId = index;
        });

        testDownButton(m_startNode, point, [&]() {
            m_buttons.start = true;
            startFingerId = index;
        });

        testDownButton(m_bNode, point, [&]() {
            m_buttons.b = true;
            bFingerId = index;
        });

        testDownButton(m_aNode, point, [&]() {
            m_buttons.a = true;
            aFingerId = index;
        });
    } else if(evtDrag) {
        updateDigitalPad(index, point);
    } else if(evtUp) {
        if(index == thumbIndex) {
            thumbIndex = kInvalidFingerId;
            m_buttons.up = false;
            m_buttons.right = false;
            m_buttons.down = false;
            m_buttons.left = false;
        } else if(index == selectFingerId) {
            selectFingerId = kInvalidFingerId;
            m_buttons.select = false;
        } else if(index == startFingerId) {
            startFingerId = kInvalidFingerId;
            m_buttons.start = false;
        } else if(index == aFingerId) {
            aFingerId = kInvalidFingerId;
            m_buttons.a = false;
        } else if(index == bFingerId) {
            bFingerId = kInvalidFingerId;
            m_buttons.b = false;
        } else if(index == rewindFingerId) {
            rewindFingerId = kInvalidFingerId;
            m_buttons.rewind = false;
        }
    }
}

const TouchControls::Buttons& TouchControls::buttons()
{
    return m_buttons;
}

void TouchControls::draw(ImDrawList* drawList, ImVec2 origin)
{
    if(!AppSettings::instance().data.input.touchControls.enabled) {
        return;
    }

    const int transparency = static_cast<int>(AppSettings::instance().data.input.touchControls.transparency * 255);
    const int opacity = 255 - transparency;

    if(m_rewindNode) {
        glm::vec2 min, max;
        m_rewindNode->getAbsoluteRect(min, max);
        drawList->AddImage(m_rewindTexture->id(),
                           ImVec2{origin.x + min.x, origin.y + min.y}, ImVec2{origin.x + max.x, origin.y + max.y}, ImVec2(0, 0), ImVec2(1, 1),
                           IM_COL32(255, 255, 255, opacity));
    }

    if(m_digitalPadNode) {
        glm::vec2 min, max;
        m_digitalPadNode->getAbsoluteRect(min, max);

        drawList->AddImage(m_digitalPagTexture->id(),
                           ImVec2{origin.x + min.x, origin.y + min.y}, ImVec2{origin.x + max.x, origin.y + max.y}, ImVec2(0, 0), ImVec2(1, 1),
                           IM_COL32(255, 255, 255, opacity));
    }

    if(m_selectNode) {
        glm::vec2 min, max;
        m_selectNode->getAbsoluteRect(min, max);
        drawList->AddImage(m_buttons.select ? m_midButtonPressedTexture->id() : m_midButtonTexture->id(),
                           ImVec2{origin.x + min.x, origin.y + min.y}, ImVec2{origin.x + max.x, origin.y + max.y}, ImVec2(0, 0), ImVec2(1, 1),
                           IM_COL32(255, 255, 255, opacity));
    }

    if(m_startNode) {
        glm::vec2 min, max;
        m_startNode->getAbsoluteRect(min, max);
        drawList->AddImage(m_buttons.start ? m_midButtonPressedTexture->id() : m_midButtonTexture->id(),
                           ImVec2{origin.x + min.x, origin.y + min.y}, ImVec2{origin.x + max.x, origin.y + max.y}, ImVec2(0, 0), ImVec2(1, 1),
                           IM_COL32(255, 255, 255, opacity));
    }

    if(m_bNode) {
        glm::vec2 min, max;
        m_bNode->getAbsoluteRect(min, max);
        drawList->AddImage(m_buttons.b ? m_rightButtonPressedTexture->id() : m_rightButtonTexture->id(),
                           ImVec2{origin.x + min.x, origin.y + min.y}, ImVec2{origin.x + max.x, origin.y + max.y}, ImVec2(0, 0), ImVec2(1, 1),
                           IM_COL32(255, 255, 255, opacity));
    }

    if(m_aNode) {
        glm::vec2 min, max;
        m_aNode->getAbsoluteRect(min, max);
        drawList->AddImage(m_buttons.a ? m_rightButtonPressedTexture->id() : m_rightButtonTexture->id(),
                           ImVec2{origin.x + min.x, origin.y + min.y}, ImVec2{origin.x + max.x, origin.y + max.y}, ImVec2(0, 0), ImVec2(1, 1),
                           IM_COL32(255, 255, 255, opacity));
    }
}

void TouchControls::update(Uint64 dt)
{
    (void)dt;
}
