/* sim_ms7.c – Milestone 7: scheduling algorithms for node entry (FCFS / SJF) */
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
#define MSG_ARRIVED   0
#define MSG_WAITING   1

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
 * Scheduling: parent controls node entry.
 * Each child sends MSG_WAITING then blocks on a pipe read waiting for 'G'.
 * Parent maintains a per-node queue and picks the next child based on
 * the chosen algorithm (FCFS or SJF), then writes 'G' to unblock it.
 *
 * FCFS: first-come first-served (queue order).
 * SJF:  shortest next edge weight (proxy for shortest remaining step).
 */

typedef enum {
    T7_IDLE, T7_WAITING, T7_IN_NODE, T7_MOVING, T7_DEST, T7_FINISHED, T7_NOPATH
} TState7;

typedef struct {
    pid_t   pid;
    int     pipeR;   /* child -> parent */
    int     pipeW;   /* parent -> child */
    TState7 state;
    Vector2 pos;
    Color   color;
    int     src, dst;
    int     atNode;
    int     nextNode;
    int     fromNode;
    int     toNode;
    float   timer;
    float   animTotal;
    float   inNodeTimer;
    int     msgCount;
    int     waitFor;
} Traveler7;

static const Color TCOLORS[MAX_TRAVELERS] = {
    {255,150,  0,255}, {  0,100,200,255}, {150,  0,200,255}, {  0,180, 50,255},
    {220, 50, 50,255}, {200,180,  0,255}, {  0,190,190,255}, {220, 80,180,255},
    { 80,200, 80,255}, {170, 90, 30,255}, { 30,150,200,255}, {200,200, 50,255},
    { 90, 90,210,255}, {200, 30,100,255}, { 30, 80,180,255}, {120,180, 80,255},
};

static void scheduleNext(int node, int isSJF,
                         int *waitCounts, int waitQueues[][MAX_TRAVELERS],
                         int *nodeOccupied, Traveler7 *travelers,
                         Edge *edges, int M) {
    if (nodeOccupied[node] || waitCounts[node] == 0) return;

    int best = 0;
    if (isSJF) {
        int minW = INF;
        for (int i = 0; i < waitCounts[node]; i++) {
            int tid  = waitQueues[node][i];
            int next = travelers[tid].nextNode;
            int w    = (next == -1) ? 0 : edgeW(node, next, edges, M);
            if (w < minW) { minW = w; best = i; }
        }
    }

    int chosen = waitQueues[node][best];
    for (int i = best; i < waitCounts[node] - 1; i++)
        waitQueues[node][i] = waitQueues[node][i + 1];
    waitCounts[node]--;

    nodeOccupied[node] = 1;
    char go = 'G';
    write(travelers[chosen].pipeW, &go, 1);
}

