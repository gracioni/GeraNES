#pragma once

#include <array>
#include <cstdint>
#include <vector>

struct ImFont;
struct PFFFT_Setup;

class NsfVisualizerUI
{
private:
    static constexpr size_t FFT_SIZE = 4096;
    static constexpr int BAR_COUNT = 56;
    static constexpr float SPECTRUM_GAIN = 2.0f;
    static constexpr float SPECTRUM_NORM_REF = 10.0f;
    static constexpr float MIN_FREQ = 20.0f;
    static constexpr float MAX_FREQ = 20000.0f;
    static constexpr float VISUAL_STEP_MS = 1000.0f / 60.0f;

    std::array<float, BAR_COUNT> m_barLevels = {};
    float m_visualStepAccumulatorMs = 0.0f;
    PFFFT_Setup* m_fftSetup = nullptr;
    float* m_fftInput = nullptr;
    float* m_fftOutput = nullptr;
    float* m_fftWork = nullptr;
    bool m_fftReady = false;

    static float clamp01(float value);
    static float smoothTowardsFixedStep(float current, float target);
    static float binMagnitude(const float* spectrum, int bin);

public:
    NsfVisualizerUI();
    ~NsfVisualizerUI();

    NsfVisualizerUI(const NsfVisualizerUI&) = delete;
    NsfVisualizerUI& operator=(const NsfVisualizerUI&) = delete;
    NsfVisualizerUI(NsfVisualizerUI&&) = delete;
    NsfVisualizerUI& operator=(NsfVisualizerUI&&) = delete;

    void draw(const std::vector<float>& samples, int sampleRate, int topMargin, int viewportWidth, int viewportHeight,
              int currentSong, int totalSongs, bool isPlaying, bool isPaused, bool hasEnded,
              uint32_t dtMs,
              ImFont* titleFont = nullptr, ImFont* subtitleFont = nullptr);
};
