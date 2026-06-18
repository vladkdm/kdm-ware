#pragma once
#include <cstdint>
#include <cmath>

struct Vector2 {
    float x, y;
};

struct Vector3 {
    float x, y, z;

    Vector3 operator-(const Vector3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vector3 operator+(const Vector3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vector3 operator+(float s) const { return { x + s, y + s, z + s }; }
    float Dot(const Vector3& o) const { return x * o.x + y * o.y + z * o.z; }
    float Length() const { return sqrtf(x * x + y * y + z * z); }
    Vector3 Normalized() const {
        float l = Length();
        if (l == 0.f) return { 0.f, 0.f, 0.f };
        return { x / l, y / l, z / l };
    }
};

struct Rect {
    float x, y, w, h;
};

struct CHandle {
    int32_t value;

    int GetIndex() const { return value & 0x7FFF; }
    bool IsValid() const { return value != -1; }
};
