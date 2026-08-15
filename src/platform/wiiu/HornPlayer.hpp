#pragma once

#include <sndcore2/voice.h>

#include <cstdint>

namespace webeast::wiiu {

class HornPlayer {
public:
    bool init(const char* pcmPath, std::uint32_t sourceSampleRate = 16000);
    void shutdown();
    void play();

    bool ready() const { return m_ready; }

private:
    AXVoice* m_voice = nullptr;
    char* m_samples = nullptr;
    std::uint32_t m_sampleBytes = 0;
    std::uint32_t m_sampleCount = 0;
    bool m_ownsAx = false;
    bool m_ready = false;
};

} // namespace webeast::wiiu
