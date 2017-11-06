#include <iostream>
#include <fstream>
#include "../frontend/options.h"
#include "linear.h"
#include "camera.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "No arguments. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    int width, height;
    float fov;
    std::string eye_str, center_str, up_str;
    ArgParser parser(argc, argv);
    parser.add_option<std::string>("eye", "e", "Sets the eye position", eye_str, "", "x,y,z");
    parser.add_option<std::string>("center", "c", "Sets the center position", center_str, "", "x,y,z");
    parser.add_option<std::string>("up", "u", "Sets the up vector", up_str, "", "x,y,z");
    parser.add_option<float>("fov", "f", "Sets the field of view", fov, 60.0f, "degrees");
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

    float3 eye, center, up;
    if (!read_float3(eye_str, eye) ||
        !read_float3(center_str, center) ||
        !read_float3(up_str, up)) {
        std::cerr << "Invalid position or vector." << std::endl;
        return EXIT_FAILURE;
    }

    std::ofstream ray_file(parser.arguments()[0], std::ofstream::binary);
    if (!ray_file) {
        std::cerr << "Cannot open output file." << std::endl;
        return EXIT_FAILURE;
    }

    Camera cam = gen_camera(eye, center, up, fov, (float)width / (float)height);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const float kx = 2 * x / (float)width - 1;
            const float ky = 1 - 2 * y / (float)height;
            const float3 dir = cam.dir + cam.right * kx + cam.up * ky;
            ray_file.write((const char*)&eye, sizeof(float) * 3);
            ray_file.write((const char*)&dir, sizeof(float) * 3);
        }
    }

    return EXIT_SUCCESS;
}
