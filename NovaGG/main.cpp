#include "Core/System/GgSystem.hpp"
#include <iostream>
#include <fstream>
#include <cstring>

using namespace NovaGG;

static void SavePpm(const std::vector<uint8_t>& rgba, int width, int height, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << width << " " << height << "\n255\n";
    for (size_t i = 0; i < rgba.size(); i += 4) {
        f.put(char(rgba[i]));
        f.put(char(rgba[i + 1]));
        f.put(char(rgba[i + 2]));
    }
}

static void WriteVdpRegister(VDP& vdp, uint8_t reg, uint8_t value) {
    vdp.WriteControl(value);
    vdp.WriteControl(uint8_t(0x80 | reg));
}

static void SetVramAddress(VDP& vdp, uint16_t addr, bool forWrite) {
    vdp.WriteControl(uint8_t(addr & 0xFF));
    vdp.WriteControl(uint8_t(((addr >> 8) & 0x3F) | (forWrite ? 0x40 : 0x00)));
}

static void SetCramAddress(VDP& vdp, uint16_t addr) {
    vdp.WriteControl(uint8_t(addr & 0xFF));
    vdp.WriteControl(uint8_t(((addr >> 8) & 0x3F) | 0xC0));
}

static void RunDemo() {
    GgSystem sys;
    Bus& bus = sys.GetBus();

    std::vector<uint8_t> rom(0x4000, 0);
    rom[0] = 0x06; rom[1] = 0x05;
    rom[2] = 0x05;
    rom[3] = 0x20; rom[4] = uint8_t(-3);
    rom[5] = 0x3E; rom[6] = 0x2A;
    bus.LoadRom(rom);
    sys.GetCpu().SetEntryPoint(0);
    sys.StepInstructions(15);

    std::cout << "--- Z80 loop demo ---\n";
    std::cout << "A = 0x" << std::hex << int(sys.GetCpu().GetA()) << std::dec
               << " (expected 0x2a - loop ran to completion)\n\n";

    VDP& vdp = sys.GetVdp();

    WriteVdpRegister(vdp, 1, 0x40);
    WriteVdpRegister(vdp, 2, 0xFF);

    SetVramAddress(vdp, 0x3800, true);
    for (int i = 0; i < 32 * 28; i++) {
        vdp.WriteData(0x00);
        vdp.WriteData(0x00);
    }

    SetVramAddress(vdp, 0x0000, true);
    for (int row = 0; row < 8; row++) {
        vdp.WriteData(0xFF);
        vdp.WriteData(0x00);
        vdp.WriteData(0x00);
        vdp.WriteData(0x00);
    }

    SetCramAddress(vdp, 0x0002);
    vdp.WriteData(0x0F);
    vdp.WriteData(0x00);

    for (int i = 0; i < VDP::TotalLines * 2; i++) {
        vdp.Step(VDP::CyclesPerLine);
    }

    SavePpm(vdp.GetFramebufferRgba(), VDP::ScreenWidth, VDP::ScreenHeight, "gg_demo.ppm");
    std::cout << "--- VDP Mode 4 demo ---\n";
    std::cout << "Framebuffer written to: gg_demo.ppm (solid red background tile)\n";
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--demo") == 0) {
        RunDemo();
        return 0;
    }

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <rom.gg>\n";
        std::cout << "       " << argv[0] << " --demo\n";
        return 1;
    }

    GgSystem sys;
    if (!sys.LoadRom(argv[1])) {
        std::cerr << "Failed to load ROM: " << argv[1] << "\n";
        return 1;
    }

    try {
        for (int frame = 0; frame < 60; frame++) {
            sys.RunFrame();
        }
    } catch (const EmulationException& ex) {
        std::cerr << "Emulation stopped: " << ex.what() << "\n";
        return 1;
    }

    SavePpm(sys.GetVdp().GetFramebufferRgba(), VDP::ScreenWidth, VDP::ScreenHeight, "gg_frame.ppm");
    std::cout << "Ran 60 frames, wrote gg_frame.ppm\n";
    return 0;
}
