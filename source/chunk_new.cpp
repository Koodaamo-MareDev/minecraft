#include "cpugx.hpp"
#include "chunk_new.hpp"
#include "block.hpp"
#include "texturedefs.h"
#include "vec3i.hpp"
#include "blocks.hpp"
#include "timers.hpp"
#include "light.hpp"
#include "opensimplexnoise/open-simplex-noise.h"
#include <vector>
#include <algorithm>
#include <gccore.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
extern bool isExiting;
std::vector<std::vector<vec3i>> water_levels(9);
const vec3i face_offsets[] = {
    vec3i{-1, +0, +0},
    vec3i{+1, +0, +0},
    vec3i{+0, -1, +0},
    vec3i{+0, +1, +0},
    vec3i{+0, +0, -1},
    vec3i{+0, +0, +1}};
void *get_aligned_pointer_32(void *ptr)
{
    return (void *)((u64(ptr) + 0x1F) & (~0x1F));
    // return (void *)((((size_t)ptr + ((((size_t)ptr & 31) != 0) << 5))) & ~31);
}

chunkvbo_t *global_vbos[VBO_COUNT];
chunk_t *chunks = nullptr;
chunk_t invalid_chunk;

bool is_drawing = false;
bool cachelock = false;

void lock_chunk_cache(chunk_t *chunk)
{
    if (chunk)
        get_chunk(chunk->x, chunk->z, false);
    cachelock = true;
}

void unlock_chunk_cache()
{
    cachelock = false;
}

chunk_t *get_chunks()
{
    return chunks;
}

chunk_t *get_chunk(int chunkX, int chunkZ, bool load)
{
    // Simple cache for fast fetching
    static int lastFetchX = 0;
    static int lastFetchZ = 0;
    static chunk_t *lastFetchChunk = nullptr;

    if (lastFetchChunk && lastFetchX == chunkX && lastFetchZ == chunkZ)
        return lastFetchChunk;
    if (!cachelock)
    {
        lastFetchX = chunkX;
        lastFetchZ = chunkZ;
    }

    // Chunk lookup based on x and z position
    for (int i = 0; i < CHUNK_COUNT; i++)
    {
        if (chunks[i].x == chunkX && chunks[i].z == chunkZ)
        {
            if (!cachelock)
                lastFetchChunk = &chunks[i];
            return &chunks[i];
        }
    }
    if (!cachelock)
        lastFetchChunk = nullptr;
    return nullptr;
    // return lastFetchChunk = &invalid_chunk;
}

// create chunk
void init_chunks()
{
    if (!chunks)
    {
        struct osn_context *ctx;

        open_simplex_noise(rand() & 32767, &ctx);
        double scale = 32;
        double intensity = 2.5;

        chunks = new chunk_t[CHUNK_COUNT]; //(chunk_t *)calloc(CHUNK_COUNT, sizeof(chunk_t));
        for (int i = 0; i < CHUNK_COUNT; i++)
        {
            chunks[i].x = (i % RENDER_DISTANCE);
            chunks[i].z = (i / RENDER_DISTANCE);
            chunks[i].valid = true;
        }
        for (int i = 0; i < CHUNK_COUNT; i++)
        {
            printf("Generating Chunk %d\n", i);
            chunk_t *chunk = &chunks[i];
            int x_offset = (chunk->x << 4);
            int z_offset = (chunk->z << 4);
            for (int x = 0, noise_x = x_offset; x < 16; x++, noise_x++)
            {
                for (int z = 0, noise_z = z_offset; z < 16; z++, noise_z++)
                {
                    double value = open_simplex_noise3(ctx, (double)noise_x / scale, (double)noise_z / scale, 1 / 1000.) * 3;
                    value *= open_simplex_noise3(ctx, (double)noise_x / scale, (double)noise_z / scale, 1 / 500.);
                    value *= open_simplex_noise3(ctx, (double)noise_x / scale, (double)noise_z / scale, 1 / 300.);
                    value = value * intensity * 16 + 64;
                    for (int y = 0; y < 256; y++)
                    {
                        block_t *block = chunk->get_block(x, y, z);
                        block->visibility_flags = 0x00;
                        block->light = 0x00;
                        double carve = open_simplex_noise3(ctx, (double)noise_x / (scale * 0.25) + 0.5, (double)y / (scale * 0.25) + 0.5, noise_z / (scale * 0.25) + 0.5);
                        double boolval = open_simplex_noise3(ctx, (double)noise_x / (scale * 0.25) + 0.25, (double)y / (scale * 0.25) + 0.25, noise_z / (scale * 0.25) + 0.25);
                        double thresh = y / 256.;
                        thresh *= thresh * thresh * 4;
                        thresh = std::max(.4, thresh);
                        if (value > 64 && carve > 0.55)
                        {
                            block->set_blockid(BlockID::air);
                        }
                        else if (y < value)
                        {
                            if (y < value - 4 && boolval > thresh)
                            {
                                block->set_blockid(BlockID::air);
                            }
                            else
                            {
                                BlockID blockid = (y < value - 1) ? BlockID::dirt : BlockID::grass;
                                blockid = (y < value - 3) ? BlockID::stone : blockid;
                                block->visibility_flags = 0x7F;
                                block->set_blockid(blockid);
                            }
                        }
                        else
                        {
                            if (y > 64)
                                block->set_blockid(BlockID::air);
                            else
                            {
                                block->visibility_flags = 0x7F;
                                block->set_blockid(BlockID::water);
                            }
                        }
                    }
                }
            }
        }
        open_simplex_noise_free(ctx);

        // Calculate lighting values

        for (int i = 0; i < CHUNK_COUNT; i++)
        {
            chunk_t *chunk = &chunks[i];
            int x_offset = (chunk->x << 4);
            int z_offset = (chunk->z << 4);
            for (int x = 0; x < 16; x++)
            {
                for (int z = 0; z < 16; z++)
                {
                    cast_skylight(x_offset + x, z_offset + z);
                }
            }
        }
    }
}

