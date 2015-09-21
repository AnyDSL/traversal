#include <iostream>
#include <fstream>
#include <chrono>
#include <functional>
#include <random>
#include <cassert>

#include <SDL/SDL.h>
#include <omp.h>
#include <thorin_runtime.h>

#include "../frontend/options.h"
#include "../frontend/traversal.h"
#include "../frontend/loaders.h"
#include "linear.h"
#include "camera.h"

struct Config {
    int width;
    int height;
    int samples;
    float clip;
    float fov;
    float ao_offset;
    float ao_tmax;
};

struct View {
    float3 eye;
    float3 forward;
    float3 right;
    float3 up;
    float dist;
    float rspeed;
    float tspeed;
};

struct RayBuffer {
    RayBuffer(int count = 0)
        : size(0), rays(nullptr), hits(nullptr) {
        allocate(count);
    }

    ~RayBuffer() {
        if (rays) thorin_free(rays);
        if (hits) thorin_free(hits);
    }

    void allocate(int count) {
        if (size != count) {
            rays = thorin_new<Ray>(count);
            hits = thorin_new<Hit>(count);
            size = count;
        }
    }

    void traverse(Node* nodes, Vec4* tris) {
#ifdef CPU
#define CHUNK 256
        #pragma omp parallel for
        for (int i = 0; i < size; i += CHUNK)
        {
            const int count = (i + CHUNK <= size) ? CHUNK : size - i;
            traverse_accel(nodes, rays + i, tris, hits + i, count);
        }
#undef CHUNK
#else
        traverse_accel(nodes, rays, tris, hits, size);
#endif
    }

    Ray* rays;
    Hit* hits;
    int size;
};

inline Vec4 make_vec4(float x, float y, float z, float w = 1.f) {
    Vec4 v { x, y, z, w };
    return v;
}

inline Vec4 make_vec4(const float3& f, float w = 1.f) {
    Vec4 v { f.x, f.y, f.z, w };
    return v;
}

inline float3 from_vec4(const Vec4& v) {
    return float3(v.x, v.y, v.z);
}

float3 cosine_weighted_sample_hemisphere(float u1, float u2)
{
    const float r = sqrt(u1);
    const float theta = 2 * M_PI * u2;
 
    const float x = r * cos(theta);
    const float y = r * sin(theta);
 
    return float3(x, y, sqrt(std::max(0.0f, 1 - u1)));
}

