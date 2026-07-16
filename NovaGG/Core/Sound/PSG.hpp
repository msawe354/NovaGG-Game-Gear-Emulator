#pragma once
#include <cstdint>
#include <array>
#include <iostream>

namespace NovaGG {

class PSG {
public:
    PSG();
    void WriteData(uint8_t value);
    void WriteStereoPanning(uint8_t value) noexcept { stereoPanning_ = value; }
    uint8_t GetStereoPanning() const noexcept { return stereoPanning_; }

    uint16_t GetToneFrequency(int channel) const noexcept { return toneFrequency_[channel]; }
    uint8_t GetVolume(int channel) const noexcept { return volume_[channel]; }
    uint8_t GetNoiseControl() const noexcept { return noiseControl_; }

    void Serialize(std::ostream& out) const;
    void Deserialize(std::istream& in);

    void StartFadeOut(float durationSeconds, float sampleRate);
    float Step(uint32_t samplesElapsed = 1);
    float GetFadeMultiplier() const noexcept { return fadeMultiplier_; }
    bool IsFadingOut() const noexcept { return fading_; }

private:
    std::array<uint16_t, 3> toneFrequency_{};
    std::array<uint8_t, 4> volume_{};
    uint8_t noiseControl_ = 0;
    uint8_t stereoPanning_ = 0xFF;

    int latchedChannel_ = 0;
    bool latchedIsVolume_ = false;

    bool fading_ = false;
    float fadeMultiplier_ = 1.0f;
    float fadeStepPerSample_ = 0.0f;
};

}
