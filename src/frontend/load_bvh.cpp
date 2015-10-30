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

    thorin::Array<Node> host_nodes(h.node_count);
    thorin::Array<Vec4> host_tris(h.prim_count);

    in.read((char*)host_nodes.data(), sizeof(Node) * h.node_count);
    in.read((char*)host_tris.data(), sizeof(Vec4) * h.prim_count);

    for (int i = 0; i < h.prim_count / 3; i++) {
        host_tris.data()[i * 3 + 1].w = as_float(i * 3);    
    }

    tris_ref = std::move(thorin::Array<Vec4>(thorin::Platform::TRAVERSAL_PLATFORM, thorin::Device(TRAVERSAL_DEVICE), h.prim_count));
    thorin::copy(host_tris, tris_ref);
    nodes_ref = std::move(thorin::Array<Node>(thorin::Platform::TRAVERSAL_PLATFORM, thorin::Device(TRAVERSAL_DEVICE), h.node_count));
    thorin::copy(host_nodes, nodes_ref);

    return true;
}
