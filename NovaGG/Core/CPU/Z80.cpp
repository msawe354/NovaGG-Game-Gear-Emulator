#include "Z80.hpp"
#include "../Memory/Bus.hpp"
#include <sstream>
#include <iomanip>
#include <fstream>

namespace NovaGG {

std::string EmulationException::ToHex(uint16_t v) {
    std::ostringstream ss;
    ss << std::hex << std::setw(4) << std::setfill('0') << v;
    return ss.str();
}

Z80::Z80(Bus& bus) : bus_(bus) { Reset(); }

void Z80::Reset() {
    a_ = f_ = b_ = c_ = d_ = e_ = h_ = l_ = 0;
    a2_ = f2_ = b2_ = c2_ = d2_ = e2_ = h2_ = l2_ = 0;
    ix_ = iy_ = 0;
    sp_ = 0xFFFF;
    pc_ = 0;
    i_ = r_ = 0;
    iff1_ = iff2_ = false;
    im_ = 0;
    halted_ = false;
    irqPending_ = false;
    nmiPending_ = false;
}

uint8_t Z80::Fetch8() {
    uint8_t value = bus_.Read8(pc_);
    pc_++;
    r_ = uint8_t((r_ & 0x80) | ((r_ + 1) & 0x7F));
    return value;
}

uint16_t Z80::Fetch16() {
    uint8_t lo = Fetch8();
    uint8_t hi = Fetch8();
    return uint16_t(lo | (hi << 8));
}

uint8_t Z80::GetReg8(int index) {
    switch (index) {
        case 0: return b_;
        case 1: return c_;
        case 2: return d_;
        case 3: return e_;
        case 4: return h_;
        case 5: return l_;
        case 6: return bus_.Read8(GetHL());
        case 7: return a_;
    }
    return 0;
}

void Z80::SetReg8(int index, uint8_t value) {
    switch (index) {
        case 0: b_ = value; break;
        case 1: c_ = value; break;
        case 2: d_ = value; break;
        case 3: e_ = value; break;
        case 4: h_ = value; break;
        case 5: l_ = value; break;
        case 6: bus_.Write8(GetHL(), value); break;
        case 7: a_ = value; break;
    }
}

uint16_t Z80::GetRegPair(int index) {
    switch (index) {
        case 0: return GetBC();
        case 1: return GetDE();
        case 2: return GetHL();
        case 3: return sp_;
    }
    return 0;
}

void Z80::SetRegPair(int index, uint16_t value) {
    switch (index) {
        case 0: SetBC(value); break;
        case 1: SetDE(value); break;
        case 2: SetHL(value); break;
        case 3: sp_ = value; break;
    }
}

void Z80::Push(uint16_t value) {
    sp_ -= 2;
    bus_.Write8(sp_, uint8_t(value));
    bus_.Write8(uint16_t(sp_ + 1), uint8_t(value >> 8));
}

uint16_t Z80::Pop() {
    uint8_t lo = bus_.Read8(sp_);
    uint8_t hi = bus_.Read8(uint16_t(sp_ + 1));
    sp_ += 2;
    return uint16_t(lo | (hi << 8));
}

bool Z80::Parity(uint8_t value) {
    int bits = 0;
    for (int i = 0; i < 8; i++) if (value & (1 << i)) bits++;
    return (bits % 2) == 0;
}

void Z80::SetXY(uint8_t value) {
    f_ = uint8_t((f_ & ~(FLAG_X | FLAG_Y)) | (value & (FLAG_X | FLAG_Y)));
}

bool Z80::CheckCondition(int cc) {
    switch (cc) {
        case 0: return !(f_ & FLAG_Z);
        case 1: return (f_ & FLAG_Z) != 0;
        case 2: return !(f_ & FLAG_C);
        case 3: return (f_ & FLAG_C) != 0;
        case 4: return !(f_ & FLAG_PV);
        case 5: return (f_ & FLAG_PV) != 0;
        case 6: return !(f_ & FLAG_S);
        case 7: return (f_ & FLAG_S) != 0;
    }
    return false;
}

void Z80::SetFlagsAdd8(uint8_t a, uint8_t b, int carryIn, uint8_t result) {
    f_ = 0;
    if (result & 0x80) f_ |= FLAG_S;
    if (result == 0) f_ |= FLAG_Z;
    if (((a & 0xF) + (b & 0xF) + carryIn) > 0xF) f_ |= FLAG_H;
    int full = int(a) + int(b) + carryIn;
    bool signA = (a & 0x80) != 0, signB = (b & 0x80) != 0, signR = (result & 0x80) != 0;
    if (signA == signB && signR != signA) f_ |= FLAG_PV;
    if (full > 0xFF) f_ |= FLAG_C;
    SetXY(result);
}

void Z80::SetFlagsSub8(uint8_t a, uint8_t b, int carryIn, uint8_t result) {
    f_ = FLAG_N;
    if (result & 0x80) f_ |= FLAG_S;
    if (result == 0) f_ |= FLAG_Z;
    if ((int(a & 0xF) - int(b & 0xF) - carryIn) < 0) f_ |= FLAG_H;
    int full = int(a) - int(b) - carryIn;
    bool signA = (a & 0x80) != 0, signB = (b & 0x80) != 0, signR = (result & 0x80) != 0;
    if (signA != signB && signR != signA) f_ |= FLAG_PV;
    if (full < 0) f_ |= FLAG_C;
    SetXY(result);
}

void Z80::SetFlagsLogic(uint8_t result, bool halfCarry) {
    f_ = 0;
    if (result & 0x80) f_ |= FLAG_S;
    if (result == 0) f_ |= FLAG_Z;
    if (halfCarry) f_ |= FLAG_H;
    if (Parity(result)) f_ |= FLAG_PV;
    SetXY(result);
}

void Z80::SetFlagsIncDec8(uint8_t before, uint8_t after, bool isDec) {
    uint8_t carry = f_ & FLAG_C;
    f_ = carry;
    if (after & 0x80) f_ |= FLAG_S;
    if (after == 0) f_ |= FLAG_Z;
    if (isDec) {
        if ((before & 0xF) == 0) f_ |= FLAG_H;
        if (before == 0x80) f_ |= FLAG_PV;
        f_ |= FLAG_N;
    } else {
        if ((after & 0xF) == 0) f_ |= FLAG_H;
        if (before == 0x7F) f_ |= FLAG_PV;
    }
    SetXY(after);
}

void Z80::EnterInterrupt(uint16_t vector) {
    halted_ = false;
    iff1_ = false;
    iff2_ = false;
    Push(pc_);
    pc_ = vector;
}

void Z80::RequestIrq(uint8_t dataBusValue) {
    irqPending_ = true;
    irqDataBus_ = dataBusValue;
}

void Z80::RequestNmi() {
    nmiPending_ = true;
}

uint32_t Z80::Step() {
    if (nmiPending_) {
        nmiPending_ = false;
        halted_ = false;
        iff2_ = iff1_;
        iff1_ = false;
        Push(pc_);
        pc_ = 0x0066;
        return 11;
    }

    if (irqPending_ && iff1_) {
        irqPending_ = false;
        if (im_ == 2) {
            uint16_t vector = uint16_t((i_ << 8) | irqDataBus_);
            uint16_t addr = uint16_t(bus_.Read8(vector) | (bus_.Read8(uint16_t(vector + 1)) << 8));
            EnterInterrupt(addr);
            return 19;
        }
        EnterInterrupt(0x0038);
        return 13;
    }

    if (halted_) {
        return 4;
    }

    if (tracingEnabled_) RecordTrace(pc_);
    uint8_t opcode = Fetch8();

    if (opcode == 0xCB) {
        uint8_t sub = Fetch8();
        return 8 + ExecuteCB(sub, 0, false);
    }
    if (opcode == 0xED) {
        uint8_t sub = Fetch8();
        return ExecuteED(sub);
    }
    if (opcode == 0xDD || opcode == 0xFD) {
        return ExecuteIndexed(opcode);
    }

    return ExecuteUnprefixed(opcode);
}

uint32_t Z80::ExecuteUnprefixed(uint8_t opcode) {
    int x = (opcode >> 6) & 0x3;
    int y = (opcode >> 3) & 0x7;
    int z = opcode & 0x7;

    if (x == 1) {
        if (y == 6 && z == 6) { halted_ = true; return 4; }
        SetReg8(y, GetReg8(z));
        return (y == 6 || z == 6) ? 7 : 4;
    }

    if (x == 2) {
        uint8_t operand = GetReg8(z);
        uint8_t av = a_;
        switch (y) {
            case 0: { uint8_t r = uint8_t(av + operand); SetFlagsAdd8(av, operand, 0, r); a_ = r; break; }
            case 1: { int cin = (f_ & FLAG_C) ? 1 : 0; uint8_t r = uint8_t(av + operand + cin); SetFlagsAdd8(av, operand, cin, r); a_ = r; break; }
            case 2: { uint8_t r = uint8_t(av - operand); SetFlagsSub8(av, operand, 0, r); a_ = r; break; }
            case 3: { int cin = (f_ & FLAG_C) ? 1 : 0; uint8_t r = uint8_t(av - operand - cin); SetFlagsSub8(av, operand, cin, r); a_ = r; break; }
            case 4: { uint8_t r = uint8_t(av & operand); SetFlagsLogic(r, true); a_ = r; break; }
            case 5: { uint8_t r = uint8_t(av ^ operand); SetFlagsLogic(r, false); a_ = r; break; }
            case 6: { uint8_t r = uint8_t(av | operand); SetFlagsLogic(r, false); a_ = r; break; }
            case 7: { uint8_t r = uint8_t(av - operand); SetFlagsSub8(av, operand, 0, r); break; }
        }
        return (z == 6) ? 7 : 4;
    }

    if (x == 3) {
        switch (z) {
            case 0: if (CheckCondition(y)) { pc_ = Pop(); return 11; } return 5;
            case 1:
                if (y % 2 == 0) {
                    int p = y / 2;
                    uint16_t v = Pop();
                    if (p == 3) SetAF(v); else SetRegPair(p, v);
                    return 10;
                }
                else {
                    int p = y / 2;
                    switch (p) {
                        case 0: pc_ = Pop(); return 10;
                        case 1: { uint16_t v = Pop(); SetHL(v); pc_ = v; return 10; }
                        case 2: sp_ = GetHL(); return 6;
                        case 3: { /* EXX */
                            std::swap(b_, b2_); std::swap(c_, c2_);
                            std::swap(d_, d2_); std::swap(e_, e2_);
                            std::swap(h_, h2_); std::swap(l_, l2_);
                            return 4;
                        }
                    }
                }
                break;
            case 2: { uint16_t addr = Fetch16(); if (CheckCondition(y)) pc_ = addr; return 10; }
            case 3:
                switch (y) {
                    case 0: pc_ = Fetch16(); return 10;
                    case 1: { uint8_t sub = Fetch8(); return 8 + ExecuteCB(sub, 0, false); }
                    case 2: { uint8_t n = Fetch8(); bus_.WritePort(n, a_); return 11; }
                    case 3: { uint8_t n = Fetch8(); a_ = bus_.ReadPort(n); return 11; }
                    case 4: { uint16_t v = Pop(); Push(GetHL()); SetHL(v); return 19; }
                    case 5: std::swap(d_, h_); std::swap(e_, l_); return 4;
                    case 6: iff1_ = iff2_ = false; return 4;
                    case 7: iff1_ = iff2_ = true; return 4;
                }
                break;
            case 4: { uint16_t addr = Fetch16(); if (CheckCondition(y)) { Push(pc_); pc_ = addr; return 17; } return 10; }
            case 5:
                if (y % 2 == 0) { int p = y / 2; Push((p == 3) ? GetAF() : GetRegPair(p)); return 11; }
                else if (y / 2 == 0) { uint16_t addr = Fetch16(); Push(pc_); pc_ = addr; return 17; }
                break;
            case 6: {
                uint8_t n = Fetch8();
                uint8_t av = a_;
                switch (y) {
                    case 0: { uint8_t r = uint8_t(av + n); SetFlagsAdd8(av, n, 0, r); a_ = r; break; }
                    case 1: { int cin = (f_ & FLAG_C) ? 1 : 0; uint8_t r = uint8_t(av + n + cin); SetFlagsAdd8(av, n, cin, r); a_ = r; break; }
                    case 2: { uint8_t r = uint8_t(av - n); SetFlagsSub8(av, n, 0, r); a_ = r; break; }
                    case 3: { int cin = (f_ & FLAG_C) ? 1 : 0; uint8_t r = uint8_t(av - n - cin); SetFlagsSub8(av, n, cin, r); a_ = r; break; }
                    case 4: { uint8_t r = uint8_t(av & n); SetFlagsLogic(r, true); a_ = r; break; }
                    case 5: { uint8_t r = uint8_t(av ^ n); SetFlagsLogic(r, false); a_ = r; break; }
                    case 6: { uint8_t r = uint8_t(av | n); SetFlagsLogic(r, false); a_ = r; break; }
                    case 7: { uint8_t r = uint8_t(av - n); SetFlagsSub8(av, n, 0, r); break; }
                }
                return 7;
            }
            case 7: Push(pc_); pc_ = uint16_t(y * 8); return 11;
        }
        return 4;
    }

    switch (z) {
        case 0:
            switch (y) {
                case 0: return 4;
                case 1: std::swap(a_, a2_); std::swap(f_, f2_); return 4;
                case 2: { int8_t d = int8_t(Fetch8()); b_--; if (b_ != 0) { pc_ = uint16_t(pc_ + d); return 13; } return 8; }
                case 3: { int8_t d = int8_t(Fetch8()); pc_ = uint16_t(pc_ + d); return 12; }
                default: { int8_t d = int8_t(Fetch8()); if (CheckCondition(y - 4)) { pc_ = uint16_t(pc_ + d); return 12; } return 7; }
            }
        case 1:
            if (y % 2 == 0) { SetRegPair(y / 2, Fetch16()); return 10; }
            else {
                uint16_t hl = GetHL();
                uint16_t operand = GetRegPair(y / 2);
                uint32_t result = uint32_t(hl) + uint32_t(operand);
                uint8_t flags = uint8_t(f_ & (FLAG_S | FLAG_Z | FLAG_PV));
                if (((hl & 0xFFF) + (operand & 0xFFF)) > 0xFFF) flags |= FLAG_H;
                if (result > 0xFFFF) flags |= FLAG_C;
                f_ = flags;
                SetXY(uint8_t(result >> 8));
                SetHL(uint16_t(result));
                return 11;
            }
        case 2: {
            switch (y) {
                case 0: bus_.Write8(GetBC(), a_); return 7;
                case 1: a_ = bus_.Read8(GetBC()); return 7;
                case 2: bus_.Write8(GetDE(), a_); return 7;
                case 3: a_ = bus_.Read8(GetDE()); return 7;
                case 4: { uint16_t addr = Fetch16(); bus_.Write8(addr, l_); bus_.Write8(uint16_t(addr + 1), h_); return 16; }
                case 5: { uint16_t addr = Fetch16(); uint8_t lo = bus_.Read8(addr); uint8_t hi = bus_.Read8(uint16_t(addr + 1)); SetHL(uint16_t(lo | (hi << 8))); return 16; }
                case 6: { uint16_t addr = Fetch16(); bus_.Write8(addr, a_); return 13; }
                case 7: { uint16_t addr = Fetch16(); a_ = bus_.Read8(addr); return 13; }
            }
            break;
        }
        case 3:
            if (y % 2 == 0) { SetRegPair(y / 2, uint16_t(GetRegPair(y / 2) + 1)); return 6; }
            else { SetRegPair(y / 2, uint16_t(GetRegPair(y / 2) - 1)); return 6; }
        case 4: { uint8_t before = GetReg8(y); uint8_t after = uint8_t(before + 1); SetReg8(y, after); SetFlagsIncDec8(before, after, false); return (y == 6) ? 11 : 4; }
        case 5: { uint8_t before = GetReg8(y); uint8_t after = uint8_t(before - 1); SetReg8(y, after); SetFlagsIncDec8(before, after, true); return (y == 6) ? 11 : 4; }
        case 6: { uint8_t n = Fetch8(); SetReg8(y, n); return (y == 6) ? 10 : 7; }
        case 7: {
            switch (y) {
                case 0: { uint8_t carry = (a_ & 0x80) ? 1 : 0; a_ = uint8_t((a_ << 1) | carry); f_ = uint8_t((f_ & (FLAG_S | FLAG_Z | FLAG_PV)) | carry); SetXY(a_); return 4; }
                case 1: { uint8_t carry = a_ & 0x1; a_ = uint8_t((a_ >> 1) | (carry << 7)); f_ = uint8_t((f_ & (FLAG_S | FLAG_Z | FLAG_PV)) | carry); SetXY(a_); return 4; }
                case 2: { uint8_t oldCarry = (f_ & FLAG_C) ? 1 : 0; uint8_t newCarry = (a_ & 0x80) ? 1 : 0; a_ = uint8_t((a_ << 1) | oldCarry); f_ = uint8_t((f_ & (FLAG_S | FLAG_Z | FLAG_PV)) | newCarry); SetXY(a_); return 4; }
                case 3: { uint8_t oldCarry = (f_ & FLAG_C) ? 1 : 0; uint8_t newCarry = a_ & 0x1; a_ = uint8_t((a_ >> 1) | (oldCarry << 7)); f_ = uint8_t((f_ & (FLAG_S | FLAG_Z | FLAG_PV)) | newCarry); SetXY(a_); return 4; }
                case 4: { // DAA
                    uint8_t a = a_;
                    uint8_t adjusted = a;
                    bool halfCarry = (f_ & FLAG_H) != 0;
                    bool carryIn = (f_ & FLAG_C) != 0;
                    bool subtract = (f_ & FLAG_N) != 0;

                    if (!subtract) {
                        if (halfCarry || (a & 0x0F) > 9) adjusted = uint8_t(adjusted + 0x06);
                        if (carryIn || a > 0x99) adjusted = uint8_t(adjusted + 0x60);
                    } else {
                        if (halfCarry) adjusted = uint8_t(adjusted - 0x06);
                        if (carryIn) adjusted = uint8_t(adjusted - 0x60);
                    }

                    uint8_t newF = uint8_t(f_ & FLAG_N);
                    if (carryIn || a > 0x99) newF |= FLAG_C;
                    newF |= uint8_t((a ^ adjusted) & FLAG_H);
                    if (adjusted & 0x80) newF |= FLAG_S;
                    if (adjusted == 0) newF |= FLAG_Z;
                    if (Parity(adjusted)) newF |= FLAG_PV;
                    f_ = newF;
                    a_ = adjusted;
                    SetXY(a_);
                    return 4;
                }
                case 5: { // CPL
                    f_ = uint8_t((f_ & (FLAG_S | FLAG_Z | FLAG_PV | FLAG_C)) | FLAG_H | FLAG_N);
                    a_ = uint8_t(~a_);
                    SetXY(a_);
                    return 4;
                }
                case 6: { // SCF
                    f_ = uint8_t((f_ & (FLAG_S | FLAG_Z | FLAG_PV)) | FLAG_C);
                    SetXY(a_);
                    return 4;
                }
                case 7: { // CCF
                    uint8_t oldCarry = (f_ & FLAG_C) ? 1 : 0;
                    f_ = uint8_t((f_ & (FLAG_S | FLAG_Z | FLAG_PV)) | (oldCarry ? FLAG_H : 0) | (oldCarry ? 0 : FLAG_C));
                    SetXY(a_);
                    return 4;
                }
            }
        }
    }
    throw EmulationException("Unknown opcode", pc_);
}

uint8_t Z80::RotateShiftOp(int op, uint8_t value) {
    uint8_t result = 0;
    uint8_t carry = 0;
    switch (op) {
        case 0: carry = (value & 0x80) ? 1 : 0; result = uint8_t((value << 1) | carry); break;
        case 1: carry = value & 0x1; result = uint8_t((value >> 1) | (carry << 7)); break;
        case 2: { uint8_t oldCarry = (f_ & FLAG_C) ? 1 : 0; carry = (value & 0x80) ? 1 : 0; result = uint8_t((value << 1) | oldCarry); break; }
        case 3: { uint8_t oldCarry = (f_ & FLAG_C) ? 1 : 0; carry = value & 0x1; result = uint8_t((value >> 1) | (oldCarry << 7)); break; }
        case 4: carry = (value & 0x80) ? 1 : 0; result = uint8_t(value << 1); break;
        case 5: carry = value & 0x1; result = uint8_t((value >> 1) | (value & 0x80)); break;
        case 6: carry = (value & 0x80) ? 1 : 0; result = uint8_t((value << 1) | 1); break;
        case 7: carry = value & 0x1; result = uint8_t(value >> 1); break;
    }
    f_ = carry;
    if (result & 0x80) f_ |= FLAG_S;
    if (result == 0) f_ |= FLAG_Z;
    if (Parity(result)) f_ |= FLAG_PV;
    SetXY(result);
    return result;
}

uint32_t Z80::ExecuteCB(uint8_t opcode, int16_t displacement, bool useDisplacement) {
    int group = (opcode >> 6) & 0x3;
    int y = (opcode >> 3) & 0x7;
    int z = opcode & 0x7;

    uint16_t addr = 0;
    uint8_t value;
    if (useDisplacement) {
        addr = uint16_t(GetHL() + displacement);
        value = bus_.Read8(addr);
    } else {
        value = GetReg8(z);
    }

    if (group == 0) {
        uint8_t result = RotateShiftOp(y, value);
        if (useDisplacement) bus_.Write8(addr, result);
        else SetReg8(z, result);
        return useDisplacement ? 15 : (z == 6 ? 15 : 8);
    }
    if (group == 1) {
        uint8_t testFlags = uint8_t(f_ & FLAG_C);
        if (!(value & (1 << y))) testFlags |= FLAG_Z | FLAG_PV;
        if (y == 7 && (value & 0x80)) testFlags |= FLAG_S;
        testFlags |= (value & (FLAG_X | FLAG_Y));
        testFlags |= FLAG_H;
        f_ = testFlags;
        return useDisplacement ? 12 : (z == 6 ? 12 : 8);
    }
    if (group == 2) {
        uint8_t result = uint8_t(value & ~(1 << y));
        if (useDisplacement) bus_.Write8(addr, result);
        else SetReg8(z, result);
        return useDisplacement ? 15 : (z == 6 ? 15 : 8);
    }
    uint8_t result = uint8_t(value | (1 << y));
    if (useDisplacement) bus_.Write8(addr, result);
    else SetReg8(z, result);
    return useDisplacement ? 15 : (z == 6 ? 15 : 8);
}

uint32_t Z80::ExecuteED(uint8_t opcode) {
    switch (opcode) {
        case 0xA0: case 0xA8: case 0xB0: case 0xB8: {
            uint8_t v = bus_.Read8(GetHL());
            bus_.Write8(GetDE(), v);
            bool inc = (opcode == 0xA0 || opcode == 0xB0);
            SetHL(uint16_t(GetHL() + (inc ? 1 : -1)));
            SetDE(uint16_t(GetDE() + (inc ? 1 : -1)));
            SetBC(uint16_t(GetBC() - 1));
            uint8_t n = uint8_t(v + a_);
            f_ = uint8_t((f_ & (FLAG_S | FLAG_Z | FLAG_C)) | (GetBC() != 0 ? FLAG_PV : 0));
            f_ = uint8_t((f_ & ~(FLAG_X | FLAG_Y)) | (n & FLAG_X) | ((n << 4) & FLAG_Y));
            if (opcode == 0xB0 || opcode == 0xB8) {
                if (GetBC() != 0) { pc_ = uint16_t(pc_ - 2); return 21; }
            }
            return 16;
        }
        case 0xA1: case 0xA9: case 0xB1: case 0xB9: {
            uint8_t v = bus_.Read8(GetHL());
            bool inc = (opcode == 0xA1 || opcode == 0xB1);
            uint8_t result = uint8_t(a_ - v);
            SetHL(uint16_t(GetHL() + (inc ? 1 : -1)));
            SetBC(uint16_t(GetBC() - 1));
            uint8_t halfBorrow = ((a_ & 0xF) < (v & 0xF)) ? 1 : 0;
            f_ = FLAG_N;
            if (result & 0x80) f_ |= FLAG_S;
            if (result == 0) f_ |= FLAG_Z;
            if (halfBorrow) f_ |= FLAG_H;
            if (GetBC() != 0) f_ |= FLAG_PV;
            uint8_t n = uint8_t(result - halfBorrow);
            f_ = uint8_t((f_ & ~(FLAG_X | FLAG_Y)) | (n & FLAG_X) | ((n << 4) & FLAG_Y));
            if (opcode == 0xB1 || opcode == 0xB9) {
                if (GetBC() != 0 && result != 0) { pc_ = uint16_t(pc_ - 2); return 21; }
            }
            return 16;
        }
        case 0xA2: case 0xAA: case 0xB2: case 0xBA: {
            bool inc = (opcode == 0xA2 || opcode == 0xB2);
            uint8_t v = bus_.ReadPort(c_);
            bus_.Write8(GetHL(), v);
            SetHL(uint16_t(GetHL() + (inc ? 1 : -1)));
            b_--;
            f_ = uint8_t(b_ == 0 ? FLAG_Z : 0);
            f_ |= FLAG_N;
            if (opcode == 0xB2 || opcode == 0xBA) {
                if (b_ != 0) { pc_ = uint16_t(pc_ - 2); return 21; }
            }
            return 16;
        }
        case 0xA3: case 0xAB: case 0xB3: case 0xBB: {
            bool inc = (opcode == 0xA3 || opcode == 0xB3);
            uint8_t v = bus_.Read8(GetHL());
            bus_.WritePort(c_, v);
            SetHL(uint16_t(GetHL() + (inc ? 1 : -1)));
            b_--;
            f_ = uint8_t(b_ == 0 ? FLAG_Z : 0);
            f_ |= FLAG_N;
            if (opcode == 0xB3 || opcode == 0xBB) {
                if (b_ != 0) { pc_ = uint16_t(pc_ - 2); return 21; }
            }
            return 16;
        }
    }

    int x = (opcode >> 6) & 0x3;
    int y = (opcode >> 3) & 0x7;
    int z = opcode & 0x7;

    if (x == 1) {
        switch (z) {
            case 0: {
                uint8_t v = bus_.ReadPort(c_);
                if (y != 6) SetReg8(y, v);
                f_ = uint8_t(f_ & FLAG_C);
                if (v & 0x80) f_ |= FLAG_S;
                if (v == 0) f_ |= FLAG_Z;
                if (Parity(v)) f_ |= FLAG_PV;
                SetXY(v);
                return 12;
            }
            case 1: {
                uint8_t v = (y == 6) ? 0 : GetReg8(y);
                bus_.WritePort(c_, v);
                return 12;
            }
            case 2: {
                uint16_t hl = GetHL();
                uint16_t operand = GetRegPair(y / 2);
                if (y % 2 == 0) {
                    int borrow = (f_ & FLAG_C) ? 1 : 0;
                    int full = int(hl) - int(operand) - borrow;
                    uint16_t result = uint16_t(full);
                    f_ = FLAG_N;
                    if (result & 0x8000) f_ |= FLAG_S;
                    if (result == 0) f_ |= FLAG_Z;
                    if ((int(hl & 0xFFF) - int(operand & 0xFFF) - borrow) < 0) f_ |= FLAG_H;
                    bool signH = (hl & 0x8000) != 0, signO = (operand & 0x8000) != 0, signR = (result & 0x8000) != 0;
                    if (signH != signO && signR != signH) f_ |= FLAG_PV;
                    if (full < 0) f_ |= FLAG_C;
                    SetXY(uint8_t(result >> 8));
                    SetHL(result);
                } else {
                    int carry = (f_ & FLAG_C) ? 1 : 0;
                    int full = int(hl) + int(operand) + carry;
                    uint16_t result = uint16_t(full);
                    f_ = 0;
                    if (result & 0x8000) f_ |= FLAG_S;
                    if (result == 0) f_ |= FLAG_Z;
                    if (((hl & 0xFFF) + (operand & 0xFFF) + carry) > 0xFFF) f_ |= FLAG_H;
                    bool signH = (hl & 0x8000) != 0, signO = (operand & 0x8000) != 0, signR = (result & 0x8000) != 0;
                    if (signH == signO && signR != signH) f_ |= FLAG_PV;
                    if (full > 0xFFFF) f_ |= FLAG_C;
                    SetXY(uint8_t(result >> 8));
                    SetHL(result);
                }
                return 15;
            }
            case 3: {
                uint16_t addr = Fetch16();
                if (y % 2 == 0) {
                    uint16_t v = GetRegPair(y / 2);
                    bus_.Write8(addr, uint8_t(v));
                    bus_.Write8(uint16_t(addr + 1), uint8_t(v >> 8));
                } else {
                    uint8_t lo = bus_.Read8(addr);
                    uint8_t hi = bus_.Read8(uint16_t(addr + 1));
                    SetRegPair(y / 2, uint16_t(lo | (hi << 8)));
                }
                return 20;
            }
            case 4: {
                uint8_t av = a_;
                uint8_t r = uint8_t(0 - av);
                SetFlagsSub8(0, av, 0, r);
                a_ = r;
                return 8;
            }
            case 5:
                iff1_ = iff2_;
                pc_ = Pop();
                return 14;
            case 6: {
                static const uint8_t table[8] = {0, 0, 1, 2, 0, 0, 1, 2};
                im_ = table[y];
                return 8;
            }
            case 7:
                switch (y) {
                    case 0: i_ = a_; return 9;
                    case 1: r_ = a_; return 9;
                    case 2: a_ = i_; f_ = uint8_t((f_ & FLAG_C) | (iff2_ ? FLAG_PV : 0)); if (a_ & 0x80) f_ |= FLAG_S; if (a_ == 0) f_ |= FLAG_Z; SetXY(a_); return 9;
                    case 3: a_ = r_; f_ = uint8_t((f_ & FLAG_C) | (iff2_ ? FLAG_PV : 0)); if (a_ & 0x80) f_ |= FLAG_S; if (a_ == 0) f_ |= FLAG_Z; SetXY(a_); return 9;
                    case 4: {
                        uint8_t hlv = bus_.Read8(GetHL());
                        uint8_t newHl = uint8_t(((a_ & 0xF) << 4) | (hlv >> 4));
                        uint8_t newA = uint8_t((a_ & 0xF0) | (hlv & 0xF));
                        bus_.Write8(GetHL(), newHl);
                        a_ = newA;
                        SetFlagsLogic(a_, false);
                        return 18;
                    }
                    case 5: {
                        uint8_t hlv = bus_.Read8(GetHL());
                        uint8_t newHl = uint8_t((hlv << 4) | (a_ & 0xF));
                        uint8_t newA = uint8_t((a_ & 0xF0) | (hlv >> 4));
                        bus_.Write8(GetHL(), newHl);
                        a_ = newA;
                        SetFlagsLogic(a_, false);
                        return 18;
                    }
                    default: return 8;
                }
        }
    }
    return 8;
}

uint32_t Z80::ExecuteIndexed(uint8_t prefix) {
    uint16_t& ixy = (prefix == 0xDD) ? ix_ : iy_;
    uint8_t opcode = Fetch8();

    switch (opcode) {
        case 0x21: ixy = Fetch16(); return 14;
        case 0x22: { uint16_t addr = Fetch16(); bus_.Write8(addr, uint8_t(ixy)); bus_.Write8(uint16_t(addr + 1), uint8_t(ixy >> 8)); return 20; }
        case 0x2A: { uint16_t addr = Fetch16(); uint8_t lo = bus_.Read8(addr); uint8_t hi = bus_.Read8(uint16_t(addr + 1)); ixy = uint16_t(lo | (hi << 8)); return 20; }
        case 0x23: ixy++; return 10;
        case 0x2B: ixy--; return 10;
        case 0x09: case 0x19: case 0x29: case 0x39: {
            int p = (opcode >> 4) & 0x3;
            uint16_t operand = (p == 2) ? ixy : GetRegPair(p);
            uint32_t result = uint32_t(ixy) + uint32_t(operand);
            uint8_t flags = uint8_t(f_ & (FLAG_S | FLAG_Z | FLAG_PV));
            if (((ixy & 0xFFF) + (operand & 0xFFF)) > 0xFFF) flags |= FLAG_H;
            if (result > 0xFFFF) flags |= FLAG_C;
            f_ = flags;
            SetXY(uint8_t(result >> 8));
            ixy = uint16_t(result);
            return 15;
        }
        case 0x34: { int8_t d = int8_t(Fetch8()); uint16_t addr = uint16_t(ixy + d); uint8_t before = bus_.Read8(addr); uint8_t after = uint8_t(before + 1); bus_.Write8(addr, after); SetFlagsIncDec8(before, after, false); return 23; }
        case 0x35: { int8_t d = int8_t(Fetch8()); uint16_t addr = uint16_t(ixy + d); uint8_t before = bus_.Read8(addr); uint8_t after = uint8_t(before - 1); bus_.Write8(addr, after); SetFlagsIncDec8(before, after, true); return 23; }
        case 0x36: { int8_t d = int8_t(Fetch8()); uint8_t n = Fetch8(); bus_.Write8(uint16_t(ixy + d), n); return 19; }
        case 0xE1: ixy = Pop(); return 14;
        case 0xE5: Push(ixy); return 15;
        case 0xE9: pc_ = ixy; return 8;
        case 0xF9: sp_ = ixy; return 10;
        case 0xE3: { uint16_t v = Pop(); Push(ixy); ixy = v; return 23; }
        case 0xCB: {
            int8_t d = int8_t(Fetch8());
            uint8_t sub = Fetch8();
            uint16_t savedHl = GetHL();
            SetHL(ixy);
            uint32_t cycles = ExecuteCB(sub, d, true);
            SetHL(savedHl);
            return 4 + cycles;
        }
    }

    if (opcode >= 0x40 && opcode <= 0x7F && opcode != 0x76) {
        int y = (opcode >> 3) & 0x7;
        int z = opcode & 0x7;
        if (y == 6 || z == 6) {
            int8_t d = int8_t(Fetch8());
            uint16_t addr = uint16_t(ixy + d);
            if (z == 6) { SetReg8(y, bus_.Read8(addr)); }
            else { bus_.Write8(addr, GetReg8(z)); }
            return 19;
        }
        SetReg8(y, GetReg8(z));
        return 8;
    }

    if (opcode >= 0x80 && opcode <= 0xBF && (opcode & 0x7) == 6) {
        int8_t d = int8_t(Fetch8());
        uint8_t operand = bus_.Read8(uint16_t(ixy + d));
        int y = (opcode >> 3) & 0x7;
        uint8_t av = a_;
        switch (y) {
            case 0: { uint8_t r = uint8_t(av + operand); SetFlagsAdd8(av, operand, 0, r); a_ = r; break; }
            case 1: { int cin = (f_ & FLAG_C) ? 1 : 0; uint8_t r = uint8_t(av + operand + cin); SetFlagsAdd8(av, operand, cin, r); a_ = r; break; }
            case 2: { uint8_t r = uint8_t(av - operand); SetFlagsSub8(av, operand, 0, r); a_ = r; break; }
            case 3: { int cin = (f_ & FLAG_C) ? 1 : 0; uint8_t r = uint8_t(av - operand - cin); SetFlagsSub8(av, operand, cin, r); a_ = r; break; }
            case 4: { uint8_t r = uint8_t(av & operand); SetFlagsLogic(r, true); a_ = r; break; }
            case 5: { uint8_t r = uint8_t(av ^ operand); SetFlagsLogic(r, false); a_ = r; break; }
            case 6: { uint8_t r = uint8_t(av | operand); SetFlagsLogic(r, false); a_ = r; break; }
            case 7: { uint8_t r = uint8_t(av - operand); SetFlagsSub8(av, operand, 0, r); break; }
        }
        return 19;
    }

    return 4 + ExecuteUnprefixed(opcode);
}

void Z80::Serialize(std::ostream& out) const {
    out.write(reinterpret_cast<const char*>(&a_), 1); out.write(reinterpret_cast<const char*>(&f_), 1);
    out.write(reinterpret_cast<const char*>(&b_), 1); out.write(reinterpret_cast<const char*>(&c_), 1);
    out.write(reinterpret_cast<const char*>(&d_), 1); out.write(reinterpret_cast<const char*>(&e_), 1);
    out.write(reinterpret_cast<const char*>(&h_), 1); out.write(reinterpret_cast<const char*>(&l_), 1);
    out.write(reinterpret_cast<const char*>(&a2_), 1); out.write(reinterpret_cast<const char*>(&f2_), 1);
    out.write(reinterpret_cast<const char*>(&b2_), 1); out.write(reinterpret_cast<const char*>(&c2_), 1);
    out.write(reinterpret_cast<const char*>(&d2_), 1); out.write(reinterpret_cast<const char*>(&e2_), 1);
    out.write(reinterpret_cast<const char*>(&h2_), 1); out.write(reinterpret_cast<const char*>(&l2_), 1);
    out.write(reinterpret_cast<const char*>(&ix_), sizeof(ix_));
    out.write(reinterpret_cast<const char*>(&iy_), sizeof(iy_));
    out.write(reinterpret_cast<const char*>(&sp_), sizeof(sp_));
    out.write(reinterpret_cast<const char*>(&pc_), sizeof(pc_));
    out.write(reinterpret_cast<const char*>(&i_), 1);
    out.write(reinterpret_cast<const char*>(&r_), 1);
    out.write(reinterpret_cast<const char*>(&iff1_), sizeof(iff1_));
    out.write(reinterpret_cast<const char*>(&iff2_), sizeof(iff2_));
    out.write(reinterpret_cast<const char*>(&im_), 1);
    out.write(reinterpret_cast<const char*>(&halted_), sizeof(halted_));
}

void Z80::Deserialize(std::istream& in) {
    in.read(reinterpret_cast<char*>(&a_), 1); in.read(reinterpret_cast<char*>(&f_), 1);
    in.read(reinterpret_cast<char*>(&b_), 1); in.read(reinterpret_cast<char*>(&c_), 1);
    in.read(reinterpret_cast<char*>(&d_), 1); in.read(reinterpret_cast<char*>(&e_), 1);
    in.read(reinterpret_cast<char*>(&h_), 1); in.read(reinterpret_cast<char*>(&l_), 1);
    in.read(reinterpret_cast<char*>(&a2_), 1); in.read(reinterpret_cast<char*>(&f2_), 1);
    in.read(reinterpret_cast<char*>(&b2_), 1); in.read(reinterpret_cast<char*>(&c2_), 1);
    in.read(reinterpret_cast<char*>(&d2_), 1); in.read(reinterpret_cast<char*>(&e2_), 1);
    in.read(reinterpret_cast<char*>(&h2_), 1); in.read(reinterpret_cast<char*>(&l2_), 1);
    in.read(reinterpret_cast<char*>(&ix_), sizeof(ix_));
    in.read(reinterpret_cast<char*>(&iy_), sizeof(iy_));
    in.read(reinterpret_cast<char*>(&sp_), sizeof(sp_));
    in.read(reinterpret_cast<char*>(&pc_), sizeof(pc_));
    in.read(reinterpret_cast<char*>(&i_), 1);
    in.read(reinterpret_cast<char*>(&r_), 1);
    in.read(reinterpret_cast<char*>(&iff1_), sizeof(iff1_));
    in.read(reinterpret_cast<char*>(&iff2_), sizeof(iff2_));
    in.read(reinterpret_cast<char*>(&im_), 1);
    in.read(reinterpret_cast<char*>(&halted_), sizeof(halted_));
}

}

namespace NovaGG {

void Z80::RecordTrace(uint16_t fetchAddr) {
    TraceEntry& e = traceBuffer_[traceNext_];
    e.pc = fetchAddr;
    e.a = a_;
    e.f = f_;
    traceNext_ = (traceNext_ + 1) % TraceBufferSize;
    if (traceCount_ < TraceBufferSize) traceCount_++;
}

bool Z80::DumpTraceToFile(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    size_t start = (traceCount_ < TraceBufferSize) ? 0 : traceNext_;
    for (size_t i = 0; i < traceCount_; i++) {
        const TraceEntry& e = traceBuffer_[(start + i) % TraceBufferSize];
        f << "PC=0x" << std::hex << std::setw(4) << std::setfill('0') << e.pc
          << " A=0x" << std::setw(2) << int(e.a)
          << " F=0x" << std::setw(2) << int(e.f)
          << std::dec << "\n";
    }
    return true;
}

}