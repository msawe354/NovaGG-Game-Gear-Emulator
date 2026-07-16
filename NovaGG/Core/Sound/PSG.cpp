#include "PSG.hpp"

namespace NovaGG {

PSG::PSG() {
    volume_.fill(0x0F);
}

void PSG::WriteData(uint8_t value) {
    if (value & 0x80) {
        int channel = (value >> 5) & 0x3;
        bool isVolume = (value & 0x10) != 0;
        latchedChannel_ = channel;
        latchedIsVolume_ = isVolume;

        if (isVolume) {
            volume_[channel] = value & 0xF;
        } else if (channel == 3) {
            noiseControl_ = value & 0x7;
        } else {
            toneFrequency_[channel] = uint16_t((toneFrequency_[channel] & 0x3F0) | (value & 0xF));
        }
    } else {
        if (latchedIsVolume_) {
            volume_[latchedChannel_] = value & 0xF;
        } else if (latchedChannel_ == 3) {
            noiseControl_ = value & 0x7;
        } else {
            toneFrequency_[latchedChannel_] =
                uint16_t((toneFrequency_[latchedChannel_] & 0xF) | ((value & 0x3F) << 4));
        }
    }
}

void PSG::Serialize(std::ostream& out) const {
    out.write(reinterpret_cast<const char*>(toneFrequency_.data()), std::streamsize(toneFrequency_.size() * sizeof(uint16_t)));
    out.write(reinterpret_cast<const char*>(volume_.data()), std::streamsize(volume_.size()));
    out.write(reinterpret_cast<const char*>(&noiseControl_), sizeof(noiseControl_));
    out.write(reinterpret_cast<const char*>(&stereoPanning_), sizeof(stereoPanning_));
    out.write(reinterpret_cast<const char*>(&latchedChannel_), sizeof(latchedChannel_));
    out.write(reinterpret_cast<const char*>(&latchedIsVolume_), sizeof(latchedIsVolume_));
}

void PSG::Deserialize(std::istream& in) {
    in.read(reinterpret_cast<char*>(toneFrequency_.data()), std::streamsize(toneFrequency_.size() * sizeof(uint16_t)));
    in.read(reinterpret_cast<char*>(volume_.data()), std::streamsize(volume_.size()));
    in.read(reinterpret_cast<char*>(&noiseControl_), sizeof(noiseControl_));
    in.read(reinterpret_cast<char*>(&stereoPanning_), sizeof(stereoPanning_));
    in.read(reinterpret_cast<char*>(&latchedChannel_), sizeof(latchedChannel_));
    in.read(reinterpret_cast<char*>(&latchedIsVolume_), sizeof(latchedIsVolume_));
}

}

namespace NovaGG {

void PSG::StartFadeOut(float durationSeconds, float sampleRate) {
    if (durationSeconds <= 0.0f || sampleRate <= 0.0f) {
        fading_ = false;
        fadeMultiplier_ = 0.0f;
        return;
    }
    fading_ = true;
    float totalSamples = durationSeconds * sampleRate;
    fadeStepPerSample_ = 1.0f / totalSamples;
}

float PSG::Step(uint32_t samplesElapsed) {
    if (fading_) {
        fadeMultiplier_ -= fadeStepPerSample_ * float(samplesElapsed);
        if (fadeMultiplier_ <= 0.0f) {
            fadeMultiplier_ = 0.0f;
            fading_ = false;
        }
    }
    return fadeMultiplier_;
}

}
