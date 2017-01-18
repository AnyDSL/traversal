#ifndef LOADERS_H
#define LOADERS_H

#include <string>
#include "traversal.h"

bool load_accel(const std::string& filename, anydsl::Array<Node>& nodes_ref, anydsl::Array<Vec4>& tris_ref);
bool load_rays(const std::string& filename, anydsl::Array<Ray>& rays_ref, float tmin, float tmax);
bool load_mesh(const std::string& filename, std::vector<int>& indices, std::vector<float>& vertices);

#endif
