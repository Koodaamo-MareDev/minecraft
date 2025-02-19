#ifndef NBT_HPP
#define NBT_HPP

#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include <iostream>
#include <bit>
#include "../crapper/client.hpp"    // For ByteBuffer
#include "../crapper/miniz/miniz.h" // For compression and decompression (RFC 1950 and RFC 1952)

// Gzip format constants for compression and decompression
constexpr size_t GZIP_HEADER_SIZE = 10;
constexpr size_t GZIP_FOOTER_SIZE = 8;
constexpr uint8_t GZIP_HEADER[GZIP_HEADER_SIZE] = {0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03};

class NBTTagCompound;

class NBTBase
{
public:
    std::string name = "";

    virtual uint8_t getType() = 0;
    static NBTBase *createTag(uint8_t type);
    static NBTBase *copyTag(NBTBase *tag);

    void writeTag(Crapper::ByteBuffer &buffer);
    void writeTag(std::ostream &stream);
    virtual void writeContents(Crapper::ByteBuffer &buffer) = 0;

    static NBTBase *readTag(Crapper::ByteBuffer &buffer);
    static NBTBase *readTag(std::istream &stream);
    virtual void readContents(Crapper::ByteBuffer &buffer) = 0;

    static NBTTagCompound *readGZip(Crapper::ByteBuffer &buffer);
    static NBTTagCompound *readGZip(std::istream &stream, uint32_t len);
    static void writeGZip(Crapper::ByteBuffer &buffer, NBTTagCompound *compound);
    static void writeGZip(std::ostream &stream, NBTTagCompound *compound);

    static NBTTagCompound *readZlib(Crapper::ByteBuffer &buffer);
    static NBTTagCompound *readZlib(std::istream &stream, uint32_t len);
    static void writeZlib(Crapper::ByteBuffer &buffer, NBTTagCompound *compound);
    static void writeZlib(std::ostream &stream, NBTTagCompound *compound);

    virtual ~NBTBase() {}
};

class NBTTagEnd : public NBTBase
{
public:
    uint8_t getType() override
    {
        return 0;
    }

    void writeContents(Crapper::ByteBuffer &buffer) override {}

    void readContents(Crapper::ByteBuffer &buffer) override {}
};

class NBTTagByte : public NBTBase
{
public:
    int8_t value = 0;

    NBTTagByte(int8_t value = 0) : value(value) {}

    uint8_t getType() override
    {
        return 1;
    }

    void writeContents(Crapper::ByteBuffer &buffer) override;

    void readContents(Crapper::ByteBuffer &buffer) override;
};

class NBTTagShort : public NBTBase
{
public:
    int16_t value = 0;

    NBTTagShort(int16_t value = 0) : value(value) {}

    uint8_t getType() override
    {
        return 2;
    }

    void writeContents(Crapper::ByteBuffer &buffer) override;

    void readContents(Crapper::ByteBuffer &buffer) override;
};

class NBTTagInt : public NBTBase
{
public:
    int32_t value = 0;

    NBTTagInt(int32_t value = 0) : value(value) {}

    uint8_t getType() override
    {
        return 3;
    }

    void writeContents(Crapper::ByteBuffer &buffer) override;

    void readContents(Crapper::ByteBuffer &buffer) override;
};

class NBTTagLong : public NBTBase
{
public:
    int64_t value = 0;

    NBTTagLong(int64_t value = 0) : value(value) {}

    uint8_t getType() override
    {
        return 4;
    }

    void writeContents(Crapper::ByteBuffer &buffer) override;

    void readContents(Crapper::ByteBuffer &buffer) override;
};

class NBTTagFloat : public NBTBase
{
public:
    float value = 0;

    NBTTagFloat(float value = 0) : value(value) {}

    uint8_t getType() override
    {
        return 5;
    }

    void writeContents(Crapper::ByteBuffer &buffer) override;

    void readContents(Crapper::ByteBuffer &buffer) override;
};

class NBTTagDouble : public NBTBase
{
public:
    double value = 0;

    NBTTagDouble(double value = 0) : value(value) {}

    uint8_t getType() override
    {
        return 6;
    }

    void writeContents(Crapper::ByteBuffer &buffer) override;

    void readContents(Crapper::ByteBuffer &buffer) override;
};

class NBTTagByteArray : public NBTBase
{
public:
    std::vector<int8_t> value;

    NBTTagByteArray(std::vector<int8_t> value = {}) : value(value) {}
    NBTTagByteArray(std::vector<uint8_t> value)
    {
        this->value.resize(value.size());
        std::memcpy(this->value.data(), value.data(), value.size());
    };

    uint8_t getType() override
    {
        return 7;
    }

    void writeContents(Crapper::ByteBuffer &buffer) override;

    void readContents(Crapper::ByteBuffer &buffer) override;
};

class NBTTagString : public NBTBase
{
public:
    std::string value = "";

    NBTTagString(std::string value = "") : value(value) {}

    uint8_t getType() override
    {
        return 8;
    }

    void writeContents(Crapper::ByteBuffer &buffer) override;

    void readContents(Crapper::ByteBuffer &buffer) override;
};

class NBTTagList : public NBTBase
{
public:
    uint8_t tagType = 0;
    std::vector<NBTBase *> value;

    uint8_t getType() override
    {
        return 9;
    }

    NBTBase *addTag(NBTBase *tag);

    NBTBase *getTag(uint32_t index);

    void writeContents(Crapper::ByteBuffer &buffer) override;

    void readContents(Crapper::ByteBuffer &buffer) override;

    ~NBTTagList()
    {
        for (NBTBase *tag : value)
        {
            if (tag)
                delete tag;
        }
    }
};

class NBTTagCompound : public NBTBase
{
public:
    std::map<std::string, NBTBase *> value;

    uint8_t getType() override
    {
        return 10;
    }

    NBTBase *setTag(std::string key, NBTBase *tag);

    NBTBase *getTag(std::string key);

    void writeContents(Crapper::ByteBuffer &buffer) override;

    void readContents(Crapper::ByteBuffer &buffer) override;

    ~NBTTagCompound()
    {
        for (auto &pair : value)
        {
            if (pair.second)
                delete pair.second;
        }
    }
};

#endif // NBT_HPP