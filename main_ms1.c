#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define INF INT_MAX

typedef struct {
    int v, weight;
} AdjEdge;

typedef struct {
    AdjEdge *edges;
    int count, capacity;
} AdjList;

typedef struct {
    int dist, node;
} HeapNode;

typedef struct {
    HeapNode *data;
    int size, capacity;
} MinHeap;

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
        HeapNode tmp        = h->data[i];
        h->data[i]          = h->data[smallest];
        h->data[smallest]   = tmp;
        i = smallest;
    }
    return top;
}

static void dijkstra(int N, AdjList *adj, int src, int dst) {
    if (src == dst) {
        printf("%d\n0\n", src);
        return;
    }

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

    if (dist[dst] == INF) {
        printf("No path found\n");
    } else {
        int *tmp = malloc(N * sizeof(int));
        int  len = 0;
        for (int v = dst; v != -1; v = parent[v])
            tmp[len++] = v;
        for (int i = len - 1; i >= 0; i--) {
            printf("%d", tmp[i]);
            if (i > 0) printf(" -> ");
        }
        printf("\n%d\n", dist[dst]);
        free(tmp);
    }

    free(dist);
    free(parent);
    free(h.data);
}

int main(int argc, char *argv[]) {
    const char *filename = argc > 1 ? argv[1] : "input.txt";

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return 1;
    }

    int N, M;
    if (fscanf(file, "%d %d", &N, &M) != 2) return 0;
    if (N < 0 || M < 0) {
        fprintf(stderr, "Invalid input: negative numbers are not allowed.\n");
        return 1;
    }

    AdjList *adj = calloc(N, sizeof(AdjList));
    for (int i = 0; i < M; i++) {
        int u, v, w;
        fscanf(file, "%d %d %d", &u, &v, &w);
        if (u < 0 || v < 0 || w < 0) {
            fprintf(stderr, "Invalid input: negative numbers are not allowed.\n");
            return 1;
        }
        adjListAdd(&adj[u], v, w);
    }

    int src, dst;
    fscanf(file, "%d %d", &src, &dst);
    fclose(file);

    if (src < 0 || dst < 0) {
        fprintf(stderr, "Invalid input: negative numbers are not allowed.\n");
        return 1;
    }

    dijkstra(N, adj, src, dst);

    for (int i = 0; i < N; i++) free(adj[i].edges);
    free(adj);
    return 0;
}
