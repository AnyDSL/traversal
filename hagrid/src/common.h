#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <cstdlib>

static const float pi = 3.14159265359f;

inline float radians(float x) {
    return x * pi / 180.0f;
}

inline float degrees(float x) {
    return x * 180.0f / pi;
}

template <typename T>
inline T clamp(T a, T b, T c) {
    return (a < b) ? b : ((a > c) ? c : a);
}

inline int float_as_int(float f) {
    union { float vf; int vi; } v;
    v.vf = f;
    return v.vi;
}

inline float int_as_float(int i) {
    union { float vf; int vi; } v;
    v.vi = i;
    return v.vf;
}

template <typename T, typename U>
T lerp(T a, T b, U u) {
    return a * (1 - u) + b * u;
}

template <typename T, typename U>
T lerp(T a, T b, T c, U u, U v) {
    return a * (1 - u - v) + b * u + c * v;
}

template <typename T>
T reflect(T v, T n) {
    return v - (2 * dot(n, v)) * n;
}

#define assert_normalized(x) check_normalized(x, __FILE__, __LINE__)

template <typename T>
inline void check_normalized(const T& n, const char* file, int line) {
#ifdef CHECK_NORMALS
    const float len = length(n);
    const float tolerance = 0.001f;
    if (len < 1.0f - tolerance || len > 1.0f + tolerance) {
        std::cerr << "Vector not normalized in " << file << ", line " << line << std::endl;
        abort();
    }
#endif
}

#endif // COMMON_H
