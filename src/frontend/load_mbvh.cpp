#include <string>
#include <vector>
#include <fstream>
#include <cfloat>
#include "thorin_runtime.h"
#include "traversal.h"
#include "bvh_format.h"

bool load_accel(const std::string& filename, Node*& nodes_ref, Vec4*& tris_ref) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in || !check_header(in) || !locate_block(in, BlockType::MBVH))
        return false;

    mbvh::Header h;
    in.read((char*)&h, sizeof(mbvh::Header));

    // Read nodes
    std::vector<mbvh::Node> nodes(h.node_count);
    in.read((char*)nodes.data(), sizeof(mbvh::Node) * h.node_count);

    // Read vertices
    std::vector<float> vertices(3 * h.vert_count);
    in.read((char*)vertices.data(), sizeof(float) * 3 * h.vert_count);
    
    std::vector<Node> node_stack;
    std::vector<Vec4> tri_stack;

    auto leaf_node = [&] (const mbvh::Node& node, int c) {
        int node_id = ~(tri_stack.size());

        for (int j = 0; j < node.prim_count[c]; j += 4) {
            union{
                struct {
                    Vec4 v0_x, v0_y, v0_z;
                    Vec4 e1_x, e1_y, e1_z;
                    Vec4 e2_x, e2_y, e2_z;
                    Vec4 n_x, n_y, n_z;
                } tri_data;
                float raw_data[4 * 4 * 3];
            } tri;

            int i = 0;
            for (; i < 4 && i < node.prim_count[c] - j; i++) {
                int i0 = node.children[c] + (i + j) * 3, i1 = i0 + 1, i2 = i0 + 2;
                const float v0_x = vertices[i0 * 3 + 0];
                const float v0_y = vertices[i0 * 3 + 1];
                const float v0_z = vertices[i0 * 3 + 2];

                tri.raw_data[0 + i] = v0_x;
                tri.raw_data[4 + i] = v0_y;
                tri.raw_data[8 + i] = v0_z;

                const float e1_x = v0_x - vertices[i1 * 3 + 0];
                const float e1_y = v0_y - vertices[i1 * 3 + 1];
                const float e1_z = v0_z - vertices[i1 * 3 + 2];
                const float e2_x = vertices[i2 * 3 + 0] - v0_x;
                const float e2_y = vertices[i2 * 3 + 1] - v0_y;
                const float e2_z = vertices[i2 * 3 + 2] - v0_z;

                tri.raw_data[12 + i] = e1_x;
                tri.raw_data[16 + i] = e1_y;
                tri.raw_data[20 + i] = e1_z;
                tri.raw_data[24 + i] = e2_x;
                tri.raw_data[28 + i] = e2_y;
                tri.raw_data[32 + i] = e2_z;
                tri.raw_data[36 + i] = e1_y * e2_z - e1_z * e2_y;
                tri.raw_data[40 + i] = e1_z * e2_x - e1_x * e2_z;
                tri.raw_data[44 + i] = e1_x * e2_y - e1_y * e2_x;
            }
            for (; i < 4; i++) {
                // Fill with zeros
                tri.raw_data[ 0 + i] = tri.raw_data[ 4 + i] = tri.raw_data[ 8 + i] = FLT_MAX;
                tri.raw_data[12 + i] = tri.raw_data[16 + i] = tri.raw_data[20 + i] = 0.0f;
                tri.raw_data[24 + i] = tri.raw_data[28 + i] = tri.raw_data[32 + i] = 0.0f;
                tri.raw_data[36 + i] = tri.raw_data[40 + i] = tri.raw_data[44 + i] = 0.0f;
            }

            tri_stack.push_back(tri.tri_data.v0_x);
            tri_stack.push_back(tri.tri_data.v0_y);
            tri_stack.push_back(tri.tri_data.v0_z);
            tri_stack.push_back(tri.tri_data.e1_x);
            tri_stack.push_back(tri.tri_data.e1_y);
            tri_stack.push_back(tri.tri_data.e1_z);
            tri_stack.push_back(tri.tri_data.e2_x);
            tri_stack.push_back(tri.tri_data.e2_y);
            tri_stack.push_back(tri.tri_data.e2_z);
            tri_stack.push_back(tri.tri_data.n_x);
            tri_stack.push_back(tri.tri_data.n_y);
            tri_stack.push_back(tri.tri_data.n_z);
        }

        // Insert sentinel
        Vec4 tri_sentinel = { -0.0f, -0.0f, -0.0f, -0.0f };
        tri_stack.push_back(tri_sentinel);
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

    nodes_ref = thorin_new<Node>(node_stack.size());
    std::copy(node_stack.begin(), node_stack.end(), nodes_ref);

    tris_ref = thorin_new<Vec4>(tri_stack.size());
    std::copy(tri_stack.begin(), tri_stack.end(), tris_ref);

    return true;
}