void prepare_chunks()
{
    if (chunks)
    {
        for (int i = 0; i < CHUNK_COUNT; i++)
        {
            chunks[i].prepare_render();
            printf("Chunk %d rendered.\n", i);
        }
    }
}

BlockID get_block_id_at(vec3i position, BlockID default_id)
{
    block_t *block = get_block_at(position);
    if (!block)
        return default_id;
    return block->get_blockid();
}
block_t *get_block_at(vec3i position)
{
    if (position.y < 0 || position.y > 255)
        return nullptr;
    chunk_t *chunk = get_chunk(position.x >> 4, position.z >> 4, false);
    if (!chunk)
        return nullptr;
    position.x &= 0x0F;
    position.y &= 0xFF;
    position.z &= 0x0F;
    return &chunk->blockstates[position.x | (position.y << 4) | (position.z << 12)];
}

block_t *chunk_t::get_block(int x, int y, int z)
{
    return this->blockstates + ((x & 0xF) | ((y & 0xFF) << 4) | ((z & 0xF) << 12));
}

void chunk_t::recalculate()
{
    for (int i = 0; i < VERTICAL_SECTION_COUNT; i++)
    {
        this->recalculate_section(i);
    }
}

void chunk_t::update_blockI(int index)
{
    vec3i pos = {index & 0xF, (index >> 4) & 0xFF, (index >> 12) & 0xF};
    update_block(pos);
}

void chunk_t::update_block(vec3i local_pos)
{
    vec3i chunk_pos = vec3i{(this->x << 4) + x, 0, (this->z << 4) + z};
    vec3i pos = local_pos + chunk_pos;
    block_t *block = this->get_block(local_pos.x, local_pos.y, local_pos.z);
    switch (block->get_blockid())
    {
    case BlockID::flowing_water:
        update_fluid(block, pos);
        break;

    default:
        break;
    }
}

void update_block_at(vec3i pos)
{
    if (pos.y > 255 || pos.y < 0)
        return;
    chunk_t *chunk = get_chunk(pos.x >> 4, pos.z >> 4, false);
    if (!chunk)
        return;
    cast_skylight(pos.x, pos.z);
    chunk->recalculate_section_later(pos.y >> 4);
}

void chunk_t::recalculate_section_later(int section)
{
    int chunkY = section << 4;
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            for (int z = 0; z < 16; z++)
            {
                block_t *block = this->get_block(x, y + chunkY, z);
                block_t *neighbor_py = get_block_at(vec3i{x, y + chunkY + 1, z});
                BlockID block_id = block->get_blockid();
                uint8_t full = 0;
                if (neighbor_py && is_fluid(neighbor_py->get_blockid()))
                    full = 1;
                block->visibility_flags = 0x7F * (block_id != BlockID::air && !(is_flowing_fluid(block_id) && full));
            }
        }
    }
}

