#include "GeraNESNetplay/NetplayWindowUI.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>

#include "ConsoleNetplay/NetplayAppRuntime.h"
#include "ConsoleNetplay/NetplayCoordinator.h"
#include "GeraNESApp/EmscriptenUtil.h"
#include "GeraNESApp/imgui_util.h"
#include "imgui.h"

namespace GeraNESNetplay {

using namespace ConsoleNetplay;

namespace {

struct NetplayChatDrawerUiState
{
    bool expanded = false;
    bool focusInput = false;
    bool scrollToBottom = false;
    float openAmount = 0.0f;
    uint64_t lastSeenSerial = 0;
    uint64_t lastObservedSerial = 0;
    std::string draftMessage;

    void reset()
    {
        expanded = false;
        focusInput = false;
        scrollToBottom = false;
        openAmount = 0.0f;
        lastSeenSerial = 0;
        lastObservedSerial = 0;
        draftMessage.clear();
    }
};

NetplayChatDrawerUiState& netplayChatDrawerUiState()
{
    static NetplayChatDrawerUiState state;
    return state;
}

constexpr const char* kChatDrawerShowIcon = "\xEF\x81\x93";
constexpr const char* kChatDrawerHideIcon = "\xEF\x81\x94";

const char* chatSessionStateLabel(SessionState state)
{
    switch(state) {
        case SessionState::Lobby: return "Lobby";
        case SessionState::ValidatingRom: return "Validating ROM";
        case SessionState::ReadyCheck: return "Ready Check";
        case SessionState::Starting: return "Starting";
        case SessionState::Running: return "Running";
        case SessionState::Resyncing: return "Resyncing";
        case SessionState::Paused: return "Paused";
        case SessionState::Ended: return "Ended";
        default: return "Unknown";
    }
}

ImU32 participantChatNameColorFromHue(uint16_t hueDegrees)
{
    const float hue = static_cast<float>(hueDegrees % 360u) / 360.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    ImGui::ColorConvertHSVtoRGB(hue, 0.52f, 0.98f, r, g, b);
    return IM_COL32(
        static_cast<int>(std::round(r * 255.0f)),
        static_cast<int>(std::round(g * 255.0f)),
        static_cast<int>(std::round(b * 255.0f)),
        255
    );
}

bool chatTextHasVisibleContent(const std::string& text)
{
    return std::find_if(text.begin(), text.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }) != text.end();
}

std::string trimChatText(std::string text)
{
    const auto first = std::find_if(text.begin(), text.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    if(first == text.end()) {
        return {};
    }

    const auto last = std::find_if(text.rbegin(), text.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base();
    return std::string(first, last);
}

} // namespace

bool drawNetplayChatDrawer(NetplayAppRuntime& runtime)
{
    NetplayChatDrawerUiState& drawer = netplayChatDrawerUiState();
    const NetplayAppRuntime::UiSnapshot snapshot = runtime.uiSnapshot();
    if(!snapshot.active && !snapshot.reconnecting) {
        drawer.reset();
        return false;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if(viewport == nullptr) {
        return false;
    }

    const uint64_t latestSerial =
        snapshot.chatHistory.empty() ? 0 : snapshot.chatHistory.back().serial;
    if(latestSerial > drawer.lastObservedSerial) {
        drawer.lastObservedSerial = latestSerial;
        drawer.scrollToBottom = true;
    }

    if(drawer.expanded && latestSerial > drawer.lastSeenSerial) {
        drawer.lastSeenSerial = latestSerial;
    }

    const bool hasUnread = latestSerial > drawer.lastSeenSerial;
    const float targetOpenAmount = drawer.expanded ? 1.0f : 0.0f;
    drawer.openAmount += (targetOpenAmount - drawer.openAmount) * 0.22f;
    if(std::abs(drawer.openAmount - targetOpenAmount) < 0.01f) {
        drawer.openAmount = targetOpenAmount;
    }

    float uiScale = 1.0f;
#ifdef __ANDROID__
    uiScale = EffectiveUiScale();
#endif
    const auto S = [uiScale](float value) { return value * uiScale; };

    const float tabWidth = S(40.0f);
    const float expandedWidth = S(360.0f);
    const float preferredDrawerWidth = tabWidth + expandedWidth * drawer.openAmount;
    const float maxDrawerWidth = std::max(tabWidth, viewport->WorkSize.x);
    const float drawerWidth = std::min(preferredDrawerWidth, maxDrawerWidth);
    const float minDrawerHeight = std::min(S(240.0f), viewport->WorkSize.y);
    const float preferredDrawerHeight = std::clamp(viewport->WorkSize.y * 0.56f, minDrawerHeight, S(420.0f));
    const float drawerHeight = std::min(preferredDrawerHeight, viewport->WorkSize.y);
    const float preferredTopOffset = std::max(S(12.0f), viewport->WorkSize.y * 0.2f);
    const float maxTopOffset = std::max(0.0f, viewport->WorkSize.y - drawerHeight);
    const float topOffset = std::min(preferredTopOffset, maxTopOffset);
    const float posX = std::clamp(
        viewport->WorkPos.x + viewport->WorkSize.x - drawerWidth,
        viewport->WorkPos.x,
        viewport->WorkPos.x + viewport->WorkSize.x - drawerWidth
    );
    const float posY = std::clamp(
        viewport->WorkPos.y + topOffset,
        viewport->WorkPos.y,
        viewport->WorkPos.y + viewport->WorkSize.y - drawerHeight
    );
    const ImVec2 windowPos(posX, posY);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(drawerWidth, drawerHeight), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    bool captureInput = false;

    if(ImGui::Begin("##NetplayChatDrawer",
                    nullptr,
                    ImGuiWindowFlags_NoDecoration |
                        ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoNavFocus)) {
        captureInput = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        const ImVec2 tabMin(origin.x, origin.y);
        const ImVec2 tabMax(origin.x + tabWidth, origin.y + size.y);
        const ImU32 tabColor = drawer.expanded ? IM_COL32(52, 63, 82, 240) : IM_COL32(34, 41, 54, 232);
        drawList->AddRectFilled(tabMin, tabMax, tabColor, S(12.0f), ImDrawFlags_RoundCornersLeft);
        drawList->AddRect(tabMin, tabMax, IM_COL32(255, 255, 255, 28), S(12.0f), ImDrawFlags_RoundCornersLeft);

        ImGui::SetCursorScreenPos(tabMin);
        ImGui::InvisibleButton("##NetplayChatDrawerToggle", ImVec2(tabWidth, size.y));
        if(ImGui::IsItemClicked()) {
            drawer.expanded = !drawer.expanded;
            if(drawer.expanded) {
                drawer.focusInput = true;
                drawer.lastSeenSerial = latestSerial;
            }
        }

        const char* iconText = drawer.expanded ? kChatDrawerHideIcon : kChatDrawerShowIcon;
        const ImVec2 iconSize = ImGui::CalcTextSize(iconText);
        drawList->AddText(
            ImVec2(
                tabMin.x + (tabWidth - iconSize.x) * 0.5f,
                tabMin.y + (size.y - iconSize.y) * 0.5f
            ),
            IM_COL32(230, 235, 242, 255),
            iconText
        );
        if(hasUnread) {
            drawList->AddCircleFilled(
                ImVec2(tabMax.x - S(10.0f), tabMin.y + S(12.0f)),
                S(5.0f),
                IM_COL32(220, 48, 48, 255)
            );
        }

        if(drawer.openAmount > 0.02f) {
            constexpr float bodyGap = 0.0f;
            ImGui::SetCursorScreenPos(ImVec2(origin.x + tabWidth + bodyGap, origin.y));
            ImGui::BeginChild("##NetplayChatDrawerBody",
                              ImVec2(std::max(1.0f, size.x - tabWidth - bodyGap), size.y),
                              false,
                              ImGuiWindowFlags_NoScrollbar);

            const ImVec2 bodyMin = ImGui::GetWindowPos();
            const ImVec2 bodyMax(bodyMin.x + ImGui::GetWindowSize().x, bodyMin.y + ImGui::GetWindowSize().y);
            drawList->AddRectFilled(bodyMin, bodyMax, IM_COL32(0, 0, 0, 191), S(12.0f), ImDrawFlags_RoundCornersNone);

            const float contentPadX = S(12.0f);
            const float contentPadTop = S(10.0f);
            const float composerGap = S(6.0f);
            const float buttonWidth = S(76.0f);
            const float composerPadX = S(10.0f);

            ImGui::SetCursorPos(ImVec2(contentPadX, contentPadTop));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(236, 240, 246, 255));
            ImGui::TextUnformatted("Netplay Chat");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.78f, 0.82f, 0.88f, 1.0f), "(%s)", chatSessionStateLabel(snapshot.room.state));
            ImGui::Separator();

            const float composerHeight = std::max(ImGui::GetFrameHeight(), S(30.0f));
            const float historyHeight =
                std::max(S(24.0f), ImGui::GetContentRegionAvail().y - composerHeight - composerGap);
            if(ImGui::BeginChild("##NetplayChatHistory",
                                 ImVec2(0.0f, historyHeight),
                                 false,
                                 ImGuiWindowFlags_HorizontalScrollbar)) {
                for(const auto& message : snapshot.chatHistory) {
                    ImGui::SetCursorPosX(contentPadX);
                    const std::string nameLabel = message.displayName + ":";
                    const ImVec2 namePos = ImGui::GetCursorScreenPos();
                    const ImVec2 nameSize = ImGui::CalcTextSize(nameLabel.c_str());
                    const ImU32 nameColor = participantChatNameColorFromHue(message.nameColorHue);
                    drawList->AddText(namePos, nameColor, nameLabel.c_str());
                    ImGui::Dummy(nameSize);
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(232, 236, 242, 255));
                    ImGui::TextWrapped("%s", message.text.c_str());
                    ImGui::PopStyleColor();
                    ImGui::Spacing();
                }

                if(drawer.scrollToBottom) {
                    ImGui::SetScrollHereY(1.0f);
                    drawer.scrollToBottom = false;
                }
            }
            ImGui::EndChild();

            std::string trimmedDraft = trimChatText(drawer.draftMessage);
            if(trimmedDraft.size() > kMaxChatMessageBytes) {
                trimmedDraft.resize(kMaxChatMessageBytes);
            }
            const bool canSend = snapshot.connected && chatTextHasVisibleContent(trimmedDraft);

            ImGui::SetCursorPosY(ImGui::GetWindowSize().y - composerHeight - S(1.0f));
            if(drawer.focusInput) {
                ImGui::SetKeyboardFocusHere();
                drawer.focusInput = false;
            }

            ImGui::SetCursorPosX(composerPadX);
            ImGui::SetNextItemWidth(-(buttonWidth + composerPadX * 2));
            const bool pressedEnter = ImGui::InputTextWithHint(
                "##NetplayChatMessage",
                snapshot.connected ? "Type a message..." : "Connect to send chat...",
                &drawer.draftMessage,
                ImGuiInputTextFlags_EnterReturnsTrue
            );
            ImGui::SameLine();
            const bool pressedButton = ImGui::Button("Send", ImVec2(buttonWidth, composerHeight));
            if((pressedEnter || pressedButton) && canSend) {
                runtime.sendChatMessage(trimmedDraft);
                drawer.draftMessage.clear();
                drawer.scrollToBottom = true;
                drawer.lastSeenSerial = latestSerial;
                drawer.focusInput = true;
            }

            ImGui::EndChild();
        }
    }

    ImGui::End();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
    return captureInput;
}

} // namespace GeraNESNetplay
