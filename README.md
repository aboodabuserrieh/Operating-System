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
make milestone5   # build IPC simulation (pipes)          -> ./sim
make milestone6   # build synchronized simulation (sems)  -> ./sim
make milestone7   # build scheduling simulation (FCFS/SJF) -> ./sim
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

### Milestone 6 — Node Synchronization via Semaphores (GUI + terminal log)
```bash
./sim input_ms6.txt
```
Travelers compete for shared nodes. Pulsing hollow circles = waiting outside node.
Solid circles = inside node (holds lock). Terminal prints waiting/arrived/finished log.

### Milestone 7 — Scheduling Algorithms (FCFS / SJF)
```bash
make milestone7
./sim -schd fcfs input_ms6.txt
./sim -schd sjf  input_ms6.txt
```
Run with FCFS (first-come first-served) or SJF (shortest next edge weight first).
The GUI title bar and HUD show which algorithm is active.

---

## Input File Format

### Milestones 1–3
```
N M
u1 v1 w1
...
src dst
```

### Milestones 4–7 (extended format)
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

### Milestone 6 — Node Synchronization via POSIX Named Semaphores

Each node has a **POSIX named semaphore** (initial value 1), created by the parent
before forking with `sem_open("/trafficsim_node_X", O_CREAT, 0666, 1)`.

**Child protocol for each node in its path:**
1. Send `MSG_WAITING` message to parent (GUI shows pulsing hollow circle)
2. Call `sem_wait(node_sem)` — blocks until the node is free
3. Send `MSG_ARRIVED` message to parent (GUI shows solid circle)
4. Sleep 1 second (critical section — exclusive node access)
5. Call `sem_post(node_sem)` — release the node
6. Sleep `weight × 300 ms` for edge traversal to next node

**Message format:** `int msg[3] = {type, node, next_node}`
- `type 1` (WAITING): traveler blocked outside node
- `type 0` (ARRIVED): traveler entered node (holds lock)

**Synchronization choice: POSIX named semaphores**
Named semaphores are the cleanest solution for mutual exclusion between sibling
processes: they are persistent across `fork()`, accessible by name from any process,
and automatically enforce the "at most one traveler per node" invariant with
`sem_wait`/`sem_post`. Mutexes would require shared memory setup; System V semaphores
would require more complex IPC key management. Named semaphores are unlinked by the
parent at exit to avoid `/dev/shm` leaks.

**Terminal log format (Milestone 6):**
```
[PID=1100] waiting for node 2
[PID=1101] waiting for node 2
[PID=1102] waiting for node 2
[PID=1100] arrived at node 2 | next node: 4
[PID=1100] finished
[PID=1101] arrived at node 2 | next node: 4
...
```

### Milestone 7 — Scheduling Algorithms

Replaces the random node-entry order (Milestone 6) with deterministic scheduling.
The parent process maintains a **waiting queue per node** and decides which blocked
child to wake up based on the chosen algorithm.

**IPC mechanism:**
Two unnamed pipes per traveler: one child→parent (status messages) and one
parent→child (the `'G'` go-signal). The child blocks on `read(p2c, &go, 1)` after
sending `MSG_WAITING`, and only proceeds into the node after the parent's scheduler
sends the signal.

**Algorithms implemented:**

| Algorithm | Description |
|-----------|-------------|
| FCFS | First-Come First-Served — travelers enter the node in the order they arrived at the queue |
| SJF  | Shortest Job First — the traveler with the shortest next edge weight goes first (shorter next step = shorter "burst") |

**How to run:**
```bash
./sim -schd fcfs input_ms6.txt   # FCFS scheduling
./sim -schd sjf  input_ms6.txt   # SJF scheduling
```

**Comparison — FCFS vs SJF:**

With `input_ms6.txt` (3 travelers, all passing through the same nodes):

- **FCFS**: travelers enter nodes strictly in arrival order. Total simulation time
  is predictable and fair — no traveler waits longer than necessary based on order.

- **SJF**: travelers with a lighter next edge go first. When next-edge weights
  differ, SJF reduces the average waiting time by letting "shorter" travelers pass
  through quickly. However, a traveler with a heavy next edge may wait longer
  (potential starvation if heavy travelers keep arriving).

On a uniform-weight graph like `input_ms6.txt` both algorithms produce identical
results since all edge weights are equal. To observe a difference, use a graph with
varying edge weights where travelers have different next-step costs.