// recalculates the blockstates of a section
void chunk_t::recalculate_section(int section)
{

    int chunkX = this->x << 4;
    int chunkZ = this->z << 4;
    int chunkY = section << 4;

    vec3i chunk_pos = {chunkX, chunkY, chunkZ};
    /*
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            for (int z = 0; z < 16; z++)
            {
                block_t *block = this->get_block(x, y + chunkY, z);
                BlockID block_id = block->get_blockid();
                if (block_id == BlockID::flowing_water)
                {
                    vec3i top_pos = {x, y + chunkY + 1, z};
                    block_t *top = this->get_block(top_pos.x, top_pos.y, top_pos.z);
                    if (top)
                    {
                        BlockID top_id = top->get_blockid();
                        if (top_id == BlockID::flowing_water || top_id == BlockID::water)
                            block->visibility_flags = 0x7F;
                    }
                }
            }
        }
    }
    */
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            for (int z = 0; z < 16; z++)
            {

                block_t *block = this->get_block(x, y + chunkY, z);
                BlockID block_id = block->get_blockid();
                vec3i local_pos = {x, y, z};
                block->visibility_flags &= ~0x3F;
                if (block->get_visibility())
                {
                    for (int i = 0; i < 6; i++)
                    {
                        vec3i other = chunk_pos + local_pos + face_offsets[i];
                        block_t *other_block = get_block_at(other);
                        BlockID other_id = !other_block ? BlockID::air : other_block->get_blockid();
                        bool face_transparent = is_face_transparent(get_face_texture_index(block, i));

                        bool other_face_transparent = false;
                        bool other_visibility = false;
                        if (other_block)
                        {
                            other_face_transparent = is_face_transparent(get_face_texture_index(other_block, i ^ 1));
                            other_visibility = other_block->get_visibility();
                        }

                        if ((!face_transparent && other_face_transparent) || !other_visibility)
                        {
                            block->set_opacity(i, true);
                        }
                        else if (face_transparent && other_block && other_id != block_id)
                        {
                            block->set_opacity(i, true);
                        }
                        if (other_block && (is_still_fluid(block_id)) && is_flowing_fluid(other_id))
                        {
                            block->set_opacity(i, false);
                        }
                        vec3i py_pos = chunk_pos + vec3i{x, y, z};
                        block_t *py_block = get_block_at(py_pos);
                        if (is_flowing_fluid(block_id) && !(py_block && is_fluid(py_block->get_blockid())))
                        {
                            block->set_opacity(i, false);
                        }
                    }
                }

                // block->visibility_flags = !(block->visibility_flags & 0x3F) ? 0 : block->visibility_flags;
            }
        }
    }
    light_engine_update();
}

void chunk_t::destroy_vbo(int section, unsigned char which)
{
    if (!which)
        return;
    if ((which & VBO_SOLID))
    {
        if (this->vbos[section].solid_buffer)
            free(this->vbos[section].solid_buffer);
        this->vbos[section].solid_buffer = nullptr;
        this->vbos[section].solid_buffer_length = 0;
    }
    if ((which & VBO_TRANSPARENT))
    {
        if (this->vbos[section].transparent_buffer)
            free(this->vbos[section].transparent_buffer);
        this->vbos[section].transparent_buffer = nullptr;
        this->vbos[section].transparent_buffer_length = 0;
    }
}

void chunk_t::build_all_vbos()
{
    for (int i = 0; i < VERTICAL_SECTION_COUNT; i++)
    {
        this->destroy_vbo(i, VBO_ALL);
        if (this->build_vbo(i, false) || this->build_vbo(i, true))
            printf("BuildVBO failed.\n");
    }
}
void chunk_t::rebuild_vbo(int section, bool transparent)
{
    destroy_vbo(section, transparent ? VBO_TRANSPARENT : VBO_SOLID);
    if (this->build_vbo(section, transparent))
        printf("BuildVBO failed.\n");
}

#define UNLOCK_RETURN(VAL)    \
    {                         \
        unlock_chunk_cache(); \
        return (VAL);         \
    }
