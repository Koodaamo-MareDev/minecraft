#include "chunk.hpp"
#include <block/blocks.hpp>
#include <render/texturedefs.hpp>
#include <math/vec2i.hpp>
#include <math/vec3i.hpp>
#include <util/timers.hpp>
#include <world/light.hpp>
#include <render/base3d.hpp>
#include <render/render.hpp>
#include <render/render_blocks.hpp>
#include <render/render_fluids.hpp>
#include <world/util/raycast.hpp>
#include <util/lock.hpp>
#include <world/chunkprovider.hpp>
#include <render/model.hpp>
#include <tuple>
#include <map>
#include <vector>
#include <deque>
#include <algorithm>
#include <gccore.h>
#include <string.h>
#include <new>
#include <math.h>
#include <stdio.h>
#include <fstream>
#include <nbt/nbt.hpp>
#include <mcregion.hpp>
#include <world/world.hpp>
#include <util/face_pair.hpp>
const Vec3i face_offsets[] = {
    Vec3i{-1, +0, +0},  // Negative X
    Vec3i{+1, +0, +0},  // Positive X
    Vec3i{+0, -1, +0},  // Negative Y
    Vec3i{+0, +1, +0},  // Positive Y
    Vec3i{+0, +0, -1},  // Negative Z
    Vec3i{+0, +0, +1}}; // Positive Z

int32_t Chunk::player_taxicab_distance()
{
    EntityPlayerLocal &player = world->player;
    Vec3f pos = player.get_position(0);
    return std::abs((this->x << 4) - (int32_t(std::floor(pos.x)) & ~15)) + std::abs((this->z << 4) - (int32_t(std::floor(pos.z)) & ~15));
}

void Chunk::update_height_map(Vec3i pos)
{
    pos.x &= 15;
    pos.z &= 15;
    uint8_t *height = &height_map[pos.x | (pos.z << 4)];
    BlockID id = get_block(pos)->get_blockid();
    if (get_block_opacity(id))
    {
        if (pos.y >= *height)
            *height = pos.y + 1;
    }
    else
    {
        while (*height > 0)
        {
            pos.y = *height - 1;
            id = get_block(pos)->get_blockid();
            if (get_block_opacity(id))
                break;
            (*height)--;
        }
    }
}

void Chunk::light_up()
{
    if (!this->lit_state)
    {
        Coord pos = Coord(Vec3i(), this);
        for (int i = 0; i < 256; i++)
        {
            pos.coords.h_index = i;
            int end_y = skycast(Vec3i(pos), this);
            if (end_y >= MAX_WORLD_Y || end_y <= 0)
                return;
            this->height_map[i] = end_y + 1;
            for (pos.coords.y = MAX_WORLD_Y; pos.coords.y > end_y; pos.coords.y--)
            {
                this->blockstates[uint16_t(pos)].sky_light = 15;
            }

            // Update top-most block under daylight
            LightEngine::post(pos);

            // Update block lights
            for (pos.coords.y = 0; pos.coords.y < end_y; pos.coords.y++)
            {
                Block *block = &this->blockstates[uint16_t(pos)];
                if (get_block_luminance(block->get_blockid()))
                {
                    LightEngine::post(pos);
                }
            }
        }
        lit_state = 1;
    }
}

void Chunk::recalculate_height_map()
{
    Coord pos = Coord(Vec3i(), this);
    for (int i = 0; i < 256; i++)
    {
        pos.coords.h_index = i;
        int end_y = skycast(Vec3i(pos), this);
        if (end_y >= MAX_WORLD_Y || end_y <= 0)
            return;
        this->height_map[i] = end_y + 1;
    }
}

