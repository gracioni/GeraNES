#pragma once

#ifdef ENABLE_NSF_PLAYER
inline void GeraNESApp::drawNsfPlayerVisualizer()
{
    const SDL_Rect clientArea = emulatorClientArea();
    m_nsfVisualizer.draw(
        m_audioOutput.getRecentMixedSamples(),
        m_audioOutput.outputSampleRate(),
        clientArea.y,
        width(),
        height(),
        m_emu.nsfCurrentSong(),
        m_emu.nsfTotalSongs(),
        m_emu.nsfIsPlaying(),
        m_emu.nsfIsPaused(),
        m_emu.nsfHasEnded(),
        m_lastMainLoopDtMs,
        m_fontNsfTitle,
        m_fontNsfSubtitle
    );
}
#endif
