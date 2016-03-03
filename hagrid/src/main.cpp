#include <iostream>
#include <sstream>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// SDL
#include <SDL2/SDL.h>

// Thorin
#include <thorin_runtime.hpp>

// Internal
#include "common.h"
#include "bbox.h"
#include "float3.h"
#include "grid.h"

#define GRID_X 100
#define GRID_Y 70
#define GRID_Z 70

struct Ref {
    int tri;
    int cell;

    Ref() {}
    Ref(int tri, int cell)
        : tri(tri), cell(cell)
    {}
};

struct Range {
    int lx; int hx;
    int ly; int hy;
    int lz; int hz;

    Range() {}
    Range(int lx, int hx,
          int ly, int hy,
          int lz, int hz)
        : lx(lx), hx(hx)
        , ly(ly), hy(hy)
        , lz(lz), hz(hz)
    {}
    
    int size() const { return (hx - lx + 1) * (hy - ly + 1) * (hz - lz + 1); }
};

struct Camera {
    float3 eye;
    float3 right;
    float3 up;
    float3 dir;
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

inline Camera gen_camera(const float3& eye, const float3& center, const float3& up, float fov, float ratio) {
    Camera cam;
    const float f = tanf(radians(fov / 2));
    cam.dir = normalize(center - eye);
    cam.right = normalize(cross(cam.dir, up)) * (f * ratio);
    cam.up = normalize(cross(cam.right, cam.dir)) * f;
    cam.eye = eye;
    return cam;
}

Range find_coverage(const BBox& bb, const BBox& grid_bb, const float3& inv_size) {
    const int lx = clamp<int>((bb.min.x - grid_bb.min.x) * inv_size.x, 0, GRID_X - 1);
    const int ly = clamp<int>((bb.min.y - grid_bb.min.y) * inv_size.y, 0, GRID_Y - 1);
    const int lz = clamp<int>((bb.min.z - grid_bb.min.z) * inv_size.z, 0, GRID_Z - 1);

    const int hx = clamp<int>((bb.max.x - grid_bb.min.x) * inv_size.x, 0, GRID_X - 1);
    const int hy = clamp<int>((bb.max.y - grid_bb.min.y) * inv_size.y, 0, GRID_Y - 1);
    const int hz = clamp<int>((bb.max.z - grid_bb.min.z) * inv_size.z, 0, GRID_Z - 1);

    return Range(lx, hx, ly, hy, lz, hz);
}

extern "C" int triBoxOverlap(float boxcenter[3],float boxhalfsize[3],float triverts[3][3]);

inline bool tri_overlap_cell(const BBox& grid_bb, const float3& cell_size, const Tri& tri, int x, int y, int z) {
    /*const float3 min = grid_bb.min + cell_size * float3(x, y, z);
    const float3 max = grid_bb.min + cell_size * float3(x + 1, y + 1, z + 1);
    const float3 n(tri.nx, tri.ny, tri.nz);
    auto d = dot(n, float3(tri.v0.x, tri.v0.y, tri.v0.z));

    float3 first, last;

    first.x = n.x > 0 ? min.x : max.x;
    first.y = n.y > 0 ? min.y : max.y;
    first.z = n.z > 0 ? min.z : max.z;

    last.x = n.x <= 0 ? min.x : max.x;
    last.y = n.y <= 0 ? min.y : max.y;
    last.z = n.z <= 0 ? min.z : max.z;

    const float d0 = dot(n, first) - d;
    const float d1 = dot(n, last)  - d;

    return d0 * d1 < 0.0f;*/
    const float3 min = grid_bb.min + cell_size * float3(x, y, z);
    const float3 max = grid_bb.min + cell_size * float3(x + 1, y + 1, z + 1);
    const float3 center = (max + min) * 0.5f;
    const float3 half_size = (max - min) * 0.5f;

    float boxcenter[3], boxhalfsize[3];
    boxcenter[0] = center.x;
    boxcenter[1] = center.y;
    boxcenter[2] = center.z;

    boxhalfsize[0] = half_size.x;
    boxhalfsize[1] = half_size.y;
    boxhalfsize[2] = half_size.z;
    float triverts[3][3];
    triverts[0][0] = tri.v0.x;
    triverts[0][1] = tri.v0.y;
    triverts[0][2] = tri.v0.z;

    triverts[1][0] = tri.v0.x - tri.e1.x;
    triverts[1][1] = tri.v0.y - tri.e1.y;
    triverts[1][2] = tri.v0.z - tri.e1.z;

    triverts[2][0] = tri.v0.x + tri.e2.x;
    triverts[2][1] = tri.v0.y + tri.e2.y;
    triverts[2][2] = tri.v0.z + tri.e2.z;
    return triBoxOverlap(boxcenter, boxhalfsize, triverts);
}

void build_grid(const std::vector<Tri>& tris, Vec3f& grid_min, Vec3f& grid_max, thorin::Array<Cell>& grid_cells, thorin::Array<Tri>& grid_tris) {
    std::vector<BBox> bboxes(tris.size());
    BBox grid_bb = BBox::empty();

    int tri_count = tris.size();

    // Compute bounding boxes and global bounding box
    #pragma omp parallel for
    for (int i = 0; i < tri_count; i++) {
        const auto& tri = tris[i];
        const float3 v0(tri.v0.x, tri.v0.y, tri.v0.z);
        const float3 e1(tri.e1.x, tri.e1.y, tri.e1.z);
        const float3 e2(tri.e2.x, tri.e2.y, tri.e2.z);
        auto v1 = v0 - e1;
        auto v2 = v0 + e2;

        bboxes[i].min = min(v0, min(v1, v2));
        bboxes[i].max = max(v0, max(v1, v2));

        #pragma omp critical
        {
            grid_bb.extend(bboxes[i]);
        }
    }

    // Compute the number of references
    const float3 grid_size = grid_bb.max - grid_bb.min;
    const float3 cell_size(grid_size.x / GRID_X, grid_size.y / GRID_Y, grid_size.z / GRID_Z);
    const float3 inv_size(GRID_X / grid_size.x, GRID_Y / grid_size.y, GRID_Z / grid_size.z);

    std::vector<Range> ranges(tri_count);

    #pragma omp parallel for
    for (int i = 0; i < tri_count; i++) {
        ranges[i] = find_coverage(bboxes[i], grid_bb, inv_size);
    }

    const int dim0  = GRID_X;
    const int dim01 = GRID_Y * GRID_X;

    // Put the references in the array
    std::vector<Ref> refs;
    for (int i = 0; i < tri_count; i++) {
        const Range& range = ranges[i];

        // Examine each cell and determine if the triangle is really inside
        for (int x = range.lx; x <= range.hx; x++) {
            for (int y = range.ly; y <= range.hy; y++) {
                for (int z = range.lz; z <= range.hz; z++) {
                    if (tri_overlap_cell(grid_bb, cell_size, tris[i], x, y, z))
                        refs.emplace_back(i, z * dim01 + y * dim0 + x);
                }
            }
        }
    }

    // Sort the references
    std::sort(refs.begin(), refs.end(), [] (const Ref& a, const Ref& b) {
        return a.cell < b.cell;
    });

    // Emit the grid
    const int ref_count = refs.size();
    thorin::Array<Cell> host_cells(GRID_X * GRID_Y * GRID_Z);
    thorin::Array<Tri>  host_tris(ref_count);

    memset(host_cells.data(), 0, sizeof(Cell) * host_cells.size());

    // Compute the size of each cell
    #pragma omp parallel for
    for (int i = 0; i < ref_count; i++) {
        #pragma omp atomic
        {
            host_cells[refs[i].cell].end += 1;
        }
    }

    // Compute the starting position of each cell (prefix sum)
    for (int i = 1; i < host_cells.size(); i++) {
        host_cells[i].begin = host_cells[i - 1].begin + host_cells[i - 1].end;
    }

    #pragma omp parallel for
    for (int i = 1; i < host_cells.size(); i++) {
        host_cells[i].end += host_cells[i].begin;
    }

    #pragma omp parallel for
    for (int i = 0; i < ref_count; i++) {
        host_tris[i] = tris[refs[i].tri];
    }

    // Upload data to the device
    grid_cells = std::move(thorin::Array<Cell>(thorin::Platform::CUDA, thorin::Device(0), GRID_X * GRID_Y * GRID_Z));
    grid_tris  = std::move(thorin::Array<Tri >(thorin::Platform::CUDA, thorin::Device(0), ref_count));
    thorin::copy(host_cells, grid_cells);
    thorin::copy(host_tris,  grid_tris);

    grid_min.x = grid_bb.min.x;
    grid_min.y = grid_bb.min.y;
    grid_min.z = grid_bb.min.z;
    grid_max.x = grid_bb.max.x;
    grid_max.y = grid_bb.max.y;
    grid_max.z = grid_bb.max.z;

    std::cout << ref_count << " references" << std::endl;
}

void flush_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {}
}

