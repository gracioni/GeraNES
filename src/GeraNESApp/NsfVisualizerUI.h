#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <string>
#include <vector>

#include "imgui_include.h"
#include "imgui_util.h"

class NsfVisualizerUI
{
private:
    static constexpr size_t FFT_SIZE = 512;
    static constexpr int BAR_COUNT = 56;

    std::array<float, BAR_COUNT> m_barLevels = {};

    static float clamp01(float value)
    {
        return std::clamp(value, 0.0f, 1.0f);
    }

    static float smoothTowards(float current, float target)
    {
        const float attack = 0.35f;
        const float release = 0.09f;
        const float alpha = target > current ? attack : release;
        return current + (target - current) * alpha;
    }

    static void fft(std::array<std::complex<float>, FFT_SIZE>& data)
    {
        for(size_t i = 1, j = 0; i < FFT_SIZE; ++i) {
            size_t bit = FFT_SIZE >> 1;
            for(; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if(i < j) std::swap(data[i], data[j]);
        }

        for(size_t len = 2; len <= FFT_SIZE; len <<= 1) {
            const float angle = -2.0f * 3.14159265358979323846f / static_cast<float>(len);
            const std::complex<float> wlen(std::cos(angle), std::sin(angle));
            for(size_t i = 0; i < FFT_SIZE; i += len) {
                std::complex<float> w(1.0f, 0.0f);
                for(size_t j = 0; j < len / 2; ++j) {
                    const std::complex<float> u = data[i + j];
                    const std::complex<float> v = data[i + j + len / 2] * w;
                    data[i + j] = u + v;
                    data[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }
    }

public:
    void draw(const std::vector<float>& samples, int sampleRate, int topMargin, int viewportWidth, int viewportHeight,
              int currentSong, int totalSongs, bool isPlaying, bool isPaused, bool hasEnded)
    {
        const ImVec2 canvasPos(0.0f, static_cast<float>(topMargin));
        const ImVec2 canvasSize(static_cast<float>(viewportWidth), static_cast<float>(std::max(0, viewportHeight - topMargin)));
        if(canvasSize.x <= 0.0f || canvasSize.y <= 0.0f) return;

        ImGui::SetNextWindowPos(canvasPos);
        ImGui::SetNextWindowSize(canvasSize);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##NsfVisualizerCanvas", nullptr,
                     ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoInputs);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        const ImVec2 max(origin.x + size.x, origin.y + size.y);

        drawList->AddRectFilledMultiColor(origin, max,
            IM_COL32(5, 10, 18, 255),
            IM_COL32(14, 24, 38, 255),
            IM_COL32(4, 6, 12, 255),
            IM_COL32(0, 0, 0, 255));

        std::array<std::complex<float>, FFT_SIZE> fftBuffer = {};
        const size_t copyCount = std::min(samples.size(), FFT_SIZE);
        const size_t srcOffset = samples.size() > FFT_SIZE ? samples.size() - FFT_SIZE : 0;
        for(size_t i = 0; i < copyCount; ++i) {
            const float t = copyCount > 1 ? static_cast<float>(i) / static_cast<float>(copyCount - 1) : 0.0f;
            const float window = 0.5f - 0.5f * std::cos(2.0f * 3.14159265358979323846f * t);
            fftBuffer[i] = std::complex<float>(samples[srcOffset + i] * window, 0.0f);
        }

        fft(fftBuffer);

        const int safeRate = std::max(sampleRate, 1);
        const float nyquist = static_cast<float>(safeRate) * 0.5f;
        const float minFreq = 20.0f;
        const float maxFreq = std::max(20000.0f, nyquist);

        std::array<float, BAR_COUNT> targets = {};
        for(int bar = 0; bar < BAR_COUNT; ++bar) {
            const float a = static_cast<float>(bar) / static_cast<float>(BAR_COUNT);
            const float b = static_cast<float>(bar + 1) / static_cast<float>(BAR_COUNT);
            const float startFreq = minFreq * std::pow(maxFreq / minFreq, a);
            const float endFreq = minFreq * std::pow(maxFreq / minFreq, b);

            int startBin = static_cast<int>(std::floor(startFreq * FFT_SIZE / safeRate));
            int endBin = static_cast<int>(std::ceil(endFreq * FFT_SIZE / safeRate));
            startBin = std::clamp(startBin, 1, static_cast<int>(FFT_SIZE / 2) - 1);
            endBin = std::clamp(endBin, startBin + 1, static_cast<int>(FFT_SIZE / 2));

            float energy = 0.0f;
            for(int bin = startBin; bin < endBin; ++bin) {
                energy += std::abs(fftBuffer[bin]);
            }
            energy /= static_cast<float>(std::max(1, endBin - startBin));

            const float normalized = clamp01(std::log1p(energy * 10.0f) / std::log1p(10.0f));
            targets[bar] = normalized;
            m_barLevels[bar] = smoothTowards(m_barLevels[bar], targets[bar]);
        }

        const float sidePadding = std::max(24.0f, size.x * 0.04f);
        const float bottomPadding = std::max(34.0f, size.y * 0.08f);
        const float topPadding = std::max(72.0f, size.y * 0.14f);
        const float barAreaHeight = std::max(10.0f, size.y - topPadding - bottomPadding);
        const float usableWidth = std::max(10.0f, size.x - sidePadding * 2.0f);
        const float gap = std::max(2.0f, usableWidth * 0.005f);
        const float barWidth = std::max(3.0f, (usableWidth - gap * (BAR_COUNT - 1)) / static_cast<float>(BAR_COUNT));

        for(int i = 0; i < BAR_COUNT; ++i) {
            const float x0 = origin.x + sidePadding + i * (barWidth + gap);
            const float x1 = x0 + barWidth;
            const float level = clamp01(m_barLevels[i]);
            const float height = std::max(4.0f, level * barAreaHeight);
            const float y1 = max.y - bottomPadding;
            const float y0 = y1 - height;

            const int lowBoost = static_cast<int>(80.0f + 120.0f * (1.0f - static_cast<float>(i) / BAR_COUNT));
            const int highBoost = static_cast<int>(90.0f + 120.0f * (static_cast<float>(i) / BAR_COUNT));

            drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
                                    IM_COL32(18, 28, 42, 190), 3.0f);
            drawList->AddRectFilledMultiColor(ImVec2(x0, y0), ImVec2(x1, y1),
                                              IM_COL32(lowBoost, 220, 255, 240),
                                              IM_COL32(lowBoost, 220, 255, 240),
                                              IM_COL32(80, highBoost, 255, 255),
                                              IM_COL32(80, highBoost, 255, 255));
        }

        const char* state = isPlaying ? "PLAYING" : (isPaused ? "PAUSED" : (hasEnded ? "ENDED" : "STOPPED"));
        const std::string title = "NSF PLAYER";
        const std::string subtitle = "Track " + std::to_string(currentSong) + " / " + std::to_string(totalSongs) + "  " + state;

        DrawTextOutlined(drawList, nullptr, 34.0f, ImVec2(origin.x + sidePadding, origin.y + 22.0f),
                         IM_COL32(245, 250, 255, 255), IM_COL32(0, 0, 0, 255), title.c_str());
        DrawTextOutlined(drawList, nullptr, 20.0f, ImVec2(origin.x + sidePadding, origin.y + 62.0f),
                         IM_COL32(140, 210, 255, 255), IM_COL32(0, 0, 0, 255), subtitle.c_str());

        ImGui::End();
        ImGui::PopStyleVar(2);
    }
};
