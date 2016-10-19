#include <string>
#include <vector>
#include <fstream>
#include <cfloat>
#include <cstring>
#include <cassert>
#include <thorin_runtime.hpp>

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

bool load_accel(const std::string& filename, thorin::Array<Node>& nodes_ref, thorin::Array<float>& tris_ref) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in || !check_header(in) || !locate_block(in, BlockType::MBVH))
        return false;

    mbvh::Header h;
    in.read((char*)&h, sizeof(mbvh::Header));

    // Read nodes
    std::vector<mbvh::Node> nodes(h.node_count);
    in.read((char*)nodes.data(), sizeof(mbvh::Node) * h.node_count);
    assert(in.gcount() == sizeof(mbvh::Node) * h.node_count);

    // Read vertices
    std::vector<float> vertices(4 * h.vert_count);
    in.read((char*)vertices.data(), sizeof(float) * 4 * h.vert_count);
    assert(in.gcount() == sizeof(float) * 4 * h.vert_count);
    
    std::vector<Node> node_stack;
    std::vector<float> tri_stack;

    auto leaf_node = [&] (const mbvh::Node& node, int c) {
        int node_id = ~(tri_stack.size() / 4);

        for (int i = 0; i < node.prim_count[c]; i++) {
            int cur = tri_stack.size();
            tri_stack.resize(cur + 4 * 13);
            memcpy(&tri_stack[cur], vertices.data() + node.children[c] * 4 + i * 4 * 13, sizeof(float) * 4 * 13);
        }

        // Insert sentinel
        tri_stack.push_back(as_float(0x80000000));
        tri_stack.push_back(as_float(0x80000000));
        tri_stack.push_back(as_float(0x80000000));
        tri_stack.push_back(as_float(0x80000000));
        return node_id;
    };

    for (int i = 0; i < h.node_count; i++) {
        const mbvh::Node& src_node = nodes[i];
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

    nodes_ref = std::move(thorin::Array<Node>(node_stack.size()));
    std::copy(node_stack.begin(), node_stack.end(), nodes_ref.begin());

    tris_ref = std::move(thorin::Array<float>(tri_stack.size()));
    std::copy(tri_stack.begin(), tri_stack.end(), tris_ref.begin());

    return true;
}
