#pragma once

inline void GeraNESApp::drawShaderWindow()
{
    SetNextWindowCenteredOnMainViewport(ImVec2(800.0f, 440.0f));

    if(!ImGui::Begin("Shader", &m_showShaderWindow)) {
        ImGui::End();
        return;
    }
    m_imGuiWindowFocusBlocksEmulator |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    auto& video = AppSettings::instance().data.video;
    auto& shaderStack = video.shaderStack;
    auto& shaderPresets = video.shaderPresets;
    if(shaderList.empty()) {
        ImGui::TextDisabled("No .glsl shader files found in shaders/.");
        ImGui::TextDisabled("Drop .glsl files there and reopen this window.");
        ImGui::End();
        return;
    }

    m_selectedAvailableShaderIndex = std::clamp(m_selectedAvailableShaderIndex, 0, static_cast<int>(shaderList.size()) - 1);
    if(shaderStack.empty()) {
        m_selectedShaderStackIndex = -1;
    } else {
        if(m_selectedShaderStackIndex < 0 || m_selectedShaderStackIndex >= static_cast<int>(shaderStack.size())) {
            m_selectedShaderStackIndex = static_cast<int>(shaderStack.size()) - 1;
        }
    }

    if(!m_selectedShaderPresetName.empty() && shaderPresets.find(m_selectedShaderPresetName) == shaderPresets.end()) {
        m_selectedShaderPresetName.clear();
    }
    if(m_selectedShaderPresetName.empty() && !shaderPresets.empty()) {
        m_selectedShaderPresetName = shaderPresets.begin()->first;
    }
    if(m_shaderPresetNameInput.empty() && !m_selectedShaderPresetName.empty()) {
        m_shaderPresetNameInput = m_selectedShaderPresetName;
    }

    auto trimPresetName = [](std::string name) {
        const size_t first = name.find_first_not_of(" \t\r\n");
        if(first == std::string::npos) return std::string{};
        const size_t last = name.find_last_not_of(" \t\r\n");
        return name.substr(first, last - first + 1);
    };
    auto applyPresetSelection = [this](const std::string& name) {
        m_selectedShaderPresetName = name;
        m_shaderPresetNameInput = name;
    };

    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##ShaderPresetName", "Preset name", &m_shaderPresetNameInput);
    ImGui::SameLine();
    if(ImGui::Button("Save Preset")) {
        const std::string presetName = trimPresetName(m_shaderPresetNameInput);
        if(presetName.empty()) {
            Logger::instance().log("Preset name cannot be empty.", Logger::Type::USER);
        } else {
            shaderPresets[presetName] = shaderStack;
            applyPresetSelection(presetName);
            AppSettings::instance().save();
            Logger::instance().log("Shader preset saved: " + presetName, Logger::Type::USER);
        }
    }
    ImGui::SameLine();
    const std::string presetPreview = m_selectedShaderPresetName.empty() ? "Load preset..." : m_selectedShaderPresetName;
    ImGui::SetNextItemWidth(220.0f);
    if(ImGui::BeginCombo("##ShaderPresetList", presetPreview.c_str())) {
        for(const auto& [presetName, presetStack] : shaderPresets) {
            (void)presetStack;
            const bool selected = presetName == m_selectedShaderPresetName;
            if(ImGui::Selectable(presetName.c_str(), selected)) {
                applyPresetSelection(presetName);
            }
            if(selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    const bool hasSelectedPreset = !m_selectedShaderPresetName.empty();
    if(!hasSelectedPreset) ImGui::BeginDisabled();
    if(ImGui::Button("Load Preset")) {
        const auto presetIt = shaderPresets.find(m_selectedShaderPresetName);
        if(presetIt != shaderPresets.end()) {
            shaderStack = presetIt->second;
            m_selectedShaderStackIndex = shaderStack.empty() ? -1 : 0;
            updateShaderConfig();
            AppSettings::instance().save();
            Logger::instance().log("Shader preset loaded: " + m_selectedShaderPresetName, Logger::Type::USER);
        }
    }
    if(!hasSelectedPreset) ImGui::EndDisabled();
    ImGui::SameLine();
    if(!hasSelectedPreset) ImGui::BeginDisabled();
    if(ImGui::Button("Delete Preset")) {
        const std::string presetName = m_selectedShaderPresetName;
        if(!presetName.empty()) {
            shaderPresets.erase(presetName);
            if(shaderPresets.empty()) {
                m_selectedShaderPresetName.clear();
                m_shaderPresetNameInput.clear();
            } else {
                applyPresetSelection(shaderPresets.begin()->first);
            }
            AppSettings::instance().save();
            Logger::instance().log("Shader preset deleted: " + presetName, Logger::Type::USER);
        }
    }
    if(!hasSelectedPreset) ImGui::EndDisabled();

    ImGui::Separator();

    ImGui::BeginChild("AvailableShaders", ImVec2(280.0f, 0.0f), true);
    ImGui::TextUnformatted("Available");
    ImGui::Separator();
    for(size_t i = 0; i < shaderList.size(); ++i) {
        const bool selected = m_selectedAvailableShaderIndex == static_cast<int>(i);
        if(ImGui::Selectable(shaderList[i].label.c_str(), selected)) {
            m_selectedAvailableShaderIndex = static_cast<int>(i);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("ShaderStackEditor", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("Passes");
    ImGui::Separator();

    const bool canAdd = !shaderList.empty() && m_selectedAvailableShaderIndex >= 0 &&
        m_selectedAvailableShaderIndex < static_cast<int>(shaderList.size());
    if(ImGui::Button("Add Pass") && canAdd) {
        AppSettings::Video::ShaderPass pass;
        pass.label = shaderList[static_cast<size_t>(m_selectedAvailableShaderIndex)].label;
        pass.enabled = true;
        shaderStack.push_back(std::move(pass));
        m_selectedShaderStackIndex = static_cast<int>(shaderStack.size()) - 1;
        updateShaderConfig();
    }
    ImGui::SameLine();
    if(ImGui::Button("Clear Stack")) {
        shaderStack.clear();
        m_selectedShaderStackIndex = -1;
        updateShaderConfig();
    }
    ImGui::SameLine();
    if(ImGui::Button("Reload")) {
        loadShaderList();
        updateShaderConfig();
    }

    ImGui::Separator();

    if(ImGui::BeginChild("ShaderStackPasses", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() * 8.0f), true)) {
        if(shaderStack.empty()) {
            ImGui::TextDisabled("Default shader only.");
        } else {
            for(size_t i = 0; i < shaderStack.size(); ++i) {
                auto& pass = shaderStack[i];
                ImGui::PushID(static_cast<int>(i));
                bool enabled = pass.enabled;
                if(ImGui::Checkbox("##Enabled", &enabled)) {
                    pass.enabled = enabled;
                    updateShaderConfig();
                }
                ImGui::SameLine();
                const std::string rowLabel = std::to_string(i + 1) + ". " + pass.label;
                if(ImGui::Selectable(rowLabel.c_str(), m_selectedShaderStackIndex == static_cast<int>(i), ImGuiSelectableFlags_SpanAvailWidth)) {
                    m_selectedShaderStackIndex = static_cast<int>(i);
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();

    const bool hasEntries = !shaderStack.empty();
    const int activeIndex = hasEntries
        ? std::clamp(m_selectedShaderStackIndex, 0, static_cast<int>(shaderStack.size()) - 1)
        : -1;

    if(!hasEntries) ImGui::BeginDisabled();
    if(ImGui::Button("Up") && activeIndex > 0) {
        std::swap(shaderStack[static_cast<size_t>(m_selectedShaderStackIndex)], shaderStack[static_cast<size_t>(m_selectedShaderStackIndex - 1)]);
        --m_selectedShaderStackIndex;
        updateShaderConfig();
    }
    ImGui::SameLine();
    if(ImGui::Button("Down") && activeIndex >= 0 && activeIndex + 1 < static_cast<int>(shaderStack.size())) {
        std::swap(shaderStack[static_cast<size_t>(m_selectedShaderStackIndex)], shaderStack[static_cast<size_t>(m_selectedShaderStackIndex + 1)]);
        ++m_selectedShaderStackIndex;
        updateShaderConfig();
    }
    ImGui::SameLine();
    if(ImGui::Button("Remove") && activeIndex >= 0) {
        shaderStack.erase(shaderStack.begin() + activeIndex);
        if(shaderStack.empty()) m_selectedShaderStackIndex = -1;
        else if(activeIndex >= static_cast<int>(shaderStack.size())) m_selectedShaderStackIndex = static_cast<int>(shaderStack.size()) - 1;
        else m_selectedShaderStackIndex = activeIndex;
        updateShaderConfig();
    }
    if(!hasEntries) ImGui::EndDisabled();

    if(activeIndex >= 0 && activeIndex < static_cast<int>(shaderStack.size())) {
        auto& configuredPass = shaderStack[static_cast<size_t>(activeIndex)];
        const ShaderItem* configuredItem = findShaderByLabel(configuredPass.label);
        ImGui::Separator();
        ImGui::Text("Parameters: %s", configuredPass.label.c_str());

        if(configuredItem == nullptr) {
            ImGui::TextDisabled("Shader file not found.");
        } else {
            std::ifstream shaderFile(configuredItem->path);
            std::string shaderText((std::istreambuf_iterator<char>(shaderFile)), std::istreambuf_iterator<char>());
            std::vector<ShaderPass::Parameter> parameters = parseShaderParameters(shaderText);
            for(ShaderPass::Parameter& parameter : parameters) {
                auto it = configuredPass.parameters.find(parameter.name);
                if(it != configuredPass.parameters.end()) {
                    parameter.value = std::clamp(it->second, parameter.minValue, parameter.maxValue);
                }
            }

            if(parameters.empty()) {
                ImGui::TextDisabled("This shader exposes no #pragma parameter entries.");
            } else {
                if(ImGui::Button("Reset To Default")) {
                    configuredPass.parameters.clear();
                    updateShaderConfig();
                }
                ImGui::Separator();

                for(const ShaderPass::Parameter& parameter : parameters) {
                    float value = parameter.value;
                    const std::string sliderId = parameter.label + "##" + parameter.name;
                    if(ImGui::SliderFloat(sliderId.c_str(), &value, parameter.minValue, parameter.maxValue, "%.3f")) {
                        configuredPass.parameters[parameter.name] = value;
                        updateShaderConfig();
                    }
                }
            }
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Passes run top to bottom. Remove every pass to go back to the default shader.");
    ImGui::EndChild();

    ImGui::End();
}