bool handle_events(View& view) {
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
                }
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_UP:    view.eye = view.eye + view.tspeed * view.forward; break;
                    case SDLK_DOWN:  view.eye = view.eye - view.tspeed * view.forward; break;
                    case SDLK_LEFT:  view.eye = view.eye - view.tspeed * view.right;   break;
                    case SDLK_RIGHT: view.eye = view.eye + view.tspeed * view.right;   break;
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

bool load_model(const std::string& file_name, std::vector<Tri>& model) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(file_name, aiProcess_Triangulate);

    if (!scene)
    {
        std::cerr << importer.GetErrorString() << std::endl;
        return false;
    }

    for (int i = 0; i < scene->mNumMeshes; i++) {
        auto mesh = scene->mMeshes[i];
        for (int j = 0; j < mesh->mNumFaces; j++) {
            auto& face = mesh->mFaces[j];
            auto& v0 = mesh->mVertices[face.mIndices[0]];
            auto& v1 = mesh->mVertices[face.mIndices[1]];
            auto& v2 = mesh->mVertices[face.mIndices[2]];
            auto e1 = v0 - v1;
            auto e2 = v2 - v0;
            auto n  = e1 ^ e2;
            
            const Tri tri = {
                {v0.x, v0.y, v0.z}, n.x,
                {e1.x, e1.y, e1.z}, n.y,
                {e2.x, e2.y, e2.z}, n.z
            };
            model.push_back(tri);
        }
    }

    std::cout << model.size() << " triangles" << std::endl;

    return true;
}

