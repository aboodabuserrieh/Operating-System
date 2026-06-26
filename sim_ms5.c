/* sim_ms5.c – Milestone 5: IPC via unnamed pipes, children compute own paths */
#define _DEFAULT_SOURCE
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define INF           INT_MAX
#define MAX_TRAVELERS 16

typedef struct { int u, v, weight; } Edge;
typedef struct { Vector2 pos; int id; } Node;
typedef struct { int v, weight; }  AdjEdge;
typedef struct { AdjEdge *edges; int count, cap; } AdjList;
typedef struct { int dist, node; } HeapNode;
typedef struct { HeapNode *data; int size, cap; } MinHeap;

static void adjAdd(AdjList *l, int v, int w) {
    if (l->count >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 4;
        l->edges = realloc(l->edges, l->cap * sizeof(AdjEdge));
    }
    l->edges[l->count++] = (AdjEdge){v, w};
}

static void hPush(MinHeap *h, int d, int n) {
    if (h->size >= h->cap) {
        h->cap = h->cap ? h->cap * 2 : 8;
        h->data = realloc(h->data, h->cap * sizeof(HeapNode));
    }
    int i = h->size++;
    h->data[i] = (HeapNode){d, n};
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->data[p].dist <= h->data[i].dist) break;
        HeapNode t = h->data[p]; h->data[p] = h->data[i]; h->data[i] = t;
        i = p;
    }
}

static HeapNode hPop(MinHeap *h) {
    HeapNode top = h->data[0];
    h->data[0] = h->data[--h->size];
    int i = 0;
    for (;;) {
        int l = 2*i+1, r = 2*i+2, s = i;
        if (l < h->size && h->data[l].dist < h->data[s].dist) s = l;
        if (r < h->size && h->data[r].dist < h->data[s].dist) s = r;
        if (s == i) break;
        HeapNode t = h->data[i]; h->data[i] = h->data[s]; h->data[s] = t;
        i = s;
    }
    return top;
}

static int *calcPath(int N, AdjList *adj, int src, int dst, int *lenOut) {
    int *dist = malloc(N * sizeof(int)), *par = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) { dist[i] = INF; par[i] = -1; }
    MinHeap h = {NULL, 0, 0};
    dist[src] = 0; hPush(&h, 0, src);
    while (h.size) {
        HeapNode c = hPop(&h);
        if (c.dist > dist[c.node]) continue;
        if (c.node == dst) break;
        for (int i = 0; i < adj[c.node].count; i++) {
            int v = adj[c.node].edges[i].v, w = adj[c.node].edges[i].weight;
            if (dist[c.node] + w < dist[v]) {
                dist[v] = dist[c.node] + w; par[v] = c.node;
                hPush(&h, dist[v], v);
            }
        }
    }
    int *path = NULL; *lenOut = 0;
    if (dist[dst] != INF) {
        int *tmp = malloc(N * sizeof(int)), len = 0;
        for (int v = dst; v != -1; v = par[v]) tmp[len++] = v;
        path = malloc(len * sizeof(int));
        for (int i = 0; i < len; i++) path[i] = tmp[len - 1 - i];
        *lenOut = len; free(tmp);
    }
    free(dist); free(par); free(h.data);
    return path;
}

static int edgeW(int u, int v, Edge *edges, int M) {
    for (int i = 0; i < M; i++)
        if (edges[i].u == u && edges[i].v == v) return edges[i].weight;
    return 1;
}

static void drawArrow(Vector2 s, Vector2 e, int w, int r) {
    float dx = e.x - s.x, dy = e.y - s.y, len = sqrtf(dx*dx + dy*dy);
    if (len < 0.001f) return;
    float ux = dx/len, uy = dy/len;
    Vector2 as = {s.x + ux*r, s.y + uy*r}, ae = {e.x - ux*r, e.y - uy*r};
    DrawLineEx(as, ae, 2, DARKGRAY);
    float a = atan2f(uy, ux), sz = 15;
    DrawTriangle(ae,
        (Vector2){ae.x - sz*cosf(a - PI/6), ae.y - sz*sinf(a - PI/6)},
        (Vector2){ae.x - sz*cosf(a + PI/6), ae.y - sz*sinf(a + PI/6)}, MAROON);
    Vector2 mid = {(as.x+ae.x)/2 - uy*18, (as.y+ae.y)/2 + ux*18};
    DrawText(TextFormat("%d", w), (int)mid.x - 10, (int)mid.y - 10, 20, DARKBLUE);
}

static void skipComments(FILE *f) {
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '#') { while ((c = fgetc(f)) != EOF && c != '\n'); }
        else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') { ungetc(c, f); return; }
    }
}

/*
 * IPC design: one unnamed pipe per traveler (child writes, parent reads).
 * Each message is two ints: {current_node, next_node}.
 * next_node == -1 signals DESTINATION.
 * Pipe EOF (child closed write end after exiting) signals "finished".
 *
 * Child sleeps weight*300 ms between messages so parent animation stays in sync.
 */

