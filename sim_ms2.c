#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct { int u, v, weight; } Edge;
typedef struct { Vector2 pos; int id; } Node;

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
        printf("Usage: ./sim_ms2 <file_name>\n");
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

    Edge *edges = malloc(M * sizeof(Edge));
    for (int i = 0; i < M; i++) {
        fscanf(file, "%d %d %d", &edges[i].u, &edges[i].v, &edges[i].weight);
        if (edges[i].u < 0 || edges[i].v < 0 || edges[i].weight < 0) {
            fprintf(stderr, "Invalid input: negative numbers are not allowed.\n");
            return 1;
        }
    }
    fclose(file);

    const int screenWidth = 800, screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Milestone 2: Static Graph Visualization");

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

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (int i = 0; i < M; i++)
            DrawArrowWithWeight(nodes[edges[i].u].pos, nodes[edges[i].v].pos, edges[i].weight, nodeRadius);

        for (int i = 0; i < N; i++) {
            DrawCircleV(nodes[i].pos, nodeRadius, LIGHTGRAY);
            DrawCircleLines(nodes[i].pos.x, nodes[i].pos.y, nodeRadius, BLACK);
            const char *label  = TextFormat("%d", nodes[i].id);
            int         labelW = MeasureText(label, 20);
            DrawText(label, (int)(nodes[i].pos.x - labelW / 2), (int)(nodes[i].pos.y - 10), 20, BLACK);
        }

        DrawText("Milestone 2 - Static Graph Display", 10, 10, 20, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
    free(edges);
    free(nodes);
    return 0;
}
