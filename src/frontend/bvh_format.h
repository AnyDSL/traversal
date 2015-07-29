#ifndef BVH_FILE_H
#define BVH_FILE_H

#include <cstdint>

enum class BlockType {
    BVH = 1,
    MBVH = 2
};

namespace bvh {
    struct Node {
        float min[3];
        float max[3];
        int32_t child_first;
        int16_t prim_count;
        int16_t axis;
    };

    struct Header {
        uint32_t node_count;
        uint32_t prim_count;
        uint32_t vert_count;
    };
}

namespace mbvh {
    struct BBox {
        float lx, ly, lz;
        float ux, uy, uz;            
    };

    struct Header {
        uint32_t node_count;
        uint32_t vert_count;
        BBox scene_bb;
    };

    struct Node {
        BBox bb[4];
        int32_t children[4];
        int32_t prim_count[4];
    };
}

inline bool check_header(std::istream& is) {
    uint32_t magic;
    is.read((char*)&magic, sizeof(uint32_t));
    return magic == 0x312F1A57;
}

inline bool locate_block(std::istream& is, BlockType type) {
    uint32_t block_type;
    uint64_t offset = 0;
    do {
        is.seekg(offset, std::istream::cur);

        is.read((char*)&offset, sizeof(uint64_t));
        if (is.gcount() != sizeof(std::uint64_t)) return false;
        is.read((char*)&block_type, sizeof(uint32_t));
        if (is.gcount() != sizeof(uint32_t)) return false;

        offset -= sizeof(BlockType);
    } while (!is.eof() && block_type != (uint32_t)type);

    return is;
}

#endif 
