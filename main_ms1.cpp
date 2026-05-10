#include <iostream>
#include <vector>
#include <fstream>
#include <queue>
#include <climits>

using namespace std;
const int INF = INT_MAX;

void dijkstra(int N, const vector<vector<pair<int, int>>>& adj, int src, int dst) {
    if (src == dst) {
        cout << src << endl;
        cout << 0 << endl;
        return;
    }

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

    if (dist[dst] == INF) {
        cout << "No path found" << endl;
        return;
    }

    vector<int> path;
    for (int v = dst; v != -1; v = parent[v]) {
        path.push_back(v);
    }

    for (int i = path.size() - 1; i >= 0; --i) {
        cout << path[i];
        if (i > 0) cout << " -> ";
    }
    cout << endl;
    cout << dist[dst] << endl;
}

int main(int argc, char* argv[]) {
    string filename = "input.txt";
    if (argc > 1) {
        filename = argv[1];
    }

    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << filename << endl;
        return 1;
    }

    int N, M;
    if (!(file >> N >> M)) return 0;
    if (N < 0 || M < 0) {
        cerr << "Invalid input: negative numbers are not allowed." << endl;
        return 1;
    }

    vector<vector<pair<int, int>>> adj(N);
    for (int i = 0; i < M; ++i) {
        int u, v, w;
        file >> u >> v >> w;
        if (u < 0 || v < 0 || w < 0) {
            cerr << "Invalid input: negative numbers are not allowed." << endl;
            return 1;
        }
        adj[u].push_back({v, w});
    }

    int src, dst;
    file >> src >> dst;
    if (src < 0 || dst < 0) {
        cerr << "Invalid input: negative numbers are not allowed." << endl;
        return 1;
    }

    dijkstra(N, adj, src, dst);
    return 0;
}
