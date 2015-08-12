#ifndef CAMERA_H
#define CAMERA_H

struct Camera {
    float3 eye;
    float3 right;
    float3 up;
    float3 dir;
};

inline Camera gen_camera(const float3& eye, const float3& center, const float3& up, float fov, float ratio) {
    Camera cam;
    const float f = tanf(radians(fov / 2));
    cam.dir = normalize(center - eye);
    cam.right = normalize(cross(cam.dir, up)) * (f * ratio);
    cam.up = cross(cam.right, cam.dir) * f;
    cam.eye = eye;
    return cam;
}

#endif // CAMERA_H
