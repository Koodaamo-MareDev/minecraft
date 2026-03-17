#include "transform.hpp"

Transform::Transform()
{
    position = {0.0f, 0.0f, 0.0f};
    rotation = {0.0f, 0.0f, 0.0f};
    scale = {1.0f, 1.0f, 1.0f};

    guMtxIdentity(matrix);
    dirty = true;
}

void Transform::set_position(const guVector &p)
{
    position = p;
    dirty = true;
}

void Transform::set_rotation(const guVector &r)
{
    rotation = r;
    dirty = true;
}

void Transform::set_scale(const guVector &s)
{
    scale = s;
    dirty = true;
}

const guVector &Transform::get_position() const
{
    return position;
}

const guVector &Transform::get_rotation() const
{
    return rotation;
}

const guVector &Transform::get_scale() const
{
    return scale;
}

void Transform::mark_dirty()
{
    dirty = true;
}

const Mtx &Transform::get_matrix()
{
    if (dirty)
        rebuild_matrix();

    return matrix;
}

const gertex::GXMatrix Transform::get_gmatrix()
{
    if (dirty)
        rebuild_matrix();
    gertex::GXMatrix ret;
    guMtxCopy(matrix, ret.mtx);
    return ret;
}

void Transform::rebuild_matrix()
{
    Mtx T, S;
    Mtx rx, ry, rz;
    Mtx rtmp, R;

    guVector axis;

    // Translation
    guMtxTrans(T, position.x, position.y, position.z);

    // Scale
    guMtxScale(S, scale.x, scale.y, scale.z);

    // Rotation X
    axis = {1.0f, 0.0f, 0.0f};
    guMtxRotAxisDeg(rx, &axis, rotation.x);

    // Rotation Y
    axis = {0.0f, 1.0f, 0.0f};
    guMtxRotAxisDeg(ry, &axis, rotation.y);

    // Rotation Z
    axis = {0.0f, 0.0f, 1.0f};
    guMtxRotAxisDeg(rz, &axis, rotation.z);

    // Combine rotations
    guMtxConcat(rz, ry, rtmp);
    guMtxConcat(rtmp, rx, R);

    // Final transform: T * R * S
    guMtxConcat(R, S, rtmp);
    guMtxConcat(T, rtmp, matrix);

    dirty = false;
}