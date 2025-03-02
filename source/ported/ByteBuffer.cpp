#include "ByteBuffer.hpp"

void ByteBuffer::writeLong(int64_t value)
{
    uint8_t temp[8];
    temp[0] = (value >> 56) & 0xFF;
    temp[1] = (value >> 48) & 0xFF;
    temp[2] = (value >> 40) & 0xFF;
    temp[3] = (value >> 32) & 0xFF;
    temp[4] = (value >> 24) & 0xFF;
    temp[5] = (value >> 16) & 0xFF;
    temp[6] = (value >> 8) & 0xFF;
    temp[7] = value & 0xFF;
    writeBytes(temp, 8);
}

void ByteBuffer::writeInt(int32_t value)
{
    uint8_t temp[4];
    temp[0] = (value >> 24) & 0xFF;
    temp[1] = (value >> 16) & 0xFF;
    temp[2] = (value >> 8) & 0xFF;
    temp[3] = value & 0xFF;
    writeBytes(temp, 4);
}

void ByteBuffer::writeShort(int16_t value)
{
    uint8_t temp[2];
    temp[0] = (value >> 8) & 0xFF;
    temp[1] = value & 0xFF;
    writeBytes(temp, 2);
}

void ByteBuffer::writeByte(uint8_t value)
{
    data.push_back(value);
}

void ByteBuffer::writeString(const std::string &value)
{
    writeShort(value.size());
    writeBytes((const uint8_t *)value.c_str(), value.size());
}

void ByteBuffer::writeBytes(const uint8_t *value, size_t length)
{
    data.insert(data.end(), value, value + length);
}

void ByteBuffer::writeFloat(float value)
{
    uint32_t temp;
    memcpy(&temp, &value, sizeof(float));
    writeInt(temp);
}

void ByteBuffer::writeDouble(double value)
{
    uint64_t temp;
    memcpy(&temp, &value, sizeof(double));
    writeLong(temp);
}

void ByteBuffer::writeBool(bool value)
{
    data.push_back(value ? 1 : 0);
}

int64_t ByteBuffer::readLong()
{
    if (offset + 8 > data.size())
    {
        underflow = true;
        return 0;
    }
    int64_t value = 0;
    value |= (int64_t(data[offset++]) & 0xFF) << 56;
    value |= (int64_t(data[offset++]) & 0xFF) << 48;
    value |= (int64_t(data[offset++]) & 0xFF) << 40;
    value |= (int64_t(data[offset++]) & 0xFF) << 32;
    value |= (int64_t(data[offset++]) & 0xFF) << 24;
    value |= (int64_t(data[offset++]) & 0xFF) << 16;
    value |= (int64_t(data[offset++]) & 0xFF) << 8;
    value |= (int64_t(data[offset++]) & 0xFF);
    return value;
}

int32_t ByteBuffer::readInt()
{
    if (offset + 4 > data.size())
    {
        underflow = true;
        return 0;
    }
    int32_t value = 0;
    value |= (int32_t(data[offset++]) & 0xFF) << 24;
    value |= (int32_t(data[offset++]) & 0xFF) << 16;
    value |= (int32_t(data[offset++]) & 0xFF) << 8;
    value |= (int32_t(data[offset++]) & 0xFF);
    return value;
}

int16_t ByteBuffer::readShort()
{
    if (offset + 2 > data.size())
    {
        underflow = true;
        return 0;
    }
    int16_t value = 0;
    value |= (int16_t(data[offset++]) & 0xFF) << 8;
    value |= (int16_t(data[offset++]) & 0xFF);
    return value;
}

uint8_t ByteBuffer::readByte()
{
    if (offset + 1 > data.size())
    {
        underflow = true;
        return 0;
    }
    return data[offset++];
}

std::string ByteBuffer::readString()
{
    size_t length = readShort();
    if (offset + length > data.size())
    {
        underflow = true;
        return "";
    }
    std::string value;
    for (size_t i = 0; i < length; i++)
    {
        value += data[offset++];
    }
    return value;
}

void ByteBuffer::readBytes(uint8_t *value, size_t length)
{
    if (offset + length > data.size())
    {
        underflow = true;
        return;
    }
    memcpy(value, &data.data()[offset], length);
    offset += length;
}

float ByteBuffer::readFloat()
{
    uint32_t temp = readInt();
    if (underflow)
    {
        return 0;
    }
    float value;
    memcpy(&value, &temp, sizeof(float));
    return value;
}

double ByteBuffer::readDouble()
{
    uint64_t temp = readLong();
    if (underflow)
    {
        return 0;
    }
    double value;
    memcpy(&value, &temp, sizeof(double));
    return value;
}

bool ByteBuffer::readBool()
{
    return readByte() != 0;
}