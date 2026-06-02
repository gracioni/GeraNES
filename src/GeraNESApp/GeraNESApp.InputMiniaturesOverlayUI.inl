#pragma once

inline void GeraNESApp::drawInputMiniaturesOverlay(ImDrawList* drawList, const ImVec2& overlayOrigin)
{
    if(drawList == nullptr || !AppSettings::instance().data.input.showMiniatures || !m_emu.valid()) {
        return;
    }

    InputState state{};
    {
        std::scoped_lock selectedFrameLock(m_selectedInputFrameMutex);
        if(m_latestSelectedInputFrame.has_value()) {
            state = m_latestSelectedInputFrame->state;
        }
        state.topology = m_inputTopology;
    }

    const SDL_Rect clientArea = emulatorClientArea();
    const int overlayAlpha = 191;
    const ImU32 baseFill = IM_COL32(173, 158, 122, overlayAlpha);
    const ImU32 baseStroke = IM_COL32(60, 52, 40, overlayAlpha);
    const ImU32 baseText = IM_COL32(34, 30, 24, overlayAlpha);
    const ImU32 idleFill = IM_COL32(73, 67, 55, overlayAlpha);
    const ImU32 activeFill = IM_COL32(196, 44, 36, overlayAlpha);
    const float gap = 6.0f;
    const float inset = 16.0f;

    auto drawCardBase = [&](const ImVec2& min, const ImVec2& max, float rounding = 4.0f) {
        drawList->AddRectFilled(min, max, baseFill, rounding);
        drawList->AddRect(min, max, baseStroke, rounding, 0, 1.0f);
    };

    auto drawPressedRect = [&](const ImVec2& min, const ImVec2& max, bool pressed, float rounding = 1.0f) {
        drawList->AddRectFilled(min, max, pressed ? activeFill : idleFill, rounding);
    };

    auto drawPressedCircle = [&](const ImVec2& center, float radius, bool pressed) {
        drawList->AddCircleFilled(center, radius, pressed ? activeFill : idleFill, 18);
    };

    auto drawLabel = [&](const ImVec2& pos, const std::string& text) {
        drawList->AddText(pos, baseText, text.c_str());
    };

    auto drawDPad = [&](const ImVec2& origin, float unit, bool up, bool down, bool left, bool right) {
        drawPressedRect(ImVec2(origin.x + unit, origin.y), ImVec2(origin.x + unit * 2.0f, origin.y + unit), up);
        drawPressedRect(ImVec2(origin.x + unit, origin.y + unit * 2.0f), ImVec2(origin.x + unit * 2.0f, origin.y + unit * 3.0f), down);
        drawPressedRect(ImVec2(origin.x, origin.y + unit), ImVec2(origin.x + unit, origin.y + unit * 2.0f), left);
        drawPressedRect(ImVec2(origin.x + unit * 2.0f, origin.y + unit), ImVec2(origin.x + unit * 3.0f, origin.y + unit * 2.0f), right);
        drawPressedRect(ImVec2(origin.x + unit, origin.y + unit), ImVec2(origin.x + unit * 2.0f, origin.y + unit * 2.0f), false);
    };

    auto drawStandardPad = [&](const ImVec2& topLeft, const std::string& label, const InputState::PadButtons& buttons, bool famicomStyle) {
        const ImVec2 max(topLeft.x + 92.0f, topLeft.y + 42.0f);
        drawCardBase(topLeft, max);
        drawLabel(ImVec2(topLeft.x + 6.0f, topLeft.y + 3.0f), label + (famicomStyle ? " FC" : " NES"));
        drawDPad(ImVec2(topLeft.x + 9.0f, topLeft.y + 16.0f), 4.0f, buttons.up, buttons.down, buttons.left, buttons.right);
        drawPressedRect(ImVec2(topLeft.x + 37.0f, topLeft.y + 25.0f), ImVec2(topLeft.x + 44.0f, topLeft.y + 28.0f), buttons.select);
        drawPressedRect(ImVec2(topLeft.x + 47.0f, topLeft.y + 25.0f), ImVec2(topLeft.x + 54.0f, topLeft.y + 28.0f), buttons.start);
        drawPressedCircle(ImVec2(topLeft.x + 69.0f, topLeft.y + 24.0f), 3.6f, buttons.b);
        drawPressedCircle(ImVec2(topLeft.x + 79.0f, topLeft.y + 24.0f), 3.6f, buttons.a);
    };

    auto drawSnesPad = [&](const ImVec2& topLeft, const std::string& label, const InputState::PadButtons& buttons) {
        const ImVec2 max(topLeft.x + 108.0f, topLeft.y + 48.0f);
        drawCardBase(topLeft, max, 8.0f);
        drawLabel(ImVec2(topLeft.x + 7.0f, topLeft.y + 4.0f), label + " SNES");
        drawPressedRect(ImVec2(topLeft.x + 18.0f, topLeft.y + 8.0f), ImVec2(topLeft.x + 42.0f, topLeft.y + 12.0f), buttons.l, 2.0f);
        drawPressedRect(ImVec2(topLeft.x + 66.0f, topLeft.y + 8.0f), ImVec2(topLeft.x + 90.0f, topLeft.y + 12.0f), buttons.r, 2.0f);
        drawDPad(ImVec2(topLeft.x + 10.0f, topLeft.y + 23.0f), 5.0f, buttons.up, buttons.down, buttons.left, buttons.right);
        drawPressedRect(ImVec2(topLeft.x + 46.0f, topLeft.y + 30.0f), ImVec2(topLeft.x + 54.0f, topLeft.y + 33.0f), buttons.select);
        drawPressedRect(ImVec2(topLeft.x + 57.0f, topLeft.y + 30.0f), ImVec2(topLeft.x + 65.0f, topLeft.y + 33.0f), buttons.start);
        drawPressedCircle(ImVec2(topLeft.x + 97.0f, topLeft.y + 30.0f), 3.5f, buttons.a);
        drawPressedCircle(ImVec2(topLeft.x + 84.0f, topLeft.y + 30.0f), 3.5f, buttons.y);
        drawPressedCircle(ImVec2(topLeft.x + 91.0f, topLeft.y + 23.0f), 3.5f, buttons.x);
        drawPressedCircle(ImVec2(topLeft.x + 91.0f, topLeft.y + 37.0f), 3.5f, buttons.b);
    };

    auto drawVirtualBoyPad = [&](const ImVec2& topLeft, const std::string& label, const InputState::PadButtons& buttons) {
        const ImVec2 max(topLeft.x + 118.0f, topLeft.y + 48.0f);
        drawCardBase(topLeft, max, 10.0f);
        drawLabel(ImVec2(topLeft.x + 7.0f, topLeft.y + 4.0f), label + " VB");
        drawPressedRect(ImVec2(topLeft.x + 18.0f, topLeft.y + 8.0f), ImVec2(topLeft.x + 34.0f, topLeft.y + 12.0f), buttons.l, 2.0f);
        drawPressedRect(ImVec2(topLeft.x + 84.0f, topLeft.y + 8.0f), ImVec2(topLeft.x + 100.0f, topLeft.y + 12.0f), buttons.r, 2.0f);
        drawDPad(ImVec2(topLeft.x + 8.0f, topLeft.y + 20.0f), 4.5f, buttons.up, buttons.down, buttons.left, buttons.right);
        drawPressedRect(ImVec2(topLeft.x + 39.0f, topLeft.y + 28.0f), ImVec2(topLeft.x + 47.0f, topLeft.y + 31.0f), buttons.select);
        drawPressedRect(ImVec2(topLeft.x + 50.0f, topLeft.y + 28.0f), ImVec2(topLeft.x + 58.0f, topLeft.y + 31.0f), buttons.start);
        drawPressedCircle(ImVec2(topLeft.x + 69.0f, topLeft.y + 29.5f), 4.0f, buttons.b);
        drawPressedCircle(ImVec2(topLeft.x + 79.0f, topLeft.y + 29.5f), 4.0f, buttons.a);
        drawDPad(ImVec2(topLeft.x + 96.5f, topLeft.y + 20.0f), 4.5f, buttons.up2, buttons.down2, buttons.left2, buttons.right2);
    };

    auto drawZapper = [&](const ImVec2& topLeft, const std::string& label, const InputState::PointerState& pointer) {
        const ImVec2 max(topLeft.x + 100.0f, topLeft.y + 28.0f);
        drawCardBase(topLeft, max, 6.0f);
        drawLabel(ImVec2(topLeft.x + 6.0f, topLeft.y + 5.0f), label);
        drawList->AddRectFilled(ImVec2(topLeft.x + 38.0f, topLeft.y + 10.0f), ImVec2(topLeft.x + 84.0f, topLeft.y + 18.0f), idleFill, 2.0f);
        drawList->AddRectFilled(ImVec2(topLeft.x + 84.0f, topLeft.y + 11.5f), ImVec2(topLeft.x + 94.0f, topLeft.y + 16.5f), idleFill, 1.0f);
        drawPressedRect(ImVec2(topLeft.x + 56.0f, topLeft.y + 18.0f), ImVec2(topLeft.x + 64.0f, topLeft.y + 24.0f), pointer.trigger, 1.0f);
    };

    auto drawMouse = [&](const ImVec2& topLeft, const std::string& label, const InputState::RelativePointerState& mouse, bool subor) {
        const ImVec2 max(topLeft.x + 84.0f, topLeft.y + 38.0f);
        drawCardBase(topLeft, max, 10.0f);
        drawLabel(ImVec2(topLeft.x + 6.0f, topLeft.y + 3.0f), label + (subor ? " SUB" : " SNES"));
        drawPressedRect(ImVec2(topLeft.x + 23.0f, topLeft.y + 13.0f), ImVec2(topLeft.x + 61.0f, topLeft.y + 33.0f), false, 8.0f);
        drawPressedRect(ImVec2(topLeft.x + 25.0f, topLeft.y + 15.0f), ImVec2(topLeft.x + 41.0f, topLeft.y + 23.0f), mouse.primary, 3.0f);
        drawPressedRect(ImVec2(topLeft.x + 43.0f, topLeft.y + 15.0f), ImVec2(topLeft.x + 59.0f, topLeft.y + 23.0f), mouse.secondary, 3.0f);
        drawPressedRect(ImVec2(topLeft.x + 41.0f, topLeft.y + 15.0f), ImVec2(topLeft.x + 43.0f, topLeft.y + 24.0f), false, 1.0f);
    };

    auto drawArkanoid = [&](const ImVec2& topLeft, const std::string& label, const InputState::ArkanoidState& arkanoid) {
        const ImVec2 max(topLeft.x + 96.0f, topLeft.y + 34.0f);
        drawCardBase(topLeft, max, 10.0f);
        drawLabel(ImVec2(topLeft.x + 6.0f, topLeft.y + 3.0f), label);
        drawPressedRect(ImVec2(topLeft.x + 12.0f, topLeft.y + 19.0f), ImVec2(topLeft.x + 74.0f, topLeft.y + 22.0f), false, 1.0f);
        const float knobX = topLeft.x + 14.0f + std::clamp(arkanoid.position, 0.0f, 1.0f) * 58.0f;
        drawPressedCircle(ImVec2(knobX, topLeft.y + 20.5f), 5.0f, false);
        drawPressedCircle(ImVec2(topLeft.x + 84.0f, topLeft.y + 21.0f), 5.0f, arkanoid.button);
    };

    auto drawPowerPad = [&](const ImVec2& topLeft, const std::string& label, const std::array<bool, 12>& buttons, bool sideB) {
        const ImVec2 max(topLeft.x + 92.0f, topLeft.y + 54.0f);
        drawCardBase(topLeft, max, 4.0f);
        drawLabel(ImVec2(topLeft.x + 6.0f, topLeft.y + 3.0f), label + (sideB ? " B" : " A"));
        static constexpr std::array<const char*, 12> names = {"1","2","3","4","5","6","7","8","9","10","11","12"};
        for(int row = 0; row < 3; ++row) {
            for(int col = 0; col < 4; ++col) {
                const int index = row * 4 + col;
                const ImVec2 cellMin(topLeft.x + 8.0f + static_cast<float>(col) * 19.0f, topLeft.y + 17.0f + static_cast<float>(row) * 11.0f);
                const ImVec2 cellMax(cellMin.x + 16.0f, cellMin.y + 8.0f);
                drawPressedRect(cellMin, cellMax, buttons[static_cast<size_t>(index)], 1.0f);
                drawList->AddText(ImVec2(cellMin.x + 2.0f, cellMin.y - 1.0f), baseText, names[static_cast<size_t>(index)]);
            }
        }
    };

    auto drawBandai = [&](const ImVec2& topLeft, const InputState::PadButtons& buttons, const InputState::PointerState& pointer) {
        const ImVec2 max(topLeft.x + 116.0f, topLeft.y + 46.0f);
        drawCardBase(topLeft, max, 6.0f);
        drawLabel(ImVec2(topLeft.x + 6.0f, topLeft.y + 3.0f), "Bandai Hyper Shot");
        drawDPad(ImVec2(topLeft.x + 8.0f, topLeft.y + 18.0f), 4.0f, buttons.up, buttons.down, buttons.left, buttons.right);
        drawPressedCircle(ImVec2(topLeft.x + 36.0f, topLeft.y + 30.0f), 3.6f, buttons.b);
        drawPressedCircle(ImVec2(topLeft.x + 36.0f, topLeft.y + 20.0f), 3.6f, buttons.a);
        drawList->AddRectFilled(ImVec2(topLeft.x + 56.0f, topLeft.y + 20.0f), ImVec2(topLeft.x + 99.0f, topLeft.y + 27.0f), idleFill, 2.0f);
        drawList->AddRectFilled(ImVec2(topLeft.x + 99.0f, topLeft.y + 21.5f), ImVec2(topLeft.x + 108.0f, topLeft.y + 25.5f), idleFill, 1.0f);
        drawPressedRect(ImVec2(topLeft.x + 73.0f, topLeft.y + 27.0f), ImVec2(topLeft.x + 80.0f, topLeft.y + 33.0f), pointer.trigger, 1.0f);
    };

    auto drawKonami = [&](const ImVec2& topLeft, const InputState::KonamiHyperShotState& konami) {
        const ImVec2 max(topLeft.x + 116.0f, topLeft.y + 42.0f);
        drawCardBase(topLeft, max, 4.0f);
        drawLabel(ImVec2(topLeft.x + 6.0f, topLeft.y + 3.0f), "Konami Hyper Shot");
        drawLabel(ImVec2(topLeft.x + 10.0f, topLeft.y + 17.0f), "P1");
        drawPressedRect(ImVec2(topLeft.x + 28.0f, topLeft.y + 16.0f), ImVec2(topLeft.x + 50.0f, topLeft.y + 26.0f), konami.p1Run, 2.0f);
        drawPressedRect(ImVec2(topLeft.x + 53.0f, topLeft.y + 16.0f), ImVec2(topLeft.x + 75.0f, topLeft.y + 26.0f), konami.p1Jump, 2.0f);
        drawLabel(ImVec2(topLeft.x + 81.0f, topLeft.y + 17.0f), "P2");
        drawPressedRect(ImVec2(topLeft.x + 28.0f, topLeft.y + 29.0f), ImVec2(topLeft.x + 50.0f, topLeft.y + 39.0f), konami.p2Run, 2.0f);
        drawPressedRect(ImVec2(topLeft.x + 53.0f, topLeft.y + 29.0f), ImVec2(topLeft.x + 75.0f, topLeft.y + 39.0f), konami.p2Jump, 2.0f);
    };

    auto drawKeyboardGrid = [&](const ImVec2& topLeft, const std::string& label, const auto& keys, int columns, int rows) {
        const float cellW = 7.0f;
        const float cellH = 5.0f;
        const float width = 10.0f + static_cast<float>(columns) * cellW;
        const float height = 16.0f + static_cast<float>(rows) * cellH;
        const ImVec2 max(topLeft.x + width, topLeft.y + height);
        drawCardBase(topLeft, max, 4.0f);
        drawLabel(ImVec2(topLeft.x + 6.0f, topLeft.y + 3.0f), label);
        size_t pressedCount = 0;
        for(bool key : keys) {
            if(key) ++pressedCount;
        }
        drawLabel(ImVec2(max.x - 28.0f, topLeft.y + 3.0f), std::to_string(pressedCount));
        for(int row = 0; row < rows; ++row) {
            for(int col = 0; col < columns; ++col) {
                const size_t index = static_cast<size_t>(row * columns + col);
                if(index >= keys.size()) {
                    continue;
                }
                const ImVec2 cellMin(topLeft.x + 5.0f + static_cast<float>(col) * cellW, topLeft.y + 12.0f + static_cast<float>(row) * cellH);
                const ImVec2 cellMax(cellMin.x + cellW - 1.0f, cellMin.y + cellH - 1.0f);
                drawPressedRect(cellMin, cellMax, keys[index], 1.0f);
            }
        }
    };

    struct MiniatureItem {
        float width = 0.0f;
        float height = 0.0f;
        std::function<void(const ImVec2&)> draw;
    };

    std::vector<MiniatureItem> items;
    auto addItem = [&](float width, float height, std::function<void(const ImVec2&)> draw) {
        items.push_back({width, height, std::move(draw)});
    };

    if(state.multitapActive()) {
        addItem(92.0f, 42.0f, [=](const ImVec2& pos) { drawStandardPad(pos, "P1", state.portButtons(1), false); });
        addItem(92.0f, 42.0f, [=](const ImVec2& pos) { drawStandardPad(pos, "P2", state.portButtons(2), false); });
        addItem(92.0f, 42.0f, [=](const ImVec2& pos) { drawStandardPad(pos, "P3", state.portButtons(3), false); });
        addItem(92.0f, 42.0f, [=](const ImVec2& pos) { drawStandardPad(pos, "P4", state.portButtons(4), false); });
    } else {
        switch(state.topology.port1Device) {
            case Settings::Device::CONTROLLER:
                addItem(92.0f, 42.0f, [=](const ImVec2& pos) { drawStandardPad(pos, "P1", state.portButtons(1), false); });
                break;
            case Settings::Device::FAMICOM_CONTROLLER:
                addItem(92.0f, 42.0f, [=](const ImVec2& pos) { drawStandardPad(pos, "P1", state.portButtons(1), true); });
                break;
            case Settings::Device::SNES_CONTROLLER:
                addItem(108.0f, 48.0f, [=](const ImVec2& pos) { drawSnesPad(pos, "P1", state.portButtons(1)); });
                break;
            case Settings::Device::VIRTUAL_BOY_CONTROLLER:
                addItem(118.0f, 48.0f, [=](const ImVec2& pos) { drawVirtualBoyPad(pos, "P1", state.portButtons(1)); });
                break;
            case Settings::Device::ZAPPER:
                addItem(100.0f, 28.0f, [=](const ImVec2& pos) { drawZapper(pos, "P1 Zapper", state.zapper(1)); });
                break;
            case Settings::Device::ARKANOID_CONTROLLER:
                addItem(96.0f, 34.0f, [=](const ImVec2& pos) { drawArkanoid(pos, "P1 Ark", state.arkanoidController(1)); });
                break;
            case Settings::Device::SNES_MOUSE:
                addItem(84.0f, 38.0f, [=](const ImVec2& pos) { drawMouse(pos, "P1", state.snesMouse(1), false); });
                break;
            case Settings::Device::SUBOR_MOUSE:
                addItem(84.0f, 38.0f, [=](const ImVec2& pos) { drawMouse(pos, "P1", state.suborMouse(1), true); });
                break;
            case Settings::Device::POWER_PAD_SIDE_A:
                addItem(92.0f, 54.0f, [=](const ImVec2& pos) { drawPowerPad(pos, "P1 Pad", state.powerPadButtons(1), false); });
                break;
            case Settings::Device::POWER_PAD_SIDE_B:
                addItem(92.0f, 54.0f, [=](const ImVec2& pos) { drawPowerPad(pos, "P1 Pad", state.powerPadButtons(1), true); });
                break;
            case Settings::Device::BANDAI_HYPERSHOT:
            case Settings::Device::NONE:
                break;
        }

        switch(state.topology.port2Device) {
            case Settings::Device::CONTROLLER:
                addItem(92.0f, 42.0f, [=](const ImVec2& pos) { drawStandardPad(pos, "P2", state.portButtons(2), false); });
                break;
            case Settings::Device::FAMICOM_CONTROLLER:
                addItem(92.0f, 42.0f, [=](const ImVec2& pos) { drawStandardPad(pos, "P2", state.portButtons(2), true); });
                break;
            case Settings::Device::SNES_CONTROLLER:
                addItem(108.0f, 48.0f, [=](const ImVec2& pos) { drawSnesPad(pos, "P2", state.portButtons(2)); });
                break;
            case Settings::Device::VIRTUAL_BOY_CONTROLLER:
                addItem(118.0f, 48.0f, [=](const ImVec2& pos) { drawVirtualBoyPad(pos, "P2", state.portButtons(2)); });
                break;
            case Settings::Device::ZAPPER:
                addItem(100.0f, 28.0f, [=](const ImVec2& pos) { drawZapper(pos, "P2 Zapper", state.zapper(2)); });
                break;
            case Settings::Device::ARKANOID_CONTROLLER:
                addItem(96.0f, 34.0f, [=](const ImVec2& pos) { drawArkanoid(pos, "P2 Ark", state.arkanoidController(2)); });
                break;
            case Settings::Device::SNES_MOUSE:
                addItem(84.0f, 38.0f, [=](const ImVec2& pos) { drawMouse(pos, "P2", state.snesMouse(2), false); });
                break;
            case Settings::Device::SUBOR_MOUSE:
                addItem(84.0f, 38.0f, [=](const ImVec2& pos) { drawMouse(pos, "P2", state.suborMouse(2), true); });
                break;
            case Settings::Device::POWER_PAD_SIDE_A:
                addItem(92.0f, 54.0f, [=](const ImVec2& pos) { drawPowerPad(pos, "P2 Pad", state.powerPadButtons(2), false); });
                break;
            case Settings::Device::POWER_PAD_SIDE_B:
                addItem(92.0f, 54.0f, [=](const ImVec2& pos) { drawPowerPad(pos, "P2 Pad", state.powerPadButtons(2), true); });
                break;
            case Settings::Device::BANDAI_HYPERSHOT:
            case Settings::Device::NONE:
                break;
        }

        switch(state.topology.expansionDevice) {
            case Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM:
                addItem(92.0f, 42.0f, [=](const ImVec2& pos) { drawStandardPad(pos, "EXP", state.portButtons(3), true); });
                break;
            case Settings::ExpansionDevice::BANDAI_HYPERSHOT:
                addItem(116.0f, 46.0f, [=](const ImVec2& pos) { drawBandai(pos, state.bandaiButtons(), state.bandaiPointer()); });
                break;
            case Settings::ExpansionDevice::KONAMI_HYPERSHOT:
                addItem(116.0f, 42.0f, [=](const ImVec2& pos) { drawKonami(pos, state.konamiHyperShot()); });
                break;
            case Settings::ExpansionDevice::ARKANOID_CONTROLLER:
                addItem(96.0f, 34.0f, [=](const ImVec2& pos) { drawArkanoid(pos, "EXP Ark", state.arkanoidExpansion()); });
                break;
            case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A:
                addItem(92.0f, 54.0f, [=](const ImVec2& pos) { drawPowerPad(pos, "Trainer", state.powerPadButtons(1), false); });
                break;
            case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B:
                addItem(92.0f, 54.0f, [=](const ImVec2& pos) { drawPowerPad(pos, "Trainer", state.powerPadButtons(1), true); });
                break;
            case Settings::ExpansionDevice::SUBOR_KEYBOARD:
                addItem(87.0f, 61.0f, [=](const ImVec2& pos) { drawKeyboardGrid(pos, "Subor", state.suborKeyboardKeys(), 11, 9); });
                break;
            case Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD:
                addItem(94.0f, 46.0f, [=](const ImVec2& pos) { drawKeyboardGrid(pos, "Basic", state.familyBasicKeyboardKeys(), 12, 6); });
                break;
            case Settings::ExpansionDevice::NONE:
                break;
        }
    }

    if(items.empty()) {
        return;
    }

    const float availableLayoutWidth = std::max(120.0f, static_cast<float>(clientArea.w) - inset * 2.0f);
    const float multitapRowWidth = 92.0f * 4.0f + gap * 3.0f;
    const float layoutWidthCap = state.multitapActive() ? multitapRowWidth : 320.0f;
    const float maxLayoutWidth = std::max(120.0f, std::min(layoutWidthCap, availableLayoutWidth));
    std::vector<std::vector<size_t>> rows;
    std::vector<float> rowWidths;
    std::vector<float> rowHeights;
    std::vector<size_t> currentRow;
    float currentWidth = 0.0f;
    float currentHeight = 0.0f;

    for(size_t index = 0; index < items.size(); ++index) {
        const MiniatureItem& item = items[index];
        const float proposedWidth = currentRow.empty() ? item.width : currentWidth + gap + item.width;
        if(!currentRow.empty() && proposedWidth > maxLayoutWidth) {
            rows.push_back(currentRow);
            rowWidths.push_back(currentWidth);
            rowHeights.push_back(currentHeight);
            currentRow.clear();
            currentWidth = 0.0f;
            currentHeight = 0.0f;
        }
        currentRow.push_back(index);
        currentWidth = currentRow.size() == 1 ? item.width : currentWidth + gap + item.width;
        currentHeight = std::max(currentHeight, item.height);
    }

    if(!currentRow.empty()) {
        rows.push_back(currentRow);
        rowWidths.push_back(currentWidth);
        rowHeights.push_back(currentHeight);
    }

    float panelWidth = 0.0f;
    float panelHeight = 0.0f;
    for(size_t row = 0; row < rows.size(); ++row) {
        panelWidth = std::max(panelWidth, rowWidths[row]);
        panelHeight += rowHeights[row];
        if(row + 1 < rows.size()) {
            panelHeight += gap;
        }
    }

    const ImVec2 panelMin(
        overlayOrigin.x + static_cast<float>(clientArea.x + clientArea.w) - panelWidth - inset,
        overlayOrigin.y + static_cast<float>(clientArea.y + clientArea.h) - panelHeight - inset
    );

    float cursorY = panelMin.y;
    for(size_t row = 0; row < rows.size(); ++row) {
        float cursorX = panelMin.x + (panelWidth - rowWidths[row]);
        for(size_t itemIndex : rows[row]) {
            items[itemIndex].draw(ImVec2(cursorX, cursorY));
            cursorX += items[itemIndex].width + gap;
        }
        cursorY += rowHeights[row] + gap;
    }
}