int chunk_t::build_vbo(int section, bool transparent)
{
    lock_chunk_cache(this);
    // uint64_t time_start = time_get();
    int faceCount = 0;
    int chunkX = this->x << 4;
    int chunkY = section << 4;
    int chunkZ = this->z << 4;
    this->vbos[section].x = chunkX;
    this->vbos[section].y = chunkY;
    this->vbos[section].z = chunkZ;
    vec3i chunk_pos = vec3i{chunkX, chunkY, chunkZ};
    // Count faces
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            for (int z = 0; z < 16; z++)
            {
                block_t *block = this->get_block(x, y + chunkY, z);
                if (!block->get_visibility() || is_fluid(block->get_blockid()))
                    continue;
                for (int i = 0; i < 6; i++)
                {
                    if (block->get_opacity(i) && transparent == is_face_transparent(get_face_texture_index(block, i)))
                        faceCount++;
                }
            }
        }
    }
    int quadVertexCount = faceCount * 4;
    int triaVertexCount = pre_render_fluid_mesh(section, transparent);
    if (!quadVertexCount && !triaVertexCount)
    {
        if (transparent)
        {
            this->vbos[section].transparent_buffer = nullptr;
            this->vbos[section].transparent_buffer_length = 0;
        }
        else
        {
            this->vbos[section].solid_buffer = nullptr;
            this->vbos[section].solid_buffer_length = 0;
        }
        UNLOCK_RETURN(0);
    }

    // Calculate potential memory usage
    int estimatedMemory = (triaVertexCount + quadVertexCount) * VERTEX_ATTR_LENGTH;
    if (quadVertexCount)
        estimatedMemory += 3;
    if (triaVertexCount)
        estimatedMemory += 3;
    estimatedMemory += 32;
    if ((estimatedMemory & 31) != 0)
    {
        estimatedMemory += 32;
        estimatedMemory &= ~31;
    }
    // Round down to nearest 32
    // estimatedMemory &= ~31;
    void *displist_vbo = memalign(32, estimatedMemory); // 32 bytes for alignment

    if (displist_vbo == nullptr)
    {
        printf("Failed to allocate %d bytes for section %d VBO at (%d, %d)\n", estimatedMemory, section, this->x, this->z);
        UNLOCK_RETURN(1);
    }
    DCInvalidateRange(ALIGNPTR(displist_vbo), estimatedMemory);
    // memset(ALIGNPTR(displist_vbo), 0, estimatedMemory);

    GX_BeginDispList(ALIGNPTR(displist_vbo), estimatedMemory);
    if (quadVertexCount)
    {
        GX_Begin(GX_QUADS, GX_VTXFMT0, quadVertexCount); // Start drawing

        // Build the mesh from the blockstates
        for (int x = 0; x < 16; x++)
        {
            for (int y = 0; y < 16; y++)
            {
                for (int z = 0; z < 16; z++)
                {
                    block_t *block = this->get_block(x, y + chunkY, z);
                    if (!block->get_visibility() || is_fluid(block->get_blockid()))
                        continue;
                    int texture_index = 0;
                    int i = 0;
                    // block_t *block_above = get_block_at(face_offsets[FACE_PY] + vec3i{x, y, z} + chunk_pos);
                    float block_height_half = 0.5f; // (is_fluid(block->get_blockid()) && !(block_above && is_fluid(block_above->get_blockid()))) ? 0.375 : 0.5f;
                    if (block->get_opacity(i) && transparent == is_face_transparent(texture_index = get_face_texture_index(block, i)))
                    {
                        vec3i other = face_offsets[i] + vec3i{x, y, z} + chunk_pos;
                        block_t *other_block = get_block_at(other);
                        float facelight = 0.05f;
                        if (other_block)
                            facelight = std::min(1.0f, other_block->get_blocklight() / 15.0f + 0.05f);
                        float lightAmount = 0.9f * facelight;

                        GX_Position3f32(x + -.5f, y + -.5f, z + -.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index), (UV_SCALE * TEXTURE_Y(texture_index) + UV_SCALE));

                        GX_Position3f32(x + -.5f, y + block_height_half, z + -.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index), (UV_SCALE * TEXTURE_Y(texture_index)));

                        GX_Position3f32(x + -.5f, y + block_height_half, z + 0.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index) + UV_SCALE, (UV_SCALE * TEXTURE_Y(texture_index)));

                        GX_Position3f32(x + -.5f, y + -.5f, z + 0.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index) + UV_SCALE, (UV_SCALE * TEXTURE_Y(texture_index) + UV_SCALE));
                    }
                    i++;

                    if (block->get_opacity(i) && transparent == is_face_transparent(texture_index = get_face_texture_index(block, i)))
                    {
                        vec3i other = face_offsets[i] + vec3i{x, y, z} + chunk_pos;
                        block_t *other_block = get_block_at(other);
                        float facelight = 0.05f;
                        if (other_block)
                            facelight = std::min(1.0f, other_block->get_blocklight() / 15.0f + 0.05f);
                        float lightAmount = 0.9f * facelight;

                        GX_Position3f32(x + 0.5f, y + -.5f, z + 0.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index) + UV_SCALE, (UV_SCALE * TEXTURE_Y(texture_index) + UV_SCALE));

                        GX_Position3f32(x + 0.5f, y + block_height_half, z + 0.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index) + UV_SCALE, (UV_SCALE * TEXTURE_Y(texture_index)));

                        GX_Position3f32(x + 0.5f, y + block_height_half, z + -.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index), (UV_SCALE * TEXTURE_Y(texture_index)));

                        GX_Position3f32(x + 0.5f, y + -.5f, z + -.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index), (UV_SCALE * TEXTURE_Y(texture_index) + UV_SCALE));
                    }
                    i++;

                    if (block->get_opacity(i) && transparent == is_face_transparent(texture_index = get_face_texture_index(block, i)))
                    {
                        vec3i other = face_offsets[i] + vec3i{x, y, z} + chunk_pos;
                        block_t *other_block = get_block_at(other);
                        float facelight = 0.05f;
                        if (other_block)
                            facelight = std::min(1.0f, other_block->get_blocklight() / 15.0f + 0.05f);
                        float lightAmount = 0.85f * facelight;

                        GX_Position3f32(x + -.5f, y + -.5f, z + -.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index), (UV_SCALE * TEXTURE_Y(texture_index) + UV_SCALE));

                        GX_Position3f32(x + -.5f, y + -.5f, z + 0.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index), (UV_SCALE * TEXTURE_Y(texture_index)));

                        GX_Position3f32(x + 0.5f, y + -.5f, z + 0.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index) + UV_SCALE, (UV_SCALE * TEXTURE_Y(texture_index)));

                        GX_Position3f32(x + 0.5f, y + -.5f, z + -.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index) + UV_SCALE, (UV_SCALE * TEXTURE_Y(texture_index) + UV_SCALE));
                    }
                    i++;

                    if (block->get_opacity(i) && transparent == is_face_transparent(texture_index = get_face_texture_index(block, i)))
                    {
                        vec3i other = face_offsets[i] + vec3i{x, y, z} + chunk_pos;
                        block_t *other_block = get_block_at(other);
                        float facelight = 0.05f;
                        if (other_block)
                            facelight = std::min(1.0f, other_block->get_blocklight() / 15.0f + 0.05f);
                        float lightAmount = 1.0f * facelight;

                        GX_Position3f32(x + 0.5f, y + block_height_half, z + -.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index) + UV_SCALE, (UV_SCALE * TEXTURE_Y(texture_index) + UV_SCALE));

                        GX_Position3f32(x + 0.5f, y + block_height_half, z + 0.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index) + UV_SCALE, (UV_SCALE * TEXTURE_Y(texture_index)));

                        GX_Position3f32(x + -.5f, y + block_height_half, z + 0.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index), (UV_SCALE * TEXTURE_Y(texture_index)));

                        GX_Position3f32(x + -.5f, y + block_height_half, z + -.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index), (UV_SCALE * TEXTURE_Y(texture_index) + UV_SCALE));
                    }
                    i++;

                    if (block->get_opacity(i) && transparent == is_face_transparent(texture_index = get_face_texture_index(block, i)))
                    {
                        vec3i other = face_offsets[i] + vec3i{x, y, z} + chunk_pos;
                        block_t *other_block = get_block_at(other);
                        float facelight = 0.05f;
                        if (other_block)
                            facelight = std::min(1.0f, other_block->get_blocklight() / 15.0f + 0.05f);
                        float lightAmount = 0.98f * facelight;

                        GX_Position3f32(x + 0.5f, y + -.5f, z + -.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index) + UV_SCALE, (UV_SCALE * TEXTURE_Y(texture_index) + UV_SCALE));

                        GX_Position3f32(x + 0.5f, y + block_height_half, z + -.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index) + UV_SCALE, (UV_SCALE * TEXTURE_Y(texture_index)));

                        GX_Position3f32(x + -.5f, y + block_height_half, z + -.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index), (UV_SCALE * TEXTURE_Y(texture_index)));

                        GX_Position3f32(x + -.5f, y + -.5f, z + -.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index), (UV_SCALE * TEXTURE_Y(texture_index) + UV_SCALE));
                    }
                    i++;

                    if (block->get_opacity(i) && transparent == is_face_transparent(texture_index = get_face_texture_index(block, i)))
                    {
                        vec3i other = face_offsets[i] + vec3i{x, y, z} + chunk_pos;
                        block_t *other_block = get_block_at(other);
                        float facelight = 0.05f;
                        if (other_block)
                            facelight = std::min(1.0f, other_block->get_blocklight() / 15.0f + 0.05f);
                        float lightAmount = 0.98f * facelight;

                        GX_Position3f32(x + -.5f, y + -.5f, z + 0.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index), (UV_SCALE * TEXTURE_Y(texture_index) + UV_SCALE));

                        GX_Position3f32(x + -.5f, y + block_height_half, z + 0.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index), (UV_SCALE * TEXTURE_Y(texture_index)));

                        GX_Position3f32(x + 0.5f, y + block_height_half, z + 0.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index) + UV_SCALE, (UV_SCALE * TEXTURE_Y(texture_index)));

                        GX_Position3f32(x + 0.5f, y + -.5f, z + 0.5f);
                        GX_Color3f32(lightAmount, lightAmount, lightAmount);
                        GX_TexCoord2u8(UV_SCALE * TEXTURE_X(texture_index) + UV_SCALE, (UV_SCALE * TEXTURE_Y(texture_index) + UV_SCALE));
                    }
                }
            }
        }
        GX_End();
    }
    if (triaVertexCount)
    {
        int newTriaVertexCount;
        if ((newTriaVertexCount = render_fluid_mesh(section, transparent, triaVertexCount)) != triaVertexCount)
        {
            printf("Invalid vtx count while rendering water: expected %d, got %d", triaVertexCount, newTriaVertexCount);
            isExiting = true;
        }
    }
    // GX_EndDispList() returns the size of the display list, so store that value and use it with GX_CallDispList().
    int preciseMemory = GX_EndDispList();
    if (!preciseMemory)
    {
        printf("Failed to create display list for section %d at (%d, %d)\n", section, this->x, this->z);
        printf("Size should be %d\n", estimatedMemory);
        UNLOCK_RETURN(2);
    }
    if (preciseMemory != estimatedMemory)
    {
        if (abs(preciseMemory - estimatedMemory) >= 64)
        {
            printf("Sizes differ too much: %d != %d, diff=%d\n", preciseMemory, estimatedMemory, preciseMemory - estimatedMemory);
            UNLOCK_RETURN(2);
        }
    }

    if (transparent)
    {
        this->vbos[section].transparent_buffer = displist_vbo;
        this->vbos[section].transparent_buffer_length = preciseMemory;
    }
    else
    {
        this->vbos[section].solid_buffer = displist_vbo;
        this->vbos[section].solid_buffer_length = preciseMemory;
    }
    // printf("Took %dms\n", time_diff_ms(time_start, time_get()));
    // printf("Face count: %d, Memory: %d", faceCount, preciseMemory);
    // free(displist_vbo);
    UNLOCK_RETURN(0);
}

