#include <fstream>
#include <string>
#include <anydsl_runtime.hpp>
#include "traversal.h"

bool load_rays(const std::string& filename, anydsl::Array<Ray>& rays_ref, float tmin, float tmax) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in) return false;

    in.seekg(0, std::ifstream::end);
    int count = in.tellg() / (sizeof(float) * 6);

    anydsl::Array<Ray> host_rays(count);
    in.seekg(0);

    for (int i = 0; i < count; i++) {
        float org_dir[6];
        in.read((char*)org_dir, sizeof(float) * 6);
        Ray& ray = host_rays.data()[i];

        ray.org.x = org_dir[0];
        ray.org.y = org_dir[1];
        ray.org.z = org_dir[2];

        ray.dir.x = org_dir[3];
        ray.dir.y = org_dir[4];
        ray.dir.z = org_dir[5];

        ray.org.w = tmin;
        ray.dir.w = tmax;
    }

    rays_ref = std::move(anydsl::Array<Ray>(anydsl::Platform::TRAVERSAL_PLATFORM, anydsl::Device(TRAVERSAL_DEVICE), count));
    anydsl::copy(host_rays, rays_ref);

    return true;
}