void Chunk::recalculate_visibility(Block *block, Vec3i pos)
{
    Block *neighbors[6];
    world->get_neighbors(pos, neighbors);
    uint8_t visibility = 0x40;
    for (int i = 0; i < 6; i++)
    {
        Block *other_block = neighbors[i];
        if (!other_block || !other_block->get_visibility())
        {
            visibility |= 1 << i;
            continue;
        }

        if (other_block->id != block->id || !properties(block->id).m_transparent)
        {
            RenderType other_rt = properties(other_block->id).m_render_type;
            visibility |= ((other_rt != RenderType::full && other_rt != RenderType::full_special) || properties(other_block->id).m_transparent) << i;
        }
    }
    block->visibility_flags = visibility;
}

// recalculates the blockstates of a section
void Chunk::refresh_section_block_visibility(int index)
{
    Vec3i chunk_pos(this->x * 16, index * 16, this->z * 16);

    Block *block = this->get_block(chunk_pos); // Gets the first block of the section
    for (int y = 0; y < 16; y++)
    {
        for (int z = 0; z < 16; z++)
        {
            for (int x = 0; x < 16; x++, block++)
            {
                if (!block->get_visibility())
                    continue;
                Vec3i pos = Vec3i(x, y, z);
                this->recalculate_visibility(block, chunk_pos + pos);
            }
        }
    }
}

// These variables are used for the flood fill algorithm
static std::vector<Vec3i> floodfill_start_points;
static uint32_t floodfill_counter = 0;
static uint32_t floodfill_to_replace = 1;
static uint32_t floodfill_flood_id = 0;
static uint16_t floodfill_grid[4096] = {0}; // 16 * 16 * 16
static uint8_t floodfill_faces_touched[6] = {0};

// Initialize the flood fill start points for visibility calculations
// This function generates the start points for the flood fill algorithm
void Chunk::init_floodfill_startpoints()
{
    floodfill_start_points.clear();
    constexpr int size = 16;
    constexpr int max = size - 1;
    // Generate faces at x = 0 and x = max
    for (int y = 0; y < size; ++y)
        for (int z = 0; z < size; ++z)
        {
            floodfill_start_points.push_back(Vec3i(0, y, z));   // x = 0
            floodfill_start_points.push_back(Vec3i(max, y, z)); // x = max
        }

    // Generate faces at y = 0 and y = max, skipping already added edges
    for (int x = 1; x < max; ++x)
        for (int z = 0; z < size; ++z)
        {
            floodfill_start_points.push_back(Vec3i(x, 0, z));   // y = 0
            floodfill_start_points.push_back(Vec3i(x, max, z)); // y = max
        }

    // Generate faces at z = 0 and z = max, skipping already added edges
    for (int x = 1; x < max; ++x)
        for (int y = 1; y < max; ++y)
        {
            floodfill_start_points.push_back(Vec3i(x, y, 0));   // z = 0
            floodfill_start_points.push_back(Vec3i(x, y, max)); // z = max
        }
}

void Chunk::vbo_visibility_flood_fill(Vec3i start_pos)
{
    std::deque<Vec3i> queue;
    queue.push_back(start_pos);

    while (!queue.empty())
    {
        Vec3i pos = queue.front();
        queue.pop_front();

        // Bounds check: If out of bounds, mark touched face and continue
        if (pos.x < 0)
        {
            floodfill_counter++;
            floodfill_faces_touched[0] = 1;
            continue;
        }
        if (pos.x > 15)
        {
            floodfill_counter++;
            floodfill_faces_touched[1] = 1;
            continue;
        }
        if (pos.y < 0)
        {
            floodfill_counter++;
            floodfill_faces_touched[2] = 1;
            continue;
        }
        if (pos.y > 15)
        {
            floodfill_counter++;
            floodfill_faces_touched[3] = 1;
            continue;
        }
        if (pos.z < 0)
        {
            floodfill_counter++;
            floodfill_faces_touched[4] = 1;
            continue;
        }
        if (pos.z > 15)
        {
            floodfill_counter++;
            floodfill_faces_touched[5] = 1;
            continue;
        }

        int index = pos.x | (pos.y << 8) | (pos.z << 4);
        if (floodfill_grid[index] != floodfill_to_replace)
            continue;

        // Mark the block as visited
        floodfill_grid[index] = floodfill_flood_id;

        // Add neighboring positions to the queue
        for (int i = 0; i < 6; i++)
        {
            queue.push_back(pos + face_offsets[i]);
        }
    }
}

