#pragma once

enum class BankSize : int {
    B1K  = 0x400,
    B2K  = 0x800,
    B4K  = 0x1000,
    B8K  = 0x2000,
    B16K = 0x4000,
    B32K = 0x8000
};

constexpr int log2(BankSize bs) {
    int s = 0;
    int v = static_cast<int>(bs);
    while (v > 1) { v >>= 1; ++s; }
    return s;
}