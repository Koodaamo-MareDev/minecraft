#include "mcregion.hpp"
#include <fstream>

uint32_t open_file_count = 0;
/**
 * Allocates the first available block of size `size` for chunk at index `index`.
 * @param size The size of the block to allocate.
 * @param index The index of the chunk to allocate the block for - used for reallocation.
 */
uint32_t mcr::Region::allocate(uint32_t size, uint16_t index)
{
    // If the size is greater than 1MB, return 0 as an error.
    if (size > 0x100000)
    {
        throw std::runtime_error("Chunk size too large");
    }

    // Convert the size to size in 4KB blocks.
    size = (size + 4095) >> 12;

    // Check if the existing block is large enough.
    if ((locations[index] >> 8) >= 2 && (locations[index] & 0xFF) >= size)
    {
        locations[index] = (locations[index] & ~0xFF) | size;
        return locations[index] >> 8;
    }

    // Search for a free block by getting the space between the current block and the next block.
    // First block is always at 2.
    uint32_t result_block = 0;
    uint32_t prev_block_end = 2;
    for (uint32_t i = 0; i < 1024; i++)
    {
        uint32_t block_start = (locations[i] >> 8);

        // Check if block is in use
        if (block_start < 2)
            continue;

        // Check if there is enough space between the previous block and the current block.
        if (block_start - prev_block_end >= size)
        {
            result_block = prev_block_end;

            for (uint32_t j = 0; j < 1024; j++)
            {
                uint32_t check_start = (locations[j] >> 8);
                uint32_t check_end = check_start + (locations[j] & 0xFF);
                if ((check_start >= result_block && check_start < result_block + size) || (check_end >= result_block && check_end < result_block + size))
                {
                    result_block = 0;
                    break;
                }
            }

            if (result_block != 0)
                break;
        }
        // Update the previous block end.
        prev_block_end = std::max(block_start + (locations[i] & 0xFF), prev_block_end);
    }

    // If no block was found, allocate at the end of the file.
    if (result_block == 0)
    {
        if (!file.is_open())
            open();
        result_block = (file.seekp(0, std::ios::end).tellp() + 4095LL) >> 12;

        // If the file is empty, start at block 2.
        if (result_block < 2)
        {
            result_block = 2;
        }
    }

    for (uint32_t i = 0; i < 1024; i++)
    {
        if (i == index)
            continue;
        if ((locations[i] >> 8) == result_block)
        {
            throw std::runtime_error("Block " + std::to_string(result_block) + " already allocated by " + std::to_string(i) + " but " + std::to_string(index) + " wants it.");
        }
    }

    // Update the location.
    locations[index] = (result_block << 8) | size;

    return result_block;
}

/**
 * Opens the region file for reading and writing.
 */
std::fstream &mcr::Region::open()
{
    // Return the file if it is already open.
    if (file.is_open())
    {
        return file;
    }

    while (open_file_count >= MAX_OPEN_FILES)
    {
        close_redundant_region(this);
    }

    std::string region_path = "region/r." + std::to_string(x) + "." + std::to_string(z) + ".mcr";

    // Attempt to open the file in read/write mode.
    file.open(region_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open())
    {
        // Create the file if it does not exist.
        std::ofstream create_file(region_path);
        create_file.close();

        // Reopen in read/write mode
        file.open(region_path, std::ios::in | std::ios::out | std::ios::binary);
    }
    if (file.is_open())
    {
        open_file_count++;
    }

    return file;
}

/**
 * Closes the region file.
 */
void mcr::Region::close()
{
    // Close the file if it is open.
    if (file.is_open())
    {
        file.close();
        if (open_file_count > 0)
            open_file_count--;
    }
}

bool mcr::Region::is_open()
{
    return file.is_open();
}

/**
 * Returns a reference to the deque of regions.
 */
std::deque<mcr::Region *> &mcr::get_regions()
{
    static std::deque<mcr::Region *> regions;
    return regions;
}

/**
 * Returns a region for the given x and z coordinates.
 * @param x The x coordinate of the region.
 * @param z The z coordinate of the region.
 */
mcr::Region *mcr::get_region(int32_t x, int32_t z)
{
    // Search for the region in the deque.
    for (mcr::Region *region : get_regions())
    {
        if (region->x == x && region->z == z)
        {
            return region;
        }
    }

    std::ifstream region_file("region/r." + std::to_string(x) + "." + std::to_string(z) + ".mcr", std::ios::binary);

    // If the file does not exist, create a new region.
    if (!region_file.is_open())
    {
        mcr::Region *new_region = new mcr::Region();
        new_region->x = x;
        new_region->z = z;
        get_regions().push_back(new_region);
        return new_region;
    }

    // Read the region file.
    mcr::Region *region = new mcr::Region();
    region->x = x;
    region->z = z;
    region_file.read(reinterpret_cast<char *>(region->locations), sizeof(region->locations));
    region_file.read(reinterpret_cast<char *>(region->last_modified), sizeof(region->last_modified));
    region_file.close();
    get_regions().push_back(region);

    return region;
}

void mcr::close_redundant_region(mcr::Region *exclude)
{
    // Close the first region that is not the excluded region.
    for (mcr::Region *region : get_regions())
    {
        if (region != exclude && region->is_open())
        {
            region->close();
            return;
        }
    }
}

void mcr::cleanup()
{
    for (mcr::Region *&region : mcr::get_regions())
    {
        region->close();
        delete region;
    }
    mcr::get_regions().clear();
}
