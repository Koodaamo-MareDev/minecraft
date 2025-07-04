#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP


#define SIMULATION_DISTANCE 2
#define RENDER_DISTANCE (5 * 16)
#define CHUNK_DISTANCE 5
#define CHUNK_COUNT ((CHUNK_DISTANCE) * (CHUNK_DISTANCE + 1) * 4)
#define FOG_DISTANCE (RENDER_DISTANCE - 16)
#define VERTICAL_SECTION_COUNT 8
#define WORLD_HEIGHT (VERTICAL_SECTION_COUNT << 4)
#define MAX_WORLD_Y (WORLD_HEIGHT - 1)

#define VBO_SOLID 1
#define VBO_TRANSPARENT 2
#define VBO_ALL 0xFF

#endif