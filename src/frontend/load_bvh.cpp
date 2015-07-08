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

struct stack_entry {
    stack_entry() {}
    stack_entry(int i, int j) : node_id(i), dst_id(j) {}
    int node_id;
    int dst_id;
};

bool load_bvh(const std::string& filename, Node*& nodes_ref, Vec3*& tris_ref) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in) return false;

    hdr h;
    in.read((char*)&h, sizeof(hdr));

    // Read nodes
    std::vector<node> nodes(h.node_count);
    in.read((char*)nodes.data(), sizeof(node) * h.node_count);

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
    std::vector<Vec3> tri_stack;
    Vec3 tri_sentinel = { -0.0f, -0.0f, -0.0f };

    auto leaf_node = [&] (const node& node) {
        int node_id = ~(tri_stack.size());
        for (int i = 0; i < node.prim_count; i++) {
            int tri_id = prim_ids[i + node.child_first];
            int i0 = tri_ids[tri_id * 3 + 0];
            int i1 = tri_ids[tri_id * 3 + 1];
            int i2 = tri_ids[tri_id * 3 + 2];
            Vec3 v0 = { vertices[i0 * 3 + 0], vertices[i0 * 3 + 1], vertices[i0 * 3 + 2] };
            Vec3 v1 = { vertices[i1 * 3 + 0], vertices[i1 * 3 + 1], vertices[i1 * 3 + 2] };
            Vec3 v2 = { vertices[i2 * 3 + 0], vertices[i2 * 3 + 1], vertices[i2 * 3 + 2] };
            tri_stack.push_back(v0);
            tri_stack.push_back(v1);
            tri_stack.push_back(v2);
        }
        tri_stack.push_back(tri_sentinel);
        return node_id;
    };

    std::vector<stack_entry> stack;
    stack.push_back(stack_entry(0, 0));
    node_stack.push_back(Node());

    while (!stack.empty()) {
        stack_entry top = stack.back();
        const node& n = nodes[top.node_id];
        stack.pop_back();

        if (n.prim_count > 0) continue;

        const node& left = nodes[n.child_first];
        const node& right = nodes[n.child_first + 1];

        int left_id;
        if (left.prim_count > 0) {
            left_id = leaf_node(left);
        } else {
            left_id = node_stack.size();
            node_stack.push_back(Node());
            stack.push_back(stack_entry(n.child_first, left_id));
        }

        int right_id;
        if (right.prim_count > 0) {
            right_id = leaf_node(right);
        } else {
            right_id = node_stack.size();
            node_stack.push_back(Node());
            stack.push_back(stack_entry(n.child_first + 1, right_id));
        }

        Vec3& lmin = node_stack[top.dst_id].left_bb.min;
        Vec3& lmax = node_stack[top.dst_id].left_bb.max;
        Vec3& rmin = node_stack[top.dst_id].right_bb.min;
        Vec3& rmax = node_stack[top.dst_id].right_bb.max;
        
        lmin.x = left.min[0];
        lmin.y = left.min[1];
        lmin.z = left.min[2];

        lmax.x = left.max[0];
        lmax.y = left.max[1];
        lmax.z = left.max[2];

        rmin.x = right.min[0];
        rmin.y = right.min[1];
        rmin.z = right.min[2];

        rmax.x = right.max[0];
        rmax.y = right.max[1];
        rmax.z = right.max[2];

        node_stack[top.dst_id].left = left_id;
        node_stack[top.dst_id].right = right_id;
    }

    nodes_ref = thorin_new<Node>(node_stack.size());
    std::copy(node_stack.begin(), node_stack.end(), nodes_ref);

    tris_ref = thorin_new<Vec3>(tri_stack.size());
    std::copy(tri_stack.begin(), tri_stack.end(), tris_ref);

    return true;
}
