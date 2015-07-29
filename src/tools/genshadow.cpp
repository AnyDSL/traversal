#include <iostream>
#include <fstream>
#include "../frontend/options.h"
#include "../frontend/bvh_format.h"
#include "linear.h"

size_t file_size(std::istream& is) {
    std::streampos pos = is.tellg();
    is.seekg(0, std::istream::end);
    size_t size = is.tellg();
    is.seekg(pos);
    return size;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "No arguments. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    std::string depth, primary, light_str;

    ArgParser parser(argc, argv);
    parser.add_option<std::string>("depth", "d", "Sets the depth buffer file", depth, "", "depth.fbuf");
    parser.add_option<std::string>("primary", "p", "Sets the primary ray distribution file", primary, "", "primary.rays");
    parser.add_option<std::string>("light", "l", "Sets the light position", light_str, "", "x,y,z");

    if (!parser.parse()) {
        parser.usage();
        return EXIT_SUCCESS;
    }

    if (parser.arguments().size() > 1) {
        std::clog << "Additional arguments ignored (";
        for (int i = 1; i < parser.arguments().size(); i++) {
            std::clog << "\'" << parser.arguments()[i] << "'";
            if (i != parser.arguments().size() - 1) std::clog << ", ";
        }
        std::clog << ")." << std::endl;
    }

    float3 light;
    if (!read_float3(light_str, light)) {
        std::cerr << "Invalid light position." << std::endl;
        return EXIT_FAILURE;
    }

    // Find the bounds from the BVH file
    std::ifstream depth_file(depth, std::ifstream::binary);
    if (!depth_file) {
        std::cerr << "Cannot open depth buffer file." << std::endl;
        return EXIT_FAILURE;
    }

    std::ifstream primary_file(primary, std::ifstream::binary);
    if (!primary_file) {
        std::cerr << "Cannot open primary ray distribution file." << std::endl;
        return EXIT_FAILURE;
    }

    std::ofstream ray_file(parser.arguments()[0], std::ofstream::binary);
    if (!ray_file) {
        std::cerr << "Cannot open output file." << std::endl;
        return EXIT_FAILURE;
    }

    size_t elem_count = file_size(depth_file) / sizeof(float);
    size_t ray_count = file_size(primary_file) / (sizeof(float) * 6);
    if (elem_count != ray_count) {
        std::cerr << "Number of rays does not match the depth file size (got "
                  << elem_count << " and " << ray_count << ")." << std::endl;
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < elem_count; i++) {
        float3 org, dir;
        float t;

        depth_file.read((char*)&t, sizeof(float));
        primary_file.read((char*)&org, sizeof(float) * 3);
        primary_file.read((char*)&dir, sizeof(float) * 3);

        const float3 new_org = org + t * dir;
        const float3 new_dir = light - new_org;
        ray_file.write((const char*)&new_org, sizeof(float) * 3);
        ray_file.write((const char*)&new_dir, sizeof(float) * 3);
    }

    return EXIT_SUCCESS;
}