void Chunk::refresh_section_visibility(int index)
{
    init_face_pairs();
    Section &vbo = this->sections[index];

    // Initialize the flood fill start points if not already initialized
    if (floodfill_start_points.empty())
        init_floodfill_startpoints();

    // Rebuild the flood fill grid
    Block *block = this->get_block(Vec3i(0, index << 4, 0));
    bool empty = true;
    for (uint32_t i = 0; i < 4096; i++, block++)
    {
        // Default to non-solid block
        floodfill_grid[i] = 1;

        // Skip air blocks
        if (!block->id)
            continue;

        BlockProperties prop = properties(block->id);

        if (prop.m_fluid || prop.m_transparent)
            continue;

        if (prop.m_render_type != RenderType::full && prop.m_render_type != RenderType::full_special)
            continue;

        // Mark the block as a solid block in the flood fill grid
        floodfill_grid[i] = 0;
        empty = false;
    }
    if (empty)
    {
        // No blocks to process, skip the flood fill
        vbo.visibility_flags = 0x7FFF;
        return;
    }
    vbo.visibility_flags = 0;
    floodfill_flood_id = 2;
    for (uint32_t i = 0; i < floodfill_start_points.size(); i++)
    {
        Vec3i pos = floodfill_start_points[i];

        // Reset the flood fill state
        floodfill_counter = 0;
        floodfill_to_replace = 1; // Reset the flood fill to replace value
        std::memset(floodfill_faces_touched, 0, sizeof(floodfill_faces_touched));

        // Start the flood fill from the current position
        vbo_visibility_flood_fill(pos);

        if (floodfill_counter > 1)
        {
            floodfill_flood_id++;

            // Connect the faces that were touched during the flood fill
            for (int j = 0; j < 6; j++)
                if (floodfill_faces_touched[j])
                    for (int k = 0; k < 6; k++)
                        if (j != k && floodfill_faces_touched[k])
                            vbo.visibility_flags |= face_pair_to_flag(j, k);
        }
        else
        {
            // Cancel the flood fill as it failed.
            floodfill_to_replace = floodfill_flood_id;
            floodfill_flood_id = 1;
            vbo_visibility_flood_fill(pos);
            floodfill_flood_id = floodfill_to_replace;
        }
    }
}

