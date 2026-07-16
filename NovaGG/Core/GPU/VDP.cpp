#include "VDP.hpp"

namespace NovaGG {

VDP::VDP() : vram_(VramSize, 0), cram_(CramSize, 0) {
    framebufferRgba_.resize(size_t(ScreenWidth) * ScreenHeight * 4, 0);
}

void VDP::WriteControl(uint8_t value) {
    if (!controlByteLatched_) {
        controlFirstByte_ = value;
        controlByteLatched_ = true;
        return;
    }
    controlByteLatched_ = false;
    addressReg_ = uint16_t(controlFirstByte_ | ((value & 0x3F) << 8));
    codeReg_ = (value >> 6) & 0x3;

    if (codeReg_ == 0) {
        readBuffer_ = vram_[addressReg_ & (VramSize - 1)];
        addressReg_ = uint16_t((addressReg_ + 1) & (VramSize - 1));
    } else if (codeReg_ == 2) {
        uint8_t reg = value & 0x0F;
        if (reg < registers_.size()) registers_[reg] = controlFirstByte_;
    }
}

void VDP::WriteCram(uint8_t addr, uint8_t value) {
    cram_[addr & (CramSize - 1)] = value;
}

void VDP::WriteData(uint8_t value) {
    controlByteLatched_ = false;
    if (codeReg_ == 3) {
        WriteCram(uint8_t(addressReg_), value);
    } else {
        vram_[addressReg_ & (VramSize - 1)] = value;
    }
    readBuffer_ = value;
    addressReg_ = uint16_t((addressReg_ + 1) & (VramSize - 1));
}

uint8_t VDP::ReadData() {
    controlByteLatched_ = false;
    uint8_t result = readBuffer_;
    readBuffer_ = vram_[addressReg_ & (VramSize - 1)];
    addressReg_ = uint16_t((addressReg_ + 1) & (VramSize - 1));
    return result;
}

uint8_t VDP::ReadStatus() {
    uint8_t result = 0;
    if (vblankPending_) result |= 0x80;
    if (spriteOverflow_) result |= 0x40;
    if (spriteCollision_) result |= 0x20;
    vblankPending_ = false;
    spriteOverflow_ = false;
    spriteCollision_ = false;
    controlByteLatched_ = false;
    return result;
}

uint8_t VDP::ReadVCounter() const noexcept {
    int exported = currentLine_;
    if (exported > 218) exported -= 6;
    return uint8_t(exported);
}

void VDP::CramColorToRgba(uint16_t color, uint8_t& r, uint8_t& g, uint8_t& b) const {
    uint8_t low = uint8_t(color & 0xFF);
    uint8_t high = uint8_t((color >> 8) & 0xFF);
    uint8_t r4 = low & 0xF;
    uint8_t g4 = (low >> 4) & 0xF;
    uint8_t b4 = high & 0xF;
    r = uint8_t(r4 * 17);
    g = uint8_t(g4 * 17);
    b = uint8_t(b4 * 17);
}

void VDP::RenderLine(int line) {
    if (!DisplayEnabled()) {
        for (int x = 0; x < ScreenWidth; x++) {
            size_t idx = (size_t(line) * ScreenWidth + x) * 4;
            framebufferRgba_[idx + 0] = 0;
            framebufferRgba_[idx + 1] = 0;
            framebufferRgba_[idx + 2] = 0;
            framebufferRgba_[idx + 3] = 255;
        }
        return;
    }

    const int leftMargin = 48;
    const int topMargin = 24;
    int hscroll = registers_[8];
    int vscroll = registers_[9];
    int internalLine = (line + topMargin + vscroll) % 224;
    int tileRow = internalLine / 8;
    int fineRow = internalLine % 8;

    for (int x = 0; x < ScreenWidth; x++) {
        int internalCol = (x + leftMargin - hscroll + 256) % 256;
        int tileCol = internalCol / 8;
        int fineCol = internalCol % 8;

        uint16_t nameAddr = uint16_t(NameTableBase() + (tileRow * 32 + tileCol) * 2);
        uint8_t lo = vram_[nameAddr & (VramSize - 1)];
        uint8_t hi = vram_[(nameAddr + 1) & (VramSize - 1)];
        uint16_t tileIndex = uint16_t(lo | ((hi & 0x1) << 8));
        bool hFlip = (hi & 0x2) != 0;
        bool vFlip = (hi & 0x4) != 0;
        uint8_t paletteSelect = (hi & 0x8) ? 1 : 0;

        int row = vFlip ? (7 - fineRow) : fineRow;
        int col = hFlip ? (7 - fineCol) : fineCol;

        uint32_t tileBase = uint32_t(tileIndex) * 32 + uint32_t(row) * 4;
        uint8_t p0 = vram_[(tileBase + 0) & (VramSize - 1)];
        uint8_t p1 = vram_[(tileBase + 1) & (VramSize - 1)];
        uint8_t p2 = vram_[(tileBase + 2) & (VramSize - 1)];
        uint8_t p3 = vram_[(tileBase + 3) & (VramSize - 1)];

        int bit = 7 - col;
        uint8_t colorIndex = uint8_t(
            ((p0 >> bit) & 1) | (((p1 >> bit) & 1) << 1) |
            (((p2 >> bit) & 1) << 2) | (((p3 >> bit) & 1) << 3)
        );

        uint8_t cramIndex = uint8_t(paletteSelect * 16 + colorIndex);
        uint16_t color = uint16_t(cram_[cramIndex * 2] | (cram_[cramIndex * 2 + 1] << 8));

        uint8_t r, g, b;
        CramColorToRgba(color, r, g, b);

        size_t idx = (size_t(line) * ScreenWidth + x) * 4;
        framebufferRgba_[idx + 0] = r;
        framebufferRgba_[idx + 1] = g;
        framebufferRgba_[idx + 2] = b;
        framebufferRgba_[idx + 3] = 255;
    }
}

void VDP::Step(uint32_t cycles) {
    lineCycleCounter_ += cycles;
    while (lineCycleCounter_ >= uint32_t(CyclesPerLine)) {
        lineCycleCounter_ -= uint32_t(CyclesPerLine);

        if (currentLine_ < VisibleLines) {
            RenderLine(currentLine_);
        }

        if (currentLine_ < VisibleLines) {
            if (lineIrqCounter_ == 0) {
                lineIrqCounter_ = registers_[10];
                if (LineIrqEnabled() && OnLineIrq) OnLineIrq();
            } else {
                lineIrqCounter_--;
            }
        } else {
            lineIrqCounter_ = registers_[10];
        }

        currentLine_++;
        if (currentLine_ == VisibleLines) {
            vblankPending_ = true;
            if (VblankIrqEnabled() && OnVblankIrq) OnVblankIrq();
        }
        if (currentLine_ >= TotalLines) {
            currentLine_ = 0;
        }
    }
}

void VDP::Serialize(std::ostream& out) const {
    out.write(reinterpret_cast<const char*>(vram_.data()), std::streamsize(vram_.size()));
    out.write(reinterpret_cast<const char*>(cram_.data()), std::streamsize(cram_.size()));
    out.write(reinterpret_cast<const char*>(registers_.data()), std::streamsize(registers_.size()));
    out.write(reinterpret_cast<const char*>(&addressReg_), sizeof(addressReg_));
    out.write(reinterpret_cast<const char*>(&codeReg_), sizeof(codeReg_));
    out.write(reinterpret_cast<const char*>(&controlByteLatched_), sizeof(controlByteLatched_));
    out.write(reinterpret_cast<const char*>(&controlFirstByte_), sizeof(controlFirstByte_));
    out.write(reinterpret_cast<const char*>(&readBuffer_), sizeof(readBuffer_));
    out.write(reinterpret_cast<const char*>(&currentLine_), sizeof(currentLine_));
    out.write(reinterpret_cast<const char*>(&lineCycleCounter_), sizeof(lineCycleCounter_));
    out.write(reinterpret_cast<const char*>(&lineIrqCounter_), sizeof(lineIrqCounter_));
}

void VDP::Deserialize(std::istream& in) {
    in.read(reinterpret_cast<char*>(vram_.data()), std::streamsize(vram_.size()));
    in.read(reinterpret_cast<char*>(cram_.data()), std::streamsize(cram_.size()));
    in.read(reinterpret_cast<char*>(registers_.data()), std::streamsize(registers_.size()));
    in.read(reinterpret_cast<char*>(&addressReg_), sizeof(addressReg_));
    in.read(reinterpret_cast<char*>(&codeReg_), sizeof(codeReg_));
    in.read(reinterpret_cast<char*>(&controlByteLatched_), sizeof(controlByteLatched_));
    in.read(reinterpret_cast<char*>(&controlFirstByte_), sizeof(controlFirstByte_));
    in.read(reinterpret_cast<char*>(&readBuffer_), sizeof(readBuffer_));
    in.read(reinterpret_cast<char*>(&currentLine_), sizeof(currentLine_));
    in.read(reinterpret_cast<char*>(&lineCycleCounter_), sizeof(lineCycleCounter_));
    in.read(reinterpret_cast<char*>(&lineIrqCounter_), sizeof(lineIrqCounter_));
}

}
