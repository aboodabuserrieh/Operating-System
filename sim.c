#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#define INF INT_MAX

typedef struct { int u, v, weight; } Edge;
typedef struct { Vector2 pos; int id; } Node;

typedef struct { int v, weight; } AdjEdge;
typedef struct { AdjEdge *edges; int count, capacity; } AdjList;

typedef struct { int dist, node; } HeapNode;
typedef struct { HeapNode *data; int size, capacity; } MinHeap;

static void adjListAdd(AdjList *list, int v, int weight) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->edges = realloc(list->edges, list->capacity * sizeof(AdjEdge));
    }
    list->edges[list->count].v      = v;
    list->edges[list->count].weight = weight;
    list->count++;
}

static void heapPush(MinHeap *h, int dist, int node) {
    if (h->size >= h->capacity) {
        h->capacity = h->capacity == 0 ? 8 : h->capacity * 2;
        h->data = realloc(h->data, h->capacity * sizeof(HeapNode));
    }
    int i = h->size++;
    h->data[i].dist = dist;
    h->data[i].node = node;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->data[parent].dist <= h->data[i].dist) break;
        HeapNode tmp    = h->data[parent];
        h->data[parent] = h->data[i];
        h->data[i]      = tmp;
        i = parent;
    }
}

static HeapNode heapPop(MinHeap *h) {
    HeapNode top = h->data[0];
    h->data[0]   = h->data[--h->size];
    int i = 0;
    for (;;) {
        int left = 2*i+1, right = 2*i+2, smallest = i;
        if (left  < h->size && h->data[left].dist  < h->data[smallest].dist) smallest = left;
        if (right < h->size && h->data[right].dist < h->data[smallest].dist) smallest = right;
        if (smallest == i) break;
        HeapNode tmp      = h->data[i];
        h->data[i]        = h->data[smallest];
        h->data[smallest] = tmp;
        i = smallest;
    }
    return top;
}

static int *getDijkstraPath(int N, AdjList *adj, int src, int dst, int *pathLen) {
    int *dist   = malloc(N * sizeof(int));
    int *parent = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) { dist[i] = INF; parent[i] = -1; }

    MinHeap h = {NULL, 0, 0};
    dist[src] = 0;
    heapPush(&h, 0, src);

    while (h.size > 0) {
        HeapNode cur = heapPop(&h);
        int d = cur.dist, u = cur.node;
        if (d > dist[u]) continue;
        if (u == dst) break;
        for (int i = 0; i < adj[u].count; i++) {
            int v = adj[u].edges[i].v;
            int w = adj[u].edges[i].weight;
            if (dist[u] + w < dist[v]) {
                dist[v]   = dist[u] + w;
                parent[v] = u;
                heapPush(&h, dist[v], v);
            }
        }
    }

    int *path = NULL;
    *pathLen  = 0;

    if (dist[dst] != INF) {
        int *tmp = malloc(N * sizeof(int));
        int  len = 0;
        for (int v = dst; v != -1; v = parent[v])
            tmp[len++] = v;
        path = malloc(len * sizeof(int));
        for (int i = 0; i < len; i++)
            path[i] = tmp[len - 1 - i];
        *pathLen = len;
        free(tmp);
    }

    free(dist);
    free(parent);
    free(h.data);
    return path;
}

static int getEdgeWeight(int u, int v, Edge *edges, int M) {
    for (int i = 0; i < M; i++)
        if (edges[i].u == u && edges[i].v == v) return edges[i].weight;
    return 1;
}