int chunk_t::pre_render_fluid_mesh(int section, bool transparent)
{
    int vertexCount = 0;
    vec3i chunk_offset = vec3i{this->x << 4, section << 4, this->z << 4};

    for (size_t i = 0; i < water_levels.size(); i++)
        water_levels[i].clear();

    for (int _x = 0; _x < 16; _x++)
    {
        for (int _z = 0; _z < 16; _z++)
        {
            for (int _y = 0; _y < 16; _y++)
            {
                vec3i pos = vec3i{_x, _y, _z} + chunk_offset;
                block_t *block = get_block(_x, _y + chunk_offset.y, _z);
                if (block)
                {
                    BlockID block_id = block->get_blockid();
                    if (is_fluid(block_id) && transparent == (basefluid(block_id) == BlockID::water))
                        water_levels[get_fluid_meta_level(block)].push_back(pos);
                }
            }
        }
    }

    // For some god damn reason we have to precalculate
    // the mesh at least 2 times to have a stable vertex
    // count. This shall be fixed at some point!
    for (size_t l = 0; l < water_levels.size(); l++)
    {
        std::vector<vec3i> &levels = water_levels[water_levels.size() - 1 - l];
        // std::vector<vec3i> &levels = water_levels[l];
        for (size_t b = 0; b < levels.size(); b++)
        {
            vec3i blockpos = levels[b];
            block_t *block = get_block(blockpos.x, blockpos.y, blockpos.z);
            vertexCount += render_fluid(block, blockpos);
        }
    }
    return vertexCount;
}
int chunk_t::render_fluid_mesh(int section, bool transparent, int vertexCount)
{
    is_drawing = true;
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, vertexCount);
    vertexCount = 0;
    for (size_t l = 0; l < water_levels.size(); l++)
    {
        std::vector<vec3i> &levels = water_levels[water_levels.size() - 1 - l];
        // std::vector<vec3i> &levels = water_levels[l];
        for (size_t b = 0; b < levels.size(); b++)
        {
            vec3i blockpos = levels[b];
            block_t *block = get_block(blockpos.x, blockpos.y, blockpos.z);
            vertexCount += render_fluid(block, blockpos);
        }
    }
    GX_End();
    is_drawing = false;
    return vertexCount;
}

