#include <fstream>
#include <string>
#include <thorin_runtime.hpp>
#include "traversal.h"

bool load_rays(const std::string& filename, thorin::Array<Ray>& rays_ref, float tmin, float tmax) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in) return false;

    in.seekg(0, std::ifstream::end);
    int count = in.tellg() / (sizeof(float) * 6);

    thorin::Array<Ray> host_rays(count);
    in.seekg(0);

    for (int i = 0; i < count; i++) {
        float org_dir[6];
        in.read((char*)org_dir, sizeof(float) * 6);
        Ray& ray = host_rays.data()[i];

        ray.org[0] = org_dir[0];
        ray.org[1] = org_dir[1];
        ray.org[2] = org_dir[2];

        ray.dir[0] = org_dir[3];
        ray.dir[1] = org_dir[4];
        ray.dir[2] = org_dir[5];

        ray.org[3] = tmin;
        ray.dir[3] = tmax;
    }

    rays_ref = std::move(thorin::Array<Ray>(thorin::Platform::TRAVERSAL_PLATFORM, thorin::Device(TRAVERSAL_DEVICE), count));
    thorin::copy(host_rays, rays_ref);

    return true;
}
