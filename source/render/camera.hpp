#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <math/vec3f.hpp>
#include <gertex/gertex.hpp>
#include <render/transform.hpp>

struct Camera
{
    float fov;    // Field of view (in degrees)
    float aspect; // Aspect ratio
    float near;   // Near clipping plane
    float far;    // Far clipping plane
    Transform transform;
    Mtx view;

    void update();

    gertex::GXMatrix apply_transform(Transform &transform);
};

#endif