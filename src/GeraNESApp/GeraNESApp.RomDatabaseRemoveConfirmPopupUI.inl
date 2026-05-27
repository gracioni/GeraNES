#pragma once

inline void GeraNESApp::drawConfirmRemoveRomDatabaseEntryPopup()
{
    SetNextWindowPosCenteredOnMainViewport();
    if(ImGui::BeginPopupModal("Confirm Remove ROM Database Entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Are you sure you want to remove this entry from db.txt?");
        ImGui::Spacing();
        if(ImGui::Button("Yes", ImVec2(120, 0))) {
            removeRomDatabaseEditor();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("No", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
