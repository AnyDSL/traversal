#include <string>
#include <vector>
#include <fstream>
#include "thorin_runtime.h"
#include "traversal.h"
#include "bvh_format.h"

struct StackEntry {
    StackEntry() {}
    StackEntry(int i, int j) : node_id(i), dst_id(j) {}
    int node_id;
    int dst_id;
};

bool load_accel(const std::string& filename, Node*& nodes_ref, Vec4*& tris_ref) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in || !check_header(in) || !locate_block(in, BlockType::BVH))
        return false;

    bvh::Header h;
    in.read((char*)&h, sizeof(bvh::Header));

    std::vector<bvh::Node> nodes(h.node_count);
    in.read((char*)nodes.data(), sizeof(bvh::Node) * h.node_count);
    
    std::vector<int32_t> prim_ids(h.prim_count);
    in.read((char*)prim_ids.data(), sizeof(int32_t) * h.prim_count);

    std::vector<float> vertices(h.vert_count * 3);
    in.read((char*)vertices.data(), sizeof(float) * 3 * h.vert_count);

    std::vector<int32_t> tri_ids(h.prim_count * 3);
    in.read((char*)tri_ids.data(), sizeof(int32_t) * 3 * h.prim_count);

    std::vector<Node> node_stack;
    std::vector<Vec4> tri_stack;
    union { unsigned int i; float f; } sentinel = { 0x80000000 };

    auto leaf_node = [&] (const bvh::Node& node) {
        int node_id = ~(tri_stack.size());
        for (int i = 0; i < node.prim_count; i++) {
            int tri_id = prim_ids[i + node.child_first];
            int i0 = tri_ids[tri_id * 3 + 0];
            int i1 = tri_ids[tri_id * 3 + 1];
            int i2 = tri_ids[tri_id * 3 + 2];
            Vec4 v0 = { vertices[i0 * 3 + 0], vertices[i0 * 3 + 1], vertices[i0 * 3 + 2], 0.0f };
            Vec4 v1 = { vertices[i1 * 3 + 0], vertices[i1 * 3 + 1], vertices[i1 * 3 + 2], 0.0f };
            Vec4 v2 = { vertices[i2 * 3 + 0], vertices[i2 * 3 + 1], vertices[i2 * 3 + 2], 0.0f };
            tri_stack.push_back(v0);
            tri_stack.push_back(v1);
            tri_stack.push_back(v2);
        }
        tri_stack.back().w = sentinel.f;
        return node_id;
    };

    std::vector<StackEntry> stack;
    stack.emplace_back(0, 0);
    node_stack.push_back(Node());

    while (!stack.empty()) {
        StackEntry top = stack.back();
        const bvh::Node& n = nodes[top.node_id];
        stack.pop_back();

        if (n.prim_count > 0) continue;

        const bvh::Node& left = nodes[n.child_first];
        const bvh::Node& right = nodes[n.child_first + 1];

        int left_id;
        if (left.prim_count > 0) {
            left_id = leaf_node(left);
        } else {
            left_id = node_stack.size();
            node_stack.push_back(Node());
            stack.emplace_back(n.child_first, left_id);
        }

        int right_id;
        if (right.prim_count > 0) {
            right_id = leaf_node(right);
        } else {
            right_id = node_stack.size();
            node_stack.push_back(Node());
            stack.emplace_back(n.child_first + 1, right_id);
        }

        BBox& left_bb = node_stack[top.dst_id].left_bb;
        BBox& right_bb = node_stack[top.dst_id].right_bb;
        
        left_bb.lo_x = left.min[0];
        left_bb.lo_y = left.min[1];
        left_bb.lo_z = left.min[2];

        left_bb.hi_x = left.max[0];
        left_bb.hi_y = left.max[1];
        left_bb.hi_z = left.max[2];

        right_bb.lo_x = right.min[0];
        right_bb.lo_y = right.min[1];
        right_bb.lo_z = right.min[2];

        right_bb.hi_x = right.max[0];
        right_bb.hi_y = right.max[1];
        right_bb.hi_z = right.max[2];

        node_stack[top.dst_id].left = left_id;
        node_stack[top.dst_id].right = right_id;
    }

    nodes_ref = thorin_new<Node>(node_stack.size());
    std::copy(node_stack.begin(), node_stack.end(), nodes_ref);

    tris_ref = thorin_new<Vec4>(tri_stack.size());
    std::copy(tri_stack.begin(), tri_stack.end(), tris_ref);

    return true;
}
