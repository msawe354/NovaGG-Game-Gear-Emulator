#pragma once
#include <cstdint>

namespace NovaGG {

enum class GgButton {
    Up = 0, Down = 1, Left = 2, Right = 3,
    Button1 = 4, Button2 = 5, Start = 6
};

class Keypad {
public:
    Keypad();
    void SetButtonState(GgButton button, bool pressed);
    uint8_t ReadPortA() const noexcept;
    uint8_t ReadPortB() const noexcept;
    uint8_t ReadStartButton() const noexcept;

private:
    uint16_t heldMask_;
};

}
