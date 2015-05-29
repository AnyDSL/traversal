#ifndef LOADERS_H
#define LOADERS_H

#include <string>
#include "interface.h"

bool load_bvh(const std::string& filename, Accel*& accel);
bool load_rays(const std::string& filename, Rays*& rays, int& count, float tmin, float tmax);

#endif
