#ifndef BYTEBUFFER_HPP
#define BYTEBUFFER_HPP

#include <vector>
#include <cstdint>
#include <string>
#include <cstring>
#include <stdexcept>

class ByteBuffer
{
private:
    std::vector<uint8_t> *data;

public:
    size_t offset = 0;
    bool underflow = false;

    // Prevent copying
    ByteBuffer(const ByteBuffer &) = delete;
    ByteBuffer &operator=(const ByteBuffer &) = delete;
    ByteBuffer(ByteBuffer &&) = delete;
    ByteBuffer &operator=(ByteBuffer &&) = delete;

    ByteBuffer()
    {
        data = new std::vector<uint8_t>();
    }

    ByteBuffer(size_t size)
    {
        data = new std::vector<uint8_t>(size);
    }

    ~ByteBuffer()
    {
        delete data;
    }

    uint8_t *ptr() const
    {
        return data->data();
    }

    size_t size() const
    {
        return data->size();
    }

    void clear()
    {
        data->clear();
    }

    void resize(size_t size)
    {
        data->resize(size);
    }

    void append(std::vector<uint8_t>::iterator begin, std::vector<uint8_t>::iterator end)
    {
        data->insert(data->end(), begin, end);
    }

    void append(const uint8_t *begin, const uint8_t *end)
    {
        data->insert(data->end(), begin, end);
    }

    std::vector<uint8_t>::iterator begin()
    {
        return data->begin();
    }

    std::vector<uint8_t>::iterator end()
    {
        return data->end();
    }

    std::vector<uint8_t>::iterator erase(std::vector<uint8_t>::iterator begin, std::vector<uint8_t>::iterator end)
    {
        return data->erase(begin, end);
    }

    void reallocateWith(std::vector<uint8_t> *new_data)
    {
        if (new_data == nullptr)
        {
            throw std::runtime_error("Cannot reallocate with null data");
        }
        delete data;
        data = new_data;
    }

    void writeLong(int64_t value);

    void writeInt(int32_t value);

    void writeShort(int16_t value);

    void writeByte(uint8_t value);

    void writeString(const std::string &value);

    void writeBytes(const uint8_t *value, size_t length);

    void writeFloat(float value);

    void writeDouble(double value);

    void writeBool(bool value);

    int64_t readLong();

    int32_t readInt();

    int16_t readShort();

    uint8_t readByte();

    std::string readString();

    void readBytes(uint8_t *value, size_t length);

    float readFloat();

    double readDouble();

    bool readBool();
};

#endif // BYTEBUFFER_HPP