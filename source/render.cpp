#include "render.hpp"

#include "chunk_new.hpp"
#include "texturedefs.h"
#include "base3d.hpp"

uint8_t get_face_brightness(uint8_t face)
{
    switch (face)
    {
    case 0:
    case 1:
        return 153;
    case 2:
        return 127;
    case 4:
    case 5:
        return 204;
    default:
        return 255;
    }
}
extern uint8_t light_map[256];
float get_face_light_index(vec3i pos, uint8_t face)
{
    vec3i other = pos + face_offsets[face];
    block_t *other_block = get_block_at(other);
    if (!other_block)
        return get_block_at(pos)->light;
    return other_block->light;
}

int render_face(vec3i pos, uint8_t face, uint32_t texture_index)
{
    if (!base3d_is_drawing || face >= 6)
    {
        // This is just a dummy call to GX_Vertex to keep buffer size up to date.
        for (int i = 0; i < 4; i++)
            GX_VertexLit(vertex_property_t(), 0, 0);
        return 4;
    }
    vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint8_t lighting = get_face_light_index(pos, face);
    switch (face)
    {
    case 0:
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 1:
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 2:
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 3:
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 4:
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    case 5:
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, face);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, face);
        break;
    default:
        break;
    }
    return 4;
}

guVector AnglesToVector(float x, float y, float distance, guVector vec)
{
    vec.x += cos(DegToRad(x)) * sin(DegToRad(y)) * distance;
    vec.y += -sin(DegToRad(x)) * distance;
    vec.z += cos(DegToRad(x)) * cos(DegToRad(y)) * distance;
    return vec;
}

// Function to calculate the signed distance from a point to a frustum plane
float DistanceToPlane(const vec3f &point, const Frustum &frustum, int planeIndex)
{
    const float *plane = frustum.planes[planeIndex];
    return plane[0] * point.x + plane[1] * point.y + plane[2] * point.z + plane[3];
}

// Function to calculate the distance from a point to a frustum
float DistanceToFrustum(const vec3f &point, const Frustum &frustum)
{
    float minDistance = DistanceToPlane(point, frustum, 0);

    for (int i = 1; i < 6; ++i)
    {
        float distance = DistanceToPlane(point, frustum, i);
        if (distance < minDistance)
        {
            minDistance = distance;
        }
    }

    return minDistance;
}
// Function to normalize a 3D vector
void Normalize(float &x, float &y, float &z)
{
    float length = std::sqrt(x * x + y * y + z * z);
    if (length != 0.0f)
    {
        float invLength = 1.0f / length;
        x *= invLength;
        y *= invLength;
        z *= invLength;
    }
}

void CrossProduct(float *a, float *b, float *out)
{
    guVector v_a = guVector{a[0], a[1], a[2]};
    guVector v_b = guVector{b[0], b[1], b[2]};
    guVector v_out;
    guVecCross(&v_a, &v_b, &v_out);
    out[0] = v_out.x;
    out[1] = v_out.y;
    out[2] = v_out.z;
}
float DotProduct(float *a, float *b)
{
    guVector v_a = guVector{a[0], a[1], a[2]};
    guVector v_b = guVector{b[0], b[1], b[2]};
    return guVecDotProduct(&v_a, &v_b);
}
// Function to calculate the frustum planes from camera parameters
Frustum CalculateFrustum(Camera &camera)
{
    Frustum frustum;

    // Calculate the camera's right and up vectors
    float right[3], up[3];
    float global_up[3] = {0.0f, 1.0f, 0.0f};
    CrossProduct(camera.forward, global_up, right);
    CrossProduct(right, camera.forward, up);

    // Calculate points on the near and far planes
    float nearCenter[3] = {
        camera.position[0] + camera.forward[0] * camera.near,
        camera.position[1] + camera.forward[1] * camera.near,
        camera.position[2] + camera.forward[2] * camera.near};

    float farCenter[3] = {
        camera.position[0] + camera.forward[0] * camera.far,
        camera.position[1] + camera.forward[1] * camera.far,
        camera.position[2] + camera.forward[2] * camera.far};

    // Calculate the normals to the frustum planes
    float normalLeft[3] = {-right[0], -right[1], -right[2]};
    float normalRight[3] = {right[0], right[1], right[2]};
    float normalBottom[3] = {-up[0], -up[1], -up[2]};
    float normalTop[3] = {up[0], up[1], up[2]};
    float normalNear[3] = {-camera.forward[0], -camera.forward[1], -camera.forward[2]};
    float normalFar[3] = {camera.forward[0], camera.forward[1], camera.forward[2]};

    // Normalize the normals
    Normalize(normalLeft[0], normalLeft[1], normalLeft[2]);
    Normalize(normalRight[0], normalRight[1], normalRight[2]);
    Normalize(normalBottom[0], normalBottom[1], normalBottom[2]);
    Normalize(normalTop[0], normalTop[1], normalTop[2]);
    Normalize(normalNear[0], normalNear[1], normalNear[2]);
    Normalize(normalFar[0], normalFar[1], normalFar[2]);

    // Calculate the frustum plane equations in the form Ax + By + Cz + D = 0
    frustum.planes[0][0] = normalLeft[0];
    frustum.planes[0][1] = normalLeft[1];
    frustum.planes[0][2] = normalLeft[2];
    frustum.planes[0][3] = DotProduct(normalLeft, nearCenter);

    frustum.planes[1][0] = normalRight[0];
    frustum.planes[1][1] = normalRight[1];
    frustum.planes[1][2] = normalRight[2];
    frustum.planes[1][3] = DotProduct(normalRight, nearCenter);

    frustum.planes[2][0] = normalBottom[0];
    frustum.planes[2][1] = normalBottom[1];
    frustum.planes[2][2] = normalBottom[2];
    frustum.planes[2][3] = DotProduct(normalBottom, nearCenter);

    frustum.planes[3][0] = normalTop[0];
    frustum.planes[3][1] = normalTop[1];
    frustum.planes[3][2] = normalTop[2];
    frustum.planes[3][3] = DotProduct(normalTop, nearCenter);

    frustum.planes[4][0] = normalNear[0];
    frustum.planes[4][1] = normalNear[1];
    frustum.planes[4][2] = normalNear[2];
    frustum.planes[4][3] = DotProduct(normalNear, nearCenter);

    frustum.planes[5][0] = normalFar[0];
    frustum.planes[5][1] = normalFar[1];
    frustum.planes[5][2] = normalFar[2];
    frustum.planes[5][3] = DotProduct(normalFar, farCenter);

    return frustum;
}