void render_image(const Camera& cam, const Config& cfg, float* img, Node* nodes, Vec4* tris, RayBuffer& primary, RayBuffer& ao_buffer) {
    const int img_size = cfg.width * cfg.height;

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<float> dist(0.f, 1.f);

    // Generate primary rays
    for (int y = 0; y < cfg.height; y++) {
        for (int x = 0; x < cfg.width; x++) {
            const float kx = 2 * x / (float)cfg.width - 1;
            const float ky = 1 - 2 * y / (float)cfg.height;
            const float3 dir = cam.dir + cam.right * kx + cam.up * ky;
            primary.rays[y * cfg.width + x].org = make_vec4(cam.eye, 0.0f);
            primary.rays[y * cfg.width + x].dir = make_vec4(dir, cfg.clip);
        }
    }

    // Intersect them
    primary.traverse(nodes, tris);

    // Proceed by groups of block_size pixels
    int block_size = std::min(img_size, ao_buffer.size / cfg.samples);

    for (int i = 0; i < img_size; i += block_size) {
        const int block_limit = (i + block_size > img_size) ? img_size - i : block_size;

        // Generate ambient occlusion rays
        int ao_rays = 0;
        for (int j = 0; j < block_limit; j++) {
            const int ray_id = i + j;
            const int tri_id = primary.hits[ray_id].tri_id;
            if (tri_id >= 0) {
                const float t = primary.hits[ray_id].tmax;
                const float3 org = from_vec4(primary.rays[ray_id].dir) * t +
                                   from_vec4(primary.rays[ray_id].org);

                // Compute the face normal, tangent, bitangent
#ifdef CPU
                const int tri = tri_id & 0x03;
                const float* tri4 = (float*)&tris[tri_id >> 2];
                const float3 v1(tri4[tri +  0], tri4[tri +  4], tri4[tri +  8]);
                const float3 e1(tri4[tri + 12], tri4[tri + 16], tri4[tri + 20]);
                float3 normal = normalize(float3(tri4[tri + 36], tri4[tri + 40], tri4[tri + 44]));
#else
                const float3 v1 = from_vec4(tris[tri_id + 0]);
                const float3 v2 = from_vec4(tris[tri_id + 1]);
                const float3 v3 = from_vec4(tris[tri_id + 2]);
                const float3 e1 = v2 - v1;
                const float3 e2 = v3 - v1;
                float3 normal = normalize(cross(e1, e2));
#endif
                // Flip it if if doesn't face the viewer
                const float d = dot(cam.eye, normal) - dot(normal, v1);
                if (d < 0) normal = normal * (-1.0f);

                const float3 tangent = normalize(e1);
                const float3 bitangent = cross(normal, tangent);

                for (int k = 0; k < cfg.samples; k++) {
                    const float u1 = dist(mt);
                    const float u2 = dist(mt);

                    const float3 s = cosine_weighted_sample_hemisphere(u1, u2);
                    const float3 dir = normalize(s.x * tangent + s.y * bitangent + s.z * normal);

                    ao_buffer.rays[j * cfg.samples + k].org = make_vec4(org, cfg.ao_offset);
                    ao_buffer.rays[j * cfg.samples + k].dir = make_vec4(dir, cfg.ao_tmax);
                }
                ao_rays++;
            } else {
                // Do not generate garbage for rays that go out of the scene
                memset(&ao_buffer.rays[j * cfg.samples], 0, sizeof(Ray) * cfg.samples);
            }
        }

        if (ao_rays > 0) {
            // Get the intersection results
            ao_buffer.traverse(nodes, tris);

            // Write the contribution to the image
            for (int j = 0; j < block_limit; j++) {
                float color = 0.0f;

                if(primary.hits[i + j].tri_id >= 0) {
                    int count = 0;
                    for(int k = j * cfg.samples; k < (j + 1) * cfg.samples; k++) {
                        if (ao_buffer.hits[k].tri_id >= 0)
                            count++;
                    }
                    color = 1.f - (count / (float)cfg.samples);
                }

                img[i + j] += color;
            }
        }
    }
}

bool handle_events(View& view, int& accum) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return true;
            case SDL_MOUSEMOTION:
                {
                    view.right = cross(view.forward, view.up);
                    view.forward = rotate(view.forward, view.right, -event.motion.yrel * view.rspeed);
                    view.forward = rotate(view.forward, view.up,    -event.motion.xrel * view.rspeed);
                    view.forward = normalize(view.forward);
                    view.up = normalize(cross(view.right, view.forward));
                    accum = 0;
                }
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_UP:    view.eye = view.eye + view.tspeed * view.forward; accum = 0; break;
                    case SDLK_DOWN:  view.eye = view.eye - view.tspeed * view.forward; accum = 0; break;
                    case SDLK_LEFT:  view.eye = view.eye - view.tspeed * view.right;   accum = 0; break;
                    case SDLK_RIGHT: view.eye = view.eye + view.tspeed * view.right;   accum = 0; break;
                    case SDLK_KP_PLUS:  view.tspeed *= 1.1f; break;
                    case SDLK_KP_MINUS: view.tspeed /= 1.1f; break;
                    case SDLK_c:
                        {
                            const float3 center = view.eye + view.forward * view.dist;
                            std::cout << "Eye: " << view.eye.x << " " << view.eye.y << " " << view.eye.z << std::endl;
                            std::cout << "Center: " << center.x << " " << center.y << " " << center.z << std::endl;
                            std::cout << "Up: " << view.up.x << " " << view.up.y << " " << view.up.z << std::endl;
                        }
                        break;
                    case SDLK_ESCAPE:
                        return true;
                }
                break;
        }
    }

    return false;
}

