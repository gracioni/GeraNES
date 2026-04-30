#pragma once

struct ImVec2;

namespace ConsoleNetplay {

class NetplayAppRuntime;

void drawNetplayWindow(bool& showWindow,
                       NetplayAppRuntime& runtime,
                       const ImVec2& viewportCenter);

} // namespace ConsoleNetplay
