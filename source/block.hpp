#ifndef _BLOCK_HPP_
#define _BLOCK_HPP_

#include <stdint.h>
#include "block_id.hpp"

#define FACE_NX 0
#define FACE_PX 1
#define FACE_NY 2
#define FACE_PY 3
#define FACE_NZ 4
#define FACE_PZ 5
#define RENDER_FLAG_VISIBLE 6

class block_t
{
public:
    uint8_t id = 0;
    uint8_t light = 0;
    uint8_t visibility_flags = 0;
    uint32_t meta = 0;
    
    uint8_t get_visibility();
    uint8_t get_opacity(uint8_t face);
    uint8_t get_skylight();
    uint8_t get_blocklight();
    uint8_t get_castlight();
    
    BlockID get_blockid();

    void set_visibility(uint8_t flag);
    void set_opacity(uint8_t face, uint8_t flag);
    void set_skylight(uint8_t value);
    void set_blocklight(uint8_t value);
    void set_blockid(BlockID value);
    
};

#endif