int Chunk::build_vbo(int index, bool transparent)
{
    Section &vbo = this->sections[index];
    static uint8_t vbo_buffer[64000 * VERTEX_ATTR_LENGTH] __attribute__((aligned(32)));
    DCInvalidateRange(vbo_buffer, sizeof(vbo_buffer));

    GX_BeginDispList(vbo_buffer, sizeof(vbo_buffer));

    // The first byte (GX_Begin command) is skipped, and after that the vertex count is stored as a uint16_t
    uint16_t *quadVtxCountPtr = (uint16_t *)(&vbo_buffer[1]);

    // Render the block mesh
    int quadVtxCount = render_section_blocks(*this, index, transparent, 64000U);

    // 3 bytes for the GX_Begin command, with 1 byte offset to access the vertex count (uint16_t)
    int offset = (quadVtxCount * VERTEX_ATTR_LENGTH) + 1 + 3;

    // The vertex count for the fluid mesh is stored as a uint16_t after the block mesh vertex data
    uint16_t *triVtxCountPtr = (uint16_t *)(&vbo_buffer[offset]);

    int triVtxCount = render_section_fluids(*this, index, transparent, 64000U);

    // End the display list - we don't need to store the size, as we can calculate it from the vertex counts
    uint32_t success = GX_EndDispList();

    if (!success)
    {
        printf("Failed to create display list for section %d at (%d, %d)\n", index, this->x, this->z);
        return (2);
    }
    if (transparent)
    {
        vbo.transparent.detach();
    }
    else
    {
        vbo.solid.detach();
    }

    if (!quadVtxCount && !triVtxCount)
    {
        return (0);
    }

    if (quadVtxCount)
    {
        // Store the vertex count for the block mesh
        quadVtxCountPtr[0] = quadVtxCount;
    }

    if (triVtxCount)
    {
        // Store the vertex count for the fluid mesh
        triVtxCountPtr[0] = triVtxCount;
    }
    uint32_t displist_size = (triVtxCount + quadVtxCount) * VERTEX_ATTR_LENGTH;
    if (quadVtxCount)
        displist_size += 3;
    if (triVtxCount)
        displist_size += 3;
    displist_size += 32;

    displist_size = (displist_size + 31) & ~31;

    uint8_t *displist_vbo = new (std::align_val_t(32), std::nothrow) uint8_t[displist_size];
    if (displist_vbo == nullptr)
    {
        printf("Failed to allocate %d bytes for section %d VBO at (%d, %d)\n", displist_size, index, this->x, this->z);
        return (1);
    }

    uint32_t pos = 0;
    // Copy the quad data
    if (quadVtxCount)
    {
        uint32_t quadSize = quadVtxCount * VERTEX_ATTR_LENGTH + 3;
        memcpy(displist_vbo, vbo_buffer, quadSize);
        pos += quadSize;
    }

    // Copy the triangle data
    if (triVtxCount)
    {
        uint32_t triSize = triVtxCount * VERTEX_ATTR_LENGTH + 3;
        memcpy((void *)((u32)displist_vbo + pos), (void *)((u32)vbo_buffer + (quadVtxCount * VERTEX_ATTR_LENGTH + 3)), triSize);
        pos += triSize;
    }

    // Set the rest of the buffer to 0
    memset(&displist_vbo[pos], 0, displist_size - pos);

    if (transparent)
    {
        vbo.transparent.buffer = displist_vbo;
        vbo.transparent.length = displist_size;
    }
    else
    {
        vbo.solid.buffer = displist_vbo;
        vbo.solid.length = displist_size;
    }
    DCFlushRange(displist_vbo, displist_size);
    return (0);
}

void Chunk::update_entities()
{
    if (!world->is_remote())
    {
        // Resolve collisions with current chunk and neighboring chunks' entities
        for (int i = 0; i <= 6; i++)
        {
            if (i == FACE_NY)
                i += 2;
            Chunk *neighbor = (i == 6 ? this : world->get_chunk(this->x + face_offsets[i].x, this->z + face_offsets[i].z));
            if (neighbor)
            {
                for (EntityPhysical *&entity : neighbor->entities)
                {
                    for (EntityPhysical *&this_entity : this->entities)
                    {
                        // Prevent entities from colliding with themselves
                        if (entity != this_entity && entity->collides(this_entity))
                        {
                            // Resolve the collision - always resolve from the perspective of the non-player entity
                            if (dynamic_cast<EntityPlayerLocal *>(this_entity))
                                entity->resolve_collision(this_entity);
                            else
                                this_entity->resolve_collision(entity);
                        }
                    }
                }
            }
        }
    }

    // Move entities to the correct chunk
    auto out_of_bounds_selector = [&](EntityPhysical *&entity)
    {
        Chunk *new_chunk = world->get_chunk_from_pos(entity->get_foot_blockpos());
        if (new_chunk != entity->chunk)
        {
            entity->chunk = new_chunk;
            if (new_chunk)
                new_chunk->entities.push_back(entity);
            return true;
        }
        return false;
    };

    // For any moved entities, remove them from the current chunk
    entities.erase(std::remove_if(entities.begin(), entities.end(), out_of_bounds_selector), entities.end());

    // When in multiplayer, the server will handle entity removal
    if (!world->is_remote())
    {
        auto remove_selector = [&](EntityPhysical *&entity)
        {
            if (entity->can_remove())
            {
                entity->chunk = nullptr;
                world->remove_entity(entity->entity_id);
                return true;
            }
            return false;
        };
        entities.erase(std::remove_if(entities.begin(), entities.end(), remove_selector), entities.end());
    }
}

