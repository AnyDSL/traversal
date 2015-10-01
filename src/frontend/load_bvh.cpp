#include <string>
#include <vector>
#include <fstream>
#include "thorin_runtime.h"
#include "traversal.h"
#include "bvh_format.h"

static inline float as_float(int i) {
    union {
        int i;
        float f;
    } u;
    u.i = i;
    return u.f;
}

bool load_accel(const std::string& filename, Node*& nodes_ref, Vec4*& tris_ref) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in || !check_header(in) || !locate_block(in, BlockType::BVH))
        return false;

    bvh::Header h;
    in.read((char*)&h, sizeof(bvh::Header));

    nodes_ref = thorin_new<Node>(h.node_count);
    in.read((char*)nodes_ref, sizeof(Node) * h.node_count);

    tris_ref = thorin_new<Vec4>(h.prim_count);
    in.read((char*)tris_ref, sizeof(Vec4) * h.prim_count);

    for (int i = 0; i < h.prim_count / 3; i++) {
        tris_ref[i * 3 + 1].w = as_float(i * 3);    
    }

    return true;
}
