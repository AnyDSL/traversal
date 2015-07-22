#ifndef BVH_FILE_INFO_
#define BVH_FILE_INFO_


/**
* BVH FILE layout:
* <bvh_file_magic_number>
* <file_next_offset> gives offset to the begining of the next data block
* <Data_Block_Type> gives block type
* [DATA]               either for the BVH or the MBVH depending on the value stored in <Data_Block_Type>
* ...
* <file_next_offset> gives offset to the begining of the next data block
* <Data_Block_Type> gives block type
* [DATA]               either for the BVH or the MBVH depending on the value stored in <Data_Block_Type>
* ...
*
* [DATA] layout:
* BVH:
*   <BVH_Header>
*   <Nodes>
*   <PrimIds>
*   <Vertices>
*
* MBVH:
*   <MBVH_Header>
*   <BVH4Node>
*   <Vec3> *verts_count
**/


namespace io {

    const uint bvh_file_magic_number = 0x312F1A51;

    enum Data_Block_Type {
        BVH = 1,
        MBVH = 2
    };

    long file_next_offset;

    namespace bvh {

        struct node {
            float min[3];
            float max[3];
            int child_first;
            unsigned short prim_count;
            unsigned short axis;
        };

        struct hdr {
            int node_count;
            int prim_count;
	        int vert_count;
        };
    }


    namespace mbvh {
        struct bbox {
            float lx, ly, lz;
            float ux, uy, uz;            
        };

        struct hdr {
            uint node_count;
            uint vert_count;
            BBox scene_bb;
        };


        struct node {
            BBox box[4];
            int children[4];           //Leaves: idx to vertices array
            int numPrims[4];           //If node is an inner node = -1. If it is a leaf it contains the number of primitive. Note that leafs can have 0 primitives
        };
    }

}



#endif
