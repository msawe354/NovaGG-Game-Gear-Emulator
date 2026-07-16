#pragma once
#include <cstdint>
#include <string>
#include <chrono>
#include "../CPU/Z80.hpp"
#include "../Memory/Bus.hpp"
#include "../GPU/VDP.hpp"
#include "../Sound/PSG.hpp"
#include "../Input/Keypad.hpp"

namespace NovaGG {

class GgSystem {
public:
    GgSystem();

    bool LoadRom(const std::string& path);
    void RunFrame(int speedMultiplier = 1);
    void StepInstructions(int count);

    bool SaveState(const std::string& path);
    bool LoadState(const std::string& path);

    int GetCurrentFps() const { return currentFps_; }

    Bus& GetBus() { return bus_; }
    Z80& GetCpu() { return cpu_; }
    VDP& GetVdp() { return vdp_; }
    Keypad& GetKeypad() { return keypad_; }

private:
    Bus bus_;
    Z80 cpu_;
    VDP vdp_;
    PSG psg_;
    Keypad keypad_;

    int frameCountThisSecond_ = 0;
    int currentFps_ = 0;
    std::chrono::steady_clock::time_point fpsWindowStart_ = std::chrono::steady_clock::now();

    static constexpr int CyclesPerFrame = 228 * 262 / 1;

    void UpdateFpsCounter();
};

}
