CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -I.

SRC = Core/Memory/Bus.cpp Core/CPU/Z80.cpp Core/GPU/VDP.cpp Core/Sound/PSG.cpp \
      Core/Input/Keypad.cpp Core/System/GgSystem.cpp main.cpp

all: NovaGG

NovaGG: $(SRC)
	$(CXX) $(CXXFLAGS) -o NovaGG $(SRC)

demo: NovaGG
	./NovaGG --demo

clean:
	rm -f NovaGG *.ppm
