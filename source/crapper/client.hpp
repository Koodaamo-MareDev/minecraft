// Crapper - a networking layer for Minecraft beta 1.3 compatibility on Wii
// Data is always sent in big-endian format
#ifndef CRAPPER_CLIENT_HPP
#define CRAPPER_CLIENT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <math/vec3i.hpp>
#include <ported/ByteBuffer.hpp>

namespace Crapper
{
    bool initNetwork();
    void deinitNetwork();

    enum class ErrorStatus
    {
        OK,
        CONNECT_ERROR,
        SEND_ERROR,
        RECEIVE_ERROR
    };

    class Client
    {
    public:
        int sockfd = -1;

        ErrorStatus status = ErrorStatus::OK;

        void connect(std::string host, uint16_t port);

        void send(ByteBuffer &buffer);

        void receive(ByteBuffer &buffer);

        void close();

        Client() : sockfd(-1), status(ErrorStatus::OK) {}

        ~Client()
        {
            close();
        }
    };

    enum class MetaType
    {
        BYTE = 0,
        SHORT = 1,
        INT = 2,
        FLOAT = 3,
        STRING = 4,
        ITEM = 5,
        POSITION = 6
    };

    class Metadata
    {
    public:
        uint8_t type;

        union
        {
            uint8_t byte_value;
            int16_t short_value;
            int32_t int_value;
            float float_value;
            int32_t x;
        };
        int32_t y = 0;
        int32_t z = 0;
        std::string string_value;

        Metadata() : type(0), int_value(0) {}
        Metadata(uint8_t type, int32_t value) : type(type), x(value) {}                                             // Integers
        Metadata(uint8_t type, float value) : type(type), float_value(value) {}                                     // Float
        Metadata(std::string string_value) : type(4), int_value(string_value.size()), string_value(string_value) {} // String
        Metadata(uint8_t type, int32_t x, int32_t y, int32_t z) : type(type), x(x), y(y), z(z) {}                   // Position and item

        void write(ByteBuffer &buffer, int index);
        bool read(ByteBuffer &buffer, int &index);
    };

    class DataWatcher
    {
    public:
        std::vector<Metadata> metadata;

        void addByte(uint8_t value);
        void addShort(int16_t value);
        void addInt(int32_t value);
        void addFloat(float value);
        void addString(std::string value);
        void addItem(uint16_t item_id, uint8_t item_count, uint8_t item_meta);
        void addPosition(int32_t x, int32_t y, int32_t z);

        uint8_t getByte(size_t index);
        int16_t getShort(size_t index);
        int32_t getInt(size_t index);
        float getFloat(size_t index);
        std::string getString(size_t index);
        void getItem(size_t index, uint16_t &item_id, uint8_t &item_count, uint8_t &item_meta);
        void getPosition(size_t index, int32_t &x, int32_t &y, int32_t &z);

        void write(ByteBuffer &buffer);

        void read(ByteBuffer &buffer);
    };

    class MinecraftClient : public Client
    {
    public:
        std::string username = "WiiUser";
        int32_t entity_id = 0;
        int64_t seed = 0;
        uint8_t dimension = 0;
        bool login_success = false;
        bool on_ground = false;
        void joinServer(std::string host, uint16_t port);
        void handleKeepAlive(ByteBuffer &buffer);
        void sendLoginRequest();
        void handleHandshake(ByteBuffer &buffer);
        void sendHandshake();
        void sendPlayerPositionLook();
        void sendPlayerPosition();
        void sendPlayerLook();
        void sendBlockDig(uint8_t status, int32_t x, uint8_t y, int32_t z, uint8_t face);
        void sendPlaceBlock(int32_t x, int32_t y, int32_t z, uint8_t face, uint16_t item_id, uint8_t item_count, uint8_t item_meta);
        void sendBlockItemSwitch(uint16_t item_id);
        void sendKeepAlive();

        bool handlePacket(ByteBuffer &buffer);
        void handleLoginRequest(ByteBuffer &buffer);
        void sendChatMessage(std::string message);
        void handleChatMessage(ByteBuffer &buffer);
        void handleTimeUpdate(ByteBuffer &buffer);
        void handlePlayerEquipment(ByteBuffer &buffer);
        void handlePlayerHealth(ByteBuffer &buffer);
        void sendRespawn();
        void handleRespawn(ByteBuffer &buffer);
        void sendGrounded(bool on_ground);
        void handlePlayerPosition(ByteBuffer &buffer);
        void handlePlayerLook(ByteBuffer &buffer);
        void handlePlayerPositionLook(ByteBuffer &buffer);
        void handleUseBed(ByteBuffer &buffer);
        void handleNamedEntitySpawn(ByteBuffer &buffer);
        void handlePickupSpawn(ByteBuffer &buffer);
        void handleCollect(ByteBuffer &buffer);
        void handleVehicleSpawn(ByteBuffer &buffer);
        void handleMobSpawn(ByteBuffer &buffer);
        void handlePaintingSpawn(ByteBuffer &buffer);
        void handleEntityMotion(ByteBuffer &buffer);
        void handleDestroyEntity(ByteBuffer &buffer);
        void handleEntity(ByteBuffer &buffer);
        void handleEntityRelMove(ByteBuffer &buffer);
        void handleEntityLook(ByteBuffer &buffer);
        void handleEntityRelMoveLook(ByteBuffer &buffer);
        void handleEntityTeleport(ByteBuffer &buffer);
        void handleEntityStatus(ByteBuffer &buffer);
        void handleAttachEntity(ByteBuffer &buffer);
        void handleMetadata(ByteBuffer &buffer);
        void handlePreChunk(ByteBuffer &buffer);
        void handleMapChunk(ByteBuffer &buffer);
        void handleBlockChange(ByteBuffer &buffer);
        void handleMultiBlockChange(ByteBuffer &buffer);
        void handleExplosion(ByteBuffer &buffer);
        void handleWindowItems(ByteBuffer &buffer);
        void handleUpdateSign(ByteBuffer &buffer);
        void handleKick(ByteBuffer &buffer);
        void handleSetSlot(ByteBuffer &buffer);
        void disconnect();

        int clientToNetworkSlot(int slot_id);
        int networkToClientSlot(int slot_id);
    };

} // namespace Crapper

#endif
