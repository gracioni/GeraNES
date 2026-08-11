#pragma once

inline void GeraNESApp::drawCpuProfilerWindow()
{
    SetNextWindowCenteredOnMainViewport(ImVec2(940.0f, 560.0f));

    if(!ImGui::Begin("CPU Profiler", &m_showCpuProfilerWindow, ImGuiWindowFlags_MenuBar)) {
        AppSettings::instance().data.debug.showCpuProfiler = m_showCpuProfilerWindow;
        m_cpuProfilerFocused = false;
        ImGui::End();
        return;
    }

    AppSettings::instance().data.debug.showCpuProfiler = m_showCpuProfilerWindow;
    m_cpuProfilerFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    m_imGuiWindowFocusBlocksEmulator |= m_cpuProfilerFocused;

    if(!m_emu.valid()) {
        m_emu.withExclusiveAccess([](auto& emu) {
            emu.setCpuProfilerEnabled(false);
            emu.clearCpuProfileData();
        });
        ImGui::TextDisabled("Load a ROM to collect CPU profile data.");
        ImGui::End();
        return;
    }

    GeraNESEmu::CpuProfileSnapshot snapshot;
    m_emu.withExclusiveAccess([&](auto& emu) {
        emu.setCpuProfilerEnabled(true);
        snapshot = emu.cpuProfileSnapshot();
    });

    bool copyTableAsCsv = false;
    if(ImGui::BeginMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("Load Symbols")) {
                loadCpuDebuggerSymbols();
            }
            ImGui::BeginDisabled(m_cpuDebugSymbols.empty());
            if(ImGui::MenuItem("Clear Symbols")) {
                m_cpuDebugSymbols.clear();
                m_cpuDebugSymbolsPath.clear();
                m_cpuDebugSymbolsStatus = "CPU symbols cleared.";
            }
            ImGui::EndDisabled();
            ImGui::Separator();
            if(ImGui::MenuItem("Copy Table as CSV")) {
                copyTableAsCsv = true;
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Clear Profile Data")) {
                m_emu.withExclusiveAccess([](auto& emu) { emu.clearCpuProfileData(); });
                snapshot = {};
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    const uint32_t capturedFrames = snapshot.currentFrame >= snapshot.startFrame
        ? snapshot.currentFrame - snapshot.startFrame
        : 0;
    if(!m_cpuDebugSymbols.empty()) {
        ImGui::TextDisabled("%zu symbols: %s", m_cpuDebugSymbols.size(), fs::path(m_cpuDebugSymbolsPath).filename().string().c_str());
        ImGui::SameLine();
    }
    ImGui::TextDisabled("Captured %u frames, %llu CPU cycles", capturedFrames, static_cast<unsigned long long>(snapshot.totalCycles));

    auto functionName = [&](uint16_t address) {
        std::ostringstream stream;
        const auto symbol = m_cpuDebugSymbols.find(address);
        if(symbol != m_cpuDebugSymbols.end()) {
            stream << symbol->second.name << " ($";
        } else {
            stream << "$";
        }
        stream << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << address;
        if(symbol != m_cpuDebugSymbols.end()) stream << ')';
        return stream.str();
    };

    std::vector<GeraNESEmu::CpuProfileEntry> rows = snapshot.entries;
    auto copyRowsAsCsv = [&]() {
        auto escapeCsvField = [](const std::string& value) {
            if(value.find_first_of(",\"\r\n") == std::string::npos) return value;

            std::string escaped;
            escaped.reserve(value.size() + 2);
            escaped.push_back('"');
            for(const char character : value) {
                if(character == '"') escaped.push_back('"');
                escaped.push_back(character);
            }
            escaped.push_back('"');
            return escaped;
        };

        std::ostringstream csv;
        csv << "Function,Calls,Inclusive,Inclusive %,Exclusive,Exclusive %,Avg,Min / Max,Frames\r\n";
        for(const auto& row : rows) {
            const double inclusivePercent = snapshot.totalCycles == 0
                ? 0.0
                : 100.0 * static_cast<double>(row.inclusiveCycles) / snapshot.totalCycles;
            const double exclusivePercent = snapshot.totalCycles == 0
                ? 0.0
                : 100.0 * static_cast<double>(row.exclusiveCycles) / snapshot.totalCycles;
            const double frameEquivalent = snapshot.totalCycles == 0
                ? 0.0
                : static_cast<double>(capturedFrames) * row.exclusiveCycles / snapshot.totalCycles;

            csv << escapeCsvField(functionName(row.address)) << ','
                << row.callCount << ','
                << row.inclusiveCycles << ','
                << std::fixed << std::setprecision(2) << inclusivePercent << ','
                << row.exclusiveCycles << ','
                << std::fixed << std::setprecision(2) << exclusivePercent << ','
                << (row.callCount == 0 ? 0 : row.inclusiveCycles / row.callCount) << ',';
            if(row.minCycles == UINT64_MAX) csv << "n/a";
            else csv << row.minCycles << " / " << row.maxCycles;
            csv << ',' << std::fixed << std::setprecision(3) << frameEquivalent << "\r\n";
        }

        const std::string csvText = csv.str();
#ifdef __EMSCRIPTEN__
        emcriptenCopyTextToClipboardExact(csvText.c_str());
#else
        ImGui::SetClipboardText(csvText.c_str());
#endif
        m_userToast.show("CPU profile table copied as CSV.");
    };

    if(ImGui::BeginTable("##CpuProfilerTable", 9,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_None, 210.0f, 0);
        ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_PreferSortDescending, 70.0f, 1);
        ImGui::TableSetupColumn("Inclusive", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending, 90.0f, 2);
        ImGui::TableSetupColumn("Inclusive %", ImGuiTableColumnFlags_PreferSortDescending, 85.0f, 3);
        ImGui::TableSetupColumn("Exclusive", ImGuiTableColumnFlags_PreferSortDescending, 90.0f, 4);
        ImGui::TableSetupColumn("Exclusive %", ImGuiTableColumnFlags_PreferSortDescending, 85.0f, 5);
        ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_PreferSortDescending, 70.0f, 6);
        ImGui::TableSetupColumn("Min / Max", ImGuiTableColumnFlags_PreferSortDescending, 100.0f, 7);
        ImGui::TableSetupColumn("Frames", ImGuiTableColumnFlags_PreferSortDescending, 70.0f, 8);
        ImGui::TableHeadersRow();

        if(ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs != nullptr && specs->SpecsCount > 0) {
            const ImGuiTableColumnSortSpecs sort = specs->Specs[0];
            auto value = [&](const auto& row) -> long double {
                switch(sort.ColumnUserID) {
                    case 1: return static_cast<long double>(row.callCount);
                    case 2:
                    case 3: return static_cast<long double>(row.inclusiveCycles);
                    case 4:
                    case 5:
                    case 8: return static_cast<long double>(row.exclusiveCycles);
                    case 6: return row.callCount == 0 ? 0.0L : static_cast<long double>(row.inclusiveCycles) / row.callCount;
                    case 7: return static_cast<long double>(row.maxCycles);
                    default: return static_cast<long double>(row.address);
                }
            };
            std::sort(rows.begin(), rows.end(), [&](const auto& left, const auto& right) {
                if(sort.ColumnUserID == 0) {
                    const std::string leftName = functionName(left.address);
                    const std::string rightName = functionName(right.address);
                    return sort.SortDirection == ImGuiSortDirection_Ascending ? leftName < rightName : leftName > rightName;
                }
                return sort.SortDirection == ImGuiSortDirection_Ascending ? value(left) < value(right) : value(left) > value(right);
            });
            specs->SpecsDirty = false;
        }

        if(copyTableAsCsv) {
            copyRowsAsCsv();
        }

        for(const auto& row : rows) {
            const double inclusivePercent = snapshot.totalCycles == 0 ? 0.0 : 100.0 * static_cast<double>(row.inclusiveCycles) / snapshot.totalCycles;
            const double exclusivePercent = snapshot.totalCycles == 0 ? 0.0 : 100.0 * static_cast<double>(row.exclusiveCycles) / snapshot.totalCycles;
            const double frameEquivalent = snapshot.totalCycles == 0 ? 0.0 : static_cast<double>(capturedFrames) * row.exclusiveCycles / snapshot.totalCycles;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(functionName(row.address).c_str());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%llu", static_cast<unsigned long long>(row.callCount));
            ImGui::TableSetColumnIndex(2); ImGui::Text("%llu", static_cast<unsigned long long>(row.inclusiveCycles));
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f%%", inclusivePercent);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%llu", static_cast<unsigned long long>(row.exclusiveCycles));
            ImGui::TableSetColumnIndex(5); ImGui::Text("%.2f%%", exclusivePercent);
            ImGui::TableSetColumnIndex(6); ImGui::Text("%llu", static_cast<unsigned long long>(row.callCount == 0 ? 0 : row.inclusiveCycles / row.callCount));
            ImGui::TableSetColumnIndex(7);
            if(row.minCycles == UINT64_MAX) ImGui::TextUnformatted("n/a");
            else ImGui::Text("%llu / %llu", static_cast<unsigned long long>(row.minCycles), static_cast<unsigned long long>(row.maxCycles));
            ImGui::TableSetColumnIndex(8); ImGui::Text("%.3f", frameEquivalent);
        }
        ImGui::EndTable();
    }

    if(!m_showCpuProfilerWindow) {
        m_cpuProfilerFocused = false;
        m_emu.withExclusiveAccess([](auto& emu) { emu.setCpuProfilerEnabled(false); });
    }
    ImGui::End();
}
