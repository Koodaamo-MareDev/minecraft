#include "chunk_manager.hpp"
#include <world/chunk.hpp>
#include <world/world.hpp>
#include <world/chunkprovider.hpp>
#include <sys/unistd.h>
#include <util/debuglog.hpp>
#include <util/lock.hpp>

void *chunk_manager_thread(ChunkManager *chunk_manager)
{
    chunk_manager->update_loop();
    return nullptr;
}

void ChunkManager::start(World *world)
{
    this->world = world;
    thread_active = true;
    worker.submit(chunk_manager_thread, this);
}

void ChunkManager::stop()
{
    thread_active = false;
}

bool ChunkManager::active()
{
    return thread_active;
}

void ChunkManager::update_loop()
{
    while (thread_active)
    {
        if (world->pending_chunks.empty())
        {
            usleep(1000);
            continue;
        }
        Chunk *chunk = world->pending_chunks.back();
        switch (chunk->state)
        {
        case ChunkState::loading:
        {
            // Generate the base terrain for the chunk
            world->chunk_provider->provide_chunk(chunk);

            // Move the chunk to the active list
            world->chunks.push_back(chunk);
            world->pending_chunks.erase(std::find(world->pending_chunks.begin(), world->pending_chunks.end(), chunk));
            uint64_t key = uint32_pair(chunk->x, chunk->z);
            world->chunk_cache.insert_or_assign(key, chunk);
            break;
        }
        case ChunkState::empty:
        {
            // Generate the base terrain for the chunk
            world->chunk_provider->provide_chunk(chunk);
        }
        // Fall to the next case immediately: features
        case ChunkState::features:
        {
            // Move the chunk to the active list
            world->chunks.push_back(chunk);
            world->pending_chunks.erase(std::find(world->pending_chunks.begin(), world->pending_chunks.end(), chunk));
            uint64_t key = uint32_pair(chunk->x, chunk->z);
            world->chunk_cache.insert_or_assign(key, chunk);

            // Finish the chunk with features
            world->chunk_provider->populate_chunk(chunk);
            break;
        }
        case ChunkState::saving:
        {
            try
            {
                chunk->write();
            }
            catch (std::runtime_error &e)
            {
                debug::print("Failed to save chunk: %s\n", e.what());
            }
        }
        // Fall to removal case after saving
        case ChunkState::invalid:
        {
            // Lock chunk_lock(world->chunk_mutex);
            world->pending_chunks.erase(std::find(world->pending_chunks.begin(), world->pending_chunks.end(), chunk));
            delete chunk;
            break;
        }
        default:
        {
            // Empty the queue to avoid a rare deadlock
            if (!thread_active)
            {
                world->pending_chunks.erase(std::find(world->pending_chunks.begin(), world->pending_chunks.end(), chunk));
                delete chunk;
            }
            break;
        }
        }
        usleep(100);
    }
}
