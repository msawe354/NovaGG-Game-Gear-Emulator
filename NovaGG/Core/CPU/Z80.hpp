#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <iostream>
#include <array>

namespace NovaGG {

class Bus;

class EmulationException : public std::runtime_error {
public:
    EmulationException(const std::string& msg, uint16_t pc)
        : std::runtime_error(msg + " (PC=0x" + ToHex(pc) + ")"), pc_(pc) {}
    uint16_t pc() const { return pc_; }
private:
    static std::string ToHex(uint16_t v);
    uint16_t pc_;
};

class Z80 {
public:
    explicit Z80(Bus& bus);

    void Reset();
    void SetEntryPoint(uint16_t address) { pc_ = address; }
    uint32_t Step();
    void RequestIrq(uint8_t dataBusValue = 0xFF);
    void RequestNmi();
    bool IsHalted() const { return halted_; }

    uint16_t GetPC() const { return pc_; }
    uint16_t GetSP() const { return sp_; }
    uint8_t GetA() const { return a_; }
    uint8_t GetF() const { return f_; }
    uint16_t GetBC() const { return uint16_t((b_ << 8) | c_); }
    uint16_t GetDE() const { return uint16_t((d_ << 8) | e_); }
    uint16_t GetHL() const { return uint16_t((h_ << 8) | l_); }
    uint16_t GetIX() const { return ix_; }
    uint16_t GetIY() const { return iy_; }

    void Serialize(std::ostream& out) const;
    void Deserialize(std::istream& in);

    struct TraceEntry { uint16_t pc; uint8_t a, f; };
    void SetTracingEnabled(bool enabled) { tracingEnabled_ = enabled; }
    bool DumpTraceToFile(const std::string& path) const;

private:
    Bus& bus_;

    uint8_t a_ = 0, f_ = 0, b_ = 0, c_ = 0, d_ = 0, e_ = 0, h_ = 0, l_ = 0;
    uint8_t a2_ = 0, f2_ = 0, b2_ = 0, c2_ = 0, d2_ = 0, e2_ = 0, h2_ = 0, l2_ = 0;
    uint16_t ix_ = 0, iy_ = 0, sp_ = 0, pc_ = 0;
    uint8_t i_ = 0, r_ = 0;
    bool iff1_ = false, iff2_ = false;
    uint8_t eiDelay_ = 0;
    uint8_t im_ = 0;
    bool halted_ = false;
    bool irqPending_ = false;
    bool nmiPending_ = false;
    uint8_t irqDataBus_ = 0xFF;

    static constexpr size_t TraceBufferSize = 1000;
    std::array<TraceEntry, TraceBufferSize> traceBuffer_{};
    size_t traceCount_ = 0;
    size_t traceNext_ = 0;
    bool tracingEnabled_ = false;
    void RecordTrace(uint16_t fetchAddr);

    static constexpr uint8_t FLAG_C = 0x01;
    static constexpr uint8_t FLAG_N = 0x02;
    static constexpr uint8_t FLAG_PV = 0x04;
    static constexpr uint8_t FLAG_X = 0x08;
    static constexpr uint8_t FLAG_H = 0x10;
    static constexpr uint8_t FLAG_Y = 0x20;
    static constexpr uint8_t FLAG_Z = 0x40;
    static constexpr uint8_t FLAG_S = 0x80;

    uint16_t GetAF() const { return uint16_t((a_ << 8) | f_); }
    void SetAF(uint16_t v) { a_ = uint8_t(v >> 8); f_ = uint8_t(v); }
    void SetBC(uint16_t v) { b_ = uint8_t(v >> 8); c_ = uint8_t(v); }
    void SetDE(uint16_t v) { d_ = uint8_t(v >> 8); e_ = uint8_t(v); }
    void SetHL(uint16_t v) { h_ = uint8_t(v >> 8); l_ = uint8_t(v); }

    uint8_t Fetch8();
    uint16_t Fetch16();
    uint8_t GetReg8(int index);
    void SetReg8(int index, uint8_t value);
    uint16_t GetRegPair(int index);
    void SetRegPair(int index, uint16_t value);

    void Push(uint16_t value);
    uint16_t Pop();

    bool CheckCondition(int cc);

    void SetFlagsAdd8(uint8_t a, uint8_t b, int carryIn, uint8_t result);
    void SetFlagsSub8(uint8_t a, uint8_t b, int carryIn, uint8_t result);
    void SetFlagsLogic(uint8_t result, bool carry);
    void SetFlagsIncDec8(uint8_t before, uint8_t after, bool isDec);
    void SetXY(uint8_t value);
    bool Parity(uint8_t value);

    uint32_t ExecuteUnprefixed(uint8_t opcode);
    uint32_t ExecuteCB(uint8_t opcode, int16_t displacement, bool useDisplacement);
    uint32_t ExecuteED(uint8_t opcode);
    uint32_t ExecuteIndexed(uint8_t prefix);

    uint8_t RotateShiftOp(int op, uint8_t value);
    void EnterInterrupt(uint16_t vector);
};

}
