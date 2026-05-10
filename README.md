# Traffic Simulation on a Directed Graph

Dijkstra's shortest-path algorithm with raylib visualization and animated entity movement.

## Group Members
- Qussay Abu Snana

## GitHub Repository
https://github.com/aboodabuserrieh/Operating-System

---

## Compilation

```bash
make milestone1   # build Dijkstra solver  -> ./dijkstra
make milestone2   # build static visualizer -> ./sim
make milestone3   # build animated sim      -> ./sim
make clean        # remove binaries
```

---

## Running

### Milestone 1 — Dijkstra (terminal output)
```bash
./dijkstra input.txt
```
Prints the shortest path and total weight, or "No path found".

### Milestone 2 / 3 — Graph Simulation (GUI)
```bash
./sim input.txt
```
Opens a window with the directed graph. Press **PLAY** to start the animation.

---

## Input File Format

```
N M
u1 v1 w1
u2 v2 w2
...
src dst
```

| Field | Meaning |
|-------|---------|
| N     | Number of nodes (0-indexed) |
| M     | Number of directed edges |
| u v w | Edge from node u to node v with weight w |
| src   | Source node |
| dst   | Destination node |

### Example (`input.txt`)
```
5 7
0 1 2
0 2 4
1 2 1
1 3 5
2 3 1
3 4 3
2 4 6
0 4
```
Shortest path: `0 -> 1 -> 2 -> 3 -> 4`  Total weight: `7`

---

## Animation Timing (Milestone 3)

| Location | Behaviour |
|----------|-----------|
| Source / Destination node | No wait |
| Intermediate node | Pause **1 second** |
| Edge with weight W | Travel time = W × 300 ms |

The orange circle is the moving entity.  
Green node = source, Red node = destination.
