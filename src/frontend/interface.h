/* interface.h : Impala interface file generated by impala */
#ifndef INTERFACE_H
#define INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

struct BBox {
    float min[3];
    float max[3];
};

struct Node {
    struct BBox left_bb;
    struct BBox right_bb;
    int right;
    int left;
    int pad0;
    int pad1;
};

struct Hit {
    int tri_id;
    float tmax;
    int pad0;
    int pad1;
};

struct Vec4{
    float x, y, z, w;
};

struct Vec3 {
    float x, y, z;
};

typedef Vec4 Ray;

void traverse_accel(struct Node* nodes_ptr, float* rays_ptr, float* tris_ptr, struct Hit* hits_ptr, int ray_count);

#ifdef __cplusplus
}
#endif

#endif /* INTERFACE_H */

