#include <iostream>
#include <fstream>
#include <chrono>
#include <functional>
#include <random>
#include <cassert>

#include <SDL2/SDL.h>
#include <thorin_runtime.h>

#include "../frontend/options.h"
#include "../frontend/traversal.h"
#include "../frontend/loaders.h"
#include "linear.h"
#include "camera.h"

#define HOST   0
#define CUDA   1
#define OPENCL 2
#if TRAVERSAL_PLATFORM == 0
#define CPU
#endif
#undef HOST
#undef CUDA
#undef OPENCL

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

class RayBuffer {
public:
    RayBuffer(int count)
        : host_rays(count)
        , host_hits(count)
#ifndef CPU
        , dev_rays(thorin::Platform::TRAVERSAL_PLATFORM, thorin::Device(TRAVERSAL_DEVICE), count)
        , dev_hits(thorin::Platform::TRAVERSAL_PLATFORM, thorin::Device(TRAVERSAL_DEVICE), count)
#endif
    {}

    template <typename F>
    void traverse(F f, thorin::Array<Node>& nodes, thorin::Array<Vec4>& tris) {
        traverse(f, nodes, tris, size());
    }

    template <typename F>
    void traverse(F f, thorin::Array<Node>& nodes, thorin::Array<Vec4>& tris, int count) {
#ifdef CPU
        f(nodes.data(), tris.data(), rays(), hits(), size());
#else
        thorin::copy(host_rays, dev_rays);
        f(nodes.data(), tris.data(), dev_rays.data(), dev_hits.data(), count);
        thorin::copy(dev_hits, host_hits);
#endif
    }

    Ray* rays() { return host_rays.data(); }
    const Ray* rays() const { return host_rays.data(); }
    Hit* hits() { return host_hits.data(); }
    const Hit* hits() const { return host_hits.data(); }

