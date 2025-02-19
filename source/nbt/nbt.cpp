#include "nbt.hpp"

uint32_t byteswap(uint32_t x)
{
    return ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) | ((x & 0xFF0000) >> 8) | ((x & 0xFF000000) >> 24);
}

NBTBase *NBTBase::createTag(uint8_t type)
{
    switch (type)
    {
    case 0:
        return new NBTTagEnd();
    case 1:
        return new NBTTagByte();
    case 2:
        return new NBTTagShort();
    case 3:
        return new NBTTagInt();
    case 4:
        return new NBTTagLong();
    case 5:
        return new NBTTagFloat();
    case 6:
        return new NBTTagDouble();
    case 7:
        return new NBTTagByteArray();
    case 8:
        return new NBTTagString();
    case 9:
        return new NBTTagList();
    case 10:
        return new NBTTagCompound();
    }
    return nullptr;
}

NBTBase *NBTBase::copyTag(NBTBase *tag)
{
    uint8_t type = tag->getType();
    NBTBase *newTag = createTag(type);
    switch (type)
    {
    case 1:
        *(NBTTagByte *)newTag = *(NBTTagByte *)tag;
        break;
    case 2:
        *(NBTTagShort *)newTag = *(NBTTagShort *)tag;
        break;
    case 3:
        *(NBTTagInt *)newTag = *(NBTTagInt *)tag;
        break;
    case 4:
        *(NBTTagLong *)newTag = *(NBTTagLong *)tag;
        break;
    case 5:
        *(NBTTagFloat *)newTag = *(NBTTagFloat *)tag;
        break;
    case 6:
        *(NBTTagDouble *)newTag = *(NBTTagDouble *)tag;
        break;
    case 7:
        *(NBTTagByteArray *)newTag = *(NBTTagByteArray *)tag;
        break;
    case 8:
        *(NBTTagString *)newTag = *(NBTTagString *)tag;
        break;
    case 9:
    {
        NBTTagList *oldList = ((NBTTagList *)tag);
        NBTTagList *newList = ((NBTTagList *)newTag);
        newList->name = oldList->name;
        newList->tagType = oldList->tagType;
        // Deep copy the list
        for (auto &value : oldList->value)
        {
            newList->value.push_back(copyTag(value));
        }
        break;
    }
    case 10:
    {
        NBTTagCompound *oldCompound = ((NBTTagCompound *)tag);
        NBTTagCompound *newCompound = ((NBTTagCompound *)newTag);
        newCompound->name = oldCompound->name;
        // Deep copy the map
        for (auto &pair : oldCompound->value)
        {
            newCompound->value[pair.first] = copyTag(pair.second);
        }
        break;
    }
    }

    return newTag;
}

void NBTBase::writeTag(Crapper::ByteBuffer &buffer)
{
    uint8_t type = getType();
    buffer.writeByte(type);
    if (type == 0)
        return;
    buffer.writeString(name);
    writeContents(buffer);
}

void NBTBase::writeTag(std::ostream &stream)
{
    Crapper::ByteBuffer buffer;
    writeTag(buffer);
    stream.write(reinterpret_cast<const char *>(buffer.data.data()), buffer.data.size());
}

NBTBase *NBTBase::readTag(Crapper::ByteBuffer &buffer)
{
    uint8_t type = buffer.readByte();
    if (type == 0)
        return new NBTTagEnd();
    NBTBase *tag = createTag(type);
    tag->name = buffer.readString();
    tag->readContents(buffer);
    return tag;
}