void Chunk::render_entities(float partial_ticks, bool transparency)
{
    for (EntityPhysical *&entity : entities)
    {
        // Make sure the entity is valid
        if (!entity)
            continue;
        // Skip rendering dead entities
        if (entity->dead)
            continue;
        entity->render(partial_ticks, transparency);
    }

    // Restore default texture
    use_texture(terrain_texture);

    // Restore default colors
    gertex::set_color_mul(GXColor{255, 255, 255, 255});
    gertex::set_color_add(GXColor{0, 0, 0, 255});
}

uint32_t Chunk::size()
{
    uint32_t base_size = sizeof(Chunk);
    for (int i = 0; i < VERTICAL_SECTION_COUNT; i++)
        base_size += this->sections[i].cached_solid.length + this->sections[i].cached_transparent.length + this->sections[i].solid.length + this->sections[i].transparent.length;
    for (EntityPhysical *&entity : entities)
        base_size += entity->size();
    return base_size;
}

Chunk::~Chunk()
{
    for (EntityPhysical *&entity : entities)
    {
        if (!entity)
            continue;
        if (entity->chunk == this)
        {
            entity->chunk = nullptr;
            if (!world->is_remote())
            {
                // Remove the entity from the global entity list
                // NOTE: This will also delete the entity as its chunk is nullptr
                world->remove_entity(entity->entity_id);
            }
        }
    }
}

void Chunk::save(NBTTagCompound &compound)
{
    compound.setTag("xPos", new NBTTagInt(x));
    compound.setTag("zPos", new NBTTagInt(z));
    compound.setTag("LastUpdate", new NBTTagLong(world->ticks));

    std::vector<uint8_t> blocks = std::vector<uint8_t>(32768);
    std::vector<uint8_t> data = std::vector<uint8_t>(16384);
    std::vector<uint8_t> blocklight = std::vector<uint8_t>(16384);
    std::vector<uint8_t> skylight = std::vector<uint8_t>(16384);
    std::vector<uint8_t> heightmap = std::vector<uint8_t>(256);
    for (int i = 0; i < 16384; i++)
    {
        uint32_t out_index = i << 1;
        uint32_t iy = out_index & 0x7F;
        uint32_t iz = (out_index >> 7) & 0xF;
        uint32_t ix = (out_index >> 11) & 0xF;
        uint32_t index1 = ix | (iz << 4) | (iy << 8);
        uint32_t index2 = index1 | 0x100;

        blocks[out_index] = blockstates[index1].id;
        blocks[out_index | 1] = blockstates[index2].id;

        data[i] = (blockstates[index1].meta & 0xF) | ((blockstates[index2].meta & 0xF) << 4);
        blocklight[i] = (blockstates[index1].block_light) | (blockstates[index2].block_light << 4);
        skylight[i] = (blockstates[index1].sky_light) | (blockstates[index2].sky_light << 4);
    }
    for (int i = 0; i < 256; i++)
    {
        heightmap[i] = lightcast(Vec3i(i & 0xF, 0, i >> 4), this);
    }
    compound.setTag("Blocks", new NBTTagByteArray(blocks));
    compound.setTag("Data", new NBTTagByteArray(data));
    compound.setTag("BlockLight", new NBTTagByteArray(blocklight));
    compound.setTag("SkyLight", new NBTTagByteArray(skylight));
    compound.setTag("HeightMap", new NBTTagByteArray(heightmap));
    compound.setTag("Entities", new NBTTagList());
    compound.setTag("TileEntities", new NBTTagList());
}

