#include <fstream>
#include <string>
#include "thorin_runtime.h"
#include "interface.h"

bool load_rays(const std::string& filename, Ray*& rays_ref, int& count, float tmin, float tmax) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in) return false;

    in.seekg(0, std::ifstream::end);
    count = in.tellg() / (sizeof(float) * 6);

    rays_ref = thorin_new<Vec4>(count * 2);
    in.seekg(0);

    for (int i = 0; i < count; i++) {
        float org_dir[6];
        in.read((char*)org_dir, sizeof(float) * 6);
        rays_ref[i * 2 + 0].x = org_dir[0];
        rays_ref[i * 2 + 0].y = org_dir[1];
        rays_ref[i * 2 + 0].z = org_dir[2];

        rays_ref[i * 2 + 1].x = org_dir[3];
        rays_ref[i * 2 + 1].y = org_dir[4];
        rays_ref[i * 2 + 1].z = org_dir[5];

        rays_ref[i * 2 + 0].w = tmin;
        rays_ref[i * 2 + 1].w = tmax;
    }

    return true;
}
