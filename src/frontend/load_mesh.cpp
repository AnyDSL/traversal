#include <string>
#include <vector>
#include <fstream>

#include "traversal.h"
#include "bvh_format.h"

bool load_mesh(const std::string& filename, std::vector<int>& indices, std::vector<float>& vertices) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in || !check_header(in) || !locate_block(in, BlockType::MESH))
        return false;

    mesh::Header h;
    in.read((char*)&h, sizeof(mesh::Header));

    indices.resize(h.tri_count * 3);
    vertices.resize(h.vert_count * 4);

    in.read((char*)vertices.data(), sizeof(float) * 4 * h.vert_count);
    in.read((char*)indices.data(), sizeof(int) * 3 * h.tri_count);

    return true;
}
