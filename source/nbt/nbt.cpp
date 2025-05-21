#include "nbt.hpp"

uint32_t byteswap(uint32_t x)
{
    return ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) | ((x & 0xFF0000) >> 8) | ((x & 0xFF000000) >> 24);
}

NBTBase *NBTBase::createTag(uint8_t type)
{
    switch (type)
    {
    case NBTBase::TAG_End:
        return new NBTTagEnd;
    case NBTBase::TAG_Byte:
        return new NBTTagByte;
    case NBTBase::TAG_Short:
        return new NBTTagShort;
    case NBTBase::TAG_Int:
        return new NBTTagInt;
    case NBTBase::TAG_Long:
        return new NBTTagLong;
    case NBTBase::TAG_Float:
        return new NBTTagFloat;
    case NBTBase::TAG_Double:
        return new NBTTagDouble;
    case NBTBase::TAG_Byte_Array:
        return new NBTTagByteArray;
    case NBTBase::TAG_String:
        return new NBTTagString;
    case NBTBase::TAG_List:
        return new NBTTagList;
    case NBTBase::TAG_Compound:
        return new NBTTagCompound;
    }
    return nullptr;
}

NBTBase *NBTBase::copyTag(NBTBase *tag)
{
    uint8_t type = tag->getType();
    NBTBase *newTag = createTag(type);
    switch (type)
    {
    case NBTBase::TAG_Byte:
        *(NBTTagByte *)newTag = *(NBTTagByte *)tag;
        break;
    case NBTBase::TAG_Short:
        *(NBTTagShort *)newTag = *(NBTTagShort *)tag;
        break;
    case NBTBase::TAG_Int:
        *(NBTTagInt *)newTag = *(NBTTagInt *)tag;
        break;
    case NBTBase::TAG_Long:
        *(NBTTagLong *)newTag = *(NBTTagLong *)tag;
        break;
    case NBTBase::TAG_Float:
        *(NBTTagFloat *)newTag = *(NBTTagFloat *)tag;
        break;
    case NBTBase::TAG_Double:
        *(NBTTagDouble *)newTag = *(NBTTagDouble *)tag;
        break;
    case NBTBase::TAG_Byte_Array:
        *(NBTTagByteArray *)newTag = *(NBTTagByteArray *)tag;
        break;
    case NBTBase::TAG_String:
        *(NBTTagString *)newTag = *(NBTTagString *)tag;
        break;
    case NBTBase::TAG_List:
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
    case NBTBase::TAG_Compound:
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

    if (!newTag)
        throw std::runtime_error("Failed to copy tag (is the game running low on memory?)");

    newTag->name = tag->name;

    return newTag;
}

void NBTBase::writeTag(ByteBuffer &buffer)
{
    uint8_t type = getType();
    buffer.writeByte(type);
    if (type == NBTBase::TAG_End)
        return;
    buffer.writeString(name);
    writeContents(buffer);
}

void NBTBase::writeTag(std::ostream &stream)
{
    ByteBuffer buffer;
    writeTag(buffer);
    stream.write(reinterpret_cast<const char *>(buffer.ptr()), buffer.size());
}

NBTBase *NBTBase::readTag(ByteBuffer &buffer)
{
    uint8_t type = buffer.readByte();
    if (buffer.underflow)
        throw std::runtime_error("Failed to read tag type");
    NBTBase *tag = createTag(type);
    if (!tag)
        throw std::runtime_error("Failed to create tag");
    if (type == NBTBase::TAG_End)
        return tag;
    tag->name = buffer.readString();
    tag->readContents(buffer);
    return tag;
}

NBTBase *NBTBase::readTag(std::istream &stream)
{
    // Read all bytes from stream
    ByteBuffer buffer;
    buffer.reallocateWith(new std::vector<uint8_t>((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>()));

    return readTag(buffer);
}

NBTTagCompound *NBTBase::readGZip(ByteBuffer &buffer)
{
    ByteBuffer output_buffer;
    try
    {
        mz_ulong decompressed_size = 512 * 1024;
        output_buffer.resize(decompressed_size); // 512 KB buffer - should be enough for most NBT data

        // Skip the GZIP header and footer
        size_t ret = tinfl_decompress_mem_to_mem(output_buffer.ptr(), decompressed_size, buffer.ptr() + GZIP_HEADER_SIZE, buffer.size() - GZIP_HEADER_SIZE - GZIP_FOOTER_SIZE, 0);

        if (ret != TINFL_DECOMPRESS_MEM_TO_MEM_FAILED)
        {
            decompressed_size = ret;
            output_buffer.resize(decompressed_size);
            NBTBase *base = NBTBase::readTag(output_buffer);
            if (!base)
                throw std::runtime_error("Failed to read tag");
            if (base->getType() != NBTBase::TAG_Compound)
            {
                delete base;
                throw std::runtime_error("Root tag is not a compound tag");
            }
            
            return (NBTTagCompound *)base;
        }
        else
        {
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
    ByteBuffer buffer;
    buffer.resize(len);
    stream.read(reinterpret_cast<char *>(buffer.ptr()), len);

    // Decompress the GZIP file
    return readGZip(buffer);
}

void NBTBase::writeGZip(ByteBuffer &buffer, NBTTagCompound *compound)
{
    ByteBuffer output_buffer;
    compound->writeTag(output_buffer);
    try
    {
        uint8_t *compressed_data = new uint8_t[output_buffer.size()];
        mz_ulong compressed_size = output_buffer.size();
        size_t ret = tdefl_compress_mem_to_mem(compressed_data, compressed_size, output_buffer.ptr(), output_buffer.size(), TDEFL_DEFAULT_MAX_PROBES);

        if (ret != TINFL_DECOMPRESS_MEM_TO_MEM_FAILED)
        {
            compressed_size = ret;

            // Write the GZIP header
            buffer.writeBytes(GZIP_HEADER, GZIP_HEADER_SIZE);

            // Write the compressed data
            buffer.writeBytes(compressed_data, compressed_size);

            // Calculate the CRC32 checksum
            uint32_t crc = mz_crc32(0, output_buffer.ptr(), output_buffer.size());

            // Write the CRC32 checksum
            buffer.writeInt(byteswap(crc));

            // Write the uncompressed size
            buffer.writeInt(byteswap(output_buffer.size()));

            // Free the temporary buffers
            output_buffer.clear();
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
    ByteBuffer buffer;
    writeGZip(buffer, compound);

    // Write Gzip stream
    stream.write(reinterpret_cast<const char *>(buffer.ptr()), buffer.size());
}

NBTTagCompound *NBTBase::readZlib(ByteBuffer &buffer)
{
    ByteBuffer output_buffer;
    try
    {
        mz_ulong decompressed_size = 512 * 1024;
        output_buffer.resize(decompressed_size); // 512 KB buffer - should be enough for most NBT data
        int ret = mz_uncompress(output_buffer.ptr(), &decompressed_size, buffer.ptr(), buffer.size());

        if (ret == MZ_BUF_ERROR)
        {
            decompressed_size = 1024 * 1024;
            output_buffer.resize(decompressed_size);
            ret = mz_uncompress(output_buffer.ptr(), &decompressed_size, buffer.ptr(), buffer.size());
        }
        if (ret == MZ_OK)
        {
            output_buffer.resize(decompressed_size);
            NBTBase *base = NBTBase::readTag(output_buffer);
            if (!base)
                throw std::runtime_error("Failed to read tag");
            if (base->getType() != NBTBase::TAG_Compound)
            {
                delete base;
                throw std::runtime_error("Root tag is not a compound tag");
            }

            return (NBTTagCompound *)base;
        }
        else
        {
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
    ByteBuffer buffer;
    buffer.resize(len);
    stream.read(reinterpret_cast<char *>(buffer.ptr()), len);
    return readZlib(buffer);
}

void NBTBase::writeZlib(ByteBuffer &buffer, NBTTagCompound *compound)
{
    ByteBuffer output_buffer;
    compound->writeTag(output_buffer);
    try
    {
        uint8_t *compressed_data = new uint8_t[output_buffer.size()];
        mz_ulong compressed_size = output_buffer.size();
        int ret = mz_compress2(compressed_data, &compressed_size, output_buffer.ptr(), output_buffer.size(), MZ_BEST_SPEED);

        // We can free the output buffer now - we don't need it anymore and the less memory we use at once, the better
        output_buffer.clear();

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
    ByteBuffer buffer;
    compound->writeTag(buffer);
    try
    {
        uint8_t *compressed_data = new uint8_t[buffer.size()];
        mz_ulong compressed_size = buffer.size();
        int ret = mz_compress2(compressed_data, &compressed_size, buffer.ptr(), buffer.size(), MZ_BEST_SPEED);

        // We can free the output buffer now - we don't need it anymore and the less memory we use at once, the better
        buffer.clear();

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

void NBTTagByte::writeContents(ByteBuffer &buffer)
{
    buffer.writeByte(value);
}

void NBTTagByte::readContents(ByteBuffer &buffer)
{
    value = buffer.readByte();
}

void NBTTagShort::writeContents(ByteBuffer &buffer)
{
    buffer.writeShort(value);
}

void NBTTagShort::readContents(ByteBuffer &buffer)
{
    value = buffer.readShort();
}

void NBTTagInt::writeContents(ByteBuffer &buffer)
{
    buffer.writeInt(value);
}

void NBTTagInt::readContents(ByteBuffer &buffer)
{
    value = buffer.readInt();
}

void NBTTagLong::writeContents(ByteBuffer &buffer)
{
    buffer.writeLong(value);
}

void NBTTagLong::readContents(ByteBuffer &buffer)
{
    value = buffer.readLong();
}

void NBTTagFloat::writeContents(ByteBuffer &buffer)
{
    buffer.writeFloat(value);
}

void NBTTagFloat::readContents(ByteBuffer &buffer)
{
    value = buffer.readFloat();
}

void NBTTagDouble::writeContents(ByteBuffer &buffer)
{
    buffer.writeDouble(value);
}

void NBTTagDouble::readContents(ByteBuffer &buffer)
{
    value = buffer.readDouble();
}

void NBTTagByteArray::writeContents(ByteBuffer &buffer)
{
    buffer.writeInt(value.size() & 0x7FFFFFFF);
    buffer.writeBytes((uint8_t *)value.data(), value.size());
}

void NBTTagByteArray::readContents(ByteBuffer &buffer)
{
    int32_t length = buffer.readInt() & 0x7FFFFFFF;
    value.resize(length);
    buffer.readBytes((uint8_t *)value.data(), length);
}

void NBTTagString::writeContents(ByteBuffer &buffer)
{
    buffer.writeString(value);
}

void NBTTagString::readContents(ByteBuffer &buffer)
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

void NBTTagList::writeContents(ByteBuffer &buffer)
{
    buffer.writeByte(tagType);
    buffer.writeInt(value.size() & 0x7FFFFFFF);
    for (NBTBase *tag : value)
    {
        tag->writeContents(buffer);
    }
}

void NBTTagList::readContents(ByteBuffer &buffer)
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

int8_t NBTTagCompound::getByte(std::string key)
{
    NBTBase *tag = getTag(key);
    if (tag && tag->getType() == NBTBase::TAG_Byte)
        return ((NBTTagByte *)tag)->value;
    return 0;
}

int16_t NBTTagCompound::getShort(std::string key)
{
    NBTBase *tag = getTag(key);
    if (tag && tag->getType() == NBTBase::TAG_Short)
        return ((NBTTagShort *)tag)->value;
    return 0;
}

int32_t NBTTagCompound::getInt(std::string key)
{
    NBTBase *tag = getTag(key);
    if (tag && tag->getType() == NBTBase::TAG_Int)
        return ((NBTTagInt *)tag)->value;
    return 0;
}

int64_t NBTTagCompound::getLong(std::string key)
{
    NBTBase *tag = getTag(key);
    if (tag && tag->getType() == NBTBase::TAG_Long)
        return ((NBTTagLong *)tag)->value;
    return 0;
}

float NBTTagCompound::getFloat(std::string key)
{
    NBTBase *tag = getTag(key);
    if (tag && tag->getType() == NBTBase::TAG_Float)
        return ((NBTTagFloat *)tag)->value;
    return 0;
}

double NBTTagCompound::getDouble(std::string key)
{
    NBTBase *tag = getTag(key);
    if (tag && tag->getType() == NBTBase::TAG_Double)
        return ((NBTTagDouble *)tag)->value;
    return 0;
}

std::vector<int8_t> NBTTagCompound::getByteArray(std::string key)
{
    NBTBase *tag = getTag(key);
    if (tag && tag->getType() == NBTBase::TAG_Byte_Array)
        return ((NBTTagByteArray *)tag)->value;
    return {};
}

std::vector<uint8_t> NBTTagCompound::getUByteArray(std::string key)
{
    NBTBase *tag = getTag(key);
    if (tag && tag->getType() == NBTBase::TAG_Byte_Array)
    {
        std::vector<int8_t> &value = ((NBTTagByteArray *)tag)->value;
        return std::vector<uint8_t>(value.begin(), value.end());
    }
    return {};
}

std::string NBTTagCompound::getString(std::string key)
{
    NBTBase *tag = getTag(key);
    if (tag && tag->getType() == NBTBase::TAG_String)
        return ((NBTTagString *)tag)->value;
    return "";
}

NBTTagList *NBTTagCompound::getList(std::string key)
{
    NBTBase *tag = getTag(key);
    if (tag && tag->getType() == NBTBase::TAG_List)
        return (NBTTagList *)tag;
    return nullptr;
}

NBTTagCompound *NBTTagCompound::getCompound(std::string key)
{
    NBTBase *tag = getTag(key);
    if (tag && tag->getType() == NBTBase::TAG_Compound)
        return (NBTTagCompound *)tag;
    return nullptr;
}

void NBTTagCompound::writeContents(ByteBuffer &buffer)
{
    for (auto &&pair : value)
    {
        pair.second->writeTag(buffer);
    }
    buffer.writeByte(0);
}

void NBTTagCompound::readContents(ByteBuffer &buffer)
{
    while (true)
    {
        NBTBase *tag = NBTBase::readTag(buffer);
        if (tag->getType() == NBTBase::TAG_End)
        {
            delete tag;
            break;
        }
        value[tag->name] = tag;
    }
}