#pragma once

struct ImVec2;

namespace GeraNESNetplay {

class GeraNESNetplayAppRuntime;

void drawNetplayWindow(bool& showWindow,
                       GeraNESNetplayAppRuntime& runtime,
                       const ImVec2& viewportCenter);

} // namespace GeraNESNetplay
