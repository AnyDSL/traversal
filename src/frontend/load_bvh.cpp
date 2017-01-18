#include <string>
#include <vector>
#include <fstream>
#include <anydsl_runtime.hpp>

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

bool load_accel(const std::string& filename, anydsl::Array<Node>& nodes_ref, anydsl::Array<Vec4>& tris_ref) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in || !check_header(in) || !locate_block(in, BlockType::BVH))
        return false;

    bvh::Header h;
    in.read((char*)&h, sizeof(bvh::Header));

    anydsl::Array<Node> host_nodes(h.node_count);
    anydsl::Array<Vec4> host_tris(h.prim_count);

    in.read((char*)host_nodes.data(), sizeof(Node) * h.node_count);
    in.read((char*)host_tris.data(), sizeof(Vec4) * h.prim_count);

    tris_ref = std::move(anydsl::Array<Vec4>(anydsl::Platform::TRAVERSAL_PLATFORM, anydsl::Device(TRAVERSAL_DEVICE), h.prim_count));
    anydsl::copy(host_tris, tris_ref);
    nodes_ref = std::move(anydsl::Array<Node>(anydsl::Platform::TRAVERSAL_PLATFORM, anydsl::Device(TRAVERSAL_DEVICE), h.node_count));
    anydsl::copy(host_nodes, nodes_ref);

    return true;
}
