#pragma once

inline void GeraNESApp::drawInputMiniaturesOverlay(ImDrawList* drawList, const ImVec2& overlayOrigin)
{
    if(drawList == nullptr || !AppSettings::instance().data.input.showMiniatures || !m_emu.valid()) {
        return;
    }

    InputState state{};
    bool stateLoaded = false;
    const auto netplayMenu = GeraNESNetplay::menuSnapshot(m_netplayRuntime);
    const bool preferQueuedNetplayFrame = netplayMenu.inputManaged;
    const auto hasRoomAssignment = [&](ConsoleNetplay::PlayerSlot slot) {
        if(!netplayMenu.inputManaged) {
            return true;
        }
        return std::find(netplayMenu.roomAssignments.begin(),
                         netplayMenu.roomAssignments.end(),
                         slot) != netplayMenu.roomAssignments.end();
    };
    if(preferQueuedNetplayFrame) {
        std::scoped_lock queuedFrameLock(m_queuedInputFrameMutex);
        if(m_latestQueuedInputFrame.has_value()) {
            state = m_latestQueuedInputFrame->state;
            stateLoaded = true;
        }
    }
    {
        std::scoped_lock selectedFrameLock(m_selectedInputFrameMutex);
        if(!stateLoaded && m_latestSelectedInputFrame.has_value()) {
            state = m_latestSelectedInputFrame->state;
            stateLoaded = true;
        }
    }

    if(!stateLoaded) {
        if(netplayMenu.inputManaged) {
            const bool roomUsesMultitap =
                hasRoomAssignment(GeraNESNetplay::kMultitapP1PlayerSlot) ||
                hasRoomAssignment(GeraNESNetplay::kMultitapP2PlayerSlot) ||
                hasRoomAssignment(GeraNESNetplay::kMultitapP3PlayerSlot) ||
                hasRoomAssignment(GeraNESNetplay::kMultitapP4PlayerSlot);
            state.topology.port1Device =
                hasRoomAssignment(GeraNESNetplay::kPort1PlayerSlot)
                    ? netplayMenu.port1Device.value_or(Settings::Device::NONE)
                    : Settings::Device::NONE;
            state.topology.port2Device =
                hasRoomAssignment(GeraNESNetplay::kPort2PlayerSlot)
                    ? netplayMenu.port2Device.value_or(Settings::Device::NONE)
                    : Settings::Device::NONE;
            state.topology.expansionDevice =
                hasRoomAssignment(GeraNESNetplay::kExpansionPlayerSlot)
                    ? netplayMenu.expansionDevice
                    : Settings::ExpansionDevice::NONE;
            state.topology.nesMultitapDevice =
                roomUsesMultitap ? netplayMenu.nesMultitapDevice : Settings::NesMultitapDevice::NONE;
            state.topology.famicomMultitapDevice =
                roomUsesMultitap ? netplayMenu.famicomMultitapDevice : Settings::FamicomMultitapDevice::NONE;
        } else {
            state.topology = m_inputTopology;
        }
    }

    const SDL_Rect clientArea = emulatorClientArea();
    const int overlayAlpha = 191;
    const ImU32 baseFill = IM_COL32(173, 158, 122, overlayAlpha);
    const ImU32 baseStroke = IM_COL32(60, 52, 40, overlayAlpha);
    const ImU32 baseText = IM_COL32(34, 30, 24, overlayAlpha);
    const ImU32 idleFill = IM_COL32(73, 67, 55, overlayAlpha);
    const ImU32 activeFill = IM_COL32(196, 44, 36, overlayAlpha);
    float overlayScale = 1.0f;
#ifdef __ANDROID__
    overlayScale = EffectiveUiScale();
#endif
    const auto S = [&](float value) { return value * overlayScale; };
    const float gap = S(6.0f);
    const float inset = S(16.0f);

    auto drawCardBase = [&](const ImVec2& min, const ImVec2& max, float rounding = 4.0f) {
        drawList->AddRectFilled(min, max, baseFill, rounding);
        drawList->AddRect(min, max, baseStroke, rounding, 0, std::max(1.0f, S(1.0f)));
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
        const ImVec2 max(topLeft.x + S(92.0f), topLeft.y + S(42.0f));
        drawCardBase(topLeft, max, S(4.0f));
        drawLabel(ImVec2(topLeft.x + S(6.0f), topLeft.y + S(3.0f)), label + (famicomStyle ? " FC" : " NES"));
        drawDPad(ImVec2(topLeft.x + S(9.0f), topLeft.y + S(16.0f)), S(4.0f), buttons.up, buttons.down, buttons.left, buttons.right);
        drawPressedRect(ImVec2(topLeft.x + S(37.0f), topLeft.y + S(25.0f)), ImVec2(topLeft.x + S(44.0f), topLeft.y + S(28.0f)), buttons.select);
        drawPressedRect(ImVec2(topLeft.x + S(47.0f), topLeft.y + S(25.0f)), ImVec2(topLeft.x + S(54.0f), topLeft.y + S(28.0f)), buttons.start);
        drawPressedCircle(ImVec2(topLeft.x + S(69.0f), topLeft.y + S(24.0f)), S(3.6f), buttons.b);
        drawPressedCircle(ImVec2(topLeft.x + S(79.0f), topLeft.y + S(24.0f)), S(3.6f), buttons.a);
    };

    auto drawSnesPad = [&](const ImVec2& topLeft, const std::string& label, const InputState::PadButtons& buttons) {
        const ImVec2 max(topLeft.x + S(108.0f), topLeft.y + S(48.0f));
        drawCardBase(topLeft, max, S(8.0f));
        drawLabel(ImVec2(topLeft.x + S(7.0f), topLeft.y + S(4.0f)), label + " SNES");
        drawPressedRect(ImVec2(topLeft.x + S(18.0f), topLeft.y + S(8.0f)), ImVec2(topLeft.x + S(42.0f), topLeft.y + S(12.0f)), buttons.l, S(2.0f));
        drawPressedRect(ImVec2(topLeft.x + S(66.0f), topLeft.y + S(8.0f)), ImVec2(topLeft.x + S(90.0f), topLeft.y + S(12.0f)), buttons.r, S(2.0f));
        drawDPad(ImVec2(topLeft.x + S(10.0f), topLeft.y + S(23.0f)), S(5.0f), buttons.up, buttons.down, buttons.left, buttons.right);
        drawPressedRect(ImVec2(topLeft.x + S(46.0f), topLeft.y + S(30.0f)), ImVec2(topLeft.x + S(54.0f), topLeft.y + S(33.0f)), buttons.select);
        drawPressedRect(ImVec2(topLeft.x + S(57.0f), topLeft.y + S(30.0f)), ImVec2(topLeft.x + S(65.0f), topLeft.y + S(33.0f)), buttons.start);
        drawPressedCircle(ImVec2(topLeft.x + S(97.0f), topLeft.y + S(30.0f)), S(3.5f), buttons.a);
        drawPressedCircle(ImVec2(topLeft.x + S(84.0f), topLeft.y + S(30.0f)), S(3.5f), buttons.y);
        drawPressedCircle(ImVec2(topLeft.x + S(91.0f), topLeft.y + S(23.0f)), S(3.5f), buttons.x);
        drawPressedCircle(ImVec2(topLeft.x + S(91.0f), topLeft.y + S(37.0f)), S(3.5f), buttons.b);
    };

    auto drawVirtualBoyPad = [&](const ImVec2& topLeft, const std::string& label, const InputState::PadButtons& buttons) {
        const ImVec2 max(topLeft.x + S(118.0f), topLeft.y + S(48.0f));
        drawCardBase(topLeft, max, S(10.0f));
        drawLabel(ImVec2(topLeft.x + S(7.0f), topLeft.y + S(4.0f)), label + " VB");
        drawPressedRect(ImVec2(topLeft.x + S(18.0f), topLeft.y + S(8.0f)), ImVec2(topLeft.x + S(34.0f), topLeft.y + S(12.0f)), buttons.l, S(2.0f));
        drawPressedRect(ImVec2(topLeft.x + S(84.0f), topLeft.y + S(8.0f)), ImVec2(topLeft.x + S(100.0f), topLeft.y + S(12.0f)), buttons.r, S(2.0f));
        drawDPad(ImVec2(topLeft.x + S(8.0f), topLeft.y + S(20.0f)), S(4.5f), buttons.up, buttons.down, buttons.left, buttons.right);
        drawPressedRect(ImVec2(topLeft.x + S(39.0f), topLeft.y + S(28.0f)), ImVec2(topLeft.x + S(47.0f), topLeft.y + S(31.0f)), buttons.select);
        drawPressedRect(ImVec2(topLeft.x + S(50.0f), topLeft.y + S(28.0f)), ImVec2(topLeft.x + S(58.0f), topLeft.y + S(31.0f)), buttons.start);
        drawPressedCircle(ImVec2(topLeft.x + S(69.0f), topLeft.y + S(29.5f)), S(4.0f), buttons.b);
        drawPressedCircle(ImVec2(topLeft.x + S(79.0f), topLeft.y + S(29.5f)), S(4.0f), buttons.a);
        drawDPad(ImVec2(topLeft.x + S(96.5f), topLeft.y + S(20.0f)), S(4.5f), buttons.up2, buttons.down2, buttons.left2, buttons.right2);
    };

    auto drawZapper = [&](const ImVec2& topLeft, const std::string& label, const InputState::PointerState& pointer) {
        const ImVec2 max(topLeft.x + S(100.0f), topLeft.y + S(28.0f));
        drawCardBase(topLeft, max, S(6.0f));
        drawLabel(ImVec2(topLeft.x + S(6.0f), topLeft.y + S(5.0f)), label);
        drawList->AddRectFilled(ImVec2(topLeft.x + S(38.0f), topLeft.y + S(10.0f)), ImVec2(topLeft.x + S(84.0f), topLeft.y + S(18.0f)), idleFill, S(2.0f));
        drawList->AddRectFilled(ImVec2(topLeft.x + S(84.0f), topLeft.y + S(11.5f)), ImVec2(topLeft.x + S(94.0f), topLeft.y + S(16.5f)), idleFill, S(1.0f));
        drawPressedRect(ImVec2(topLeft.x + S(56.0f), topLeft.y + S(18.0f)), ImVec2(topLeft.x + S(64.0f), topLeft.y + S(24.0f)), pointer.trigger, S(1.0f));
    };

    auto drawMouse = [&](const ImVec2& topLeft, const std::string& label, const InputState::RelativePointerState& mouse, bool subor) {
        const ImVec2 max(topLeft.x + S(84.0f), topLeft.y + S(38.0f));
        drawCardBase(topLeft, max, S(10.0f));
        drawLabel(ImVec2(topLeft.x + S(6.0f), topLeft.y + S(3.0f)), label + (subor ? " SUB" : " SNES"));
        drawPressedRect(ImVec2(topLeft.x + S(23.0f), topLeft.y + S(13.0f)), ImVec2(topLeft.x + S(61.0f), topLeft.y + S(33.0f)), false, S(8.0f));
        drawPressedRect(ImVec2(topLeft.x + S(25.0f), topLeft.y + S(15.0f)), ImVec2(topLeft.x + S(41.0f), topLeft.y + S(23.0f)), mouse.primary, S(3.0f));
        drawPressedRect(ImVec2(topLeft.x + S(43.0f), topLeft.y + S(15.0f)), ImVec2(topLeft.x + S(59.0f), topLeft.y + S(23.0f)), mouse.secondary, S(3.0f));
        drawPressedRect(ImVec2(topLeft.x + S(41.0f), topLeft.y + S(15.0f)), ImVec2(topLeft.x + S(43.0f), topLeft.y + S(24.0f)), false, S(1.0f));
    };

    auto drawArkanoid = [&](const ImVec2& topLeft, const std::string& label, const InputState::ArkanoidState& arkanoid) {
        const ImVec2 max(topLeft.x + S(96.0f), topLeft.y + S(34.0f));
        drawCardBase(topLeft, max, S(10.0f));
        drawLabel(ImVec2(topLeft.x + S(6.0f), topLeft.y + S(3.0f)), label);
        drawPressedRect(ImVec2(topLeft.x + S(12.0f), topLeft.y + S(19.0f)), ImVec2(topLeft.x + S(74.0f), topLeft.y + S(22.0f)), false, S(1.0f));
        const float knobX = topLeft.x + S(14.0f) + std::clamp(arkanoid.position, 0.0f, 1.0f) * S(58.0f);
        drawPressedCircle(ImVec2(knobX, topLeft.y + S(20.5f)), S(5.0f), false);
        drawPressedCircle(ImVec2(topLeft.x + S(84.0f), topLeft.y + S(21.0f)), S(5.0f), arkanoid.button);
    };

    auto drawPowerPad = [&](const ImVec2& topLeft, const std::string& label, const std::array<bool, 12>& buttons, bool sideB) {
        const ImVec2 max(topLeft.x + S(92.0f), topLeft.y + S(54.0f));
        drawCardBase(topLeft, max, S(4.0f));
        drawLabel(ImVec2(topLeft.x + S(6.0f), topLeft.y + S(3.0f)), label + (sideB ? " B" : " A"));
        for(int row = 0; row < 3; ++row) {
            for(int col = 0; col < 4; ++col) {
                const int index = row * 4 + col;
                const ImVec2 cellMin(topLeft.x + S(8.0f) + static_cast<float>(col) * S(19.0f), topLeft.y + S(17.0f) + static_cast<float>(row) * S(11.0f));
                const ImVec2 cellMax(cellMin.x + S(16.0f), cellMin.y + S(8.0f));
                drawPressedRect(cellMin, cellMax, buttons[static_cast<size_t>(index)], S(1.0f));
            }
        }
    };

    auto drawBandai = [&](const ImVec2& topLeft, const InputState::PadButtons& buttons, const InputState::PointerState& pointer) {
        const ImVec2 max(topLeft.x + S(116.0f), topLeft.y + S(46.0f));
        drawCardBase(topLeft, max, S(6.0f));
        drawLabel(ImVec2(topLeft.x + S(6.0f), topLeft.y + S(3.0f)), "Bandai Hyper Shot");
        drawDPad(ImVec2(topLeft.x + S(8.0f), topLeft.y + S(18.0f)), S(4.0f), buttons.up, buttons.down, buttons.left, buttons.right);
        drawPressedCircle(ImVec2(topLeft.x + S(36.0f), topLeft.y + S(30.0f)), S(3.6f), buttons.b);
        drawPressedCircle(ImVec2(topLeft.x + S(36.0f), topLeft.y + S(20.0f)), S(3.6f), buttons.a);
        drawList->AddRectFilled(ImVec2(topLeft.x + S(56.0f), topLeft.y + S(20.0f)), ImVec2(topLeft.x + S(99.0f), topLeft.y + S(27.0f)), idleFill, S(2.0f));
        drawList->AddRectFilled(ImVec2(topLeft.x + S(99.0f), topLeft.y + S(21.5f)), ImVec2(topLeft.x + S(108.0f), topLeft.y + S(25.5f)), idleFill, S(1.0f));
        drawPressedRect(ImVec2(topLeft.x + S(73.0f), topLeft.y + S(27.0f)), ImVec2(topLeft.x + S(80.0f), topLeft.y + S(33.0f)), pointer.trigger, S(1.0f));
    };

    auto drawKonami = [&](const ImVec2& topLeft, const InputState::KonamiHyperShotState& konami) {
        const ImVec2 max(topLeft.x + S(116.0f), topLeft.y + S(42.0f));
        drawCardBase(topLeft, max, S(4.0f));
        drawLabel(ImVec2(topLeft.x + S(6.0f), topLeft.y + S(3.0f)), "Konami Hyper Shot");
        drawLabel(ImVec2(topLeft.x + S(10.0f), topLeft.y + S(17.0f)), "P1");
        drawPressedRect(ImVec2(topLeft.x + S(28.0f), topLeft.y + S(16.0f)), ImVec2(topLeft.x + S(50.0f), topLeft.y + S(26.0f)), konami.p1Run, S(2.0f));
        drawPressedRect(ImVec2(topLeft.x + S(53.0f), topLeft.y + S(16.0f)), ImVec2(topLeft.x + S(75.0f), topLeft.y + S(26.0f)), konami.p1Jump, S(2.0f));
        drawLabel(ImVec2(topLeft.x + S(81.0f), topLeft.y + S(17.0f)), "P2");
        drawPressedRect(ImVec2(topLeft.x + S(28.0f), topLeft.y + S(29.0f)), ImVec2(topLeft.x + S(50.0f), topLeft.y + S(39.0f)), konami.p2Run, S(2.0f));
        drawPressedRect(ImVec2(topLeft.x + S(53.0f), topLeft.y + S(29.0f)), ImVec2(topLeft.x + S(75.0f), topLeft.y + S(39.0f)), konami.p2Jump, S(2.0f));
    };

    auto drawKeyboardGrid = [&](const ImVec2& topLeft, const std::string& label, const auto& keys, int columns, int rows) {
        const float cellW = S(7.0f);
        const float cellH = S(5.0f);
        const float width = S(10.0f) + static_cast<float>(columns) * cellW;
        const float height = S(16.0f) + static_cast<float>(rows) * cellH;
        const ImVec2 max(topLeft.x + width, topLeft.y + height);
        drawCardBase(topLeft, max, S(4.0f));
        drawLabel(ImVec2(topLeft.x + S(6.0f), topLeft.y + S(3.0f)), label);
        size_t pressedCount = 0;
        for(bool key : keys) {
            if(key) ++pressedCount;
        }
        drawLabel(ImVec2(max.x - S(28.0f), topLeft.y + S(3.0f)), std::to_string(pressedCount));
        for(int row = 0; row < rows; ++row) {
            for(int col = 0; col < columns; ++col) {
                const size_t index = static_cast<size_t>(row * columns + col);
                if(index >= keys.size()) {
                    continue;
                }
                const ImVec2 cellMin(topLeft.x + S(5.0f) + static_cast<float>(col) * cellW, topLeft.y + S(12.0f) + static_cast<float>(row) * cellH);
                const ImVec2 cellMax(cellMin.x + cellW - S(1.0f), cellMin.y + cellH - S(1.0f));
                drawPressedRect(cellMin, cellMax, keys[index], S(1.0f));
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
        if(hasRoomAssignment(GeraNESNetplay::kMultitapP1PlayerSlot)) {
            addItem(S(92.0f), S(42.0f), [=](const ImVec2& pos) { drawStandardPad(pos, "P1", state.portButtons(1), false); });
        }
        if(hasRoomAssignment(GeraNESNetplay::kMultitapP2PlayerSlot)) {
            addItem(S(92.0f), S(42.0f), [=](const ImVec2& pos) { drawStandardPad(pos, "P2", state.portButtons(2), false); });
        }
        if(hasRoomAssignment(GeraNESNetplay::kMultitapP3PlayerSlot)) {
            addItem(S(92.0f), S(42.0f), [=](const ImVec2& pos) { drawStandardPad(pos, "P3", state.portButtons(3), false); });
        }
        if(hasRoomAssignment(GeraNESNetplay::kMultitapP4PlayerSlot)) {
            addItem(S(92.0f), S(42.0f), [=](const ImVec2& pos) { drawStandardPad(pos, "P4", state.portButtons(4), false); });
        }
    } else {
        if(hasRoomAssignment(GeraNESNetplay::kPort1PlayerSlot)) {
            switch(state.topology.port1Device) {
            case Settings::Device::CONTROLLER:
                addItem(S(92.0f), S(42.0f), [=](const ImVec2& pos) { drawStandardPad(pos, "P1", state.portButtons(1), false); });
                break;
            case Settings::Device::FAMICOM_CONTROLLER:
                addItem(S(92.0f), S(42.0f), [=](const ImVec2& pos) { drawStandardPad(pos, "P1", state.portButtons(1), true); });
                break;
            case Settings::Device::SNES_CONTROLLER:
                addItem(S(108.0f), S(48.0f), [=](const ImVec2& pos) { drawSnesPad(pos, "P1", state.portButtons(1)); });
                break;
            case Settings::Device::VIRTUAL_BOY_CONTROLLER:
                addItem(S(118.0f), S(48.0f), [=](const ImVec2& pos) { drawVirtualBoyPad(pos, "P1", state.portButtons(1)); });
                break;
            case Settings::Device::ZAPPER:
                addItem(S(100.0f), S(28.0f), [=](const ImVec2& pos) { drawZapper(pos, "P1 Zapper", state.zapper(1)); });
                break;
            case Settings::Device::ARKANOID_CONTROLLER:
                addItem(S(96.0f), S(34.0f), [=](const ImVec2& pos) { drawArkanoid(pos, "P1 Ark", state.arkanoidController(1)); });
                break;
            case Settings::Device::SNES_MOUSE:
                addItem(S(84.0f), S(38.0f), [=](const ImVec2& pos) { drawMouse(pos, "P1", state.snesMouse(1), false); });
                break;
            case Settings::Device::SUBOR_MOUSE:
                addItem(S(84.0f), S(38.0f), [=](const ImVec2& pos) { drawMouse(pos, "P1", state.suborMouse(1), true); });
                break;
            case Settings::Device::POWER_PAD_SIDE_A:
                addItem(S(92.0f), S(54.0f), [=](const ImVec2& pos) { drawPowerPad(pos, "P1 Pad", state.powerPadButtons(1), false); });
                break;
            case Settings::Device::POWER_PAD_SIDE_B:
                addItem(S(92.0f), S(54.0f), [=](const ImVec2& pos) { drawPowerPad(pos, "P1 Pad", state.powerPadButtons(1), true); });
                break;
            case Settings::Device::BANDAI_HYPERSHOT:
            case Settings::Device::NONE:
                break;
            }
        }

        if(hasRoomAssignment(GeraNESNetplay::kPort2PlayerSlot)) {
            switch(state.topology.port2Device) {
            case Settings::Device::CONTROLLER:
                addItem(S(92.0f), S(42.0f), [=](const ImVec2& pos) { drawStandardPad(pos, "P2", state.portButtons(2), false); });
                break;
            case Settings::Device::FAMICOM_CONTROLLER:
                addItem(S(92.0f), S(42.0f), [=](const ImVec2& pos) { drawStandardPad(pos, "P2", state.portButtons(2), true); });
                break;
            case Settings::Device::SNES_CONTROLLER:
                addItem(S(108.0f), S(48.0f), [=](const ImVec2& pos) { drawSnesPad(pos, "P2", state.portButtons(2)); });
                break;
            case Settings::Device::VIRTUAL_BOY_CONTROLLER:
                addItem(S(118.0f), S(48.0f), [=](const ImVec2& pos) { drawVirtualBoyPad(pos, "P2", state.portButtons(2)); });
                break;
            case Settings::Device::ZAPPER:
                addItem(S(100.0f), S(28.0f), [=](const ImVec2& pos) { drawZapper(pos, "P2 Zapper", state.zapper(2)); });
                break;
            case Settings::Device::ARKANOID_CONTROLLER:
                addItem(S(96.0f), S(34.0f), [=](const ImVec2& pos) { drawArkanoid(pos, "P2 Ark", state.arkanoidController(2)); });
                break;
            case Settings::Device::SNES_MOUSE:
                addItem(S(84.0f), S(38.0f), [=](const ImVec2& pos) { drawMouse(pos, "P2", state.snesMouse(2), false); });
                break;
            case Settings::Device::SUBOR_MOUSE:
                addItem(S(84.0f), S(38.0f), [=](const ImVec2& pos) { drawMouse(pos, "P2", state.suborMouse(2), true); });
                break;
            case Settings::Device::POWER_PAD_SIDE_A:
                addItem(S(92.0f), S(54.0f), [=](const ImVec2& pos) { drawPowerPad(pos, "P2 Pad", state.powerPadButtons(2), false); });
                break;
            case Settings::Device::POWER_PAD_SIDE_B:
                addItem(S(92.0f), S(54.0f), [=](const ImVec2& pos) { drawPowerPad(pos, "P2 Pad", state.powerPadButtons(2), true); });
                break;
            case Settings::Device::BANDAI_HYPERSHOT:
            case Settings::Device::NONE:
                break;
            }
        }

        if(hasRoomAssignment(GeraNESNetplay::kExpansionPlayerSlot)) {
            switch(state.topology.expansionDevice) {
            case Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM:
                addItem(S(92.0f), S(42.0f), [=](const ImVec2& pos) { drawStandardPad(pos, "EXP", state.portButtons(3), true); });
                break;
            case Settings::ExpansionDevice::BANDAI_HYPERSHOT:
                addItem(S(116.0f), S(46.0f), [=](const ImVec2& pos) { drawBandai(pos, state.bandaiButtons(), state.bandaiPointer()); });
                break;
            case Settings::ExpansionDevice::KONAMI_HYPERSHOT:
                addItem(S(116.0f), S(42.0f), [=](const ImVec2& pos) { drawKonami(pos, state.konamiHyperShot()); });
                break;
            case Settings::ExpansionDevice::ARKANOID_CONTROLLER:
                addItem(S(96.0f), S(34.0f), [=](const ImVec2& pos) { drawArkanoid(pos, "EXP Ark", state.arkanoidExpansion()); });
                break;
            case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A:
                addItem(S(92.0f), S(54.0f), [=](const ImVec2& pos) { drawPowerPad(pos, "Trainer", state.powerPadButtons(1), false); });
                break;
            case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B:
                addItem(S(92.0f), S(54.0f), [=](const ImVec2& pos) { drawPowerPad(pos, "Trainer", state.powerPadButtons(1), true); });
                break;
            case Settings::ExpansionDevice::SUBOR_KEYBOARD:
                addItem(S(87.0f), S(61.0f), [=](const ImVec2& pos) { drawKeyboardGrid(pos, "Subor", state.suborKeyboardKeys(), 11, 9); });
                break;
            case Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD:
                addItem(S(94.0f), S(46.0f), [=](const ImVec2& pos) { drawKeyboardGrid(pos, "Basic", state.familyBasicKeyboardKeys(), 12, 6); });
                break;
            case Settings::ExpansionDevice::NONE:
                break;
            }
        }
    }

    if(items.empty()) {
        return;
    }

    const float availableLayoutWidth = std::max(S(120.0f), static_cast<float>(clientArea.w) - inset * 2.0f);
    const float multitapRowWidth = S(92.0f) * 4.0f + gap * 3.0f;
    const float layoutWidthCap = state.multitapActive() ? multitapRowWidth : S(320.0f);
    const float maxLayoutWidth = std::max(S(120.0f), std::min(layoutWidthCap, availableLayoutWidth));
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
