#ifndef BVH_FILE_H
#define BVH_FILE_H

namespace io {
    typedef unsigned int uint;

    enum BlockType {
        BVH = 1,
        MBVH = 2
    };

    namespace bvh {
        struct Node {
            float min[3];
            float max[3];
            int child_first;
            unsigned short prim_count;
            unsigned short axis;
        };

        struct Header {
            int node_count;
            int prim_count;
	        int vert_count;
        };
    }

    namespace mbvh {
        struct BBox {
            float lx, ly, lz;
            float ux, uy, uz;            
        };

        struct Header {
            uint node_count;
            uint vert_count;
            BBox scene_bb;
        };

        struct Node {
            BBox bb[4];
            int children[4];
            int prim_count[4];
        };
    }

    inline bool check_header(std::istream& is) {
        uint magic;
        is.read((char*)&magic, sizeof(uint));
        return magic == 0x312F1A57;
    }

    inline bool locate_block(std::istream& is, BlockType type) {
        BlockType block_type;
        long offset = 0;
        do {
            is.seekg(offset, std::istream::cur);

            is.read((char*)&offset, sizeof(long));
            if (is.gcount() != sizeof(long)) return false;
            is.read((char*)&block_type, sizeof(BlockType));
            if (is.gcount() != sizeof(BlockType)) return false;

            offset -= sizeof(BlockType);
        } while (!is.eof() && block_type != type);

        return is;
    }
}

#endif 