static void DrawArrowWithWeight(Vector2 start, Vector2 end, int weight, int nodeRadius) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float length = sqrtf(dx * dx + dy * dy);
    if (length == 0) return;

    float dirX = dx / length, dirY = dy / length;
    Vector2 adjStart = { start.x + dirX * nodeRadius, start.y + dirY * nodeRadius };
    Vector2 adjEnd   = { end.x   - dirX * nodeRadius, end.y   - dirY * nodeRadius };

    DrawLineEx(adjStart, adjEnd, 2.0f, DARKGRAY);

    float angle     = atan2f(dirY, dirX);
    float arrowSize = 15.0f;
    Vector2 p1 = adjEnd;
    Vector2 p2 = { adjEnd.x - arrowSize * cosf(angle - PI / 6.0f), adjEnd.y - arrowSize * sinf(angle - PI / 6.0f) };
    Vector2 p3 = { adjEnd.x - arrowSize * cosf(angle + PI / 6.0f), adjEnd.y - arrowSize * sinf(angle + PI / 6.0f) };
    DrawTriangle(p1, p2, p3, MAROON);

    Vector2 midPoint = { (adjStart.x + adjEnd.x) / 2.0f, (adjStart.y + adjEnd.y) / 2.0f };
    Vector2 textPos  = { midPoint.x - dirY * 18.0f, midPoint.y + dirX * 18.0f };
    DrawText(TextFormat("%d", weight), (int)(textPos.x - 10), (int)(textPos.y - 10), 20, DARKBLUE);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./sim <file_name>\n");
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", argv[1]);
        return 1;
    }

    int N, M;
    if (fscanf(file, "%d %d", &N, &M) != 2) return 1;
    if (N < 0 || M < 0) {
        fprintf(stderr, "Invalid input: negative numbers are not allowed.\n");
        return 1;
    }

    Edge    *edges = malloc(M * sizeof(Edge));
    AdjList *adj   = calloc(N, sizeof(AdjList));

    for (int i = 0; i < M; i++) {
        fscanf(file, "%d %d %d", &edges[i].u, &edges[i].v, &edges[i].weight);
        if (edges[i].u < 0 || edges[i].v < 0 || edges[i].weight < 0) {
            fprintf(stderr, "Invalid input: negative numbers are not allowed.\n");
            return 1;
        }
        adjListAdd(&adj[edges[i].u], edges[i].v, edges[i].weight);
    }

    int src, dst;
    fscanf(file, "%d %d", &src, &dst);
    fclose(file);

    if (src < 0 || dst < 0) {
        fprintf(stderr, "Invalid input: negative numbers are not allowed.\n");
        return 1;
    }

    int  pathLen = 0;
    int *path    = getDijkstraPath(N, adj, src, dst, &pathLen);

    const int screenWidth = 800, screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Traffic Simulation - Milestone 3");

    Node *nodes = malloc(N * sizeof(Node));
    float centerX      = screenWidth  / 2.0f;
    float centerY      = screenHeight / 2.0f;
    float layoutRadius = 200.0f;
    int   nodeRadius   = 25;

    for (int i = 0; i < N; i++) {
        float angle  = (2.0f * PI / N) * i - PI / 2.0f;
        nodes[i].id  = i;
        nodes[i].pos = (Vector2){ centerX + layoutRadius * cosf(angle),
                                  centerY + layoutRadius * sinf(angle) };
    }

    typedef enum { AT_NODE, MOVING, FINISHED, NO_PATH } AnimState;
    AnimState state     = (pathLen == 0) ? NO_PATH : AT_NODE;
    int       isPlaying = 0;
    int       pathIdx   = 0;
    float     timer     = 0.0f;
    Vector2   entityPos = (pathLen == 0) ? (Vector2){0, 0} : nodes[path[0]].pos;

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        Rectangle btnBounds = { 10, 10, 100, 40 };
        if (CheckCollisionPointRec(GetMousePosition(), btnBounds) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            state != FINISHED && state != NO_PATH) {
            isPlaying = !isPlaying;
        }

        if (isPlaying && state != FINISHED && state != NO_PATH) {
            if (state == AT_NODE) {
                if (pathIdx == 0 || pathIdx == pathLen - 1) {
                    state = (pathIdx == pathLen - 1) ? FINISHED : MOVING;
                    timer = 0.0f;
                } else {
                    timer += dt;
                    if (timer >= 1.0f) { state = MOVING; timer = 0.0f; }
                }
            } else if (state == MOVING) {
                int   u             = path[pathIdx];
                int   v             = path[pathIdx + 1];
                int   weight        = getEdgeWeight(u, v, edges, M);
                float edgeTotalTime = weight * 0.3f;

                timer += dt;
                if (timer >= edgeTotalTime) {
                    pathIdx++;
                    timer     = 0.0f;
                    entityPos = nodes[v].pos;
                    state     = (pathIdx == pathLen - 1) ? FINISHED : AT_NODE;
                } else {
                    float t   = timer / edgeTotalTime;
                    entityPos = (Vector2){
                        nodes[u].pos.x + (nodes[v].pos.x - nodes[u].pos.x) * t,
                        nodes[u].pos.y + (nodes[v].pos.y - nodes[u].pos.y) * t
                    };
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (int i = 0; i < M; i++)
            DrawArrowWithWeight(nodes[edges[i].u].pos, nodes[edges[i].v].pos, edges[i].weight, nodeRadius);

        if (pathLen > 0) {
            for (int i = 0; i + 1 < pathLen; i++) {
                int   u    = path[i], v = path[i + 1];
                float dx   = nodes[v].pos.x - nodes[u].pos.x;
                float dy   = nodes[v].pos.y - nodes[u].pos.y;
                float len  = sqrtf(dx * dx + dy * dy);
                float dirX = dx / len, dirY = dy / len;
                Vector2 s  = { nodes[u].pos.x + dirX * nodeRadius, nodes[u].pos.y + dirY * nodeRadius };
                Vector2 e  = { nodes[v].pos.x - dirX * nodeRadius, nodes[v].pos.y - dirY * nodeRadius };
                DrawLineEx(s, e, 4.0f, ORANGE);
            }
        }

        for (int i = 0; i < N; i++) {
            Color fill = LIGHTGRAY;
            if (pathLen > 0 && nodes[i].id == path[0])           fill = GREEN;
            if (pathLen > 0 && nodes[i].id == path[pathLen - 1]) fill = RED;
            DrawCircleV(nodes[i].pos, nodeRadius, fill);
            DrawCircleLines(nodes[i].pos.x, nodes[i].pos.y, nodeRadius, BLACK);
            const char *label  = TextFormat("%d", nodes[i].id);
            int         labelW = MeasureText(label, 20);
            DrawText(label, (int)(nodes[i].pos.x - labelW / 2), (int)(nodes[i].pos.y - 10), 20, BLACK);
        }

        DrawRectangleRec(btnBounds, isPlaying ? RED : GREEN);
        DrawText(isPlaying ? "STOP" : "PLAY",
                 (int)(btnBounds.x + 25), (int)(btnBounds.y + 10), 20, WHITE);

        if (state != NO_PATH) {
            DrawCircleV(entityPos, 15, ORANGE);
            DrawCircleLines(entityPos.x, entityPos.y, 15, DARKBROWN);
        }

        if (state == NO_PATH)
            DrawText("No path found!", screenWidth / 2 - 90, 20, 24, RED);
        else if (state == FINISHED)
            DrawText("Arrived at destination!", screenWidth / 2 - 130, 20, 24, DARKGREEN);

        DrawText("Source: GREEN   Destination: RED   Entity: ORANGE",
                 10, screenHeight - 30, 16, DARKGRAY);

        EndDrawing();
    }

    CloseWindow();
    free(path);
    free(edges);
    for (int i = 0; i < N; i++) free(adj[i].edges);
    free(adj);
    free(nodes);
    return 0;
}
