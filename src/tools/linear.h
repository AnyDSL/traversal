#ifndef LINEAR_H
#define LINEAR_H

#include <cmath>

struct float3 {
    float x, y, z;
    float3() {}
    float3(float x, float y, float z) : x(x), y(y), z(z) {}
};

inline float3 operator * (float a, float3 b) {
    return float3(a * b.x, a * b.y, a * b.z);
}

inline float3 operator * (float3 a, float b) {
    return float3(a.x * b, a.y * b, a.z * b);
}

inline float3 operator - (float3 a, float3 b) {
    return float3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline float3 operator + (float3 a, float3 b) {
    return float3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline float3 operator * (float3 a, float3 b) {
    return float3(a.x * b.x, a.y * b.y, a.z * b.z);
}

inline float3 cross(float3 a, float3 b) {
    return float3(a.y * b.z - a.z * b.y,
                  a.z * b.x - a.x * b.z, 
                  a.x * b.y - a.y * b.x);
}

inline float dot(float3 a, float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float3 normalize(float3 a) {
    float lensq = dot(a, a);
    return a * (1.0f / sqrtf(lensq));
}

inline float3 rotate(const float3& v, const float3& axis, float angle) {
    float q[4];
    q[0] = axis.x * sinf(angle / 2);
    q[1] = axis.y * sinf(angle / 2);
    q[2] = axis.z * sinf(angle / 2);
    q[3] = cosf(angle / 2);

    float p[4];
    p[0] = q[3] * v.x + q[1] * v.z - q[2] * v.y;
    p[1] = q[3] * v.y - q[0] * v.z + q[2] * v.x;
    p[2] = q[3] * v.z + q[0] * v.y - q[1] * v.x;
    p[3] = -(q[0] * v.x + q[1] * v.y + q[2] * v.z);

    return float3(p[3] * -q[0] + p[0] * q[3] + p[1] * -q[2] - p[2] * -q[1],
                  p[3] * -q[1] - p[0] * -q[2] + p[1] * q[3] + p[2] * -q[0],
                  p[3] * -q[2] + p[0] * -q[1] - p[1] * -q[0] + p[2] * q[3]);
}

inline bool read_float3(const std::string& str, float3& v) {
    // Reads a float3 from a string containing "x,y,z"
    char* ptr = const_cast<char*>(str.c_str());
    v.x = strtof(ptr, &ptr);
    if (*(ptr++) != ',') return false;
    v.y = strtof(ptr, &ptr);
    if (*(ptr++) != ',') return false;
    v.z = strtof(ptr, &ptr);
    return true;
}

static const float pi = 3.14159265359f;

inline float radians(float x) {
    return x * pi / 180.0f;
}

inline float degrees(float x) {
    return x * 180.0f / pi;
}

#endif // LINEAR_H
