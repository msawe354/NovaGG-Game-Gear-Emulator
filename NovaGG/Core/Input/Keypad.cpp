#include "Keypad.hpp"

namespace NovaGG {

Keypad::Keypad() : heldMask_(0) {}

void Keypad::SetButtonState(GgButton button, bool pressed) {
    uint16_t bit = uint16_t(1u << int(button));
    if (pressed) heldMask_ |= bit;
    else heldMask_ &= ~bit;
}

uint8_t Keypad::ReadPortA() const noexcept {
    uint8_t up = (heldMask_ & (1 << int(GgButton::Up))) ? 0 : 1;
    uint8_t down = (heldMask_ & (1 << int(GgButton::Down))) ? 0 : 1;
    uint8_t left = (heldMask_ & (1 << int(GgButton::Left))) ? 0 : 1;
    uint8_t right = (heldMask_ & (1 << int(GgButton::Right))) ? 0 : 1;
    uint8_t b1 = (heldMask_ & (1 << int(GgButton::Button1))) ? 0 : 1;
    uint8_t b2 = (heldMask_ & (1 << int(GgButton::Button2))) ? 0 : 1;
    return uint8_t(up | (down << 1) | (left << 2) | (right << 3) | (b1 << 4) | (b2 << 5) | 0xC0);
}

uint8_t Keypad::ReadPortB() const noexcept {
    return 0xFF;
}

uint8_t Keypad::ReadStartButton() const noexcept {
    uint8_t start = (heldMask_ & (1 << int(GgButton::Start))) ? 0 : 1;
    return uint8_t(0x7F | (start << 7));
}

}
