#pragma once

inline void GeraNESApp::drawCpuDebuggerWindow()
{
    SetNextWindowCenteredOnMainViewport(ImVec2(860.0f, 620.0f));

    if(!ImGui::Begin("CPU Debugger", &m_showCpuDebuggerWindow, ImGuiWindowFlags_MenuBar)) {
        AppSettings::instance().data.debug.showCpuDebugger = m_showCpuDebuggerWindow;
        m_cpuDebuggerFocused = false;
        ImGui::End();
        if(!m_showCpuDebuggerWindow) {
            disableCpuDebugging();
        }
        return;
    }

    AppSettings::instance().data.debug.showCpuDebugger = m_showCpuDebuggerWindow;
    m_cpuDebuggerFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    m_imGuiWindowFocusBlocksEmulator |= m_cpuDebuggerFocused;

    const bool hasRomLoaded = m_emu.valid();
    const auto netplaySnapshot = m_netplayRuntime.uiSnapshot();
    const bool debugBlockedByNetplay =
        netplaySnapshot.active ||
        netplaySnapshot.hosting ||
        netplaySnapshot.connected ||
        netplaySnapshot.reconnecting;

    if(!hasRomLoaded) {
        ImGui::TextDisabled("Load a ROM to inspect CPU state.");
        ImGui::End();
        if(!m_showCpuDebuggerWindow) {
            disableCpuDebugging();
        }
        return;
    }

    if(debugBlockedByNetplay) {
        ImGui::TextDisabled("Opening the CPU debugger disconnects the current netplay session.");
        ImGui::End();
        if(!m_showCpuDebuggerWindow) {
            disableCpuDebugging();
        }
        return;
    }

    if(!AppSettings::instance().data.debug.cpuDebuggerEnabled) {
        requestEnableCpuDebugger();
    }

    if(!AppSettings::instance().data.debug.cpuDebuggerEnabled) {
        ImGui::TextDisabled("Waiting for CPU debugger to become available.");
        ImGui::End();
        if(!m_showCpuDebuggerWindow) {
            disableCpuDebugging();
        }
        return;
    }

    auto clearCpuDebuggerSymbols = [&]() {
        m_cpuDebugSymbols.clear();
        m_cpuDebugSymbolsPath.clear();
        m_cpuDebugSymbolsStatus = "CPU symbols cleared.";
        m_cpuDebuggerSymbolSearch[0] = '\0';
        Logger::instance().log(m_cpuDebugSymbolsStatus, Logger::Type::USER);
    };

    CPU2A03::DebugState cpuState;
    GeraNESEmu::ExecutionPoint execPoint;
    GeraNESEmu::DebugBreakpointHit breakpointHit;
    std::array<uint8_t, 0x10000> cpuMemorySnapshot = {};
    m_emu.withExclusiveAccess([&](auto& emu) {
        cpuState = emu.getConsole().cpu().debugState();
        execPoint = emu.executionPoint();
        breakpointHit = emu.debugBreakpointHit();
        for(size_t i = 0; i < cpuMemorySnapshot.size(); ++i) {
            cpuMemorySnapshot[i] = emu.debugPeekCpuMemory(static_cast<uint16_t>(i));
        }
    });
    const bool paused = m_emu.paused();
    auto toggleCpuDebuggerPause = [&]() {
        m_cpuDebuggerAutoPaused = false;
        m_emu.withExclusiveAccess([&](auto& emu) {
            emu.clearDebugBreakpointHit();
            emu.setPaused(!paused);
        });
    };
    auto stepCpuDebugger = [&]() {
        if(!paused) return;
        m_emu.withExclusiveAccess([](auto& emu) {
            emu.clearDebugBreakpointHit();
            emu.debugStepInstruction();
        });
    };

    if(m_cpuDebuggerFollowPc && m_cpuDebuggerViewAddress != cpuState.pc) {
        m_cpuDebuggerViewAddress = cpuState.pc;
        m_cpuDebuggerScrollToViewAddress = true;
    }
    auto navigateCpuDebuggerTo = [&](uint16_t address, bool followPc) {
        if(m_cpuDebuggerHistory.empty()) {
            m_cpuDebuggerHistory.push_back(m_cpuDebuggerViewAddress);
            m_cpuDebuggerHistoryIndex = 0;
        }
        if(m_cpuDebuggerHistoryIndex + 1 < m_cpuDebuggerHistory.size()) {
            m_cpuDebuggerHistory.erase(m_cpuDebuggerHistory.begin() + static_cast<std::ptrdiff_t>(m_cpuDebuggerHistoryIndex + 1), m_cpuDebuggerHistory.end());
        }
        if(m_cpuDebuggerHistory.back() != address) {
            m_cpuDebuggerHistory.push_back(address);
            m_cpuDebuggerHistoryIndex = m_cpuDebuggerHistory.size() - 1;
        }
        m_cpuDebuggerViewAddress = address;
        m_cpuDebuggerFollowPc = followPc;
        m_cpuDebuggerScrollToViewAddress = true;
    };

    auto readCpuSnapshot = [&](uint16_t addr) {
        return cpuMemorySnapshot[addr];
    };
    auto symbolForAddress = [&](uint16_t addr) -> std::string {
        const auto symbol = m_cpuDebugSymbols.find(addr);
        return symbol != m_cpuDebugSymbols.end() ? symbol->second.name : std::string();
    };
    auto symbolSearchMatches = [](std::string_view name, std::string_view query) {
        if(query.empty()) return false;
        auto toLowerAscii = [](char c) {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        };
        for(size_t i = 0; i + query.size() <= name.size(); ++i) {
            bool matches = true;
            for(size_t j = 0; j < query.size(); ++j) {
                if(toLowerAscii(name[i + j]) != toLowerAscii(query[j])) {
                    matches = false;
                    break;
                }
            }
            if(matches) return true;
        }
        return false;
    };
    auto findNextCpuSymbolOccurrence = [&]() -> std::optional<uint16_t> {
        const std::string_view query(m_cpuDebuggerSymbolSearch.data());
        if(query.empty()) return std::nullopt;

        auto rowMatches = [&](uint16_t address) {
            const auto symbolAtAddress = m_cpuDebugSymbols.find(address);
            if(symbolAtAddress != m_cpuDebugSymbols.end() && symbolSearchMatches(symbolAtAddress->second.name, query)) {
                return true;
            }

            const CPU2A03DebugLine line = CPU2A03Debug::disassembleAt(
                address,
                cpuState.pc,
                readCpuSnapshot,
                symbolForAddress
            );
            return !line.operandSymbol.empty() && symbolSearchMatches(line.operandSymbol, query);
        };

        for(uint32_t address = static_cast<uint32_t>(m_cpuDebuggerViewAddress) + 1u; address <= 0xFFFFu; ++address) {
            if(rowMatches(static_cast<uint16_t>(address))) {
                return static_cast<uint16_t>(address);
            }
        }
        for(uint32_t address = 0; address <= static_cast<uint32_t>(m_cpuDebuggerViewAddress); ++address) {
            if(rowMatches(static_cast<uint16_t>(address))) {
                return static_cast<uint16_t>(address);
            }
        }
        return std::nullopt;
    };
    auto findNextCpuSymbol = [&]() {
        if(m_cpuDebugSymbols.empty() || m_cpuDebuggerSymbolSearch[0] == '\0') return;
        if(const auto nextMatch = findNextCpuSymbolOccurrence(); nextMatch.has_value()) {
            navigateCpuDebuggerTo(*nextMatch, false);
        }
    };
    auto formatCpuDebuggerAsmLine = [&](uint16_t address) {
        const CPU2A03DebugLine line = CPU2A03Debug::disassembleAt(address, cpuState.pc, readCpuSnapshot, symbolForAddress);
        const auto symbolAtLine = m_cpuDebugSymbols.find(line.address);
        if(symbolAtLine != m_cpuDebugSymbols.end() && symbolAtLine->second.kind == CpuDebugSymbolKind::Data) {
            std::ostringstream stream;
            stream << symbolAtLine->second.name << ": .db $";
            stream << std::uppercase << std::hex;
            stream.width(2);
            stream.fill('0');
            stream << static_cast<unsigned int>(readCpuSnapshot(line.address));
            return stream.str();
        }
        if(symbolAtLine != m_cpuDebugSymbols.end()) {
            return symbolAtLine->second.name + ":\n    " + line.mnemonic;
        }
        return std::string("    ") + line.mnemonic;
    };
    auto copySelectedCpuDebuggerLine = [&]() {
        if(!m_cpuDebuggerHasSelection) return;
        const uint16_t firstAddress = std::min(m_cpuDebuggerSelectionAnchor, m_cpuDebuggerSelectedAddress);
        const uint16_t lastAddress = std::max(m_cpuDebuggerSelectionAnchor, m_cpuDebuggerSelectedAddress);
        std::ostringstream text;
        for(uint32_t address = firstAddress; address <= lastAddress; ++address) {
            if(address > firstAddress) text << '\n';
            text << formatCpuDebuggerAsmLine(static_cast<uint16_t>(address));
        }
        const std::string copyText = text.str();
#ifdef __EMSCRIPTEN__
        emcriptenCopyTextToClipboardExact(copyText.c_str());
#else
        ImGui::SetClipboardText(copyText.c_str());
#endif
    };
    if((m_cpuDebuggerFocused || m_cpuBreakpointsFocused) && !ImGui::GetIO().WantTextInput) {
        if(ImGui::IsKeyPressed(ImGuiKey_F9, false)) {
            toggleCpuDebuggerPause();
        }
        if(ImGui::IsKeyPressed(ImGuiKey_F8, false)) {
            stepCpuDebugger();
        }
        if(ImGui::IsKeyPressed(ImGuiKey_F3, false)) {
            findNextCpuSymbol();
        }
        if(m_cpuDebuggerHasSelection && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
            copySelectedCpuDebuggerLine();
        }
    }
    auto symbolKindLabel = [](CpuDebugSymbolKind kind) {
        switch(kind) {
            case CpuDebugSymbolKind::Function: return "Function";
            case CpuDebugSymbolKind::Label: return "Label";
            case CpuDebugSymbolKind::Data: return "Data";
            default: return "Unknown";
        }
    };
    auto showSymbolTooltip = [&](const CpuDebugSymbol& symbol, uint16_t address) {
        ImGui::BeginTooltip();
        ImGui::Text("Symbol: %s", symbol.name.c_str());
        ImGui::Text("Kind: %s", symbolKindLabel(symbol.kind));
        ImGui::Text("CPU address: $%04X", static_cast<unsigned int>(address));
        if(!m_cpuDebugSymbolsPath.empty()) {
            ImGui::Text("File: %s", fs::path(m_cpuDebugSymbolsPath).filename().string().c_str());
        }
        ImGui::EndTooltip();
    };
    const ImVec4 dataSymbolColor = ImVec4(0.0f, 0.36f, 0.90f, 1.0f);
    const ImVec4 labelReferenceColor = ImVec4(0.82f, 0.08f, 0.08f, 1.0f);
    auto calcCpuDebuggerTextWidth = [](const std::string& text) {
        const std::string measured = text + ".";
        return ImGui::CalcTextSize(measured.c_str()).x - ImGui::CalcTextSize(".").x;
    };
    auto cpuDebuggerText = [&](ImDrawList* drawList, ImVec2 pos, ImU32 color, const std::string& text) {
        drawList->AddText(pos, color, text.c_str());
        pos.x += calcCpuDebuggerTextWidth(text);
        return pos;
    };

    if(ImGui::BeginMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("Load Symbols")) {
                loadCpuDebuggerSymbols();
            }
            ImGui::BeginDisabled(m_cpuDebugSymbols.empty());
            if(ImGui::MenuItem("Clear Symbols")) {
                clearCpuDebuggerSymbols();
            }
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Debug")) {
            if(ImGui::MenuItem(paused ? "Resume" : "Pause", "F9")) {
                toggleCpuDebuggerPause();
            }
            ImGui::BeginDisabled(!paused);
            if(ImGui::MenuItem("Step", "F8")) {
                stepCpuDebugger();
            }
            ImGui::EndDisabled();
            if(ImGui::MenuItem("Breakpoints")) {
                m_showCpuBreakpointsWindow = true;
                m_cpuBreakpointsRequestFocus = true;
                AppSettings::instance().data.debug.showCpuBreakpoints = true;
            }
            ImGui::BeginDisabled(!breakpointHit.valid);
            if(ImGui::MenuItem("Clear Breakpoint Hit")) {
                m_emu.withExclusiveAccess([](auto& emu) {
                    emu.clearDebugBreakpointHit();
                });
            }
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Navigate")) {
            uint16_t requestedViewAddress = m_cpuDebuggerViewAddress;
            ImGui::TextUnformatted("Goto Address");
            ImGui::SetNextItemWidth(90.0f);
            if(ImGui::InputScalar("##CpuDebuggerMenuViewAddress", ImGuiDataType_U16, &requestedViewAddress, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                navigateCpuDebuggerTo(requestedViewAddress, false);
            }
            ImGui::SameLine();
            if(ImGui::Button("Go")) {
                navigateCpuDebuggerTo(requestedViewAddress, false);
            }
            if(ImGui::MenuItem("Goto PC")) {
                navigateCpuDebuggerTo(cpuState.pc, true);
            }
            const bool wasFollowingPc = m_cpuDebuggerFollowPc;
            if(ImGui::MenuItem("Follow PC", nullptr, m_cpuDebuggerFollowPc)) {
                m_cpuDebuggerFollowPc = !m_cpuDebuggerFollowPc;
                if(m_cpuDebuggerFollowPc && !wasFollowingPc) {
                    navigateCpuDebuggerTo(cpuState.pc, true);
                }
            }
            ImGui::Separator();
            ImGui::BeginDisabled(m_cpuDebuggerHistoryIndex == 0 || m_cpuDebuggerHistory.empty());
            if(ImGui::MenuItem("Back")) {
                --m_cpuDebuggerHistoryIndex;
                m_cpuDebuggerViewAddress = m_cpuDebuggerHistory[m_cpuDebuggerHistoryIndex];
                m_cpuDebuggerFollowPc = false;
                m_cpuDebuggerScrollToViewAddress = true;
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(m_cpuDebuggerHistory.empty() || m_cpuDebuggerHistoryIndex + 1 >= m_cpuDebuggerHistory.size());
            if(ImGui::MenuItem("Forward")) {
                ++m_cpuDebuggerHistoryIndex;
                m_cpuDebuggerViewAddress = m_cpuDebuggerHistory[m_cpuDebuggerHistoryIndex];
                m_cpuDebuggerFollowPc = false;
                m_cpuDebuggerScrollToViewAddress = true;
            }
            ImGui::EndDisabled();
            ImGui::Separator();
            ImGui::BeginDisabled(m_cpuDebugSymbols.empty());
            ImGui::SetNextItemWidth(180.0f);
            const bool searchSubmitted = ImGui::InputText(
                "Search Symbol",
                m_cpuDebuggerSymbolSearch.data(),
                m_cpuDebuggerSymbolSearch.size(),
                ImGuiInputTextFlags_EnterReturnsTrue
            );
            ImGui::BeginDisabled(m_cpuDebuggerSymbolSearch[0] == '\0');
            if((ImGui::MenuItem("Find Next", "F3") || searchSubmitted) && m_cpuDebuggerSymbolSearch[0] != '\0') {
                findNextCpuSymbol();
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::Separator();
            ImGui::BeginDisabled(!m_cpuDebuggerHasSelection);
            if(ImGui::MenuItem("Copy", "Ctrl+C")) {
                copySelectedCpuDebuggerLine();
            }
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::TextDisabled(paused ? "Stopped" : "Running");
    ImGui::Separator();
    DrawCpuBreakpointHitSummary(breakpointHit);

    ImGui::Separator();

    if(ImGui::BeginTable("CpuRegisters", 4, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("PC  %04X", cpuState.pc);
        ImGui::TableNextColumn(); ImGui::Text("A   %02X", cpuState.a);
        ImGui::TableNextColumn(); ImGui::Text("X   %02X", cpuState.x);
        ImGui::TableNextColumn(); ImGui::Text("Y   %02X", cpuState.y);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("SP  %02X", cpuState.sp);
        ImGui::TableNextColumn(); ImGui::Text("P   %02X", cpuState.status);
        ImGui::TableNextColumn(); ImGui::Text("Flags  %s", CPU2A03Debug::formatStatus(cpuState.status).c_str());
        ImGui::TableNextColumn(); ImGui::Text("Frame  %u", execPoint.frame);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("CPU Cycles  %u", cpuState.cycleCounter);
        ImGui::TableNextColumn(); ImGui::Text("Tick  %llu", static_cast<unsigned long long>(execPoint.emulationTick));
        ImGui::TableNextColumn(); ImGui::Text("Pending Cycles  %u", execPoint.cpuCyclesRemaining);
        ImGui::TableNextColumn(); ImGui::Text("Last Opcode  %02X", cpuState.opcode);
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Disassembly");
    if(!m_cpuDebugSymbols.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%zu symbols: %s",
            m_cpuDebugSymbols.size(),
            fs::path(m_cpuDebugSymbolsPath).filename().string().c_str()
        );
    } else if(!m_cpuDebugSymbolsStatus.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_cpuDebugSymbolsStatus.c_str());
    }

    if(ImGui::BeginChild("CpuDisassembly", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        const float rowHeight = lineHeight;
        if(m_cpuDebuggerScrollToViewAddress) {
            const float centerOffset = std::max(0.0f, (ImGui::GetWindowHeight() - rowHeight) * 0.5f);
            const float targetScrollY = std::max(0.0f, static_cast<float>(m_cpuDebuggerViewAddress) * rowHeight - centerOffset);
            ImGui::SetScrollY(targetScrollY);
            m_cpuDebuggerScrollToViewAddress = false;
        }

        ImGuiListClipper clipper;
        clipper.Begin(0x10000, rowHeight);
        while(clipper.Step()) {
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const CPU2A03DebugLine line = CPU2A03Debug::disassembleAt(
                    static_cast<uint16_t>(row),
                    cpuState.pc,
                    readCpuSnapshot,
                    symbolForAddress
                );

                const auto symbolAtLine = m_cpuDebugSymbols.find(line.address);
                const CpuDebugSymbol* symbol = symbolAtLine != m_cpuDebugSymbols.end() ? &symbolAtLine->second : nullptr;
                const bool isDataSymbol = symbol != nullptr && symbol->kind == CpuDebugSymbolKind::Data;

                ImGui::PushID(row);
                if(symbol != nullptr && !isDataSymbol) {
                    const ImVec4 symbolDeclarationColor = symbol->kind == CpuDebugSymbolKind::Label
                        ? labelReferenceColor
                        : ImGuiTheme::accentActive();
                    ImGui::TextColored(symbolDeclarationColor, "  %s:", symbol->name.c_str());
                    if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        navigateCpuDebuggerTo(line.address, false);
                    }
                    if(ImGui::IsItemHovered()) {
                        showSymbolTooltip(*symbol, line.address);
                    }
                }

                std::ostringstream addressStream;
                addressStream << std::uppercase << std::hex;
                addressStream.width(4);
                addressStream.fill('0');
                addressStream << static_cast<unsigned int>(line.address);

                const auto operandSymbol = !line.operandSymbol.empty()
                    ? m_cpuDebugSymbols.find(line.operandSymbolAddress)
                    : m_cpuDebugSymbols.end();
                const bool referencesColoredSymbol = operandSymbol != m_cpuDebugSymbols.end() &&
                    (operandSymbol->second.kind == CpuDebugSymbolKind::Data ||
                     operandSymbol->second.kind == CpuDebugSymbolKind::Label);
                const ImVec4 referenceSymbolColor = operandSymbol != m_cpuDebugSymbols.end() &&
                    operandSymbol->second.kind == CpuDebugSymbolKind::Label
                    ? labelReferenceColor
                    : dataSymbolColor;
                const uint16_t selectionFirst = std::min(m_cpuDebuggerSelectionAnchor, m_cpuDebuggerSelectedAddress);
                const uint16_t selectionLast = std::max(m_cpuDebuggerSelectionAnchor, m_cpuDebuggerSelectedAddress);
                const bool rowInSelection = m_cpuDebuggerHasSelection &&
                    line.address >= selectionFirst &&
                    line.address <= selectionLast;
                const bool selectedRow = line.isCurrent || rowInSelection;

                if(selectedRow) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGuiTheme::textOnAccent());
                    ImGui::PushStyleColor(ImGuiCol_Header, line.isCurrent ? ImGuiTheme::accentActive() : ImGuiTheme::accentHovered());
                }
                if(ImGui::Selectable("##CpuDisassemblyRow", selectedRow, ImGuiSelectableFlags_SpanAvailWidth, ImVec2(0.0f, lineHeight))) {
                    if(!ImGui::GetIO().KeyShift || !m_cpuDebuggerHasSelection) {
                        m_cpuDebuggerSelectionAnchor = line.address;
                    }
                    m_cpuDebuggerSelectedAddress = line.address;
                    m_cpuDebuggerHasSelection = true;
                }
                if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if(isDataSymbol) {
                        navigateCpuDebuggerTo(line.address, false);
                    } else if(!line.operandSymbol.empty()) {
                        navigateCpuDebuggerTo(line.operandSymbolAddress, false);
                    }
                }
                const ImVec2 rowTextPos = ImGui::GetItemRectMin();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImU32 normalTextColor = selectedRow
                    ? ImGuiTheme::toU32(ImGuiTheme::textOnAccent())
                    : ImGui::GetColorU32(ImGuiCol_Text);
                const ImU32 symbolColor = ImGuiTheme::toU32(referenceSymbolColor);
                if(selectedRow) {
                    ImGui::PopStyleColor(2);
                }

                const float arrowX = rowTextPos.x;
                const float addressX = arrowX + calcCpuDebuggerTextWidth("-> ");
                const float bytesX = addressX + calcCpuDebuggerTextWidth("0000  ");
                const float mnemonicX = bytesX + calcCpuDebuggerTextWidth("00 00 00  ");
                const ImVec2 arrowPos(arrowX, rowTextPos.y);
                const ImVec2 addressPos(addressX, rowTextPos.y);
                const ImVec2 bytesPos(bytesX, rowTextPos.y);
                const ImVec2 mnemonicPos(mnemonicX, rowTextPos.y);

                drawList->AddText(arrowPos, normalTextColor, line.isCurrent ? "->" : "  ");
                drawList->AddText(addressPos, normalTextColor, addressStream.str().c_str());
                if(isDataSymbol) {
                    std::ostringstream dataStream;
                    dataStream << symbol->name << ": .db $" << std::uppercase << std::hex;
                    dataStream.width(2);
                    dataStream.fill('0');
                    dataStream << static_cast<unsigned int>(readCpuSnapshot(line.address));

                    if(ImGui::IsItemHovered()) {
                        showSymbolTooltip(*symbol, line.address);
                    }
                    drawList->AddText(bytesPos, ImGuiTheme::toU32(dataSymbolColor), dataStream.str().c_str());
                    ImGui::PopID();
                    continue;
                }

                drawList->AddText(bytesPos, normalTextColor, line.bytes.c_str());
                if(referencesColoredSymbol) {
                    const std::size_t symbolPos = line.mnemonic.find(line.operandSymbol);
                    if(symbolPos != std::string::npos) {
                        const std::string beforeSymbol = line.mnemonic.substr(0, symbolPos);
                        const std::string afterSymbol = line.mnemonic.substr(symbolPos + line.operandSymbol.size());
                        ImVec2 cursor = mnemonicPos;
                        cursor = cpuDebuggerText(drawList, cursor, normalTextColor, beforeSymbol);
                        cursor = cpuDebuggerText(drawList, cursor, symbolColor, line.operandSymbol);
                        drawList->AddText(cursor, normalTextColor, afterSymbol.c_str());
                    } else {
                        drawList->AddText(mnemonicPos, normalTextColor, line.mnemonic.c_str());
                    }
                } else {
                    drawList->AddText(mnemonicPos, normalTextColor, line.mnemonic.c_str());
                }
                if(ImGui::IsItemHovered()) {
                    if(isDataSymbol) {
                        showSymbolTooltip(*symbol, line.address);
                    } else if(!line.operandSymbol.empty()) {
                        if(operandSymbol != m_cpuDebugSymbols.end()) {
                            showSymbolTooltip(operandSymbol->second, line.operandSymbolAddress);
                        }
                    }
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();

    if(!m_showCpuDebuggerWindow) {
        m_cpuDebuggerFocused = false;
        disableCpuDebugging();
    }
}
