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
