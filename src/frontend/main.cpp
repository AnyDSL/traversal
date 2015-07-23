#include <iostream>
#include <fstream>
#include <chrono>
#include <functional>
#include "options.h"
#include "interface.h"
#include "loaders.h"
#include "thorin_runtime.h"
#include "thorin_utils.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "No arguments. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    std::string accel_file, rays_file;
    std::string output;
    float tmin, tmax;
    int times, warmup;
    bool help;

    ArgParser parser(argc, argv);
    parser.add_option<bool>("help", "h", "Shows this message", help, false);
    parser.add_option<std::string>("accel", "a", "Sets the acceleration structure file name", accel_file, "input.bvh", "input.bvh");
    parser.add_option<std::string>("rays", "r", "Sets the input ray distribution file name", rays_file, "input.rays", "input.rays");
    parser.add_option<int>("times", "n", "Sets the iteration count", times, 100, "count");
    parser.add_option<int>("warmup", "d", "Sets the number of dry runs", warmup, 10, "count");
    parser.add_option<std::string>("output", "o", "Sets the output file name", output, "output.fbuf", "output.fbuf");
    parser.add_option<float>("tmin", "tmin", "Sets the minimum t parameter along the rays", tmin, 0.0f, "t");
    parser.add_option<float>("tmax", "tmax", "Sets the maximum t parameter along the rays", tmax, 1e9f, "t");

    if (!parser.parse()) {
        return EXIT_FAILURE;
    }

    if (help) {
        parser.usage();
        return EXIT_SUCCESS;
    }

    thorin_init();

    Node* nodes;
    Vec4* tris;
    if (!load_accel(accel_file, nodes, tris)) {
        std::cerr << "Cannot load acceleration structure file." << std::endl;
        return EXIT_FAILURE;
    }

    Ray* rays;
    int ray_count;
    if (!load_rays(rays_file, rays, ray_count, tmin, tmax)) {
        std::cerr << "Cannot load ray distribution file." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << ray_count << " ray(s) in the distribution file." << std::endl;

    Hit* hits = thorin_new<Hit>(ray_count);
    auto reset_hits = [&] () {
        for (int i = 0; i < ray_count; i++) {
            hits[i].tri_id = -1;
            hits[i].tmax = rays[i].dir.w;        
        }
    };

    // Warmup iterations
    for (int i = 0; i < warmup; i++) {
        reset_hits();
        traverse_accel(nodes, rays, tris, hits, ray_count);
    }

    // Compute traversal time
    std::vector<long long> iter_times(times);
    for (int i = 0; i < times; i++) {
        reset_hits();
        long long t0 = get_time();
        traverse_accel(nodes, rays, tris, hits, ray_count);
        long long t1 = get_time();
        iter_times[i] = t1 - t0;
    }

    std::sort(iter_times.begin(), iter_times.end());

    long long median = iter_times[times / 2];
    long long sum = std::accumulate(iter_times.begin(), iter_times.end(), 0);
    std::cout << sum / 1000.0 << "ms for " << times << " iteration(s)." << std::endl;
    std::cout << ray_count * times * 1000000.0 / sum << " rays/sec." << std::endl;
    std::cout << "Average: " << sum /1000.0 / times << " ms" << std::endl;
    std::cout << "Median: " << median / 1000.0 << " ms" << std::endl;
    std::cout << "Min: " << iter_times[0] / 1000.0 << " ms" << std::endl;

    
    int intr = 0;
    for (int i = 0; i < ray_count; i++) {
        if (hits[i].tri_id >= 0) {
            intr++;
        }
    }
    std::cout << intr << " intersection(s)." << std::endl;

    std::ofstream out(output, std::ofstream::binary);
    for (int i = 0; i < ray_count; i++)
        out.write((char*)&hits[i].tmax, sizeof(float));

    return EXIT_SUCCESS;
}