void Chunk::load(NBTTagCompound &stream)
{
    std::vector<uint8_t> blocks = stream.getUByteArray("Blocks");
    std::vector<uint8_t> data = stream.getUByteArray("Data");
    std::vector<uint8_t> blocklight = stream.getUByteArray("BlockLight");
    std::vector<uint8_t> skylight = stream.getUByteArray("SkyLight");
    std::vector<uint8_t> heightmap = stream.getUByteArray("HeightMap");

    if (blocks.size() != 32768 || data.size() != 16384 || blocklight.size() != 16384 || skylight.size() != 16384 || heightmap.size() != 256)
    {
        throw std::runtime_error("Invalid chunk data");
    }

    for (int i = 0; i < 16384; i++)
    {
        uint32_t in_index = i << 1;
        uint32_t iy = in_index & 0x7F;
        uint32_t iz = (in_index >> 7) & 0xF;
        uint32_t ix = (in_index >> 11) & 0xF;
        uint32_t index1 = ix | (iz << 4) | (iy << 8);
        uint32_t index2 = index1 | 0x100;

        blockstates[index1].set_blockid((BlockID)blocks[in_index]);
        blockstates[index2].set_blockid((BlockID)blocks[in_index | 1]);

        blockstates[index1].meta = data[i] & 0xF;
        blockstates[index2].meta = (data[i] >> 4) & 0xF;

        blockstates[index1].block_light = blocklight[i] & 0xF;
        blockstates[index2].block_light = (blocklight[i] >> 4) & 0xF;

        blockstates[index1].sky_light = skylight[i] & 0xF;
        blockstates[index2].sky_light = (skylight[i] >> 4) & 0xF;
    }

    for (int i = 0; i < 256; i++)
    {
        height_map[i] = heightmap[i];
    }
}

void Chunk::write()
{
    // Write the chunk using mcregion format

    mcr::Region *region = mcr::get_region(x >> 5, z >> 5);
    if (!region)
    {
        throw std::runtime_error("Failed to get region for chunk " + std::to_string(x) + ", " + std::to_string(z));
    }

    std::fstream &chunk_file = region->open();

    if (!chunk_file.is_open())
    {
        throw std::runtime_error("Failed to open region file for writing");
    }

    // Save the chunk data to an NBT compound
    // Due to the way C++ manages memory, it's wise to fill in the compounds AFTER adding them to their parent
    NBTTagCompound root_compound;
    save(*(NBTTagCompound *)root_compound.setTag("Level", new NBTTagCompound));

    // Write the uncompressed compound to a buffer
    ByteBuffer uncompressed_buffer;
    root_compound.writeTag(uncompressed_buffer);

    // Compress the data
    uint32_t uncompressed_size = uncompressed_buffer.size();
    mz_ulong compressed_size = uncompressed_size;

    ByteBuffer buffer;
    buffer.resize(uncompressed_size);

    int result = mz_compress2(buffer.ptr(), &compressed_size, uncompressed_buffer.ptr(), uncompressed_size, MZ_BEST_SPEED);

    uncompressed_buffer.clear();

    buffer.resize(compressed_size);
    if (result != MZ_OK)
    {
        throw std::runtime_error("Failed to compress chunk data");
    }

    // Calculate the offset of the chunk in the file
    uint16_t offset = (x & 0x1F) | ((z & 0x1F) << 5);

    // Allocate the buffer in file
    uint32_t chunk_offset = region->allocate(buffer.size() + 5, offset);
    if (chunk_offset == 0)
    {
        throw std::runtime_error("Failed to allocate space for chunk");
    }

    // Write the header

    // Calculate the location of the chunk in the file
    uint32_t loc = (((buffer.size() + 5 + 4095) >> 12) & 0xFF) | (chunk_offset << 8);

    // Write the chunk location
    chunk_file.seekp(offset << 2);
    chunk_file.write(reinterpret_cast<char *>(&loc), sizeof(uint32_t));

    // Write the timestamp
    uint32_t timestamp = 1000; // TODO: Get the current time
    chunk_file.seekp((offset << 2) | 4096);
    chunk_file.write(reinterpret_cast<char *>(&timestamp), sizeof(uint32_t));

    // Get the length of the compressed data
    uint32_t length = compressed_size + 1;

    // Using zlib compression
    uint8_t compression = 2;

    // Write the chunk header
    chunk_file.seekp(chunk_offset << 12);
    chunk_file.write(reinterpret_cast<char *>(&length), sizeof(uint32_t));
    chunk_file.write(reinterpret_cast<char *>(&compression), sizeof(uint8_t));

    // Write the compressed data
    chunk_file.write(reinterpret_cast<char *>(buffer.ptr()), buffer.size());
    chunk_file.flush();

    uint32_t pos = chunk_file.seekg(0, std::ios::end).tellg();
    if ((pos & 0xFFF) != 0)
    {
        chunk_file.seekp(pos | 0xFFF);
        uint8_t padding = 0;
        chunk_file.write(reinterpret_cast<char *>(&padding), 1);
    }

    chunk_file.flush();
}

