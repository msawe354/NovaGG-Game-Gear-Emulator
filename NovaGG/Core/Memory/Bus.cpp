#include "Bus.hpp"
#include "../GPU/VDP.hpp"
#include "../Sound/PSG.hpp"
#include "../Input/Keypad.hpp"
#include <algorithm>

namespace NovaGG {

Bus::Bus() : ram_(RamSize, 0), cartRam_(CartRamSize, 0) {}

void Bus::LoadRom(const std::vector<uint8_t>& data) {
    size_t length = std::min(data.size(), MaxRomSize);
    rom_.assign(data.begin(), data.begin() + length);
}

size_t Bus::RomBankCount() const {
    if (rom_.empty()) return 1;
    size_t count = rom_.size() / 0x4000;
    return count == 0 ? 1 : count;
}

uint8_t Bus::Read8(uint16_t address) const {
    if (address < 0x0400) {
        return address < rom_.size() ? rom_[address] : uint8_t(0xFF);
    }
    if (address < 0x4000) {
        size_t offset = size_t(bank0_ % RomBankCount()) * 0x4000 + address;
        return offset < rom_.size() ? rom_[offset] : uint8_t(0xFF);
    }
    if (address < 0x8000) {
        size_t offset = size_t(bank1_ % RomBankCount()) * 0x4000 + (address - 0x4000);
        return offset < rom_.size() ? rom_[offset] : uint8_t(0xFF);
    }
    if (address < 0xC000) {
        if (mapperControl_ & 0x08) {
            size_t ramBank = (mapperControl_ & 0x04) ? 1 : 0;
            return cartRam_[ramBank * 0x4000 + (address - 0x8000)];
        }
        size_t offset = size_t(bank2_ % RomBankCount()) * 0x4000 + (address - 0x8000);
        return offset < rom_.size() ? rom_[offset] : uint8_t(0xFF);
    }
    if (address < 0xE000) {
        return ram_[address - 0xC000];
    }
    return ram_[(address - 0xC000) & (RamSize - 1)];
}

void Bus::Write8(uint16_t address, uint8_t value) {
    if (address < 0x8000) {
        return;
    }
    if (address < 0xC000) {
        if (mapperControl_ & 0x08) {
            size_t ramBank = (mapperControl_ & 0x04) ? 1 : 0;
            cartRam_[ramBank * 0x4000 + (address - 0x8000)] = value;
        }
        return;
    }
    if (address < 0xE000) {
        ram_[address - 0xC000] = value;
        return;
    }

    if (address >= 0xFFFC) {
        switch (address) {
            case 0xFFFC: mapperControl_ = value; return;
            case 0xFFFD: bank0_ = value; return;
            case 0xFFFE: bank1_ = value; return;
            case 0xFFFF: bank2_ = value; return;
        }
    }
    ram_[(address - 0xC000) & (RamSize - 1)] = value;
}

uint8_t Bus::ReadPort(uint8_t port) const {
    if (port == 0x00) {
        return keypad_ ? keypad_->ReadStartButton() : 0xFF;
    }
    if (port <= 0x06) {
        return 0xFF;
    }
    if (port >= 0x40 && port <= 0x7F) {
        if ((port & 1) == 0) return vdp_ ? vdp_->ReadVCounter() : 0xFF;
        return 0xFF;
    }
    if (port >= 0x80 && port <= 0xBF) {
        bool isEven = (port & 1) == 0;
        if (isEven) return vdp_ ? const_cast<VDP*>(vdp_)->ReadData() : 0xFF;
        return vdp_ ? const_cast<VDP*>(vdp_)->ReadStatus() : 0xFF;
    }
    if (port >= 0xC0) {
        bool isEven = (port & 1) == 0;
        if (isEven) return keypad_ ? keypad_->ReadPortA() : 0xFF;
        return keypad_ ? keypad_->ReadPortB() : 0xFF;
    }
    return 0xFF;
}

void Bus::WritePort(uint8_t port, uint8_t value) {
    if (port <= 0x06) {
        if (port == 0x06 && psg_) psg_->WriteStereoPanning(value);
        return;
    }
    if (port >= 0x40 && port <= 0x7F) {
        if (psg_) psg_->WriteData(value);
        return;
    }
    if (port >= 0x80 && port <= 0xBF) {
        bool isEven = (port & 1) == 0;
        if (vdp_) {
            if (isEven) vdp_->WriteData(value);
            else vdp_->WriteControl(value);
        }
        return;
    }
}

void Bus::Serialize(std::ostream& out) const {
    out.write(reinterpret_cast<const char*>(ram_.data()), std::streamsize(ram_.size()));
    out.write(reinterpret_cast<const char*>(cartRam_.data()), std::streamsize(cartRam_.size()));
    out.write(reinterpret_cast<const char*>(&mapperControl_), sizeof(mapperControl_));
    out.write(reinterpret_cast<const char*>(&bank0_), sizeof(bank0_));
    out.write(reinterpret_cast<const char*>(&bank1_), sizeof(bank1_));
    out.write(reinterpret_cast<const char*>(&bank2_), sizeof(bank2_));
}

void Bus::Deserialize(std::istream& in) {
    in.read(reinterpret_cast<char*>(ram_.data()), std::streamsize(ram_.size()));
    in.read(reinterpret_cast<char*>(cartRam_.data()), std::streamsize(cartRam_.size()));
    in.read(reinterpret_cast<char*>(&mapperControl_), sizeof(mapperControl_));
    in.read(reinterpret_cast<char*>(&bank0_), sizeof(bank0_));
    in.read(reinterpret_cast<char*>(&bank1_), sizeof(bank1_));
    in.read(reinterpret_cast<char*>(&bank2_), sizeof(bank2_));
}

}