NBTBase *NBTBase::readTag(std::istream &stream)
{
    // Read all bytes from stream
    Crapper::ByteBuffer buffer;
    buffer.data = std::vector<uint8_t>((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    return readTag(buffer);
}

NBTTagCompound *NBTBase::readGZip(Crapper::ByteBuffer &buffer)
{
    Crapper::ByteBuffer output_buffer;
    try
    {
        uint8_t *decompressed_data = new uint8_t[512 * 1024]; // 512 KB buffer - should be enough for most NBT data
        mz_ulong decompressed_size = output_buffer.data.size();

        // Skip the GZIP header and footer
        size_t ret = tinfl_decompress_mem_to_mem(decompressed_data, decompressed_size, buffer.data.data() + GZIP_HEADER_SIZE, buffer.data.size() - GZIP_HEADER_SIZE - GZIP_FOOTER_SIZE, 0);

        if (ret != TINFL_DECOMPRESS_MEM_TO_MEM_FAILED)
        {
            decompressed_size = ret;
            buffer.writeBytes(decompressed_data, decompressed_size);
            delete[] decompressed_data;
            NBTTagCompound *compound = new NBTTagCompound();
            compound->readTag(buffer);
            return compound;
        }
        else
        {
            delete[] decompressed_data;
            throw std::runtime_error("Failed to decompress data");
        }
    }
    catch (std::bad_alloc &e)
    {
        throw std::runtime_error("Failed to allocate memory for compression");
    }
}

NBTTagCompound *NBTBase::readGZip(std::istream &stream, uint32_t len)
{
    // Read entire file into memory
    Crapper::ByteBuffer buffer;
    buffer.data.resize(len);
    stream.read(reinterpret_cast<char *>(buffer.data.data()), len);

    // Decompress the GZIP file
    return readGZip(buffer);
}

void NBTBase::writeGZip(Crapper::ByteBuffer &buffer, NBTTagCompound *compound)
{
    Crapper::ByteBuffer output_buffer;
    compound->writeTag(output_buffer);
    try
    {
        uint8_t *compressed_data = new uint8_t[output_buffer.data.size()];
        mz_ulong compressed_size = output_buffer.data.size();
        size_t ret = tdefl_compress_mem_to_mem(compressed_data, compressed_size, output_buffer.data.data(), output_buffer.data.size(), TDEFL_DEFAULT_MAX_PROBES);

        if (ret != TINFL_DECOMPRESS_MEM_TO_MEM_FAILED)
        {
            compressed_size = ret;

            // Write the GZIP header
            buffer.writeBytes(GZIP_HEADER, GZIP_HEADER_SIZE);

            // Write the compressed data
            buffer.writeBytes(compressed_data, compressed_size);

            // Calculate the CRC32 checksum
            uint32_t crc = mz_crc32(0, output_buffer.data.data(), output_buffer.data.size());

            // Write the CRC32 checksum
            buffer.writeInt(byteswap(crc));

            // Write the uncompressed size
            buffer.writeInt(byteswap(output_buffer.data.size()));

            // Free the temporary buffers
            output_buffer.data.clear();
            delete[] compressed_data;
        }
        else
        {
            delete[] compressed_data;
            throw std::runtime_error("Failed to compress data");
        }
    }
    catch (std::bad_alloc &e)
    {
        throw std::runtime_error("Failed to allocate memory for compression");
    }
}

void NBTBase::writeGZip(std::ostream &stream, NBTTagCompound *compound)
{
    Crapper::ByteBuffer buffer;
    writeGZip(buffer, compound);

    // Write Gzip stream
    stream.write(reinterpret_cast<const char *>(buffer.data.data()), buffer.data.size());
}

NBTTagCompound *NBTBase::readZlib(Crapper::ByteBuffer &buffer)
{
    Crapper::ByteBuffer output_buffer;
    try
    {
        uint8_t *decompressed_data = new uint8_t[512 * 1024]; // 512 KB buffer - should be enough for most NBT data
        mz_ulong decompressed_size = output_buffer.data.size();
        int ret = mz_uncompress(decompressed_data, &decompressed_size, buffer.data.data(), buffer.data.size());

        if (ret == MZ_OK)
        {
            buffer.writeBytes(decompressed_data, decompressed_size);
        }
        else if (ret == MZ_BUF_ERROR)
        {
            delete[] decompressed_data;
            decompressed_data = new uint8_t[1024 * 1024]; // 1 MB buffer at most - otherwise just give up
            ret = mz_uncompress(decompressed_data, &decompressed_size, buffer.data.data(), buffer.data.size());
        }
        if (ret == MZ_OK)
        {
            output_buffer.writeBytes(decompressed_data, decompressed_size);
            delete[] decompressed_data;
            NBTTagCompound *compound = new NBTTagCompound();
            compound->readTag(output_buffer);
            return compound;
        }
        else
        {
            delete[] decompressed_data;
            throw std::runtime_error("Failed to decompress data");
        }
    }
    catch (std::bad_alloc &e)
    {
        throw std::runtime_error("Failed to allocate memory for compression");
    }
}

NBTTagCompound *NBTBase::readZlib(std::istream &stream, uint32_t len)
{
    Crapper::ByteBuffer buffer;
    buffer.data.resize(len);
    stream.read(reinterpret_cast<char *>(buffer.data.data()), len);
    return readZlib(buffer);
}

void NBTBase::writeZlib(Crapper::ByteBuffer &buffer, NBTTagCompound *compound)
{
    Crapper::ByteBuffer output_buffer;
    compound->writeTag(output_buffer);
    try
    {
        uint8_t *compressed_data = new uint8_t[output_buffer.data.size()];
        mz_ulong compressed_size = output_buffer.data.size();
        int ret = mz_compress2(compressed_data, &compressed_size, output_buffer.data.data(), output_buffer.data.size(), MZ_BEST_SPEED);

        // We can free the output buffer now - we don't need it anymore and the less memory we use at once, the better
        output_buffer.data.clear();

        if (ret == MZ_OK)
        {
            buffer.writeBytes(compressed_data, compressed_size);
        }
        else
        {
            throw std::runtime_error("Failed to compress data");
        }

        delete[] compressed_data;
    }
    catch (std::bad_alloc &e)
    {
        throw std::runtime_error("Failed to allocate memory for compression");
    }
}

void NBTBase::writeZlib(std::ostream &stream, NBTTagCompound *compound)
{
    Crapper::ByteBuffer buffer;
    compound->writeTag(buffer);
    try
    {
        uint8_t *compressed_data = new uint8_t[buffer.data.size()];
        mz_ulong compressed_size = buffer.data.size();
        int ret = mz_compress2(compressed_data, &compressed_size, buffer.data.data(), buffer.data.size(), MZ_BEST_SPEED);

        // We can free the output buffer now - we don't need it anymore and the less memory we use at once, the better
        buffer.data.clear();

        if (ret == MZ_OK)
        {
            stream.write(reinterpret_cast<const char *>(compressed_data), compressed_size);
        }
        else
        {
            throw std::runtime_error("Failed to compress data");
        }

        delete[] compressed_data;
    }
    catch (std::bad_alloc &e)
    {
        throw std::runtime_error("Failed to allocate memory for compression");
    }
}

void NBTTagByte::writeContents(Crapper::ByteBuffer &buffer)
{
    buffer.writeByte(value);
}

void NBTTagByte::readContents(Crapper::ByteBuffer &buffer)
{
    value = buffer.readByte();
}

void NBTTagShort::writeContents(Crapper::ByteBuffer &buffer)
{
    buffer.writeShort(value);
}

void NBTTagShort::readContents(Crapper::ByteBuffer &buffer)
{
    value = buffer.readShort();
}

void NBTTagInt::writeContents(Crapper::ByteBuffer &buffer)
{
    buffer.writeInt(value);
}

void NBTTagInt::readContents(Crapper::ByteBuffer &buffer)
{
    value = buffer.readInt();
}

void NBTTagLong::writeContents(Crapper::ByteBuffer &buffer)
{
    buffer.writeLong(value);
}

void NBTTagLong::readContents(Crapper::ByteBuffer &buffer)
{
    value = buffer.readLong();
}

void NBTTagFloat::writeContents(Crapper::ByteBuffer &buffer)
{
    buffer.writeFloat(value);
}

void NBTTagFloat::readContents(Crapper::ByteBuffer &buffer)
{
    value = buffer.readFloat();
}

void NBTTagDouble::writeContents(Crapper::ByteBuffer &buffer)
{
    buffer.writeDouble(value);
}

void NBTTagDouble::readContents(Crapper::ByteBuffer &buffer)
{
    value = buffer.readDouble();
}

void NBTTagByteArray::writeContents(Crapper::ByteBuffer &buffer)
{
    buffer.writeInt(value.size() & 0x7FFFFFFF);
    buffer.writeBytes((uint8_t *)value.data(), value.size());
}

void NBTTagByteArray::readContents(Crapper::ByteBuffer &buffer)
{
    int32_t length = buffer.readInt() & 0x7FFFFFFF;
    value.resize(length);
    buffer.readBytes((uint8_t *)value.data(), length);
}

void NBTTagString::writeContents(Crapper::ByteBuffer &buffer)
{
    buffer.writeString(value);
}

void NBTTagString::readContents(Crapper::ByteBuffer &buffer)
{
    value = buffer.readString();
}

// NOTE: Deletes the input tag and returns the new tag
NBTBase *NBTTagList::addTag(NBTBase *tag)
{
    // Copy the tag
    NBTBase *newTag = NBTBase::copyTag(tag);
    value.push_back(newTag);
    tagType = newTag->getType();

    // Delete the input tag
    delete tag;

    return newTag;
}

NBTBase *NBTTagList::getTag(uint32_t index)
{
    if (index >= value.size())
        return nullptr;
    return value[index];
}

void NBTTagList::writeContents(Crapper::ByteBuffer &buffer)
{
    buffer.writeByte(tagType);
    buffer.writeInt(value.size() & 0x7FFFFFFF);
    for (NBTBase *tag : value)
    {
        tag->writeContents(buffer);
    }
}

void NBTTagList::readContents(Crapper::ByteBuffer &buffer)
{
    tagType = buffer.readByte();
    int32_t length = buffer.readInt() & 0x7FFFFFFF;
    for (int i = 0; i < length; i++)
    {
        NBTBase *tag = NBTBase::createTag(tagType);
        tag->readContents(buffer);
        value.push_back(tag);
    }
}

// NOTE: Deletes the input tag and returns the new tag
NBTBase *NBTTagCompound::setTag(std::string key, NBTBase *tag)
{
    // Copy the tag
    NBTBase *newTag = NBTBase::copyTag(tag);
    newTag->name = key;

    // Delete the old tag if it exists
    auto it = value.find(key);
    if (it != value.end())
        delete it->second;

    // Assign the new tag
    value[key] = newTag;

    // Delete the input tag
    delete tag;

    return newTag;
}

NBTBase *NBTTagCompound::getTag(std::string key)
{
    auto it = value.find(key);
    if (it == value.end())
        return nullptr;
    return it->second;
}

void NBTTagCompound::writeContents(Crapper::ByteBuffer &buffer)
{
    for (auto &&pair : value)
    {
        pair.second->writeTag(buffer);
    }
    buffer.writeByte(0);
}

void NBTTagCompound::readContents(Crapper::ByteBuffer &buffer)
{
    while (true)
    {
        NBTBase *tag = NBTBase::readTag(buffer);
        if (tag->getType() == 0)
        {
            delete tag;
            break;
        }
        value[tag->name] = tag;
    }
}