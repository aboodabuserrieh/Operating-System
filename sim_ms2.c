#include "raylib.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>

using namespace std;

struct Edge { int u, v, weight; };
struct Node { Vector2 pos; int id; };

void DrawArrowWithWeight(Vector2 start, Vector2 end, int weight, int nodeRadius) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float length = sqrt(dx * dx + dy * dy);
    if (length == 0) return;

    float dirX = dx / length, dirY = dy / length;
    Vector2 adjStart = { start.x + dirX * nodeRadius, start.y + dirY * nodeRadius };
    Vector2 adjEnd   = { end.x   - dirX * nodeRadius, end.y   - dirY * nodeRadius };

    DrawLineEx(adjStart, adjEnd, 2.0f, DARKGRAY);

    float angle     = atan2(dirY, dirX);
    float arrowSize = 15.0f;
    Vector2 p1 = adjEnd;
    Vector2 p2 = { adjEnd.x - arrowSize * cos(angle - PI / 6), adjEnd.y - arrowSize * sin(angle - PI / 6) };
    Vector2 p3 = { adjEnd.x - arrowSize * cos(angle + PI / 6), adjEnd.y - arrowSize * sin(angle + PI / 6) };
    DrawTriangle(p1, p2, p3, MAROON);

    Vector2 midPoint = { (adjStart.x + adjEnd.x) / 2.0f, (adjStart.y + adjEnd.y) / 2.0f };
    Vector2 textPos  = { midPoint.x - dirY * 18.0f, midPoint.y + dirX * 18.0f };
    DrawText(TextFormat("%d", weight), (int)(textPos.x - 10), (int)(textPos.y - 10), 20, DARKBLUE);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: ./sim <file_name>" << endl;
        return 1;
    }

    ifstream file(argv[1]);
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << argv[1] << endl;
        return 1;
    }

    int N, M;
    if (!(file >> N >> M)) return 1;
    if (N < 0 || M < 0) {
        cerr << "Invalid input: negative numbers are not allowed." << endl;
        return 1;
    }

    vector<Edge> edges(M);
    for (int i = 0; i < M; ++i) {
        file >> edges[i].u >> edges[i].v >> edges[i].weight;
    }
    file.close();

    const int screenWidth = 800, screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Milestone 2: Static Graph Visualization");

    vector<Node> nodes(N);
    float centerX      = screenWidth  / 2.0f;
    float centerY      = screenHeight / 2.0f;
    float layoutRadius = 200.0f;
    int   nodeRadius   = 25;

    for (int i = 0; i < N; ++i) {
        float angle  = (2 * PI / N) * i - PI / 2;
        nodes[i].id  = i;
        nodes[i].pos = { centerX + layoutRadius * cos(angle),
                         centerY + layoutRadius * sin(angle) };
    }

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (const auto& edge : edges) {
            DrawArrowWithWeight(nodes[edge.u].pos, nodes[edge.v].pos, edge.weight, nodeRadius);
        }

        for (const auto& node : nodes) {
            DrawCircleV(node.pos, nodeRadius, LIGHTGRAY);
            DrawCircleLines(node.pos.x, node.pos.y, nodeRadius, BLACK);
            const char* label  = TextFormat("%d", node.id);
            int         labelW = MeasureText(label, 20);
            DrawText(label, (int)(node.pos.x - labelW / 2), (int)(node.pos.y - 10), 20, BLACK);
        }

        DrawText("Milestone 2 - Static Graph Display", 10, 10, 20, DARKGRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
