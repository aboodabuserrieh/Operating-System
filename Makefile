CXX      = g++
CXXFLAGS = -Wall -std=c++17
RAYFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all: milestone1 milestone3

milestone1: main_ms1.cpp
	$(CXX) $(CXXFLAGS) main_ms1.cpp -o dijkstra

milestone2: sim.cpp
	$(CXX) $(CXXFLAGS) sim.cpp -o sim $(RAYFLAGS)

milestone3: sim.cpp
	$(CXX) $(CXXFLAGS) sim.cpp -o sim $(RAYFLAGS)

clean:
	rm -f dijkstra sim