Vec3f make_vec3f(float3 v) {
    Vec3f w { v.x, v.y, v.z };
    return w;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: hagrid 3d-model-file" << std::endl;
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Cannot initialize SDL." << std::endl;        
        return 1;
    }

    std::vector<Tri> model;
    if (!load_model(argv[1], model)) {
        std::cerr << "Cannot load model" << std::endl;
        return 1;
    }

    const int w = 1024, h = 1024;
    SDL_Window* win = SDL_CreateWindow("HaGrid", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, 0);
    SDL_Surface* screen = SDL_GetWindowSurface(win);

    Vec3i dims = { GRID_X, GRID_Y, GRID_Z };
    Vec3f min, max;
    thorin::Array<Cell> cells;
    thorin::Array<Tri> tris;

    // Build the grid for the model
    build_grid(model, min, max, cells, tris);

    thorin::Array<Ray> host_rays(w * h);
    thorin::Array<Hit> host_hits(w * h);
    thorin::Array<Ray> rays(thorin::Platform::CUDA, thorin::Device(0), w * h);
    thorin::Array<Hit> hits(thorin::Platform::CUDA, thorin::Device(0), w * h);

    flush_events();

    bool done = false;

    const float fov = 60.0f;
    Camera cam = gen_camera(float3(10.4209, -0.270721, -0.058378),
                            float3(-80.3837, -42.0259, -3.38165),
                            float3(-0.418404, 0.907921, 0.0),
                            fov, (float)w / (float)h);
    View view = {
        cam.eye,                 // Eye
        normalize(cam.dir),      // Forward
        normalize(cam.right),    // Right
        normalize(cam.up),       // Up
        100.0f, 0.005f, 1.0f     // View distance, rotation speed, translation speed
    };

    const float clip = 5000.f;
    long ktime = 0;
    int frames = 0;
    int ticks = 0;

    while (!done) {
        // Generate primary rays
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                const float kx = 2 * x / (float)w - 1;
                const float ky = 1 - 2 * y / (float)h;
                const float3 dir = cam.dir + cam.right * kx + cam.up * ky;

                auto& ray = host_rays[y * w + x];
                ray.org = make_vec3f(cam.eye);
                ray.dir = make_vec3f(dir);
                ray.tmin = 0.0f;
                ray.tmax = clip;
            }
        }

        if (SDL_GetTicks() - ticks >= 1000) {
            std::ostringstream caption;
            caption << "HaGrid [" << (double)((long)frames * w * h) / ktime << " MRays/s]";
            SDL_SetWindowTitle(win, caption.str().c_str());
            ticks = SDL_GetTicks();
            ktime = 0;
            frames = 0;
        }

        thorin::copy(host_rays, rays);
        long t0 = thorin_get_kernel_time();
        traverse_grid_gpu(cells.data(), &dims, &min, &max, tris.data(), rays.data(), hits.data(), rays.size());
        ktime += thorin_get_kernel_time() - t0;
        frames++;
        thorin::copy(hits, host_hits);

        // Copy image to screen
        SDL_LockSurface(screen);
        for (int y = 0; y < h; y++) {
            char* row = (char*)screen->pixels + screen->pitch * y;
            for (int x = 0; x < w; x++) {
                char color = host_hits[y * w + x].t * 255.0f / clip;
                row[x * 4 + 0] = color;
                row[x * 4 + 1] = color;
                row[x * 4 + 2] = color;
                row[x * 4 + 3] = 255;
            }
        }
        SDL_UnlockSurface(screen);

        SDL_UpdateWindowSurface(win);

        done = handle_events(view);
        cam = gen_camera(view.eye, view.eye + view.forward * view.dist, view.up, fov, (float)w / (float)h);
    }

    SDL_DestroyWindow(win);
    SDL_Quit();
    
    return 0;
}

