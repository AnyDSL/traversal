#include <fstream>
#include <string>
#include "thorin_runtime.h"
#include "interface.h"

bool load_rays(const std::string& filename, Ray*& rays, int& count, float tmin, float tmax) {
    std::ifstream in(filename, std::ifstream::binary);
    if (!in) return false;

    in.seekg(0, std::ifstream::end);
    count = in.tellg() / (sizeof(float) * 6);

    Ray* buffer = thorin_new<Ray>(count);
    in.seekg(0);

    for (int i = 0; i < count; i++) {
        in.read((char*)buffer[i].org, sizeof(float) * 3);
        in.read((char*)buffer[i].dir, sizeof(float) * 3);
        buffer[i].tri = -1;
        buffer[i].tmin = tmin;
        buffer[i].tmax = tmax;
        buffer[i].u = 0.0f;
        buffer[i].v = 0.0f;
    }

    rays = buffer;

    return true;
}
