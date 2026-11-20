#include "../Core/CPU/Z80.hpp"
#include "../Core/Memory/Bus.hpp"
#include <cassert>
#include <cstdint>
#include <sstream>
#include <vector>

using namespace NovaGG;

static void LoadProgram(Bus& bus, const std::vector<uint8_t>& program) {
    std::vector<uint8_t> rom(0x4000, 0);
    for (size_t i = 0; i < program.size(); ++i) rom[i] = program[i];
    bus.LoadRom(rom);
}

static void TestEiDefersPendingIrq() {
    Bus bus;
    LoadProgram(bus, {0xFB, 0x00});
    Z80 cpu(bus);
    cpu.RequestIrq();

    assert(cpu.Step() == 4);
    assert(cpu.GetPC() == 1);
    assert(cpu.Step() == 4);
    assert(cpu.GetPC() == 2);
    assert(cpu.Step() == 13);
    assert(cpu.GetPC() == 0x0038);
}

static void TestDiCancelsPendingIrqAfterEi() {
    Bus bus;
    LoadProgram(bus, {0xFB, 0xF3, 0x00});
    Z80 cpu(bus);
    cpu.RequestIrq();

    assert(cpu.Step() == 4);
    assert(cpu.Step() == 4);
    assert(cpu.GetPC() == 2);
    assert(cpu.Step() == 4);
    assert(cpu.GetPC() == 3);
}

static void TestNmiPreservesEiDelay() {
    Bus bus;
    std::vector<uint8_t> rom(0x4000, 0);
    rom[0] = 0x31;
    rom[1] = 0x02;
    rom[2] = 0xE0;
    rom[3] = 0xFB;
    rom[0x66] = 0xED;
    rom[0x67] = 0x45;
    bus.LoadRom(rom);
    Z80 cpu(bus);
    cpu.RequestIrq();

    assert(cpu.Step() == 10);
    assert(cpu.Step() == 4);
    cpu.RequestNmi();
    assert(cpu.Step() == 11);
    assert(cpu.GetPC() == 0x0066);
    assert(cpu.Step() == 14);
    assert(cpu.GetPC() == 4);
    assert(cpu.Step() == 13);
    assert(cpu.GetPC() == 0x0038);
}

static void TestEiDelaySurvivesSerialization() {
    Bus bus;
    LoadProgram(bus, {0xFB, 0x00});
    Z80 cpu(bus);
    assert(cpu.Step() == 4);

    std::stringstream state;
    cpu.Serialize(state);
    Z80 restoredCpu(bus);
    restoredCpu.Deserialize(state);
    restoredCpu.RequestIrq();

    assert(restoredCpu.Step() == 4);
    assert(restoredCpu.GetPC() == 2);
    assert(restoredCpu.Step() == 13);
    assert(restoredCpu.GetPC() == 0x0038);
}

int main() {
    TestEiDefersPendingIrq();
    TestDiCancelsPendingIrqAfterEi();
    TestNmiPreservesEiDelay();
    TestEiDelaySurvivesSerialization();
}
