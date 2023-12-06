#include "vec3i.hpp"
#include "vec3f.hpp"  // Include the necessary header for vec3f if needed

bool vec3i::operator==(vec3i const& a) const {
    return x == a.x && y == a.y && z == a.z;
}

vec3i vec3i::operator+(vec3i const& a) const {
    return vec3i{x + a.x, y + a.y, z + a.z};
}

vec3i vec3i::operator-(vec3i const& a) const {
    return vec3i{x - a.x, y - a.y, z - a.z};
}

vec3f vec3i::operator+(vec3f const& a) const {
    return vec3f{x + a.x, y + a.y, z + a.z};
}

vec3f vec3i::operator-(vec3f const& a) const {
    return vec3f{x - a.x, y - a.y, z - a.z};
}


bool vec3u::operator==(vec3u const& a) const {
    return x == a.x && y == a.y && z == a.z;
}

vec3u vec3u::operator+(vec3u const& a) const {
    return vec3u{x + a.x, y + a.y, z + a.z};
}

vec3u vec3u::operator-(vec3u const& a) const {
    return vec3u{x - a.x, y - a.y, z - a.z};
}