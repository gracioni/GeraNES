#pragma once

enum class WindowSize : int {
    W1K  = 0x400,
    W2K  = 0x800,
    W4K  = 0x1000,
    W8K  = 0x2000,
    W16K = 0x4000,
    W32K = 0x8000
};

constexpr int log2(WindowSize ws) {
    int s = 0;
    int v = static_cast<int>(ws);
    while (v > 1) { v >>= 1; ++s; }
    return s;
}