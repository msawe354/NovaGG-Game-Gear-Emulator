#pragma once
#include <cstdint>
#include <vector>
#include <iostream>

namespace NovaGG {

class VDP;
class PSG;
class Keypad;

class Bus {
public:
    static constexpr size_t RamSize = 0x2000;
    static constexpr size_t MaxRomSize = 0x100000;
    static constexpr size_t CartRamSize = 0x8000;

    Bus();

    void AttachVDP(VDP* vdp) { vdp_ = vdp; }
    void AttachPSG(PSG* psg) { psg_ = psg; }
    void AttachKeypad(Keypad* keypad) { keypad_ = keypad; }

    void LoadRom(const std::vector<uint8_t>& data);

    uint8_t Read8(uint16_t address) const;
    void Write8(uint16_t address, uint8_t value);

    uint8_t ReadPort(uint8_t port) const;
    void WritePort(uint8_t port, uint8_t value);

    void Serialize(std::ostream& out) const;
    void Deserialize(std::istream& in);

private:
    std::vector<uint8_t> rom_;
    std::vector<uint8_t> ram_;
    std::vector<uint8_t> cartRam_;

    uint8_t mapperControl_ = 0;
    uint8_t bank0_ = 0;
    uint8_t bank1_ = 1;
    uint8_t bank2_ = 2;

    VDP* vdp_ = nullptr;
    PSG* psg_ = nullptr;
    Keypad* keypad_ = nullptr;

    size_t RomBankCount() const;
};

}
