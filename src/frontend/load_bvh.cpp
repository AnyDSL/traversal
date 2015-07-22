#include <string>
#include <vector>
#include <fstream>
#include "thorin_runtime.h"
#include "interface.h"

namespace bvh {

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

struct stack_entry {
    stack_entry() {}
    stack_entry(int i, int j) : node_id(i), dst_id(j) {}
    int node_id;
    int dst_id;
};

}

bool load_accel(const std::string& filename, Node*& nodes_ref, Vec4*& tris_ref) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in) return false;

    bvh::hdr h;
    in.read((char*)&h, sizeof(bvh::hdr));

    // Read nodes
    std::vector<bvh::node> nodes(h.node_count);
    in.read((char*)nodes.data(), sizeof(bvh::node) * h.node_count);

    // Read bvh primitive indices
    std::vector<int> prim_ids(h.prim_count);
    in.read((char*)prim_ids.data(), sizeof(int) * h.prim_count);

    // Read vertices
    std::vector<float> vertices(3 * h.vert_count);
    in.read((char*)vertices.data(), sizeof(float) * 3 * h.vert_count);
    
    // Skip normals = 3 * float * verts, texcoords = 2 * float * verts, materials = int * prims
    in.seekg(sizeof(float) * 5 * h.vert_count + sizeof(int) * h.prim_count, std::ifstream::cur);

    // Read triangle vertices indices
    std::vector<int> tri_ids(h.prim_count * 3);
    in.read((char*)tri_ids.data(), sizeof(int) * 3 * h.prim_count);

    std::vector<Node> node_stack;
    std::vector<Vec4> tri_stack;
    union { unsigned int i; float f; } sentinel = { 0x80000000 };

    auto leaf_node = [&] (const bvh::node& node) {
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

    std::vector<bvh::stack_entry> stack;
    stack.push_back(bvh::stack_entry(0, 0));
    node_stack.push_back(Node());

    while (!stack.empty()) {
        bvh::stack_entry top = stack.back();
        const bvh::node& n = nodes[top.node_id];
        stack.pop_back();

        if (n.prim_count > 0) continue;

        const bvh::node& left = nodes[n.child_first];
        const bvh::node& right = nodes[n.child_first + 1];

        int left_id;
        if (left.prim_count > 0) {
            left_id = leaf_node(left);
        } else {
            left_id = node_stack.size();
            node_stack.push_back(Node());
            stack.push_back(bvh::stack_entry(n.child_first, left_id));
        }

        int right_id;
        if (right.prim_count > 0) {
            right_id = leaf_node(right);
        } else {
            right_id = node_stack.size();
            node_stack.push_back(Node());
            stack.push_back(bvh::stack_entry(n.child_first + 1, right_id));
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