struct vertex_property_t
{
    vec3f pos;
    uint8_t x_uv;
    uint8_t y_uv;
    uint8_t index;
};

bool CompareVertices(const vertex_property_t &a, const vertex_property_t &b)
{
    return a.pos.y < b.pos.y;
}

inline void GX_Vertex(vertex_property_t vert, float light)
{
    if (!is_drawing)
        return;
    GX_Position3f32(vert.pos.x, vert.pos.y, vert.pos.z);
    GX_Color3f32(light, light, light);
    GX_TexCoord2u8(vert.x_uv, vert.y_uv);
}

int DrawTopSplit(vertex_property_t p0, vertex_property_t p1, vertex_property_t p2, vertex_property_t p3, float light)
{
    static std::vector<vertex_property_t> vertices(4);
    vertices[0] = p0;
    vertices[1] = p1;
    vertices[2] = p2;
    vertices[3] = p3;
    vertex_property_t min_vertex = *std::min_element(vertices.begin(), vertices.end(), CompareVertices);
    uint8_t curr = min_vertex.index;
    for (int i = 0; i < 3; i++)
    {
        GX_Vertex(vertices[curr], light);
        curr = (curr + 1) & 3;
    }
    curr = (min_vertex.index + 2) & 3;
    for (int i = 0; i < 3; i++)
    {
        GX_Vertex(vertices[curr], light);
        curr = (curr + 1) & 3;
    }
    return 2;
}

