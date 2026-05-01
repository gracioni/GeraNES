#pragma once

struct ImVec2;

namespace ConsoleNetplay {
class NetplayAppRuntime;
}

namespace GeraNESNetplay {

void drawNetplayWindow(bool& showWindow,
                       ConsoleNetplay::NetplayAppRuntime& runtime,
                       const ImVec2& viewportCenter);

} // namespace GeraNESNetplay