typedef enum { T5_ACTIVE, T5_DEST, T5_FINISHED, T5_NOPATH } TState5;

typedef struct {
    pid_t   pid;
    int     pipeR;
    int     pipeW;   /* parent -> child */
    TState5 state;
    Vector2 pos;
    Color   color;
    int     src, dst;
    int     curNode, nextNode;
    float   timer, animTotal;
    int     msgCount;
} Traveler5;

static const Color TCOLORS[MAX_TRAVELERS] = {
    {255,150,  0,255}, {  0,100,200,255}, {150,  0,200,255}, {  0,180, 50,255},
    {220, 50, 50,255}, {200,180,  0,255}, {  0,190,190,255}, {220, 80,180,255},
    { 80,200, 80,255}, {170, 90, 30,255}, { 30,150,200,255}, {200,200, 50,255},
    { 90, 90,210,255}, {200, 30,100,255}, { 30, 80,180,255}, {120,180, 80,255},
};

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Usage: ./sim <file_name>\n"); return 1; }

    FILE *f = fopen(argv[1], "r");
    if (!f) { fprintf(stderr, "Cannot open %s\n", argv[1]); return 1; }

    skipComments(f);
    int N, M;
    fscanf(f, "%d %d", &N, &M);

    Edge    *edges = malloc(M * sizeof(Edge));
    AdjList *adj   = calloc(N, sizeof(AdjList));
    for (int i = 0; i < M; i++) {
        skipComments(f);
        fscanf(f, "%d %d %d", &edges[i].u, &edges[i].v, &edges[i].weight);
        adjAdd(&adj[edges[i].u], edges[i].v, edges[i].weight);
    }

    skipComments(f);
    int K;
    fscanf(f, "%d", &K);
    if (K > MAX_TRAVELERS) K = MAX_TRAVELERS;

    Traveler5 travelers[MAX_TRAVELERS];
    memset(travelers, 0, sizeof(travelers));
    for (int i = 0; i < K; i++) {
        skipComments(f);
        fscanf(f, "%d %d", &travelers[i].src, &travelers[i].dst);
        travelers[i].color    = TCOLORS[i % MAX_TRAVELERS];
        travelers[i].state    = T5_ACTIVE;
        travelers[i].curNode  = -1;
        travelers[i].nextNode = -1;
    }
    fclose(f);

    /* create all pipes before forking */
    int pfd[MAX_TRAVELERS][2];    /* child -> parent */
    int p2c[MAX_TRAVELERS][2];    /* parent -> child */
    for (int i = 0; i < K; i++) {
        if (pipe(pfd[i]) < 0 || pipe(p2c[i]) < 0) { perror("pipe"); return 1; }
    }

    /* fork children */
    for (int i = 0; i < K; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }

        if (pid == 0) {
            /* ── child process ── */
            /* close every pipe fd except own write end (pfd) and own read end (p2c) */
            for (int j = 0; j < K; j++) {
                close(pfd[j][0]);
                close(p2c[j][1]);
                if (j != i) { close(pfd[j][1]); close(p2c[j][0]); }
            }

            int pathLen;
            int *path = calcPath(N, adj, travelers[i].src, travelers[i].dst, &pathLen);

            for (int j = 0; j < pathLen; j++) {
                int msg[2] = {path[j], j + 1 < pathLen ? path[j + 1] : -1};
                write(pfd[i][1], msg, sizeof(msg));
                /* wait for parent confirmation before moving to next node */
                char ack;
                read(p2c[i][0], &ack, 1);
                /* sleep to match parent animation time */
                if (j + 1 < pathLen) {
                    int w = edgeW(path[j], path[j + 1], edges, M);
                    usleep((unsigned int)(w * 300000));
                }
            }

            if (path) free(path);
            close(pfd[i][1]);
            close(p2c[i][0]);

            /* free child's copies of heap allocations */
            for (int j = 0; j < N; j++) free(adj[j].edges);
            free(adj); free(edges);
            exit(0);
        }

        travelers[i].pid   = pid;
        travelers[i].pipeR = pfd[i][0];
        travelers[i].pipeW = p2c[i][1];
    }

    /* parent: close write ends of pfd and read ends of p2c, make pfd reads non-blocking */
    for (int i = 0; i < K; i++) {
        close(pfd[i][1]);
        close(p2c[i][0]);
        fcntl(pfd[i][0], F_SETFL, O_NONBLOCK);
    }

    /* ── parent: GUI ── */
    const int W = 800, H = 600;
    InitWindow(W, H, "Traffic Simulation - Milestone 5");
    int nodeR = 25;

    Node *nodes = malloc(N * sizeof(Node));
    for (int i = 0; i < N; i++) {
        float a = (2.0f * PI / N) * i - PI / 2.0f;
        nodes[i] = (Node){{W/2.0f + 200*cosf(a), H/2.0f + 200*sinf(a)}, i};
    }
    for (int i = 0; i < K; i++)
        travelers[i].pos = nodes[travelers[i].src].pos;

    SetTargetFPS(60);
    int allDone = 0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* ── read pipe messages ── */
        for (int i = 0; i < K; i++) {
            if (travelers[i].state == T5_FINISHED || travelers[i].state == T5_NOPATH) continue;

            int     msg[2];
            ssize_t r = read(travelers[i].pipeR, msg, sizeof(msg));

            if (r == (ssize_t)sizeof(msg)) {
                int cur = msg[0], nxt = msg[1];
                travelers[i].pos     = nodes[cur].pos;
                travelers[i].curNode = cur;
                travelers[i].timer   = 0;
                travelers[i].msgCount++;

                if (nxt == -1) {
                    printf("[PID=%d] arrived at node %d | DESTINATION\n", (int)travelers[i].pid, cur);
                    fflush(stdout);
                    travelers[i].nextNode = -1;
                    travelers[i].state    = T5_DEST;
                } else {
                    printf("[PID=%d] arrived at node %d | next node: %d\n", (int)travelers[i].pid, cur, nxt);
                    fflush(stdout);
                    travelers[i].nextNode  = nxt;
                    travelers[i].animTotal = edgeW(cur, nxt, edges, M) * 0.3f;
                    travelers[i].state     = T5_ACTIVE;
                }
                /* send confirmation so child can continue to next node */
                char ack = 'A';
                write(travelers[i].pipeW, &ack, 1);
            } else if (r == 0) {
                /* EOF: child exited */
                if (travelers[i].msgCount == 0) {
                    travelers[i].state = T5_NOPATH;
                    printf("[PID=%d] no path found\n", (int)travelers[i].pid);
                } else {
                    travelers[i].state = T5_FINISHED;
                    printf("[PID=%d] finished\n", (int)travelers[i].pid);
                }
                fflush(stdout);
                waitpid(travelers[i].pid, NULL, 0);
                close(travelers[i].pipeR);
                close(travelers[i].pipeW);
            }
            /* r == -1 with EAGAIN: no data yet, continue */
        }

        /* ── animate ── */
        for (int i = 0; i < K; i++) {
            if (travelers[i].state != T5_ACTIVE || travelers[i].nextNode < 0) continue;
            travelers[i].timer += dt;
            float pct = travelers[i].timer / travelers[i].animTotal;
            if (pct > 1.0f) pct = 1.0f;
            travelers[i].pos = (Vector2){
                nodes[travelers[i].curNode].pos.x +
                    (nodes[travelers[i].nextNode].pos.x - nodes[travelers[i].curNode].pos.x) * pct,
                nodes[travelers[i].curNode].pos.y +
                    (nodes[travelers[i].nextNode].pos.y - nodes[travelers[i].curNode].pos.y) * pct
            };
        }

        allDone = 1;
        for (int i = 0; i < K; i++)
            if (travelers[i].state == T5_ACTIVE || travelers[i].state == T5_DEST) { allDone = 0; break; }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (int i = 0; i < M; i++)
            drawArrow(nodes[edges[i].u].pos, nodes[edges[i].v].pos, edges[i].weight, nodeR);

        for (int i = 0; i < N; i++) {
            DrawCircleV(nodes[i].pos, nodeR, LIGHTGRAY);
            DrawCircleLines(nodes[i].pos.x, nodes[i].pos.y, nodeR, BLACK);
            const char *lbl = TextFormat("%d", i);
            DrawText(lbl, (int)(nodes[i].pos.x - MeasureText(lbl, 20)/2), (int)(nodes[i].pos.y - 10), 20, BLACK);
        }

        for (int i = 0; i < K; i++) {
            if (travelers[i].state == T5_ACTIVE || travelers[i].state == T5_DEST)
                DrawCircleV(travelers[i].pos, 12, travelers[i].color);
        }

        for (int i = 0; i < K; i++) {
            int ly = 60 + i * 22;
            DrawRectangle(10, ly, 16, 16, travelers[i].color);
            const char *s = travelers[i].state == T5_NOPATH   ? "(no path)" :
                            travelers[i].state == T5_FINISHED ||
                            travelers[i].state == T5_DEST      ? "(done)" : "";
            DrawText(TextFormat("T%d: %d->%d %s", i, travelers[i].src, travelers[i].dst, s),
                     32, ly, 16, DARKGRAY);
        }

        DrawText("Milestone 5 - IPC via pipes", 10, 10, 18, DARKGRAY);
        if (allDone) DrawText("All arrived!", W/2 - 80, 20, 24, DARKGREEN);

        EndDrawing();
    }

    /* clean up any remaining children if window was closed early */
    for (int i = 0; i < K; i++) {
        if (travelers[i].state != T5_FINISHED && travelers[i].state != T5_NOPATH) {
            close(travelers[i].pipeR);
            close(travelers[i].pipeW);
            waitpid(travelers[i].pid, NULL, 0);
        }
    }

    CloseWindow();
    for (int i = 0; i < N; i++) free(adj[i].edges);
    free(adj); free(edges); free(nodes);
    return 0;
}
