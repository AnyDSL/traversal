#ifndef LOADERS_H
#define LOADERS_H

#include <string>
#include "interface.h"

bool load_bvh(const std::string& filename, Node*& nodes_ref, Vec4*& tris_ref);
bool load_rays(const std::string& filename, Ray*& rays_ref, int& count, float tmin, float tmax);

#endif
