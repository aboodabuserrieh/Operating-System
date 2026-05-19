/* sim_ms4.c – Milestone 4: multiple travelers via fork */
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

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

typedef enum { T_AT_NODE, T_MOVING, T_FINISHED, T_NOPATH } TState;

typedef struct {
    int    *path;
    int     pathLen, pathIdx;
    float   timer;
    TState  state;
    Vector2 pos;
    Color   color;
    pid_t   pid;
    int     src, dst, reaped;
} Traveler;

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

    Traveler travelers[MAX_TRAVELERS];
    memset(travelers, 0, sizeof(travelers));
    for (int i = 0; i < K; i++) {
        skipComments(f);
        fscanf(f, "%d %d", &travelers[i].src, &travelers[i].dst);
    }
    fclose(f);

    /* compute Dijkstra paths in parent before forking */
    for (int i = 0; i < K; i++) {
        travelers[i].path  = calcPath(N, adj, travelers[i].src, travelers[i].dst, &travelers[i].pathLen);
        travelers[i].color = TCOLORS[i % MAX_TRAVELERS];
        travelers[i].state = travelers[i].pathLen ? T_AT_NODE : T_NOPATH;
    }

    /* fork children — must happen before InitWindow */
    for (int i = 0; i < K; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) {
            /* child: announce self, then sleep until killed */
            printf("[%d] started\n", (int)getpid());
            fflush(stdout);
            pause(); /* wakes on SIGTERM, then exits */
            exit(0);
        }
        travelers[i].pid = pid;
    }

    /* ── parent: GUI ── */
    const int W = 800, H = 600;
    InitWindow(W, H, "Traffic Simulation - Milestone 4");
    int nodeR = 25;

    Node *nodes = malloc(N * sizeof(Node));
    for (int i = 0; i < N; i++) {
        float a = (2.0f * PI / N) * i - PI / 2.0f;
        nodes[i] = (Node){{W/2.0f + 200*cosf(a), H/2.0f + 200*sinf(a)}, i};
    }
    for (int i = 0; i < K; i++)
        if (travelers[i].pathLen) travelers[i].pos = nodes[travelers[i].path[0]].pos;

    int playing = 0, allDone = 0;
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Rectangle btn = {10, 10, 100, 40};

        if (!allDone && CheckCollisionPointRec(GetMousePosition(), btn) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            playing = !playing;

        if (playing) {
            for (int i = 0; i < K; i++) {
                Traveler *t = &travelers[i];
                if (t->state == T_FINISHED || t->state == T_NOPATH) continue;

                if (t->state == T_AT_NODE) {
                    if (t->pathIdx == t->pathLen - 1) {
                        t->state = T_FINISHED;
                        if (!t->reaped) {
                            kill(t->pid, SIGTERM);
                            waitpid(t->pid, NULL, 0);
                            t->reaped = 1;
                        }
                    } else if (t->pathIdx == 0) {
                        t->state = T_MOVING; t->timer = 0;
                    } else {
                        t->timer += dt;
                        if (t->timer >= 1.0f) { t->state = T_MOVING; t->timer = 0; }
                    }
                } else if (t->state == T_MOVING) {
                    int   u     = t->path[t->pathIdx], v = t->path[t->pathIdx + 1];
                    float total = edgeW(u, v, edges, M) * 0.3f;
                    t->timer += dt;
                    if (t->timer >= total) {
                        t->pathIdx++; t->timer = 0; t->pos = nodes[v].pos; t->state = T_AT_NODE;
                    } else {
                        float p = t->timer / total;
                        t->pos = (Vector2){
                            nodes[u].pos.x + (nodes[v].pos.x - nodes[u].pos.x) * p,
                            nodes[u].pos.y + (nodes[v].pos.y - nodes[u].pos.y) * p
                        };
                    }
                }
            }

            int done = 0;
            for (int i = 0; i < K; i++)
                if (travelers[i].state == T_FINISHED || travelers[i].state == T_NOPATH) done++;
            if (done == K) { allDone = 1; playing = 0; }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        /* edges */
        for (int i = 0; i < M; i++)
            drawArrow(nodes[edges[i].u].pos, nodes[edges[i].v].pos, edges[i].weight, nodeR);

        /* highlight each traveler's path with its color */
        for (int i = 0; i < K; i++) {
            Traveler *t = &travelers[i];
            if (!t->path) continue;
            Color lc = t->color; lc.a = 90;
            for (int j = 0; j + 1 < t->pathLen; j++) {
                int u = t->path[j], v = t->path[j + 1];
                float dx = nodes[v].pos.x - nodes[u].pos.x, dy = nodes[v].pos.y - nodes[u].pos.y;
                float l  = sqrtf(dx*dx + dy*dy);
                Vector2 ps = {nodes[u].pos.x + dx/l*nodeR, nodes[u].pos.y + dy/l*nodeR};
                Vector2 pe = {nodes[v].pos.x - dx/l*nodeR, nodes[v].pos.y - dy/l*nodeR};
                DrawLineEx(ps, pe, 3, lc);
            }
        }

        /* nodes */
        for (int i = 0; i < N; i++) {
            DrawCircleV(nodes[i].pos, nodeR, LIGHTGRAY);
            DrawCircleLines(nodes[i].pos.x, nodes[i].pos.y, nodeR, BLACK);
            const char *lbl = TextFormat("%d", i);
            DrawText(lbl, (int)(nodes[i].pos.x - MeasureText(lbl, 20)/2), (int)(nodes[i].pos.y - 10), 20, BLACK);
        }

        /* traveler circles */
        for (int i = 0; i < K; i++) {
            Traveler *t = &travelers[i];
            if (t->state == T_MOVING || t->state == T_AT_NODE)
                DrawCircleV(t->pos, 12, t->color);
        }

        /* legend */
        for (int i = 0; i < K; i++) {
            int ly = 60 + i * 22;
            DrawRectangle(10, ly, 16, 16, travelers[i].color);
            const char *status = travelers[i].state == T_NOPATH   ? "(no path)" :
                                 travelers[i].state == T_FINISHED  ? "(arrived)" : "";
            DrawText(TextFormat("T%d: %d->%d %s", i, travelers[i].src, travelers[i].dst, status),
                     32, ly, 16, DARKGRAY);
        }

        DrawRectangleRec(btn, playing ? RED : GREEN);
        DrawText(playing ? "STOP" : "PLAY", (int)(btn.x + 25), (int)(btn.y + 10), 20, WHITE);
        if (allDone) DrawText("All arrived!", W/2 - 80, 20, 24, DARKGREEN);

        EndDrawing();
    }

    /* kill any children still running (e.g. window closed early) */
    for (int i = 0; i < K; i++) {
        if (!travelers[i].reaped) {
            kill(travelers[i].pid, SIGTERM);
            waitpid(travelers[i].pid, NULL, 0);
        }
        free(travelers[i].path);
    }

    CloseWindow();
    for (int i = 0; i < N; i++) free(adj[i].edges);
    free(adj); free(edges); free(nodes);
    return 0;
}
