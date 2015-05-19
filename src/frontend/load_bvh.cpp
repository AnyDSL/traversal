#include <string>
#include <vector>
#include <fstream>
#include "thorin_runtime.h"
#include "interface.h"

struct hdr {
    int node_count;
    int prim_count;
    int vert_count;
    int norm_count;
    int tex_count;
    int mat_count;
};

struct node {
    float min[3];
    float max[3];
    int child_first;
    unsigned short prim_count;
    unsigned short axis;
};

bool load_bvh(const std::string& filename, Accel*& accel) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in) return false;

    hdr h;
    in.read((char*)&h, sizeof(hdr));

    Accel* bvh = thorin_new<Accel>(1);
    bvh->nodes = thorin_new<BvhNode>(h.node_count);
    bvh->indices = thorin_new<int>(h.prim_count * 3);
    bvh->vertices = thorin_new<float>(h.vert_count * 3);

    // Read nodes
    for (int i = 0; i < h.node_count; i++) {
        node n;
        in.read((char*)&n, sizeof(node));

        bvh->nodes[i].min[0] = n.min[0];
        bvh->nodes[i].min[1] = n.min[1];
        bvh->nodes[i].min[2] = n.min[2];

        bvh->nodes[i].max[0] = n.max[0];
        bvh->nodes[i].max[1] = n.max[1];
        bvh->nodes[i].max[2] = n.max[2];

        bvh->nodes[i].child_tri = n.child_first;
        bvh->nodes[i].prim_count = n.prim_count;
    }

    // Read bvh primitive indices
    std::vector<int> prim_ids(h.prim_count);
    in.read((char*)prim_ids.data(), sizeof(int) * h.prim_count);

    // Read vertices
    in.read((char*)bvh->vertices, sizeof(float) * 3 * h.vert_count);
    
    // Skip normals = 3 * float * verts, texcoords = 2 * float * verts, materials = int * prims
    in.seekg(sizeof(float) * 5 * h.vert_count + sizeof(int) * h.prim_count, std::ifstream::cur);

    // Read triangle vertices indices
    std::vector<int> tri_ids(h.prim_count * 3);
    in.read((char*)tri_ids.data(), sizeof(int) * 3 * h.prim_count);
    for (int i = 0; i < h.prim_count; i++) {
        int prim = prim_ids[i];
        bvh->indices[i * 3 + 0] = tri_ids[prim * 3 + 0];
        bvh->indices[i * 3 + 1] = tri_ids[prim * 3 + 1];
        bvh->indices[i * 3 + 2] = tri_ids[prim * 3 + 2];
    }

    bvh->root = 0;

    accel = bvh;

    return true;
}
