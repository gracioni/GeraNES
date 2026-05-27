#pragma once

inline void GeraNESApp::drawConfirmSaveRomDatabaseEntryPopup()
{
    SetNextWindowPosCenteredOnMainViewport();
    if(ImGui::BeginPopupModal("Confirm Save ROM Database Entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Are you sure you want to save this entry to db.txt?");
        ImGui::Spacing();
        if(ImGui::Button("Yes", ImVec2(120, 0))) {
            saveRomDatabaseEditor();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("No", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