void Chunk::read()
{
    mcr::Region *region = mcr::get_region(x >> 5, z >> 5);

    // Calculate the offset of the chunk in the file
    uint16_t offset = (x & 0x1F) | ((z & 0x1F) << 5);

    // Check if the chunk is stored in the region
    if (region->locations[offset] == 0)
    {
        return;
    }

    // Get the location data
    uint32_t loc = region->locations[offset];

    // Calculate the offset of the chunk in the file
    uint32_t chunk_offset = loc >> 8;

    // Open the region file
    std::fstream &chunk_file = region->open();
    if (!chunk_file.is_open())
    {
        printf("Failed to open region file for reading\n");
        return;
    }

    // Read the chunk header
    chunk_file.seekg(chunk_offset << 12);
    uint32_t length;
    uint8_t compression;
    chunk_file.read(reinterpret_cast<char *>(&length), sizeof(uint32_t));
    chunk_file.read(reinterpret_cast<char *>(&compression), sizeof(uint8_t));

    if (length >= 0x100000) // 1MB
    {
        printf("Resetting chunk because its length is too large: %u\n", length);

        // Erase the chunk from the region
        region->locations[offset] = 0;

        // Regenerate the chunk
        this->generation_stage = ChunkGenStage::empty;
        return;
    }

    // Read the compressed data
    ByteBuffer buffer(length - 1);
    chunk_file.read(reinterpret_cast<char *>(buffer.ptr()), length - 1);

    NBTTagCompound *compound = nullptr;
    try
    {
        if (compression == 1)
        {
            // Read as gzip compressed data
            compound = NBTBase::readGZip(buffer);
        }
        else if (compression == 2)
        {
            // Read as zlib compressed data
            compound = NBTBase::readZlib(buffer);
        }
        else
        {
            throw std::runtime_error("Unsupported compression type");
        }
    }
    catch (std::runtime_error &e)
    {
        printf("Failed to read chunk data: %s\n", e.what());

        // Regenerate the chunk
        this->generation_stage = ChunkGenStage::empty;
        return;
    }

    try
    {
        // Load the chunk data
        NBTTagCompound *level = compound->getCompound("Level");
        if (!level)
        {
            throw std::runtime_error("Missing Level compound");
        }

        load(*level);
    }
    catch (std::runtime_error &e)
    {
        printf("Failed to load chunk data: %s\n", e.what());

        // Regenerate the chunk
        this->generation_stage = ChunkGenStage::empty;

        // Clean up
        delete compound;
        return;
    }
    delete compound;

    lit_state = 1;
    generation_stage = ChunkGenStage::done;

    Vec3i pos(this->x * 16, 0, this->z * 16);
    Lock lock(world->tick_mutex);
    for (int i = 0; i < 32768; i++)
    {
        Block *block = &this->blockstates[i];
        if (!block->id)
            continue;
        Vec3i block_pos = pos + Vec3i(i & 0xF, (i >> 8) & 0x7F, (i >> 4) & 0xF);
        BlockProperties &prop = properties(block->id);
        if (prop.m_tick_on_load)
            world->schedule_block_update(block_pos, block->get_blockid(), 0);
    }

    // Mark chunk as dirty
    for (int vbo_index = 0; vbo_index < VERTICAL_SECTION_COUNT; vbo_index++)
    {
        sections[vbo_index].dirty = true;
    }
}
