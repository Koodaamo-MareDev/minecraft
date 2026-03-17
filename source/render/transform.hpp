#ifndef TRANSFORM_HPP
#define TRANSFORM_HPP

#include <gccore.h>
#include <gertex/gertex.hpp>

class Transform
{
public:
    Transform();

    // setters
    void set_position(const guVector &p);
    void set_rotation(const guVector &r);
    void set_scale(const guVector &s);

    // getters
    const guVector &get_position() const;
    const guVector &get_rotation() const;
    const guVector &get_scale() const;

    // mark transform as needing rebuild
    void mark_dirty();

    // retrieve transform matrix (rebuilds if needed)
    const Mtx &get_matrix();
    const gertex::GXMatrix get_gmatrix();

private:
    void rebuild_matrix();

    guVector position;
    guVector rotation;
    guVector scale;

    Mtx matrix;
    bool dirty;
};
#endif // TRANSFORM_HPP