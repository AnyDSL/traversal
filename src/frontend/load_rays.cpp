#include <fstream>
#include <string>
#include "thorin_runtime.h"
#include "interface.h"

bool load_rays(const std::string& filename, Rays*& rays, int& count, float tmin, float tmax) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in) return false;

    in.seekg(0, std::ifstream::end);
    count = in.tellg() / (sizeof(float) * 6);

    rays = thorin_new<Rays>(1);
    rays->org_x = thorin_new<float>(count);
    rays->org_y = thorin_new<float>(count);
    rays->org_z = thorin_new<float>(count);

    rays->dir_x = thorin_new<float>(count);
    rays->dir_y = thorin_new<float>(count);
    rays->dir_z = thorin_new<float>(count);

    rays->tri = thorin_new<int>(count);
    rays->tmax = thorin_new<float>(count);
    rays->tmin = thorin_new<float>(count);
    rays->u = thorin_new<float>(count);
    rays->v = thorin_new<float>(count);

    in.seekg(0);

    for (int i = 0; i < count; i++) {
        float org_dir[6];
        in.read((char*)org_dir, sizeof(float) * 6);
        rays->org_x[i] = org_dir[0];
        rays->org_y[i] = org_dir[1];
        rays->org_z[i] = org_dir[2];

        rays->dir_x[i] = org_dir[3];
        rays->dir_y[i] = org_dir[4];
        rays->dir_z[i] = org_dir[5];

        rays->tri[i] = -1;
        rays->tmin[i] = tmin;
        rays->tmax[i] = tmax;
        rays->u[i] = 0.0f;
        rays->v[i] = 0.0f;
    }

    return true;
}
