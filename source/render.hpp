#ifndef _RENDER_HPP_
#define _RENDER_HPP_
#include "vec3i.hpp"
#include "vec3f.hpp"
#include <cstdint>
#include <cmath>
#include <ogc/gx.h>


struct Frustum
{
    float planes[6][4]; // Six planes of the frustum
};

struct Camera
{
    float position[3]; // Camera position
    float forward[3];  // Camera forward vector
    float fov;         // Field of view angle
    float aspect;      // Aspect ratio
    float near;        // Near clipping plane
    float far;         // Far clipping plane
};

float get_face_light_index(vec3i pos, uint8_t face);
uint8_t get_face_brightness(uint8_t face);
int render_face(vec3i pos, uint8_t face, uint32_t texture_index);

guVector AnglesToVector(float x, float y, float distance, guVector vec = guVector());

float DistanceToPlane(const vec3f &point, const Frustum &frustum, int planeIndex);

float DistanceToFrustum(const vec3f &point, const Frustum &frustum);

void Normalize(float &x, float &y, float &z);

void CrossProduct(float *a, float *b, float *out);

float DotProduct(float *a, float *b);

Frustum CalculateFrustum(Camera &camera);
#endif