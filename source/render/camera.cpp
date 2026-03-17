#include "camera.hpp"

void Camera::update()
{
    const guVector pos = transform.get_position();
    guVector inv_pos = {-pos.x, -pos.y, -pos.z};

    Mtx T;
    guMtxTrans(T, inv_pos.x, inv_pos.y, inv_pos.z);

    Mtx rx, ry, rz, rtmp, R;
    guVector axis;

    const guVector rotation = transform.get_rotation();

    axis = {1, 0, 0};
    guMtxRotAxisDeg(rx, &axis, -rotation.x);

    axis = {0, 1, 0};
    guMtxRotAxisDeg(ry, &axis, -rotation.y);

    axis = {0, 0, 1};
    guMtxRotAxisDeg(rz, &axis, -rotation.z);

    guMtxConcat(rx, ry, rtmp);
    guMtxConcat(rtmp, rz, R);

    guMtxConcat(R, T, view);
}

gertex::GXMatrix Camera::apply_transform(Transform &transform)
{
    gertex::GXMatrix modelview;
    guMtxConcat(view, transform.get_matrix(), modelview.mtx);
    return modelview;
}