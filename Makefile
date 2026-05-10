CXX      = g++
CXXFLAGS = -Wall -std=c++17
RAYLIB_DIR = raylib-local
RAYFLAGS = -I$(RAYLIB_DIR)/include -L$(RAYLIB_DIR)/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all: milestone1 milestone3

setup:
	wget -q https://github.com/raysan5/raylib/releases/download/5.0/raylib-5.0_linux_amd64.tar.gz
	tar -xzf raylib-5.0_linux_amd64.tar.gz
	mkdir -p $(RAYLIB_DIR)/include $(RAYLIB_DIR)/lib
	cp raylib-5.0_linux_amd64/include/raylib.h $(RAYLIB_DIR)/include/
	cp raylib-5.0_linux_amd64/lib/libraylib.a  $(RAYLIB_DIR)/lib/
	rm -rf raylib-5.0_linux_amd64 raylib-5.0_linux_amd64.tar.gz
	@echo "Raylib ready!"

milestone1: main_ms1.cpp
	$(CXX) $(CXXFLAGS) main_ms1.cpp -o dijkstra

milestone2: sim_ms2.cpp
	$(CXX) $(CXXFLAGS) sim_ms2.cpp -o sim $(RAYFLAGS)

milestone3: sim.cpp
	$(CXX) $(CXXFLAGS) sim.cpp -o sim $(RAYFLAGS)

clean:
	rm -f dijkstra sim
