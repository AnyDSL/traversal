#include <fstream>
#include <string>
#include "thorin_runtime.h"
#include "interface.h"

bool load_rays(const std::string& filename, Ray*& rays_ref, int& count, float tmin, float tmax) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in) return false;

    in.seekg(0, std::ifstream::end);
    count = in.tellg() / (sizeof(float) * 6);

    rays_ref = thorin_new<Ray>(count);
    in.seekg(0);

    for (int i = 0; i < count; i++) {
        float org_dir[6];
        in.read((char*)org_dir, sizeof(float) * 6);

        rays_ref[i].org.x = org_dir[0];
        rays_ref[i].org.y = org_dir[1];
        rays_ref[i].org.z = org_dir[2];

        rays_ref[i].dir.x = org_dir[3];
        rays_ref[i].dir.y = org_dir[4];
        rays_ref[i].dir.z = org_dir[5];

        rays_ref[i].tmin = tmin;
        rays_ref[i].tmax = tmax;
    }

    return true;
}
