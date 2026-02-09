#include "chunk.hpp"
#include <block/blocks.hpp>
#include <registry/textures.hpp>
#include <math/vec2i.hpp>
#include <math/vec3i.hpp>
#include <util/timers.hpp>
#include <world/light.hpp>
#include <render/base3d.hpp>
#include <render/render.hpp>
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
#include <world/util/coord.hpp>
#include <world/tile_entity/tile_entity.hpp>
#include <util/debuglog.hpp>
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

TileEntity *Chunk::get_tile_entity(const Vec3i &position)
{
    for (size_t i = 0; i < tile_entities.size(); i++)
    {
        TileEntity *tile_entity = tile_entities[i];
        if (tile_entity->pos == position)
            return tile_entity;
    }
    return nullptr;
}

void Chunk::remove_tile_entity(TileEntity *entity)
{
    if (!entity)
        return;
    if (entity->chunk != this)
        return;
    if (auto it = std::find(tile_entities.begin(), tile_entities.end(), entity); it != tile_entities.end())
        tile_entities.erase(it);
    delete entity;
}

void Chunk::update_height_map(Vec3i pos)
{
    height_map[(pos.x & 0xF) | ((pos.z & 0xF) << 4)] = skycast_fast(Vec3i(pos.x, 0, pos.z), this) + 1;
}

void Chunk::light_up()
{
    if (!this->lit_state)
    {
        std::memset(height_map, MAX_WORLD_Y, 256);
        for (int i = 0; i < 256; i++)
        {
            update_height_map(Vec3i(i & 15, 0, (i >> 4) & 15));
            Block *low_block = &blockstates[i];
            Block *high_block = &low_block[MAX_WORLD_Y << 8];
            Block *max_height_block = &low_block[int(height_map[i]) << 8];
            for (Block *block = high_block; block >= max_height_block; block -= 256)
            {
                block->sky_light = 15;
            }

            // Update top-most block under daylight
            Vec3i pos = Vec3i((i & 15) | (this->x << 4), height_map[i], ((i >> 4) & 15) | (this->z << 4));
            LightEngine::post(pos);

            // Update block lights
            pos.y = 0;
            for (Block *block = low_block; block < max_height_block; block += 256, pos.y++)
            {
                if (get_block_luminance(block->blockid))
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
    std::memset(height_map, MAX_WORLD_Y, 256);
    for (int x = 0; x < 16; x++)
        for (int z = 0; z < 16; z++)
        {
            update_height_map(Vec3i(x, 0, z));
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

        if (other_block->id != block->id || !properties(block->id).m_transparent || (!render_fast_leaves && block->id == BlockID::leaves))
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

void Chunk::tick_tile_entities()
{
    for (size_t i = 0; i < tile_entities.size(); i++)
    {
        TileEntity *tile_entity = tile_entities[i];
        tile_entity->tick(world);
    }
}

void Chunk::render_entities(float partial_ticks, bool transparency)
{
    gertex::GXState state = gertex::get_state();
    for (EntityPhysical *&entity : entities)
    {
        // Make sure the entity is valid
        if (!entity)
            continue;
        // Skip rendering dead entities
        if (entity->dead)
            continue;
        entity->render(partial_ticks, transparency);

        // Restore the previous state after rendering the entity
        gertex::set_state(state);
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
    for (TileEntity *&tile_entity : tile_entities)
    {
        if (!tile_entity)
            continue;
        delete tile_entity;
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

    // Save tile entities
    NBTTagList *tile_entities_list = (NBTTagList *)compound.setTag("TileEntities", new NBTTagList);
    for (size_t i = 0; i < tile_entities.size(); i++)
    {
        tile_entities_list->addTag(tile_entities[i]->serialize());
    }
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

        blockstates[index1].id = (blocks[in_index]);
        blockstates[index2].id = (blocks[in_index | 1]);

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

    // Load tile entities
    NBTTagList *tile_entities_list = stream.getList("TileEntities");
    if (tile_entities_list && tile_entities_list->tagType == NBTBase::TAG_Compound)
    {
        for (size_t i = 0; i < tile_entities_list->value.size(); i++)
        {
            NBTTagCompound *compound = (NBTTagCompound *)tile_entities_list->getTag(i);
            try
            {
                TileEntity *tile_entity = TileEntity::load(compound, this);
                tile_entities.push_back(tile_entity);
            }
            catch (std::exception &e)
            {
                debug::print("Skipping TileEntity: %s", e.what());
            }
        }
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
        this->state = ChunkState::empty;
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
        this->state = ChunkState::empty;
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
        this->state = ChunkState::empty;

        // Clean up
        delete compound;
        return;
    }
    delete compound;

    lit_state = 1;
    state = ChunkState::done;

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
            world->schedule_block_update(block_pos, block->blockid, 0);
    }

    // Mark chunk as dirty
    for (int vbo_index = 0; vbo_index < VERTICAL_SECTION_COUNT; vbo_index++)
    {
        sections[vbo_index].dirty = true;
    }
}
