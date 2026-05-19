# Traffic Simulation on a Directed Graph

Dijkstra's shortest-path algorithm with raylib visualization and animated entity movement.

## Group Members
- Qussay Abu Snana
- Abood Abu Sirrieh
- Lana Abu Hamed

## GitHub Repository
https://github.com/aboodabuserrieh/Operating-System

---

## Compilation

```bash
make milestone1   # build Dijkstra solver          -> ./dijkstra
make milestone2   # build static graph visualizer  -> ./sim
make milestone3   # build animated simulation       -> ./sim
make milestone4   # build multi-traveler (fork)     -> ./sim
make milestone5   # build IPC simulation (pipes)    -> ./sim
make clean        # remove compiled binaries
```

Run `make setup` first to download raylib (Linux only).

---

## Running

### Milestone 1 — Dijkstra (terminal output)
```bash
./dijkstra input.txt
```
Prints the shortest path and total weight, or "No path found".

### Milestones 2 / 3 — Graph Simulation (GUI, single traveler)
```bash
./sim input.txt
```
Opens a window with the directed graph. Press **PLAY** to animate the shortest path.

### Milestone 4 — Multiple Travelers via `fork` (GUI)
```bash
./sim input_ms4.txt
```
Press **PLAY** to animate all travelers simultaneously in different colors.

### Milestone 5 — IPC via Pipes (GUI + terminal log)
```bash
./sim input_ms4.txt
```
Travelers start automatically. Terminal prints arrival log for each traveler.

---

## Input File Format

### Milestones 1–3
```
N M
u1 v1 w1
...
src dst
```

### Milestones 4–5 (extended format)
```
# graph definition
N M
u1 v1 w1
...
# travelers
K
src1 dst1
src2 dst2
...
```

| Field | Meaning |
|-------|---------|
| N     | Number of nodes (0-indexed) |
| M     | Number of directed edges |
| u v w | Edge from u to v with weight w |
| K     | Number of travelers |
| src dst | Source and destination node per traveler |

Lines starting with `#` are comments and are ignored.

### Example (`input_ms4.txt`)
```
# graph definition
5 7
0 1 4
0 2 2
1 3 5
2 1 1
2 3 8
3 4 2
1 4 6
# travelers
2
0 4
2 3
```

---

## Animation Timing (Milestones 3–5)

| Location | Behaviour |
|----------|-----------|
| Source / Destination node | No wait |
| Intermediate node (MS3 only) | Pause 1 second |
| Edge with weight W | Travel time = W × 300 ms |

---

## Milestone Descriptions

### Milestone 1 — Dijkstra (terminal)
Reads a directed weighted graph and source/destination from a file. Computes the
shortest path using Dijkstra's algorithm with a min-heap priority queue and prints
the path and total cost to stdout.

### Milestone 2 — Static Graph Visualization
Renders the directed graph in a raylib window. Nodes are arranged in a circle;
edges are drawn as arrows with weight labels.

### Milestone 3 — Animated Simulation
Adds Dijkstra pathfinding to the GUI. A colored circle animates along the shortest
path with speed proportional to edge weights. Source node is green, destination red.

### Milestone 4 — Multiple Processes (`fork`)
Supports multiple travelers defined in the extended input format. The parent process
computes all Dijkstra paths, then forks one child per traveler. Each child prints
`[PID] started` and sleeps (via `pause()`). The parent manages the GUI, animating
all travelers simultaneously in different colors. When a traveler reaches its
destination the parent sends `SIGTERM` to its child and waits for it.

### Milestone 5 — IPC via Unnamed Pipes
Each child process independently computes its own Dijkstra path. As it "travels",
it sends `{current_node, next_node}` messages to the parent through an unnamed pipe,
sleeping `weight × 300 ms` between messages to match the animation speed. The parent
reads all pipes in non-blocking mode (`O_NONBLOCK`) inside the game loop, updates
traveler positions in the GUI, and prints the arrival log to the terminal:

```
[PID=1021] arrived at node 0 | next node: 2
[PID=1022] arrived at node 2 | next node: 1
...
[PID=1022] arrived at node 3 | DESTINATION
[PID=1022] finished
```

**IPC choice: unnamed pipes**
Pipes were chosen because communication is strictly one-directional (child → parent)
and scoped to the parent-child relationship, making them the simplest and most
appropriate mechanism. Shared memory would require additional synchronization
(semaphores) with no benefit here.
