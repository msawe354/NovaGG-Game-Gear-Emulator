CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -I.

SRC = Core/Memory/Bus.cpp Core/CPU/Z80.cpp Core/GPU/VDP.cpp Core/Sound/PSG.cpp \
      Core/Input/Keypad.cpp Core/System/GgSystem.cpp main.cpp
TEST_SRC = $(filter-out main.cpp,$(SRC)) tests/Z80InterruptTests.cpp

all: NovaGG

NovaGG: $(SRC)
	$(CXX) $(CXXFLAGS) -o NovaGG $(SRC)

test: NovaGGTests
	./NovaGGTests

NovaGGTests: $(TEST_SRC)
	$(CXX) $(CXXFLAGS) -o NovaGGTests $(TEST_SRC)

demo: NovaGG
	./NovaGG --demo

clean:
	rm -f NovaGG NovaGGTests *.ppm

.PHONY: all test demo clean
