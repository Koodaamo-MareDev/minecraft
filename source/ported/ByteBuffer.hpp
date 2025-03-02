#ifndef BYTEBUFFER_HPP
#define BYTEBUFFER_HPP

#include <vector>
#include <cstdint>
#include <string>
#include <cstring>

class ByteBuffer
{
public:
    std::vector<uint8_t> data;
    size_t offset = 0;
    bool underflow = false;

    ByteBuffer()
    {
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