int main(int argc, char *argv[]) {
    /* expected: ./sim -schd <fcfs|sjf> <file_name> */
    if (argc < 4 || strcmp(argv[1], "-schd") != 0) {
        printf("Usage: ./sim -schd <fcfs|sjf> <file_name>\n");
        return 1;
    }
    int isSJF = 0;
    if (strcmp(argv[2], "sjf") == 0) {
        isSJF = 1;
    } else if (strcmp(argv[2], "fcfs") != 0) {
        printf("Error: algorithm must be 'fcfs' or 'sjf'\n");
        return 1;
    }

    FILE *f = fopen(argv[3], "r");
    if (!f) { fprintf(stderr, "Cannot open %s\n", argv[3]); return 1; }

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

    Traveler7 travelers[MAX_TRAVELERS];
    memset(travelers, 0, sizeof(travelers));
    for (int i = 0; i < K; i++) {
        skipComments(f);
        fscanf(f, "%d %d", &travelers[i].src, &travelers[i].dst);
        travelers[i].color   = TCOLORS[i % MAX_TRAVELERS];
        travelers[i].state   = T7_IDLE;
        travelers[i].waitFor = -1;
        travelers[i].atNode  = -1;
    }
    fclose(f);

    int pfd[MAX_TRAVELERS][2];    /* child -> parent */
    int p2c[MAX_TRAVELERS][2];    /* parent -> child */
    for (int i = 0; i < K; i++) {
        if (pipe(pfd[i]) < 0 || pipe(p2c[i]) < 0) { perror("pipe"); return 1; }
    }

    for (int i = 0; i < K; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }

        if (pid == 0) {
            /* close all except own write (pfd) and own read (p2c) */
            for (int j = 0; j < K; j++) {
                close(pfd[j][0]);
                close(p2c[j][1]);
                if (j != i) { close(pfd[j][1]); close(p2c[j][0]); }
            }

            int pathLen;
            int *path = calcPath(N, adj, travelers[i].src, travelers[i].dst, &pathLen);

            for (int j = 0; j < pathLen; j++) {
                int node = path[j];
                int next = j + 1 < pathLen ? path[j + 1] : -1;

                /* ask parent for permission to enter node */
                int msg[3] = {MSG_WAITING, node, next};
                write(pfd[i][1], msg, sizeof(msg));

                /* block until parent sends 'G' */
                char go;
                read(p2c[i][0], &go, 1);

                /* notify parent: entered node */
                msg[0] = MSG_ARRIVED;
                write(pfd[i][1], msg, sizeof(msg));

                /* critical section: stay 1 second */
                sleep(1);

                /* traverse edge to next node */
                if (next != -1) {
                    int w = edgeW(node, next, edges, M);
                    usleep((unsigned int)(w * 300000));
                }
            }

            if (path) free(path);
            close(pfd[i][1]);
            close(p2c[i][0]);
            for (int j = 0; j < N; j++) free(adj[j].edges);
            free(adj); free(edges);
            exit(0);
        }

        travelers[i].pid   = pid;
        travelers[i].pipeR = pfd[i][0];
        travelers[i].pipeW = p2c[i][1];
    }

    for (int i = 0; i < K; i++) {
        close(pfd[i][1]);
        close(p2c[i][0]);
        fcntl(travelers[i].pipeR, F_SETFL, O_NONBLOCK);
    }

    const int W = 800, H = 600;
    char title[64];
    snprintf(title, sizeof(title), "Traffic Sim - Milestone 7 [%s]", isSJF ? "SJF" : "FCFS");
    InitWindow(W, H, title);
    int nodeR = 25;

    Node *nodes = malloc(N * sizeof(Node));
    for (int i = 0; i < N; i++) {
        float a = (2.0f * PI / N) * i - PI / 2.0f;
        nodes[i] = (Node){{W/2.0f + 200*cosf(a), H/2.0f + 200*sinf(a)}, i};
    }
    for (int i = 0; i < K; i++) travelers[i].pos = nodes[travelers[i].src].pos;

    int waitQueues[MAX_TRAVELERS][MAX_TRAVELERS];
    int waitCounts[MAX_TRAVELERS];
    int nodeOccupied[MAX_TRAVELERS];
    memset(waitCounts,   0, sizeof(waitCounts));
    memset(nodeOccupied, 0, sizeof(nodeOccupied));

    int *waiterIdx = calloc(N, sizeof(int));
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* read pipe messages */
        for (int i = 0; i < K; i++) {
            if (travelers[i].state == T7_FINISHED || travelers[i].state == T7_NOPATH) continue;

            int msg[3];
            ssize_t r = read(travelers[i].pipeR, msg, sizeof(msg));

            if (r == (ssize_t)sizeof(msg)) {
                int type = msg[0], node = msg[1], next = msg[2];
                travelers[i].msgCount++;

                if (type == MSG_WAITING) {
                    travelers[i].state    = T7_WAITING;
                    travelers[i].waitFor  = node;
                    travelers[i].nextNode = next;
                    travelers[i].pos      = nodes[node].pos;

                    waitQueues[node][waitCounts[node]++] = i;
                    scheduleNext(node, isSJF, waitCounts, waitQueues,
                                 nodeOccupied, travelers, edges, M);

                } else { /* MSG_ARRIVED */
                    travelers[i].state       = T7_IN_NODE;
                    travelers[i].atNode      = node;
                    travelers[i].nextNode    = next;
                    travelers[i].inNodeTimer = 0;
                    travelers[i].pos         = nodes[node].pos;
                }

            } else if (r == 0) { /* EOF: child exited */
                travelers[i].state = (travelers[i].msgCount == 0) ? T7_NOPATH : T7_FINISHED;
                waitpid(travelers[i].pid, NULL, 0);
                close(travelers[i].pipeR);
                close(travelers[i].pipeW);
            }
        }

        /* update animations */
        for (int i = 0; i < K; i++) {
            Traveler7 *t = &travelers[i];

            if (t->state == T7_IN_NODE) {
                t->inNodeTimer += dt;
                if (t->inNodeTimer >= 1.0f) {
                    int freed = t->atNode;
                    nodeOccupied[freed] = 0;

                    if (t->nextNode == -1) {
                        t->state = T7_DEST;
                    } else {
                        t->state     = T7_MOVING;
                        t->fromNode  = t->atNode;
                        t->toNode    = t->nextNode;
                        t->timer     = 0;
                        t->animTotal = edgeW(t->atNode, t->nextNode, edges, M) * 0.3f;
                    }

                    scheduleNext(freed, isSJF, waitCounts, waitQueues,
                                 nodeOccupied, travelers, edges, M);
                }
            } else if (t->state == T7_MOVING) {
                t->timer += dt;
                float pct = t->timer / t->animTotal;
                if (pct > 1.0f) pct = 1.0f;
                t->pos = (Vector2){
                    nodes[t->fromNode].pos.x + (nodes[t->toNode].pos.x - nodes[t->fromNode].pos.x) * pct,
                    nodes[t->fromNode].pos.y + (nodes[t->toNode].pos.y - nodes[t->fromNode].pos.y) * pct
                };
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (int i = 0; i < M; i++)
            drawArrow(nodes[edges[i].u].pos, nodes[edges[i].v].pos, edges[i].weight, nodeR);

        for (int i = 0; i < N; i++) {
            Color fill = nodeOccupied[i] ? (Color){255,220,200,255} : LIGHTGRAY;
            DrawCircleV(nodes[i].pos, nodeR, fill);
            DrawCircleLines(nodes[i].pos.x, nodes[i].pos.y, nodeR, BLACK);
            const char *lbl = TextFormat("%d", i);
            DrawText(lbl, (int)(nodes[i].pos.x - MeasureText(lbl,20)/2), (int)(nodes[i].pos.y - 10), 20, BLACK);
        }

        /* waiting travelers: orbit around their target node */
        memset(waiterIdx, 0, N * sizeof(int));
        for (int i = 0; i < K; i++) {
            if (travelers[i].state != T7_WAITING) continue;
            int   nd    = travelers[i].waitFor;
            int   idx   = waiterIdx[nd]++;
            int   total = waitCounts[nd] > 0 ? waitCounts[nd] : 1;
            float angle = ((float)idx / total) * 2.0f * PI;
            float orbitR = nodeR + 20.0f;
            Vector2 wp = {
                nodes[nd].pos.x + cosf(angle) * orbitR,
                nodes[nd].pos.y + sinf(angle) * orbitR
            };
            float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 6.0f);
            Color c = travelers[i].color;
            c.a = (unsigned char)(120 + 135 * pulse);
            DrawCircleLines(wp.x, wp.y, 10, c);
            DrawCircleLines(wp.x, wp.y, 11, c);
            DrawCircleLines(wp.x, wp.y, 12, c);
        }

        /* in-node and moving travelers */
        for (int i = 0; i < K; i++) {
            Traveler7 *t = &travelers[i];
            if (t->state == T7_IN_NODE || t->state == T7_DEST) {
                DrawCircleV(t->pos, 13, t->color);
                DrawCircleLines(t->pos.x, t->pos.y, 13, BLACK);
            } else if (t->state == T7_MOVING) {
                DrawCircleV(t->pos, 10, t->color);
                DrawCircleLines(t->pos.x, t->pos.y, 10, BLACK);
            }
        }

        /* HUD */
        DrawText(TextFormat("Milestone 7 - Scheduling: %s", isSJF ? "SJF" : "FCFS"),
                 10, 10, 16, isSJF ? DARKBLUE : DARKGREEN);
        DrawText("Solid = in node   Pulsing ring = waiting", 10, 30, 14, DARKGRAY);
        for (int i = 0; i < K; i++) {
            int ly = 55 + i * 20;
            DrawRectangle(10, ly, 14, 14, travelers[i].color);
            const char *s =
                travelers[i].state == T7_WAITING  ? "(waiting)" :
                travelers[i].state == T7_IN_NODE  ? "(in node)" :
                travelers[i].state == T7_MOVING   ? "(moving)"  :
                travelers[i].state == T7_DEST     ? "(arrived)" :
                travelers[i].state == T7_FINISHED ? "(done)"    :
                travelers[i].state == T7_NOPATH   ? "(no path)" : "";
            DrawText(TextFormat("T%d: %d->%d %s", i, travelers[i].src, travelers[i].dst, s),
                     28, ly, 14, DARKGRAY);
        }

        EndDrawing();
    }

    /* cleanup */
    for (int i = 0; i < K; i++) {
        if (travelers[i].state != T7_FINISHED && travelers[i].state != T7_NOPATH) {
            close(travelers[i].pipeR);
            close(travelers[i].pipeW);
            waitpid(travelers[i].pid, NULL, 0);
        }
    }
    CloseWindow();
    free(waiterIdx);
    for (int i = 0; i < N; i++) free(adj[i].edges);
    free(adj); free(edges); free(nodes);
    return 0;
}
