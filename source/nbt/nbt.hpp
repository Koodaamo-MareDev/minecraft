#ifndef NBT_HPP
#define NBT_HPP

#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include <iostream>
#include <bit>
#include <ported/ByteBuffer.hpp>
#include <miniz/miniz.h> // For compression and decompression (RFC 1950 and RFC 1952)

// Gzip format constants for compression and decompression
constexpr size_t GZIP_HEADER_SIZE = 10;
constexpr size_t GZIP_FOOTER_SIZE = 8;
constexpr uint8_t GZIP_HEADER[GZIP_HEADER_SIZE] = {0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03};

class NBTTagCompound;

class NBTBase
{
protected:
    NBTBase() {}

public:
    static const uint8_t TAG_End = 0;
    static const uint8_t TAG_Byte = 1;
    static const uint8_t TAG_Short = 2;
    static const uint8_t TAG_Int = 3;
    static const uint8_t TAG_Long = 4;
    static const uint8_t TAG_Float = 5;
    static const uint8_t TAG_Double = 6;
    static const uint8_t TAG_Byte_Array = 7;
    static const uint8_t TAG_String = 8;
    static const uint8_t TAG_List = 9;
    static const uint8_t TAG_Compound = 10;

    std::string name = "";

    virtual uint8_t getType() = 0;
    static NBTBase *createTag(uint8_t type);
    static NBTBase *copyTag(NBTBase *tag);

    void writeTag(ByteBuffer &buffer);
    void writeTag(std::ostream &stream);
    virtual void writeContents(ByteBuffer &buffer) = 0;

    static NBTBase *readTag(ByteBuffer &buffer);
    static NBTBase *readTag(std::istream &stream);
    virtual void readContents(ByteBuffer &buffer) = 0;

    static NBTTagCompound *readGZip(ByteBuffer &buffer);
    static NBTTagCompound *readGZip(std::istream &stream, uint32_t len);
    static void writeGZip(ByteBuffer &buffer, NBTTagCompound *compound);
    static void writeGZip(std::ostream &stream, NBTTagCompound *compound);

    static NBTTagCompound *readZlib(ByteBuffer &buffer);
    static NBTTagCompound *readZlib(std::istream &stream, uint32_t len);
    static void writeZlib(ByteBuffer &buffer, NBTTagCompound *compound);
    static void writeZlib(std::ostream &stream, NBTTagCompound *compound);

    virtual ~NBTBase() {}
};

class NBTTagEnd : public NBTBase
{
public:
    uint8_t getType() override
    {
        return NBTBase::TAG_End;
    }

    void writeContents(ByteBuffer &buffer) override {}

    void readContents(ByteBuffer &buffer) override {}
};

class NBTTagByte : public NBTBase
{
public:
    int8_t value = 0;

    NBTTagByte(int8_t value = 0) : value(value) {}

    uint8_t getType() override
    {
        return NBTBase::TAG_Byte;
    }

    void writeContents(ByteBuffer &buffer) override;

    void readContents(ByteBuffer &buffer) override;
};

class NBTTagShort : public NBTBase
{
public:
    int16_t value = 0;

    NBTTagShort(int16_t value = 0) : value(value) {}

    uint8_t getType() override
    {
        return NBTBase::TAG_Short;
    }

    void writeContents(ByteBuffer &buffer) override;

    void readContents(ByteBuffer &buffer) override;
};

class NBTTagInt : public NBTBase
{
public:
    int32_t value = 0;

    NBTTagInt(int32_t value = 0) : value(value) {}

    uint8_t getType() override
    {
        return NBTBase::TAG_Int;
    }

    void writeContents(ByteBuffer &buffer) override;

    void readContents(ByteBuffer &buffer) override;
};

class NBTTagLong : public NBTBase
{
public:
    int64_t value = 0;

    NBTTagLong(int64_t value = 0) : value(value) {}

    uint8_t getType() override
    {
        return NBTBase::TAG_Long;
    }

    void writeContents(ByteBuffer &buffer) override;

    void readContents(ByteBuffer &buffer) override;
};

class NBTTagFloat : public NBTBase
{
public:
    float value = 0;

    NBTTagFloat(float value = 0) : value(value) {}

    uint8_t getType() override
    {
        return NBTBase::TAG_Float;
    }

    void writeContents(ByteBuffer &buffer) override;

    void readContents(ByteBuffer &buffer) override;
};

class NBTTagDouble : public NBTBase
{
public:
    double value = 0;

    NBTTagDouble(double value = 0) : value(value) {}

    uint8_t getType() override
    {
        return NBTBase::TAG_Double;
    }

    void writeContents(ByteBuffer &buffer) override;

    void readContents(ByteBuffer &buffer) override;
};

class NBTTagByteArray : public NBTBase
{
public:
    std::vector<int8_t> value;

    NBTTagByteArray(const std::vector<int8_t> &value = {}) : value(value) {}
    NBTTagByteArray(const std::vector<uint8_t> &value)
    {
        this->value.resize(value.size());
        std::memcpy(this->value.data(), value.data(), value.size());
    };

    uint8_t getType() override
    {
        return NBTBase::TAG_Byte_Array;
    }

    void writeContents(ByteBuffer &buffer) override;

    void readContents(ByteBuffer &buffer) override;
};

class NBTTagString : public NBTBase
{
public:
    std::string value = "";

    NBTTagString(std::string value = "") : value(value) {}

    uint8_t getType() override
    {
        return NBTBase::TAG_String;
    }

    void writeContents(ByteBuffer &buffer) override;

    void readContents(ByteBuffer &buffer) override;
};

class NBTTagList : public NBTBase
{
public:
    uint8_t tagType = 0;
    std::vector<NBTBase *> value;

    uint8_t getType() override
    {
        return NBTBase::TAG_List;
    }

    NBTBase *addTag(NBTBase *tag);

    NBTBase *getTag(uint32_t index);

    void writeContents(ByteBuffer &buffer) override;

    void readContents(ByteBuffer &buffer) override;

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
        return NBTBase::TAG_Compound;
    }

    NBTBase *setTag(std::string key, NBTBase *tag);

    NBTBase *getTag(std::string key);

    int8_t getByte(std::string key);

    int16_t getShort(std::string key);

    int32_t getInt(std::string key);

    int64_t getLong(std::string key);

    float getFloat(std::string key);

    double getDouble(std::string key);

    std::vector<int8_t> getByteArray(std::string key);

    std::vector<uint8_t> getUByteArray(std::string key);

    std::string getString(std::string key);

    NBTTagList *getList(std::string key);

    NBTTagCompound *getCompound(std::string key);

    void writeContents(ByteBuffer &buffer) override;

    void readContents(ByteBuffer &buffer) override;

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