void flush_events() {
    SDL_PumpEvents();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {}
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "No arguments. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    std::string accel_file;
    std::string eye_str, center_str, up_str;
    std::string output;
    Config cfg;

    ArgParser parser(argc, argv);
    parser.add_option<std::string>("accel", "a", "Sets the acceleration structure file name", accel_file, "input.bvh", "input.bvh");
    parser.add_option<float>("clip-dist", "clip", "Sets the view clipping distance", cfg.clip, 1e9f, "t");
    parser.add_option<std::string>("output", "o", "Write an fbuf file containing the rendered image and exit", output, "", "path");
    parser.add_option<int>("ao-samples", "s", "Number of ambient occlusion samples", cfg.samples, 4, " samples");
    parser.add_option<float>("ao-offset", "off", "Sets the offset for ambient occlusion rays", cfg.ao_offset, 0.001f, "t");
    parser.add_option<float>("ao-dist", "d", "Sets the maximum distance for ambient occlusion rays", cfg.ao_tmax, 10.f, "t");
    parser.add_option<int>("width", "w", "Sets the viewport width", cfg.width, 512, "pixels");
    parser.add_option<int>("height", "h", "Sets the viewport height", cfg.height, 512, "pixels");
    parser.add_option<std::string>("eye", "e", "Sets the eye position", eye_str, "0,0,-10", "x,y,z");
    parser.add_option<std::string>("center", "c", "Sets the center position", center_str, "0,0,0", "x,y,z");
    parser.add_option<std::string>("up", "u", "Sets the up vector", up_str, "0,1,0", "x,y,z");
    parser.add_option<float>("fov", "f", "Sets the field of view", cfg.fov, 60.0f, "degrees");

    if (!parser.parse()) {
        return EXIT_FAILURE;
    }

    float3 eye, center, up;
    if (!read_float3(eye_str, eye) ||
        !read_float3(center_str, center) ||
        !read_float3(up_str, up)) {
        std::cerr << "Invalid position or vector." << std::endl;
        return EXIT_FAILURE;
    }

    thorin_init();

    Node* nodes;
    Vec4* tris;
    if (!load_accel(accel_file, nodes, tris)) {
        std::cerr << "Cannot load acceleration structure file." << std::endl;
        return EXIT_FAILURE;
    }

    Camera cam = gen_camera(eye, center, up, cfg.fov, (float)cfg.width / (float)cfg.height);

    std::vector<float> image(cfg.width * cfg.height);
    RayBuffer primary(image.size());
    RayBuffer ao_buffer(image.size() * 4);

    if (output.length()) {
        // Render to a file and exit
        render_image(cam, cfg, image.data(), nodes, tris, primary, ao_buffer);
        std::ofstream out(output, std::ofstream::binary);
        out.write((const char*)image.data(), image.size() * sizeof(float));
        return EXIT_SUCCESS;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Cannot initialize SDL." << std::endl;
        return EXIT_FAILURE;
    }

    SDL_WM_SetCaption("Viewer", nullptr);
    SDL_WM_GrabInput(SDL_GRAB_ON);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

    SDL_Surface* screen = SDL_SetVideoMode(cfg.width, cfg.height, 32, SDL_DOUBLEBUF);
    View view = {
        cam.eye,                 // Eye
        normalize(cam.dir),      // Forward
        normalize(cam.right),    // Right
        normalize(cam.up),       // Up
        100.0f, 0.005f, 1.0f     // View distance, rotation speed, translation speed
    };

    flush_events();

    bool done = false;
    int accum = 0;
    while (!done) {
        if (accum == 0) {
            // Reset image when the camera moves
            memset(image.data(), 0, sizeof(float) * image.size());
        }

        render_image(cam, cfg, image.data(), nodes, tris, primary, ao_buffer);
        accum++;

        // Copy image to screen
        SDL_LockSurface(screen);
        for (int y = 0; y < cfg.height; y++) {
            char* row = (char*)screen->pixels + screen->pitch * y;
            for (int x = 0; x < cfg.width; x++) {
                char color = image[y * cfg.width + x] * 255.0f / accum;
                row[x * 4 + 0] = color;
                row[x * 4 + 1] = color;
                row[x * 4 + 2] = color;
                row[x * 4 + 3] = 255;
            }
        }
        SDL_UnlockSurface(screen);

        SDL_Flip(screen);

        done = handle_events(view, accum);
        cam = gen_camera(view.eye, view.eye + view.forward * view.dist, view.up, cfg.fov, (float)cfg.width / (float)cfg.height);
    }

    SDL_WM_GrabInput(SDL_GRAB_OFF);
    SDL_Quit();

    return EXIT_SUCCESS;
}
