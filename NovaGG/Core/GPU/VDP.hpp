#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <functional>
#include <iostream>

namespace NovaGG {

class VDP {
public:
    static constexpr int ScreenWidth = 160;
    static constexpr int ScreenHeight = 144;
    static constexpr int VramSize = 0x4000;
    static constexpr int CramSize = 0x40;
    static constexpr int CyclesPerLine = 228;
    static constexpr int TotalLines = 262;
    static constexpr int VisibleLines = 144;
    static constexpr int TopBorderLines = 24;

    VDP();

    void WriteControl(uint8_t value);
    void WriteData(uint8_t value);
    uint8_t ReadData();
    uint8_t ReadStatus();
    uint8_t ReadVCounter() const noexcept;

    void Step(uint32_t cycles);

    const std::vector<uint8_t>& GetFramebufferRgba() const { return framebufferRgba_; }

    std::function<void()> OnVblankIrq;
    std::function<void()> OnLineIrq;

    void Serialize(std::ostream& out) const;
    void Deserialize(std::istream& in);

private:
    std::vector<uint8_t> vram_;
    std::vector<uint8_t> cram_;
    std::array<uint8_t, 11> registers_{};

    uint16_t addressReg_ = 0;
    uint8_t codeReg_ = 0;
    bool controlByteLatched_ = false;
    uint8_t controlFirstByte_ = 0;
    uint8_t readBuffer_ = 0;

    bool vblankPending_ = false;
    bool spriteOverflow_ = false;
    bool spriteCollision_ = false;

    int currentLine_ = 0;
    uint32_t lineCycleCounter_ = 0;
    int lineIrqCounter_ = 0;

    std::vector<uint8_t> framebufferRgba_;

    uint16_t NameTableBase() const noexcept { return uint16_t((registers_[2] & 0x0E) << 10); }
    bool DisplayEnabled() const noexcept { return (registers_[1] & 0x40) != 0; }
    bool VblankIrqEnabled() const noexcept { return (registers_[1] & 0x20) != 0; }
    bool LineIrqEnabled() const noexcept { return (registers_[0] & 0x10) != 0; }

    void RenderLine(int line);
    void WriteCram(uint8_t addr, uint8_t value);
    void CramColorToRgba(uint16_t color, uint8_t& r, uint8_t& g, uint8_t& b) const;
};

}
