#include "raylib.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <queue>
#include <climits>

using namespace std;

const int INF = INT_MAX;

struct Edge { int u, v, weight; };
struct Node { Vector2 pos; int id; };

vector<int> getDijkstraPath(int N, const vector<vector<pair<int, int>>>& adj, int src, int dst) {
    vector<int> dist(N, INF);
    vector<int> parent(N, -1);
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;

    dist[src] = 0;
    pq.push({0, src});

    while (!pq.empty()) {
        int d = pq.top().first;
        int u = pq.top().second;
        pq.pop();

        if (d > dist[u]) continue;
        if (u == dst) break;

        for (const auto& edge : adj[u]) {
            int v = edge.first;
            int weight = edge.second;
            if (dist[u] + weight < dist[v]) {
                dist[v] = dist[u] + weight;
                parent[v] = u;
                pq.push({dist[v], v});
            }
        }
    }

    vector<int> path;
    if (dist[dst] == INF) return path;

    for (int v = dst; v != -1; v = parent[v]) {
        path.insert(path.begin(), v);
    }
    return path;
}

int getEdgeWeight(int u, int v, const vector<Edge>& edges) {
    for (const auto& e : edges) {
        if (e.u == u && e.v == v) return e.weight;
    }
    return 1;
}

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
    vector<vector<pair<int, int>>> adj(N);
    for (int i = 0; i < M; ++i) {
        file >> edges[i].u >> edges[i].v >> edges[i].weight;
        if (edges[i].u < 0 || edges[i].v < 0 || edges[i].weight < 0) {
            cerr << "Invalid input: negative numbers are not allowed." << endl;
            return 1;
        }
        adj[edges[i].u].push_back({edges[i].v, edges[i].weight});
    }

    int src, dst;
    file >> src >> dst;
    file.close();

    if (src < 0 || dst < 0) {
        cerr << "Invalid input: negative numbers are not allowed." << endl;
        return 1;
    }

    vector<int> path = getDijkstraPath(N, adj, src, dst);

    const int screenWidth = 800, screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Traffic Simulation - Milestone 3");

    vector<Node> nodes(N);
    float centerX      = screenWidth  / 2.0f;
    float centerY      = screenHeight / 2.0f;
    float layoutRadius = 200.0f;
    int   nodeRadius   = 25;

    for (int i = 0; i < N; ++i) {
        float angle   = (2 * PI / N) * i - PI / 2;
        nodes[i].id   = i;
        nodes[i].pos  = { centerX + layoutRadius * cos(angle),
                          centerY + layoutRadius * sin(angle) };
    }

    enum AnimState { AT_NODE, MOVING, FINISHED, NO_PATH };
    AnimState state       = path.empty() ? NO_PATH : AT_NODE;
    bool      isPlaying   = false;
    int       pathIdx     = 0;
    float     timer       = 0.0f;
    Vector2   entityPos   = path.empty() ? Vector2{0, 0} : nodes[path[0]].pos;

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
                /* no wait at source or destination */
                if (pathIdx == 0 || pathIdx == (int)path.size() - 1) {
                    state = (pathIdx == (int)path.size() - 1) ? FINISHED : MOVING;
                    timer = 0.0f;
                } else {
                    timer += dt;
                    if (timer >= 1.0f) {
                        state = MOVING;
                        timer = 0.0f;
                    }
                }
            } else if (state == MOVING) {
                int u      = path[pathIdx];
                int v      = path[pathIdx + 1];
                int weight = getEdgeWeight(u, v, edges);
                float edgeTotalTime = weight * 0.3f;

                timer += dt;
                if (timer >= edgeTotalTime) {
                    pathIdx++;
                    timer     = 0.0f;
                    entityPos = nodes[v].pos;
                    state     = (pathIdx == (int)path.size() - 1) ? FINISHED : AT_NODE;
                } else {
                    float t   = timer / edgeTotalTime;
                    entityPos = { nodes[u].pos.x + (nodes[v].pos.x - nodes[u].pos.x) * t,
                                  nodes[u].pos.y + (nodes[v].pos.y - nodes[u].pos.y) * t };
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        /* draw edges */
        for (const auto& edge : edges) {
            DrawArrowWithWeight(nodes[edge.u].pos, nodes[edge.v].pos, edge.weight, nodeRadius);
        }

        /* highlight shortest path edges */
        if (!path.empty()) {
            for (int i = 0; i + 1 < (int)path.size(); ++i) {
                int u = path[i], v = path[i + 1];
                float dx  = nodes[v].pos.x - nodes[u].pos.x;
                float dy  = nodes[v].pos.y - nodes[u].pos.y;
                float len = sqrt(dx * dx + dy * dy);
                float dirX = dx / len, dirY = dy / len;
                Vector2 s = { nodes[u].pos.x + dirX * nodeRadius,
                              nodes[u].pos.y + dirY * nodeRadius };
                Vector2 e = { nodes[v].pos.x - dirX * nodeRadius,
                              nodes[v].pos.y - dirY * nodeRadius };
                DrawLineEx(s, e, 4.0f, ORANGE);
            }
        }

        /* draw nodes */
        for (const auto& node : nodes) {
            Color fill = LIGHTGRAY;
            if (!path.empty() && node.id == path[0])                    fill = GREEN;
            if (!path.empty() && node.id == path[(int)path.size() - 1]) fill = RED;

            DrawCircleV(node.pos, nodeRadius, fill);
            DrawCircleLines(node.pos.x, node.pos.y, nodeRadius, BLACK);
            const char* label    = TextFormat("%d", node.id);
            int         labelW   = MeasureText(label, 20);
            DrawText(label, (int)(node.pos.x - labelW / 2), (int)(node.pos.y - 10), 20, BLACK);
        }

        /* play/stop button */
        DrawRectangleRec(btnBounds, isPlaying ? RED : GREEN);
        DrawText(isPlaying ? "STOP" : "PLAY",
                 (int)(btnBounds.x + 25), (int)(btnBounds.y + 10), 20, WHITE);

        /* moving entity */
        if (state != NO_PATH) {
            DrawCircleV(entityPos, 15, ORANGE);
            DrawCircleLines(entityPos.x, entityPos.y, 15, DARKBROWN);
        }

        /* status messages */
        if (state == NO_PATH) {
            DrawText("No path found!", screenWidth / 2 - 90, 20, 24, RED);
        } else if (state == FINISHED) {
            DrawText("Arrived at destination!", screenWidth / 2 - 130, 20, 24, DARKGREEN);
        }

        /* legend */
        DrawText("Source: GREEN   Destination: RED   Entity: ORANGE",
                 10, screenHeight - 30, 16, DARKGRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
