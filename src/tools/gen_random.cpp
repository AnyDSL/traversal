#include <iostream>
#include <fstream>
#include <random>
#include "../frontend/options.h"
#include "../frontend/bvh_format.h"
#include "linear.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "No arguments. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    int seed, width, height;
    std::string bvh;
    std::random_device rd;

    ArgParser parser(argc, argv);
    parser.add_option<std::string>("bvh", "b", "Sets the bvh file to find the bounds from", bvh, "", "file.bvh");
    parser.add_option<int>("seed", "s", "Sets the random generator seed", seed, rd(), "number");
    parser.add_option<int>("width", "w", "Sets the viewport width", width, 1024, "pixels");
    parser.add_option<int>("height", "h", "Sets the viewport height", height, 1024, "pixels");

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

    // Find the bounds from the BVH file
    std::ifstream bvh_file(bvh, std::ifstream::binary);
    if (!bvh_file || !check_header(bvh_file) || !locate_block(bvh_file, BlockType::MBVH)) {
        std::cerr << "Invalid BVH file." << std::endl;
        return EXIT_FAILURE;
    }

    mbvh::Header hdr;
    bvh_file.read((char*)&hdr, sizeof(mbvh::Header));
    float3 min(hdr.scene_bb.lx, hdr.scene_bb.ly, hdr.scene_bb.lz);
    float3 max(hdr.scene_bb.ux, hdr.scene_bb.uy, hdr.scene_bb.uz);
    float3 ext = max - min;

    std::mt19937_64 gen(seed);
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    std::ofstream ray_file(parser.arguments()[0], std::ofstream::binary);
    if (!ray_file) {
        std::cerr << "Cannot open output file." << std::endl;
        return EXIT_FAILURE;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const float3 rnd1 = min + ext * float3(dis(gen), dis(gen), dis(gen));
            const float3 rnd2 = min + ext * float3(dis(gen), dis(gen), dis(gen));
            const float3 dir = rnd2 - rnd1;
            ray_file.write((const char*)&rnd1, sizeof(float) * 3);
            ray_file.write((const char*)&dir, sizeof(float) * 3);
        }
    }

    return EXIT_SUCCESS;
}
