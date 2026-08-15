#include "HornPlayer.hpp"

#include <coreinit/cache.h>
#include <sndcore2/core.h>
#include <sndcore2/device.h>
#include <whb/file.h>

#include <cstring>

namespace webeast::wiiu {

bool HornPlayer::init(const char* pcmPath, std::uint32_t sourceSampleRate) {
    shutdown();
    if (!pcmPath || sourceSampleRate == 0) return false;

    if (!AXIsInit()) {
        AXInitParams params{};
        params.renderer = AX_INIT_RENDERER_32KHZ;
        params.pipeline = AX_INIT_PIPELINE_SINGLE;
        AXInitWithParams(&params);
        m_ownsAx = true;
    }

    m_samples = WHBReadWholeFile(pcmPath, &m_sampleBytes);
    if (!m_samples || m_sampleBytes < sizeof(std::int16_t) ||
        (m_sampleBytes % sizeof(std::int16_t)) != 0) {
        shutdown();
        return false;
    }

    m_sampleCount = m_sampleBytes / sizeof(std::int16_t);
    DCStoreRange(m_samples, m_sampleBytes);

    m_voice = AXAcquireVoice(31, nullptr, nullptr);
    if (!m_voice) {
        shutdown();
        return false;
    }

    AXVoiceBegin(m_voice);
    AXSetVoiceType(m_voice, AX_VOICE_TYPE_UNKNOWN);

    AXVoiceVeData volume{};
    volume.volume = 0x7000;
    volume.delta = 0;
    AXSetVoiceVe(m_voice, &volume);

    // Route the mono horn equally to left/right on both TV and GamePad.
    AXVoiceDeviceMixData mix[6];
    std::memset(mix, 0, sizeof(mix));
    mix[0].bus[0].volume = 0x7000;
    mix[1].bus[0].volume = 0x7000;
    AXSetVoiceDeviceMix(m_voice, AX_DEVICE_TYPE_TV, 0, mix);
    AXSetVoiceDeviceMix(m_voice, AX_DEVICE_TYPE_DRC, 0, mix);

    const std::uint32_t rendererRate = AXGetInputSamplesPerSec();
    if (rendererRate == 0) {
        AXVoiceEnd(m_voice);
        shutdown();
        return false;
    }

    AXSetVoiceSrcType(m_voice, AX_VOICE_SRC_TYPE_LINEAR);
    if (AXSetVoiceSrcRatio(m_voice,
                           static_cast<float>(sourceSampleRate) /
                           static_cast<float>(rendererRate)) !=
        AX_VOICE_RATIO_RESULT_SUCCESS) {
        AXVoiceEnd(m_voice);
        shutdown();
        return false;
    }

    AXVoiceOffsets offsets{};
    offsets.dataType = AX_VOICE_FORMAT_LPCM16;
    offsets.loopingEnabled = AX_VOICE_LOOP_DISABLED;
    offsets.loopOffset = 0;
    offsets.endOffset = m_sampleCount - 1;
    offsets.currentOffset = 0;
    offsets.data = m_samples;
    AXSetVoiceOffsets(m_voice, &offsets);
    AXVoiceEnd(m_voice);

    AXSetVoiceState(m_voice, AX_VOICE_STATE_STOPPED);
    m_ready = true;
    return true;
}

void HornPlayer::play() {
    if (!m_ready || !m_voice) return;

    // Rewind before every warning so repeated car passes always replay the SFX.
    AXSetVoiceState(m_voice, AX_VOICE_STATE_STOPPED);
    AXSetVoiceCurrentOffset(m_voice, 0);
    AXSetVoiceState(m_voice, AX_VOICE_STATE_PLAYING);
}

void HornPlayer::shutdown() {
    m_ready = false;

    if (m_voice) {
        AXSetVoiceState(m_voice, AX_VOICE_STATE_STOPPED);
        AXFreeVoice(m_voice);
        m_voice = nullptr;
    }

    if (m_samples) {
        WHBFreeWholeFile(m_samples);
        m_samples = nullptr;
    }

    m_sampleBytes = 0;
    m_sampleCount = 0;

    if (m_ownsAx) {
        AXQuit();
        m_ownsAx = false;
    }
}

} // namespace webeast::wiiu
