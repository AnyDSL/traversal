#include <iostream>
#include <fstream>
#include <chrono>
#include <functional>
#include "options.h"
#include "traversal.h"
#include "loaders.h"
#include "thorin_runtime.h"
#include "thorin_utils.h"

#include <random>

long node_count = 0;
extern "C" {
    void inc(void) { node_count++; }
    void put(const char* str) { printf("%s", str); fflush(stdout); }
    void put_int(int i) { printf("%d", i); fflush(stdout); }
    void put_float(float f) { printf("%f", f); fflush(stdout); }
}



inline Vec4 make_vec4(const float x, const float y, const float z, const float w=1.f) {
    Vec4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

inline Vec4 make_vec4(const float a) {
    Vec4 v;
    v.x = a;
    v.y = a;
    v.z = a;
    v.w = a;
    return v;
}

inline Vec4 operator - (Vec4 a, Vec4 b) {
    return make_vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

inline Vec4 operator + (Vec4 a, Vec4 b) {
    return make_vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

inline Vec4 operator * (Vec4 a, Vec4 b) {
    return make_vec4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

inline float dot(Vec4 a, Vec4 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec4 normalize(Vec4 a) {
    float lensq = dot(a, a);
    return a * make_vec4(1.0f / sqrtf(lensq));
}

inline float length(Vec4 a) {
   float lensq = dot(a, a);
   return sqrt(lensq);
}


inline Vec4 cross(Vec4 a, Vec4 b) {
    return make_vec4(a.y * b.z - a.z * b.y,
                    a.z * b.x - a.x * b.z, 
                    a.x * b.y - a.y * b.x);
}


Vec4 cosine_weighted_sample_hemisphere(float u1, float u2)
{
    const float r = sqrt(u1);
    const float theta = 2 * M_PI * u2;
 
    const float x = r * cos(theta);
    const float y = r * sin(theta);
 
    return make_vec4(x, y, sqrt(std::max(0.0f, 1 - u1)));
}


Vec4 sample_sphere(float u1, float u2) {
    const float theta = 2 * M_PI * u1;
    const float phi = acos(2 * u2 -1);

    const float x = sin(theta) * cos(phi);
    const float y = sin(theta) * sin(phi);
    const float z = cos(theta);

    return make_vec4(x,y,z); 
}

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

    std::string ao_path("");
    int ao_samples;
    float ao_tmin, ao_tmax;

    ArgParser parser(argc, argv);
    parser.add_option<bool>("help", "h", "Shows this message", help, false);
    parser.add_option<std::string>("accel", "a", "Sets the acceleration structure file name", accel_file, "input.bvh", "input.bvh");
    parser.add_option<std::string>("rays", "r", "Sets the input ray distribution file name", rays_file, "input.rays", "input.rays");
    parser.add_option<int>("times", "n", "Sets the iteration count", times, 100, "count");
    parser.add_option<int>("warmup", "d", "Sets the number of dry runs", warmup, 10, "count");
    parser.add_option<std::string>("output", "o", "Sets the output file name", output, "output.fbuf", "output.fbuf");
    parser.add_option<float>("tmin", "tmin", "Sets the minimum t parameter along the rays", tmin, 0.0f, "t");
    parser.add_option<float>("tmax", "tmax", "Sets the maximum t parameter along the rays", tmax, 1e9f, "t");
    parser.add_option<std::string>("ao", "ao", "Write ao file", ao_path, "", "path");
    parser.add_option<int>("ao-samples", "aos", "Nbr of ao samples", ao_samples, 16, " samples");
    parser.add_option<float>("ao-tmin", "aotmin", "Sets the minimum distance for ao rays", ao_tmin, 0.001f, "length");
    parser.add_option<float>("ao-tmax", "aotmax", "Sets the maximum distance for ao rays", ao_tmax, 10.f, "length");

    if (!parser.parse()) {
        return EXIT_FAILURE;
    }

    if (help) {
        parser.usage();
        return EXIT_SUCCESS;
    }

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<float> dist(0.f, 1.f);


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
    std::cout << "# Average: " << sum / 1000.0 / times << " ms" << std::endl;
    std::cout << "# Median: " << median / 1000.0 << " ms" << std::endl;
    std::cout << "# Min: " << iter_times[0] / 1000.0 << " ms" << std::endl;

    int intr = 0;
    for (int i = 0; i < ray_count; i++) {
        if (hits[i].tri_id >= 0) {
            intr++;
        }
    }
    std::cout << intr << " intersection(s)." << std::endl;

    std::ofstream out(output, std::ofstream::binary);
    for (int i = 0; i < ray_count; i++) {
        out.write((char*)&hits[i].tmax, sizeof(float));
    }

    std::cout << "NODE COUNT: " << node_count << std::endl;


    //Ambient Occlusion
    if(ao_path.compare("") != 0) {

        FILE *ao_f = fopen(ao_path.c_str(), "wb");

        if (ao_f==NULL) {
            std::cerr << "Cannot open: " << ao_path;
            exit(1);
        }

        //For convinience we create as many blocks as we have want to take ao_samples
        //Hence, each block contains only ray_count rays;
        int nbr_of_blocks = ao_samples;       
        uint ao_samples_per_block = ray_count;

        Ray *ao_rays =  thorin_new<Ray>(ao_samples_per_block);
        int block_size = ray_count / nbr_of_blocks;
        Hit* ao_hits = thorin_new<Hit>(ao_samples_per_block);

        for(int block = 0; block < nbr_of_blocks; block++) {


            for (int i = block * block_size; i < (block+1) * block_size; i++) {
                int triid = hits[i].tri_id;
               
                if(triid!=-1) {
                    float t = hits[i].tmax;
                    Vec4 o = make_vec4(t) * rays[i].dir + rays[i].org;
                    o.w = ao_tmin;

                    //Face normal
                    Vec4 v1 = tris[triid + 0];
                    Vec4 v2 = tris[triid + 1];
                    Vec4 v3 = tris[triid + 2];
                
                    Vec4 e1 = v2-v1;
                    Vec4 e2 = v3-v1;
                
                    Vec4 normal = normalize(cross(e1, e2));
                
                    //Flip normal?
                    float d = -dot(normal, v1);
                    float absDist = (dot(rays[i].org, normal) + d);

                    if(absDist<0) {
                        normal = normal * make_vec4(-1);
                    }
                    


                    for(int j=0; j < ao_samples; j++) {           
                        
                        //Sample Hemisphere
                        float u1 = dist(mt);
                        float u2 = dist(mt);

                        Vec4 nh = cosine_weighted_sample_hemisphere(u1, u2);
                        Vec4 tangent = normalize(e1);
                        Vec4 bitangent = normalize(cross(normal, tangent));

                        Vec4 d;
                        d.x = nh.x * tangent.x + nh.y * bitangent.x + nh.z * normal.x;
                        d.y = nh.x * tangent.y + nh.y * bitangent.y + nh.z * normal.y;
                        d.z = nh.x * tangent.z + nh.y * bitangent.z + nh.z * normal.z;
        
                        d = normalize(d);
                        d.w = ao_tmax;

                        //Spherical Sampling
                        /*                                        
                        float u1 = dist(mt);
                        float u2 = dist(mt);

                        Vec4 d = sample_sphere(u1, u2);
                        d.w = ao_tmax;
                        */                  

                        Ray ao_ray;
                        ao_ray.org = o;
                        ao_ray.dir = d;
                        ao_rays[(i - block * block_size ) * ao_samples + j] = ao_ray;
                    }
                } else {
                    memset(&(ao_rays[(i - block * block_size ) * ao_samples]), 0, sizeof(Ray) * ao_samples);
                }
            }
        
            //thorin_free(hits);
            //thorin_free(rays);

            //Reset hits
            /*
            for (int i = 0; i < ao_samples_per_block; i++) {
                    ao_hits[i].tri_id = -1;
                    ao_hits[i].tmax = ao_tmax; 
            }
            */

            //intersect
            traverse_accel(nodes, ao_rays, tris, ao_hits, ao_samples_per_block);


            //Write results
            for(int i = 0; i < block_size; i++) {
                int sum=0;
                if(hits[block * block_size + i].tri_id != -1) {
                    for(int j = 0; j < ao_samples; j++) {
                        Hit h = ao_hits[i * ao_samples + j];
                        if(h.tri_id!=-1)
                            sum++;
                    }

                    float ao_fac =  1.f - (sum/(float) ao_samples);
                    fwrite (&ao_fac , sizeof(float), 1, ao_f);
                } else {
                    float zero=0.f;
                    fwrite (&zero , sizeof(float), 1, ao_f);
                }
            }


        }
        thorin_free(ao_rays);

        fclose(ao_f);
    } //ao



    return EXIT_SUCCESS;
}