int DrawSideSplit(vertex_property_t p0, vertex_property_t p1, vertex_property_t p2, vertex_property_t p3, float light)
{
    static std::vector<vertex_property_t> vertices(4);
    vertices[0] = p0;
    vertices[1] = p1;
    vertices[2] = p2;
    vertices[3] = p3;
    int faceCount = 0;
    uint8_t curr = 0;
    if (p0.y_uv != p1.y_uv)
    {
        for (int i = 0; i < 3; i++)
        {
            GX_Vertex(vertices[curr], light);
            curr = (curr + 1) & 3;
        }
        faceCount++;
    }
    curr = 2;
    if (p2.y_uv != p3.y_uv)
    {
        for (int i = 0; i < 3; i++)
        {
            GX_Vertex(vertices[curr], light);
            curr = (curr + 1) & 3;
        }
        faceCount++;
    }
    return faceCount;
}

void get_neighbors(vec3i pos, block_t **neighbors)
{
    for (int x = 0; x < 6; x++)
        neighbors[x] = get_block_at(pos + face_offsets[x]);
}

int chunk_t::render_fluid(block_t *block, vec3i pos)
{
    BlockID block_id = block->get_blockid();

    int faceCount = 0;
    vec3i local_pos = {pos.x & 0xF, pos.y & 0xF, pos.z & 0xF};
    uint8_t fluid_level = get_fluid_meta_level(block);

    // Used to check block types around the fluid
    block_t *neighbors[6];
    BlockID neighbor_ids[6];

    // Sorted maximum and minimum values of the corners above
    uint8_t corner_min[4];
    uint8_t corner_max[4];

    // These determine the placement of the quad
    float corner_bottoms[4];
    float corner_tops[4];

    get_neighbors(pos, neighbors);
    for (int x = 0; x < 6; x++)
    {
        neighbor_ids[x] = neighbors[x] ? neighbors[x]->get_blockid() : BlockID::air;
    }

    vec3i corner_offsets[4] = {
        {0, 0, 0},
        {0, 0, 1},
        {1, 0, 1},
        {1, 0, 0},
    };
    for (int i = 0; i < 4; i++)
    {
        corner_max[i] = get_fluid_visual_level(pos + corner_offsets[i], block_id);
        corner_min[i] = 0;
        corner_tops[i] = float(corner_max[i]) / 8.0f;
        corner_bottoms[i] = 0.0f;
    }

    float light = std::min(1.0f, float(block->get_blocklight()) / 15.0f + 0.05f);

    // TEXTURE HANDLING: Check surrounding water levels > this:
    // If surrounded by 1, the water texture is flowing to the opposite direction
    // If surrounded by 2 in I-shaped pattern, the water texture is still
    // If surrounded by 2 diagonally, the water texture is flowing diagonally to the other direction
    // If surrounded by 3, the water texture is flowing to the 1 other direction
    // If surrounded by 4, the water texture is still

    const uint8_t texture_offsets[5] = {
        205, 206, 207, 222, 223};
    uint8_t texture_offset = texture_offsets[0];
    uint8_t count = 0;
    uint8_t bitmask = 0;

    for (int i = 0, j = 0; i < 6; i++)
    {
        if (i == 2 || i == 3)
            continue;
        if (get_fluid_meta_level(neighbors[i]) < fluid_level)
        {
            bitmask |= (1 << j);
            count++;
            switch (count)
            {
            case 1:
                texture_offset = texture_offsets[1 + (j >> 1)];
                break;
            case 2:
                texture_offset = texture_offsets[1 + (((!(bitmask & 3)) != (!(bitmask & 12))) << 1)];
                break;
            case 3:
                texture_offset = texture_offsets[1 + ((!(bitmask & 1)) != (!(bitmask & 2)))];
                break;
            case 4:
                texture_offset = texture_offsets[0];
                break;

            default:
                break;
            }
        }
        j++;
    }
    uint8_t texture_start_x = TEXTURE_X(texture_offset) * UV_SCALE;
    uint8_t texture_start_y = TEXTURE_Y(texture_offset) * UV_SCALE;
    uint8_t texture_end_x = texture_start_x + UV_SCALE;
    uint8_t texture_end_y = texture_start_y + UV_SCALE;

    vertex_property_t bottomPlaneCoords[4] = {
        {(local_pos + vec3f{-.5f, -.5f, -.5f}), texture_start_x, texture_end_y},
        {(local_pos + vec3f{-.5f, -.5f, +.5f}), texture_start_x, texture_start_y},
        {(local_pos + vec3f{+.5f, -.5f, +.5f}), texture_end_x, texture_start_y},
        {(local_pos + vec3f{+.5f, -.5f, -.5f}), texture_end_x, texture_end_y},
    };
    if (!is_same_fluid(block_id, neighbor_ids[FACE_NY]))
        faceCount += DrawTopSplit(bottomPlaneCoords[0], bottomPlaneCoords[1], bottomPlaneCoords[2], bottomPlaneCoords[3], light);

    vertex_property_t topPlaneCoords[4] = {
        {(local_pos + vec3f{+.5f, -.5f + corner_tops[3], -.5f}), texture_end_x, texture_end_y},
        {(local_pos + vec3f{+.5f, -.5f + corner_tops[2], +.5f}), texture_end_x, texture_start_y},
        {(local_pos + vec3f{-.5f, -.5f + corner_tops[1], +.5f}), texture_start_x, texture_start_y},
        {(local_pos + vec3f{-.5f, -.5f + corner_tops[0], -.5f}), texture_start_x, texture_end_y},
    };
    if (!is_same_fluid(block_id, neighbor_ids[FACE_PY]))
        faceCount += DrawTopSplit(topPlaneCoords[0], topPlaneCoords[1], topPlaneCoords[2], topPlaneCoords[3], light);

    texture_offset = basefluid(block_id) == BlockID::water ? WATER_SIDE : LAVA_SIDE;

    vertex_property_t sideCoords[4] = {0};

    texture_start_x = TEXTURE_X(texture_offset) * UV_SCALE;
    texture_end_x = texture_start_x + UV_SCALE;
    texture_start_y = TEXTURE_Y(texture_offset) * UV_SCALE;

    sideCoords[0].x_uv = sideCoords[1].x_uv = texture_start_x;
    sideCoords[2].x_uv = sideCoords[3].x_uv = texture_end_x;
    float facelight = 0.05f;
    if (neighbors[FACE_NZ])
        facelight = std::min(1.0f, neighbors[FACE_NZ]->get_blocklight() / 15.0f + 0.05f);
    sideCoords[3].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[0], -.5f};
    sideCoords[2].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[0], -.5f};
    sideCoords[1].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[3], -.5f};
    sideCoords[0].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[3], -.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[0];
    sideCoords[2].y_uv = texture_start_y + corner_max[0];
    sideCoords[1].y_uv = texture_start_y + corner_max[3];
    sideCoords[0].y_uv = texture_start_y + corner_min[3];
    if (!is_same_fluid(block_id, neighbor_ids[FACE_NZ]))
        faceCount += DrawSideSplit(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], facelight * 0.9f);

    facelight = 0.05f;
    if (neighbors[FACE_PZ])
        facelight = std::min(1.0f, neighbors[FACE_PZ]->get_blocklight() / 15.0f + 0.05f);
    sideCoords[3].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[2], +.5f};
    sideCoords[2].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[2], +.5f};
    sideCoords[1].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[1], +.5f};
    sideCoords[0].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[1], +.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[2];
    sideCoords[2].y_uv = texture_start_y + corner_max[2];
    sideCoords[1].y_uv = texture_start_y + corner_max[1];
    sideCoords[0].y_uv = texture_start_y + corner_min[1];
    if (!is_same_fluid(block_id, neighbor_ids[FACE_PZ]))
        faceCount += DrawSideSplit(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], facelight * 0.9f);

    facelight = 0.05f;
    if (neighbors[FACE_NX])
        facelight = std::min(1.0f, neighbors[FACE_NX]->get_blocklight() / 15.0f + 0.05f);
    sideCoords[3].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[1], +.5f};
    sideCoords[2].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[1], +.5f};
    sideCoords[1].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[0], -.5f};
    sideCoords[0].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[0], -.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[1];
    sideCoords[2].y_uv = texture_start_y + corner_max[1];
    sideCoords[1].y_uv = texture_start_y + corner_max[0];
    sideCoords[0].y_uv = texture_start_y + corner_min[0];
    if (!is_same_fluid(block_id, neighbor_ids[FACE_NX]))
        faceCount += DrawSideSplit(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], facelight * 0.85f);

    facelight = 0.05f;
    if (neighbors[FACE_PX])
        facelight = std::min(1.0f, neighbors[FACE_PX]->get_blocklight() / 15.0f + 0.05f);
    sideCoords[3].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[3], -.5f};
    sideCoords[2].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[3], -.5f};
    sideCoords[1].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[2], +.5f};
    sideCoords[0].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[2], +.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[3];
    sideCoords[2].y_uv = texture_start_y + corner_max[3];
    sideCoords[1].y_uv = texture_start_y + corner_max[2];
    sideCoords[0].y_uv = texture_start_y + corner_min[2];
    if (!is_same_fluid(block_id, neighbor_ids[FACE_PX]))
        faceCount += DrawSideSplit(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], facelight * 0.85f);
    return faceCount * 3;
}

void chunk_t::prepare_render()
{
    this->recalculate();
    this->build_all_vbos();
}

// destroy chunk
void destroy_chunk(chunk_t *chunk)
{
    for (int i = 0; i < VERTICAL_SECTION_COUNT; i++)
    {
        chunk->destroy_vbo(i, VBO_ALL);
    }
}