#pragma once

struct ImVec2;

namespace Netplay {

class NetplayAppRuntime;

void drawNetplayWindow(bool& showWindow,
                       NetplayAppRuntime& runtime,
                       const ImVec2& viewportCenter);

} // namespace Netplay
