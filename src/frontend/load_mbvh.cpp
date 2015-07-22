#include <string>
#include <vector>
#include <fstream>
#include "thorin_runtime.h"
#include "interface.h"
#include "bvh_file.h"

bool load_accel(const std::string& filename, Node*& nodes_ref, Vec4*& tris_ref) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in || !io::check_header(in) ||
        !io::locate_block(in, io::MBVH)) {
        return false;
    }

    io::mbvh::Header h;
    in.read((char*)&h, sizeof(io::mbvh::Header));

    // Read nodes
    std::vector<io::mbvh::Node> nodes(h.node_count);
    in.read((char*)nodes.data(), sizeof(io::mbvh::Node) * h.node_count);

    // Read vertices
    std::vector<float> vertices(3 * h.vert_count);
    in.read((char*)vertices.data(), sizeof(float) * 3 * h.vert_count);
    
    std::vector<Node> node_stack;
    std::vector<Vec4> tri_stack;
    union { unsigned int i; float f; } sentinel = { 0x80000000 };

    auto leaf_node = [&] (const io::mbvh::Node& node, int c) {
        int node_id = ~(tri_stack.size());
        for (int i = 0; i < node.prim_count[c]; i++) {
            int i0 = node.children[c] + i * 3, i1 = i0 + 1, i2 = i0 + 2;
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

    for (int i = 0; i < h.node_count; i++) {
        const io::mbvh::Node& src_node = nodes[i];
        Node dst_node;
        int k = 0;
        for (int j = 0  ; j < 4; j++) {
            if (src_node.prim_count[j] == 0) {
                // Empty leaf
                continue;
            }

            dst_node.min_x[k] = src_node.bb[j].lx;
            dst_node.min_y[k] = src_node.bb[j].ly;
            dst_node.min_z[k] = src_node.bb[j].lz;

            dst_node.max_x[k] = src_node.bb[j].ux;
            dst_node.max_y[k] = src_node.bb[j].uy;
            dst_node.max_z[k] = src_node.bb[j].uz;

            if (src_node.prim_count[j] < 0) {
                // Inner node
                dst_node.children[k] = src_node.children[j];
            } else {
                // Leaf
                dst_node.children[k] = leaf_node(src_node, j);
            }

            k++;
        }

        for (; k < 4; k++) {
            dst_node.min_x[k] = 1.0f;
            dst_node.min_y[k] = 1.0f;
            dst_node.min_z[k] = 1.0f;

            dst_node.max_x[k] = -1.0f;
            dst_node.max_y[k] = -1.0f;
            dst_node.max_z[k] = -1.0f;

            dst_node.children[k] = 0;
        }

        node_stack.push_back(dst_node);
    }

    nodes_ref = thorin_new<Node>(node_stack.size());
    std::copy(node_stack.begin(), node_stack.end(), nodes_ref);

    tris_ref = thorin_new<Vec4>(tri_stack.size());
    std::copy(tri_stack.begin(), tri_stack.end(), tris_ref);

    return true;
}
