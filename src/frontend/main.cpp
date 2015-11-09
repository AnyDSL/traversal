#include <iostream>
#include <fstream>
#include <chrono>
#include <functional>
#include <numeric>
#include <thorin_runtime.hpp>

#include "options.h"
#include "traversal.h"
#include "loaders.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "No arguments. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    std::string accel_file, rays_file;
    std::string output;
    float tmin, tmax;
    int times, warmup;
    bool help, any;

    ArgParser parser(argc, argv);
    parser.add_option<bool>("help", "h", "Shows this message", help, false);
    parser.add_option<std::string>("accel", "a", "Sets the acceleration structure file name", accel_file, "input.bvh", "input.bvh");
    parser.add_option<std::string>("rays", "r", "Sets the input ray distribution file name", rays_file, "input.rays", "input.rays");
    parser.add_option<int>("times", "n", "Sets the iteration count", times, 100, "count");
    parser.add_option<int>("warmup", "d", "Sets the number of dry runs", warmup, 10, "count");
    parser.add_option<std::string>("output", "o", "Sets the output file name", output, "output.fbuf", "output.fbuf");
    parser.add_option<float>("tmin", "tmin", "Sets the minimum t parameter along the rays", tmin, 0.0f, "t");
    parser.add_option<float>("tmax", "tmax", "Sets the maximum t parameter along the rays", tmax, 1e9f, "t");
    parser.add_option<bool>("any", "any", "Stops at the first intersection", any, false);

    if (!parser.parse()) {
        return EXIT_FAILURE;
    }

    if (help) {
        parser.usage();
        return EXIT_SUCCESS;
    }

    auto traversal = any ? occluded : intersect;

    thorin::Array<Node> nodes;
    thorin::Array<Vec4> tris;
    if (!load_accel(accel_file, nodes, tris)) {
        std::cerr << "Cannot load acceleration structure file." << std::endl;
        return EXIT_FAILURE;
    }

    thorin::Array<Ray> rays;
    if (!load_rays(rays_file, rays, tmin, tmax)) {
        std::cerr << "Cannot load ray distribution file." << std::endl;
        return EXIT_FAILURE;
    }

    int ray_count = rays.size();

    std::cout << ray_count << " ray(s) in the distribution file." << std::endl;

    thorin::Array<Hit> hits(thorin::Platform::TRAVERSAL_PLATFORM, thorin::Device(TRAVERSAL_DEVICE), ray_count);

    // Warmup iterations
    for (int i = 0; i < warmup; i++) {
        traversal(nodes.data(), tris.data(), rays.data(), hits.data(), ray_count);
    }

    // Compute traversal time
    std::vector<long long> iter_times(times);
    for (int i = 0; i < times; i++) {
        long long t0 = get_time();
        traversal(nodes.data(), tris.data(), rays.data(), hits.data(), ray_count);
        long long t1 = get_time();
        iter_times[i] = t1 - t0;
    }

    // Read the result from the device
    thorin::Array<Hit> host_hits(ray_count);
    thorin::copy(hits, host_hits);

    std::sort(iter_times.begin(), iter_times.end());

    long long median = iter_times[times / 2];
    long long sum = std::accumulate(iter_times.begin(), iter_times.end(), 0);
    std::cout << sum / 1000.0 << "ms for " << times << " iteration(s)." << std::endl;
    std::cout << ray_count * times * 1000000.0 / sum << " rays/sec." << std::endl;
    std::cout << "# Average: " << sum / 1000.0 / times << " ms" << std::endl;
    std::cout << "# Median: " << median / 1000.0 << " ms" << std::endl;
    std::cout << "# Min: " << iter_times[0] / 1000.0 << " ms" << std::endl;
    int intr = 0;
    for (int i = 0; i < ray_count; i++) {
        if (host_hits[i].tri_id >= 0) {
            intr++;
        }
    }
    std::cout << intr << " intersection(s)." << std::endl;

    std::ofstream out(output, std::ofstream::binary);
    for (int i = 0; i < ray_count; i++) {
        out.write((char*)&host_hits[i].tmax, sizeof(float));
    }

    return EXIT_SUCCESS;
}
