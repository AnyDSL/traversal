#include <string>
#include <vector>
#include <fstream>
#include "thorin_runtime.h"
#include "interface.h"

#include "bvh_file_info.hpp"


struct stack_entry {
    stack_entry() {}
    stack_entry(int i, int j) : node_id(i), dst_id(j) {}
    int node_id;
    int dst_id;
};

bool load_accel(const std::string& filename, Node*& nodes_ref, Vec4*& tris_ref) {
    
    FILE *in = NULL;
    in = fopen(filename.c_str(), "rb");
    if (!in) {  
        return false;
    }

    //Check Magic Number
    int magic=0;
    fread(&magic, sizeof(int), 1, in);
 

    if(magic != io::bvh_file_magic_number) {
        fclose(in);
        return false;
    }


    std::vector<io::bvh::node> nodes;
    std::vector<int> prim_ids;
    std::vector<float> vertices;
    std::vector<int> tri_ids;

    while(!feof(in)) {
        long next=0;
        fread(&next, sizeof(long), 1, in);

        io::Data_Block_Type type;
        fread(&type, sizeof(io::Data_Block_Type), 1, in);

        if(type == io::Data_Block_Type::BVH) {
            //Read file header
            io::bvh::hdr h;
            fread( (char*)&h, sizeof (io::bvh::hdr), 1, in);

            // Read nodes
            nodes.resize(h.node_count);
            fread((char*) nodes.data(), sizeof(io::bvh::node), h.node_count, in);

            // Read bvh primitive indices            
            prim_ids.resize(h.prim_count); 
            fread( (char*) prim_ids.data(), sizeof(int), h.prim_count, in);

            // Read vertices
            vertices.resize(3 * h.vert_count);
            fread((char*)vertices.data(), sizeof(float), 3 * h.vert_count, in);
             
            // Read triangle vertices indices
            tri_ids.resize(h.prim_count * 3);
            fread((char*)tri_ids.data(), sizeof(int), 3 * h.prim_count, in);

            break;
        } else {
           fseek(in, next, SEEK_SET);
        }

    }
    fclose(in);


    std::vector<Node> node_stack;
    std::vector<Vec4> tri_stack;
    union { unsigned int i; float f; } sentinel = { 0x80000000 };

    auto leaf_node = [&] (const io::bvh::node& node) {
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

    std::vector<stack_entry> stack;
    stack.push_back(stack_entry(0, 0));
    node_stack.push_back(Node());

    while (!stack.empty()) {
        stack_entry top = stack.back();
        const io::bvh::node& n = nodes[top.node_id];
        stack.pop_back();

        if (n.prim_count > 0) continue;

        const io::bvh::node& left = nodes[n.child_first];
        const io::bvh::node& right = nodes[n.child_first + 1];

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