    int size() const { return host_rays.size(); }

private:
    thorin::Array<Ray> host_rays;
    thorin::Array<Hit> host_hits;
#ifndef CPU
    thorin::Array<Ray> dev_rays;
    thorin::Array<Hit> dev_hits;
#endif
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

float3 sample_hemisphere(float u1, float u2)
{
    const float r = sqrt(1.0f - u1 * u1);
    const float phi = 2 * M_PI * u2;

    return float3(cos(phi) * r, sin(phi) * r, u1);
}

void gen_local_coords(const std::vector<int>& indices, const std::vector<float>& vertices, std::vector<float>& local_coords) {
    local_coords.resize(indices.size() * 3);
    for (int i = 0, j = 0; i < indices.size(); i += 3, j++) {
        const int i0 = indices[i + 0];
        const int i1 = indices[i + 1];
        const int i2 = indices[i + 2];
        const float3 v0(vertices[i0 * 4 + 0], vertices[i0 * 4 + 1], vertices[i0 * 4 + 2]);
        const float3 v1(vertices[i1 * 4 + 0], vertices[i1 * 4 + 1], vertices[i1 * 4 + 2]);
        const float3 v2(vertices[i2 * 4 + 0], vertices[i2 * 4 + 1], vertices[i2 * 4 + 2]);
        const float3 e1 = v1 - v0;
        const float3 e2 = v2 - v0;
        const float3 normal = normalize(cross(e1, e2));
        const float3 tangent = normalize(e1);

        local_coords[j * 9 + 0] = v0.x;
        local_coords[j * 9 + 1] = v0.y;
        local_coords[j * 9 + 2] = v0.z;
        local_coords[j * 9 + 3] = normal.x;
        local_coords[j * 9 + 4] = normal.y;
        local_coords[j * 9 + 5] = normal.z;
        local_coords[j * 9 + 6] = tangent.x;
        local_coords[j * 9 + 7] = tangent.y;
        local_coords[j * 9 + 8] = tangent.z;
    }
}

void render_image(bool gen_primary,
                  const std::string& dump_rays,
                  const Camera& cam,
                  const Config& cfg,
                  float* img,
                  thorin::Array<Node>& nodes,
                  thorin::Array<Vec4>& tris,
                  const std::vector<float>& local_coords,
                  RayBuffer& primary,
                  RayBuffer& ao_buffer) {
    const int img_size = cfg.width * cfg.height;

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<float> dist(0.f, 1.f);

    // Generate primary rays
    if (gen_primary) {
        for (int y = 0; y < cfg.height; y++) {
            for (int x = 0; x < cfg.width; x++) {
                const float kx = 2 * x / (float)cfg.width - 1;
                const float ky = 1 - 2 * y / (float)cfg.height;
                const float3 dir = cam.dir + cam.right * kx + cam.up * ky;
                primary.rays()[y * cfg.width + x].org = make_vec4(cam.eye, 0.0f);
                primary.rays()[y * cfg.width + x].dir = make_vec4(dir, cfg.clip);
            }
        }
    }

    // Intersect them
    primary.traverse(intersect, nodes, tris);

    // Assume that the eye position is the origin of the first ray
    const float3 eye = from_vec4(primary.rays()[0].org);

    std::ofstream rays_out;
    if (dump_rays.length())
        rays_out.open(dump_rays, std::ofstream::binary);

    int ao_rays = 0, first_pixel = 0;
    for (int cur_pixel = 0; cur_pixel < img_size; cur_pixel++) {
        // Generate ambient occlusion rays
        const int tri_id = primary.hits()[cur_pixel].tri_id;
        if (tri_id >= 0) {
            const float t = primary.hits()[cur_pixel].tmax;
            const float3 org = from_vec4(primary.rays()[cur_pixel].dir) * t +
                               from_vec4(primary.rays()[cur_pixel].org);

            // Get the face normal, tangent, bitangent
            const int tri_id = primary.hits()[cur_pixel].tri_id;
            const float3 v1(local_coords[tri_id * 9 + 0],
                            local_coords[tri_id * 9 + 1],
                            local_coords[tri_id * 9 + 2]);
            float3 normal(local_coords[tri_id * 9 + 3],
                          local_coords[tri_id * 9 + 4],
                          local_coords[tri_id * 9 + 5]);

            // Flip it if if doesn't face the viewer
            const float d = dot(eye, normal) - dot(normal, v1);
            if (d < 0) normal = normal * (-1.0f);

            const float3 tangent(local_coords[tri_id * 9 + 6],
                                 local_coords[tri_id * 9 + 7],
                                 local_coords[tri_id * 9 + 8]);

            const float3 bitangent = cross(normal, tangent);

            for (int k = 0; k < cfg.samples; k++) {
                const float u1 = dist(mt);
                const float u2 = dist(mt);

                const float3 s = sample_hemisphere(u1, u2);
                const float3 dir = normalize(s.x * tangent + s.y * bitangent + s.z * normal);

                ao_buffer.rays()[ao_rays + k].org = make_vec4(org, cfg.ao_offset);
                ao_buffer.rays()[ao_rays + k].dir = make_vec4(dir, cfg.ao_tmax);
            }
        } else {
            for (int k = 0; k < cfg.samples; k++) {
                ao_buffer.rays()[ao_rays + k].org = make_vec4(0, 0, 0, 1);
                ao_buffer.rays()[ao_rays + k].dir = make_vec4(1, 1, 1, 0);
            }
        }
        ao_rays += cfg.samples;

        if (ao_rays >= ao_buffer.size() || (ao_rays > 0 && cur_pixel == img_size - 1)) {
            ao_buffer.traverse(occluded, nodes, tris, std::max(ao_rays, 64));

            if(dump_rays.length()) {
                for (int i = 0; i < ao_rays; i++) {
                    rays_out.write((const char*)&ao_buffer.rays()[i].org, sizeof(float) * 3);
                    rays_out.write((const char*)&ao_buffer.rays()[i].dir, sizeof(float) * 3);
                }
            }

            for (int i = 0; i < ao_rays / cfg.samples; i++) {
                const int p = first_pixel + i;
                const int q = i * cfg.samples;

                int count = 0;
                for (int j = 0; j < cfg.samples; j++) {
                    if (ao_buffer.hits()[q + j].tri_id >= 0)
                        count++;
                }

                img[p] += 1.f - (count / (float)cfg.samples);
            }

            first_pixel = cur_pixel + 1;
            ao_rays = 0;
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
                if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT)) {
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

    std::string accel_file, rays_file, dump_rays;
    std::string eye_str, center_str, up_str;
    std::string output;
    Config cfg;

    ArgParser parser(argc, argv);
    parser.add_option<std::string>("accel", "a", "Sets the acceleration structure file name", accel_file, "input.bvh", "input.bvh");
    parser.add_option<float>("clip-dist", "clip", "Sets the view clipping distance", cfg.clip, 1e9f, "t");
    parser.add_option<std::string>("output", "o", "Write an fbuf file containing the rendered image and exit", output, "", "path");
    parser.add_option<std::string>("rays", "r", "Uses the input ray distribution when rendering to a file", rays_file, "", "path");
    parser.add_option<std::string>("dump-rays", "dump", "Dumps the ambient occlusion rays when rendering to a file", dump_rays, "", "path");
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

    thorin::Array<Node> nodes;
    thorin::Array<Vec4> tris;
    if (!load_accel(accel_file, nodes, tris)) {
        std::cerr << "Cannot load acceleration structure file." << std::endl;
        return EXIT_FAILURE;
    }

    Camera cam = gen_camera(eye, center, up, cfg.fov, (float)cfg.width / (float)cfg.height);

    // Generate a local coordinate system for each triangle
    std::vector<float> local_coords;
    {
        std::vector<float> vertices;
        std::vector<int> indices;
        if (!load_mesh(accel_file, indices, vertices)) {
            std::cerr << "Cannot load geometry." << std::endl;
            return EXIT_FAILURE;
        }
        gen_local_coords(indices, vertices, local_coords);
    }

    std::vector<float> image(cfg.width * cfg.height);
    RayBuffer primary(image.size());
    RayBuffer ao_buffer(image.size() * cfg.samples);

    const bool gen_primary = rays_file.length() == 0;

    if (output.length()) {
        if (!gen_primary) {
            std::ifstream in(rays_file, std::ifstream::binary);
            if (!in) {
                std::cerr << "Cannot open ray file." << std::endl;
                return EXIT_FAILURE;
            }

            in.seekg(0, std::ifstream::end);
            int count = in.tellg() / (sizeof(float) * 6);
            in.seekg(0);

            if (count != cfg.width * cfg.height) {
                std::cerr << "Number of rays in ray file does not match resolution." << std::endl;
                return EXIT_FAILURE;
            }

            for (int i = 0; i < count; i++) {
                float org_dir[6];
                in.read((char*)org_dir, sizeof(float) * 6);
                primary.rays()[i].org = make_vec4(org_dir[0], org_dir[1], org_dir[2], 0.0f);
                primary.rays()[i].dir = make_vec4(org_dir[3], org_dir[4], org_dir[5], cfg.clip);
            }
        }

        // Render to a file and exit
        render_image(gen_primary, dump_rays, cam, cfg, image.data(), nodes, tris, local_coords, primary, ao_buffer);
        std::ofstream out(output, std::ofstream::binary);
        out.write((const char*)image.data(), image.size() * sizeof(float));
        return EXIT_SUCCESS;
    }

    if (!gen_primary) {
        std::cerr << "Ray file cannot be used in interactive mode." << std::endl;
        return EXIT_FAILURE;
    }

    if (dump_rays.length()) {
        std::cerr << "Cannot dump ambient occlusion rays in interactive mode." << std::endl;
        return EXIT_FAILURE;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Cannot initialize SDL." << std::endl;
        return EXIT_FAILURE;
    }

    SDL_Window* win = SDL_CreateWindow("Viewer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, cfg.width, cfg.height, 0);

    SDL_Surface* screen = SDL_GetWindowSurface(win);
    View view = {
        cam.eye,                 // Eye
        normalize(cam.dir),      // Forward
        normalize(cam.right),    // Right
        normalize(cam.up),       // Up
        100.0f, 0.005f, 1.0f     // View distance, rotation speed, translation speed
    };

    flush_events();

    bool done = false;
    int accum = 0, frames = 0;
    long last = SDL_GetTicks();
    while (!done) {
        if (accum == 0) {
            // Reset image when the camera moves
            memset(image.data(), 0, sizeof(float) * image.size());
        }

        if (frames == 5) {
            auto to_string_p = [] (float f) {
                int ip = f;
                return std::to_string(ip) + "." + std::to_string((int)((f - ip) * 100));
            };
            float fps = 5 * 1000.0f / (SDL_GetTicks() - last);
            float mrays = cfg.width * cfg.height * fps * (cfg.samples + 1) / 1e6f;
            std::string str = "Viewer [" + to_string_p(fps) + " FPS, " + to_string_p(mrays) + " Mray/s]";
            SDL_SetWindowTitle(win, str.c_str());
            last = SDL_GetTicks();
            frames = 0;
        }

        render_image(true, "", cam, cfg, image.data(), nodes, tris, local_coords, primary, ao_buffer);
        frames++;
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

        SDL_UpdateWindowSurface(win);

        done = handle_events(view, accum);
        cam = gen_camera(view.eye, view.eye + view.forward * view.dist, view.up, cfg.fov, (float)cfg.width / (float)cfg.height);
    }

    SDL_DestroyWindow(win);
    SDL_Quit();

    return EXIT_SUCCESS;
}
