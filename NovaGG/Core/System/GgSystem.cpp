#include "GgSystem.hpp"
#include <fstream>

namespace NovaGG {

GgSystem::GgSystem() : cpu_(bus_), vdp_(), psg_(), keypad_() {
    bus_.AttachVDP(&vdp_);
    bus_.AttachPSG(&psg_);
    bus_.AttachKeypad(&keypad_);

    vdp_.OnVblankIrq = [this]() { cpu_.RequestIrq(); };
    vdp_.OnLineIrq = [this]() { cpu_.RequestIrq(); };
}

bool GgSystem::LoadRom(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    auto size = static_cast<size_t>(f.tellg());
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), std::streamsize(size));
    bus_.LoadRom(data);
    cpu_.SetEntryPoint(0);
    return true;
}

void GgSystem::UpdateFpsCounter() {
    frameCountThisSecond_++;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - fpsWindowStart_).count();
    if (elapsed >= 1000) {
        currentFps_ = frameCountThisSecond_;
        frameCountThisSecond_ = 0;
        fpsWindowStart_ = now;
    }
}

void GgSystem::StepInstructions(int count) {
    for (int i = 0; i < count; i++) {
        uint32_t cycles = cpu_.Step();
        vdp_.Step(cycles);
    }
}

void GgSystem::RunFrame(int speedMultiplier) {
    if (speedMultiplier < 1) speedMultiplier = 1;
    const uint32_t targetCycles = uint32_t(CyclesPerFrame) * uint32_t(speedMultiplier);
    uint32_t accumulated = 0;
    while (accumulated < targetCycles) {
        uint32_t cycles = cpu_.Step();
        vdp_.Step(cycles);
        accumulated += cycles;
    }
    UpdateFpsCounter();
}

bool GgSystem::SaveState(const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const uint32_t magic = 0x4E475347;
    const uint32_t version = 2;
    f.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    f.write(reinterpret_cast<const char*>(&version), sizeof(version));
    bus_.Serialize(f);
    cpu_.Serialize(f);
    vdp_.Serialize(f);
    psg_.Serialize(f);
    return bool(f);
}

bool GgSystem::LoadState(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    uint32_t magic = 0, version = 0;
    f.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    f.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (magic != 0x4E475347 || version != 2) return false;
    bus_.Deserialize(f);
    cpu_.Deserialize(f);
    vdp_.Deserialize(f);
    psg_.Deserialize(f);
    return bool(f);
}

}
