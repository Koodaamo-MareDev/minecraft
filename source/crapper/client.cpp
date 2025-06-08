#include "client.hpp"

#include <network.h> // Wii network library
#include <miniz/miniz.h>

#include "../inventory.hpp"
#include "../entity.hpp"
#include "../chunk.hpp"
#include "../lock.hpp"
#include "../sounds.hpp"
#include "../sound.hpp"
#include "../world.hpp"
#include "../gui_dirtscreen.hpp"

extern float xrot;
extern float yrot;
void dbgprintf(const char *fmt, ...); // in minecraft.cpp
namespace Crapper
{
    bool initNetwork()
    {
        int i = 0;
        while (net_init() == -EAGAIN)
        {
            if (++i >= 10)
            {
                return false;
            }
        }
        // Configure the network interface
        char localip[16] = {0};
        char gateway[16] = {0};
        char netmask[16] = {0};
        s32 ret;
        ret = if_config(localip, netmask, gateway, true, 20);
        if (ret >= 0)
        {
            dbgprintf("Network configured, ip: %s, gw: %s, mask %s\n", localip, gateway, netmask);
            return true;
        }
        dbgprintf("Error: Network configuration failed\n");
        return false;
    }

    void deinitNetwork()
    {
        net_deinit();
    }

    void Client::connect(std::string host, uint16_t port)
    {

        // Create socket
        sockfd = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sockfd < 0)
        {
            dbgprintf("Error creating socket\n");
            status = ErrorStatus::CONNECT_ERROR;
            return;
        }

        // Set up server address
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);

        // Resolve hostname
        hostent *server = net_gethostbyname(host.c_str());
        if (!server)
        {
            dbgprintf("Unknown hostname. Fallback to plain IP\n");
            if (inet_aton(host.c_str(), &server_addr.sin_addr) == 0)
            {
                dbgprintf("Error resolving hostname\n");
                status = ErrorStatus::CONNECT_ERROR;
                close();
                return;
            }
        }
        else
            memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

        if (net_connect(sockfd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            close();
            status = ErrorStatus::CONNECT_ERROR;
            return;
        }
    }

    void Client::send(ByteBuffer &buffer)
    {
        if (status != ErrorStatus::OK)
        {
            return;
        }
        int nonblock = 0;
        net_ioctl(sockfd, FIONBIO, &nonblock);
        // Due to the way the Wii network library works, we have to send the data in chunks of 2048 bytes.
        size_t offset = 0;
        while (offset < buffer.size())
        {
            size_t length = std::min(buffer.size() - offset, 512U);

            int sent = net_write(sockfd, &buffer.ptr()[offset], length);
            if (sent < 0)
            {
                dbgprintf("Error sending data: %d %s\n", sent, strerror(-sent));
                status = ErrorStatus::SEND_ERROR;
                break;
            }
            offset += sent;
        }
    }

    // NOTE: This function does not block. It will return immediately if there is no data to read.
    void Client::receive(ByteBuffer &buffer)
    {
        if (status != ErrorStatus::OK)
        {
            return;
        }
        int nonblock = 1;
        net_ioctl(sockfd, FIONBIO, &nonblock);

        uint8_t tmp[512];
        int received;
        do
        {
            if (buffer.size() > 131072)
                break; // Limit the buffer size to 128KB

            received = net_read(sockfd, tmp, sizeof(tmp));
            if (received < 0)
            {
                if (received == -EAGAIN)
                    break; // No data to read
                dbgprintf("Error receiving data: %d %s\n", received, strerror(-received));
                status = ErrorStatus::RECEIVE_ERROR;
                return;
            }
            else if (received == 0)
            {
                // No data to read
                break;
            }

            // Append the received data to the buffer
            buffer.append(tmp, tmp + received);
        } while (received > 0);
    }

    void Client::close()
    {
        net_close(sockfd);
    }

    void Metadata::write(ByteBuffer &buffer, int index)
    {
        buffer.writeByte((type << 5) | (index & 0x1F));
        switch (type)
        {
        case 0:
            buffer.writeByte(byte_value);
            break;
        case 1:
            buffer.writeShort(short_value);
            break;
        case 2:
            buffer.writeInt(int_value);
            break;
        case 3:
            buffer.writeFloat(float_value);
            break;
        case 4:
            buffer.writeString(string_value);
            break;
        case 5:
            buffer.writeShort(x);
            buffer.writeByte(y);
            buffer.writeShort(z);
            break;
        case 6:
            buffer.writeInt(x);
            buffer.writeInt(y);
            buffer.writeInt(z);
            break;
        }
    }

    bool Metadata::read(ByteBuffer &buffer, int &index)
    {
        uint8_t header = buffer.readByte();
        if (header == 0x7F)
        {
            return false;
        }
        type = (header >> 5) & 0x7;
        index = header & 0x1F;
        switch (type)
        {
        case 0:
            byte_value = buffer.readByte();
            break;
        case 1:
            short_value = buffer.readShort();
            break;
        case 2:
            int_value = buffer.readInt();
            break;
        case 3:
            float_value = buffer.readFloat();
            break;
        case 4:
            string_value = buffer.readString();
            break;
        case 5:
            x = buffer.readShort();
            y = buffer.readByte();
            z = buffer.readShort();
            break;
        case 6:
            x = buffer.readInt();
            y = buffer.readInt();
            z = buffer.readInt();
            break;
        }
        return true;
    }

    uint8_t DataWatcher::getByte(size_t index)
    {
        if (index >= metadata.size())
            return 0;
        if (metadata[index].type != 0)
            return 0;
        return metadata[index].byte_value;
    }

    int16_t DataWatcher::getShort(size_t index)
    {
        if (index >= metadata.size())
            return 0;
        if (metadata[index].type != 1)
            return 0;
        return metadata[index].short_value;
    }

    int32_t DataWatcher::getInt(size_t index)
    {
        if (index >= metadata.size())
            return 0;
        if (metadata[index].type != 2)
            return 0;
        return metadata[index].x;
    }

    float DataWatcher::getFloat(size_t index)
    {
        if (index >= metadata.size())
            return 0;
        if (metadata[index].type != 3)
            return 0;
        return metadata[index].float_value;
    }

    std::string DataWatcher::getString(size_t index)
    {
        if (index >= metadata.size())
            return "";
        if (metadata[index].type != 4)
            return "";
        return metadata[index].string_value;
    }

    void DataWatcher::getItem(size_t index, uint16_t &item_id, uint8_t &item_count, uint8_t &item_meta)
    {
        if (index >= metadata.size())
            return;
        if (metadata[index].type != 5)
            return;
        item_id = metadata[index].x;
        item_count = metadata[index].y;
        item_meta = metadata[index].z;
    }

    void DataWatcher::getPosition(size_t index, int32_t &x, int32_t &y, int32_t &z)
    {
        if (index >= metadata.size())
            return;
        if (metadata[index].type != 6)
            return;
        x = metadata[index].x;
        y = metadata[index].y;
        z = metadata[index].z;
    }

    void DataWatcher::addByte(uint8_t value)
    {
        metadata.push_back(Metadata(0, value));
    }

    void DataWatcher::addShort(int16_t value)
    {
        metadata.push_back(Metadata(1, value));
    }

    void DataWatcher::addInt(int32_t value)
    {
        metadata.push_back(Metadata(2, value));
    }

    void DataWatcher::addFloat(float value)
    {
        metadata.push_back(Metadata(3, value));
    }

    void DataWatcher::addString(std::string value)
    {
        metadata.push_back(Metadata(value));
    }

    void DataWatcher::addItem(uint16_t item_id, uint8_t item_count, uint8_t item_meta)
    {
        metadata.push_back(Metadata(5, item_id, item_count, item_meta));
    }

    void DataWatcher::addPosition(int32_t x, int32_t y, int32_t z)
    {
        metadata.push_back(Metadata(6, x, y, z));
    }

    void DataWatcher::write(ByteBuffer &buffer)
    {
        int index = 0;
        for (Metadata &data : metadata)
        {
            if (index >= 32)
                break;
            data.write(buffer, index);
            index++;
        }
        buffer.writeByte(0x7F);
    }

    void DataWatcher::read(ByteBuffer &buffer)
    {
        metadata.resize(32);
        int index = 0;
        while (true)
        {
            Metadata data;
            if (!data.read(buffer, index))
                break;
            metadata[index] = data;
        }
    }

    void MinecraftClient::joinServer(std::string host, uint16_t port)
    {
        dbgprintf("Connecting to server %s:%d\n", host.c_str(), port);
        status = ErrorStatus::OK;
        current_world->set_remote(true);
        connect(host, port);

        if (status != ErrorStatus::OK)
        {
            current_world->set_remote(false);
            dbgprintf("Error connecting to server\n");
            return;
        }

        sendHandshake();

        if (status != ErrorStatus::OK)
        {
            current_world->set_remote(false);
            dbgprintf("Error connecting to server\n");
            return;
        }
    }

    void MinecraftClient::handleKeepAlive(ByteBuffer &buffer)
    {
        sendKeepAlive();
    }

    void MinecraftClient::sendHandshake()
    {
        // Send handshake packet
        ByteBuffer buffer;
        buffer.writeByte(0x02); // Packet ID
        buffer.writeString(username);
        send(buffer);
    }

    void MinecraftClient::handleHandshake(ByteBuffer &buffer)
    {
        // Read response
        std::string server_hash = buffer.readString();
        if (buffer.underflow)
            return;

        dbgprintf("Server hash: %s\n", server_hash.c_str());
        sendLoginRequest();
    }

    void MinecraftClient::sendLoginRequest()
    {
        // Send login packet
        ByteBuffer buffer;
        buffer.writeByte(0x01); // Packet ID
        buffer.writeInt(9);     // Protocol version
        buffer.writeString(username);

        // This is the password field which is not used by the vanilla server.
        // We can use it to indicate the server that we are a Wii client.
        buffer.writeString("WiiMCcli");

        buffer.writeLong(0); // Not used (map seed)
        buffer.writeByte(0); // Not used (dimension)
        send(buffer);
    }

    void MinecraftClient::handleLoginRequest(ByteBuffer &buffer)
    {
        // Read response
        int32_t entity_id = buffer.readInt();
        buffer.readString(); // Not used

        // We'll use this unused field to ensure that the server is compatible with the Wii client.
        std::string srv_checksum = buffer.readString();

        int64_t seed = buffer.readLong();
        uint8_t dimension = buffer.readByte();

        if (buffer.underflow)
            return;

        this->entity_id = entity_id;
        this->seed = seed;
        this->dimension = dimension;

        if (srv_checksum != "WiiMCsrv")
        {
            dbgprintf("Incompatible server\n");
            status = ErrorStatus::RECEIVE_ERROR;
            return;
        }
        set_world_hell(dimension);

        dbgprintf("Player entity ID: %d\n", entity_id);
        dbgprintf("World seed: %lld\n", seed);

        if (current_world->player.m_entity)
        {
            if (current_world->player.m_entity->chunk)
            {
                dbgprintf("Player entity already exists in a chunk even though no chunks should be loaded yet. This must be a bug!!!\n");
                status = ErrorStatus::RECEIVE_ERROR;
                return;
            }

            // Remove the old player entity
            remove_entity(current_world->player.m_entity->entity_id);
        }

        // Create a new player entity
        current_world->player.m_entity = new entity_player_local(vec3f(0.5, -999, 0.5));
        current_world->player.m_entity->entity_id = entity_id;
        add_entity(current_world->player.m_entity);

        if (!current_world->loaded)
        {
            gui_dirtscreen *dirtscreen = dynamic_cast<gui_dirtscreen *>(gui::get_gui());
            if (!dirtscreen)
                dirtscreen = new gui_dirtscreen(gertex::GXView());
            dirtscreen->set_text("Loading level\n\nDownloading terrain");
            dirtscreen->set_progress(0, 0);
            gui::set_gui(dirtscreen);
        }
        login_success = true;
    }

    void MinecraftClient::sendChatMessage(std::string message)
    {
        if (message.size() > 100)
        {
            dbgprintf("Chat message too long\n");
            return;
        }
        // Send chat message packet
        ByteBuffer buffer;
        buffer.writeByte(0x03); // Packet ID
        buffer.writeString(message);
        send(buffer);
    }

    void MinecraftClient::handleChatMessage(ByteBuffer &buffer)
    {
        // Read chat message
        std::string message = buffer.readString();
        if (buffer.underflow)
            return;
        dbgprintf("[CHAT] %s\n", message.c_str());
    }

    void MinecraftClient::handleTimeUpdate(ByteBuffer &buffer)
    {
        // Read time update
        int64_t time = buffer.readLong();
        if (buffer.underflow)
            return;
        int32_t new_time = time % 24000;
        // Update the time of day if the difference is greater than half a second
        if (std::abs(new_time - current_world->time_of_day) > 10)
        {
            current_world->time_of_day = new_time;
        }
    }

    void MinecraftClient::handlePlayerEquipment(ByteBuffer &buffer)
    {
        // Read entity equipment
        buffer.readInt();   // Entity ID
        buffer.readShort(); // Slot
        buffer.readShort(); // Item ID
        buffer.readShort(); // Item metadata
    }

    void MinecraftClient::handlePlayerHealth(ByteBuffer &buffer)
    {
        // Read entity health
        int16_t health = buffer.readShort();
        if (current_world->player.m_entity)
        {
            current_world->player.m_entity->health = health;
            if (health <= 0)
            {
                sendRespawn();
            }
        }
    }

    void MinecraftClient::sendRespawn()
    {
        // Send respawn packet
        ByteBuffer buffer;
        buffer.writeByte(0x09); // Packet ID
        send(buffer);
    }

    void MinecraftClient::handleRespawn(ByteBuffer &buffer)
    {
        dbgprintf("Respawned\n");
    }

    void MinecraftClient::sendGrounded(bool on_ground)
    {
        // Cache the player's on ground state
        this->on_ground = on_ground;

        // Send player on ground packet
        ByteBuffer buffer;
        buffer.writeByte(0x0A); // Packet ID
        buffer.writeBool(on_ground);
        send(buffer);
    }

    void MinecraftClient::handlePlayerPosition(ByteBuffer &buffer)
    {
        // Read entity position
        double x = buffer.readDouble();
        double y = buffer.readDouble();
        buffer.readDouble(); // Stance
        double z = buffer.readDouble();
        bool on_ground = buffer.readBool();

        if (buffer.underflow)
            return;

        if (!current_world->player.m_entity)
            return;

        current_world->player.m_entity->teleport(vec3f(x, y, z));
        current_world->player.m_entity->on_ground = on_ground;
    }

    void MinecraftClient::handlePlayerLook(ByteBuffer &buffer)
    {
        // Read entity look
        float yaw = buffer.readFloat();
        float pitch = buffer.readFloat();
        bool on_ground = buffer.readBool(); // On ground

        if (buffer.underflow)
            return;

        if (!current_world->player.m_entity)
            return;

        current_world->player.m_entity->rotation = vec3f(-pitch, -yaw, 0);
        current_world->player.m_entity->on_ground = on_ground;
        xrot = pitch;
        yrot = yaw;
    }

    void MinecraftClient::handlePlayerPositionLook(ByteBuffer &buffer)
    {
        // Read entity position and look
        double x = buffer.readDouble();
        double y = buffer.readDouble();
        buffer.readDouble(); // Stance
        double z = buffer.readDouble();
        float yaw = buffer.readFloat();
        float pitch = buffer.readFloat();
        bool on_ground = buffer.readBool();

        if (buffer.underflow)
            return;

        if (!current_world->player.m_entity)
            return;

        current_world->player.m_entity->teleport(vec3f(x, y, z));
        current_world->player.m_entity->rotation = vec3f(-pitch, -yaw, 0);
        current_world->player.m_entity->on_ground = on_ground;

        // Mirror the packet back to the server
        sendPlayerPositionLook();
        dbgprintf("Player position: %f %f %f and look: %f %f\n", x, y, z, yaw, pitch);
    }

    void MinecraftClient::handleUseBed(ByteBuffer &buffer)
    {
        // Read use bed
        int32_t entity_id = buffer.readInt();
        int8_t in_bed = buffer.readByte();
        int32_t x = buffer.readInt();
        uint8_t y = buffer.readByte();
        int32_t z = buffer.readInt();

        if (buffer.underflow)
            return;

        entity_physical *entity = get_entity_by_id(entity_id);
        entity_player *player = dynamic_cast<entity_player *>(entity);
        if (player)
        {
            player->in_bed = in_bed;
            player->bed_pos = vec3i(x, y, z);
        }
    }

    void MinecraftClient::sendPlayerPositionLook()
    {
        if (!current_world->player.m_entity)
            return;

        // Send player position and look packet
        ByteBuffer buffer;
        buffer.writeByte(0x0D); // Packet ID
        buffer.writeDouble(current_world->player.m_entity->position.x);
        buffer.writeDouble(current_world->player.m_entity->aabb.min.y);
        buffer.writeDouble(current_world->player.m_entity->position.y);
        buffer.writeDouble(current_world->player.m_entity->position.z);
        buffer.writeFloat(180 - current_world->player.m_entity->rotation.y); // Yaw
        buffer.writeFloat(-current_world->player.m_entity->rotation.x);      // Pitch
        buffer.writeBool(current_world->player.m_entity->on_ground);
        send(buffer);
    }

    void MinecraftClient::sendPlayerPosition()
    {
        if (!current_world->player.m_entity)
            return;

        // Send player position packet
        ByteBuffer buffer;
        buffer.writeByte(0x0B); // Packet ID
        buffer.writeDouble(current_world->player.m_entity->position.x);
        buffer.writeDouble(current_world->player.m_entity->position.y);
        buffer.writeDouble(current_world->player.m_entity->position.z);
        buffer.writeBool(current_world->player.m_entity->on_ground);
        send(buffer);
    }

    void MinecraftClient::sendPlayerLook()
    {
        if (!current_world->player.m_entity)
            return;

        // Send player look packet
        ByteBuffer buffer;
        buffer.writeByte(0x0C);                                              // Packet ID
        buffer.writeFloat(180 - current_world->player.m_entity->rotation.y); // Yaw
        buffer.writeFloat(-current_world->player.m_entity->rotation.x);      // Pitch
        buffer.writeBool(current_world->player.m_entity->on_ground);
        send(buffer);
    }

    void MinecraftClient::sendBlockDig(uint8_t status, int32_t x, uint8_t y, int32_t z, uint8_t face)
    {
        // Send player dig block packet
        ByteBuffer buffer;
        buffer.writeByte(0x0E); // Packet ID
        buffer.writeByte(status);
        buffer.writeInt(x);
        buffer.writeByte(y);
        buffer.writeInt(z);
        buffer.writeByte(face);
        send(buffer);
    }

    void MinecraftClient::sendPlaceBlock(int32_t x, int32_t y, int32_t z, uint8_t face, uint16_t item_id, uint8_t item_count, uint8_t item_meta)
    {
        // Send player place block packet
        ByteBuffer buffer;
        buffer.writeByte(0x0F); // Packet ID
        buffer.writeInt(x);
        buffer.writeByte(y);
        buffer.writeInt(z);
        buffer.writeByte(face);
        if (item_id < 0)
        {
            buffer.writeShort(-1);
        }
        else
        {
            buffer.writeShort(item_id);
            buffer.writeByte(item_count);
            buffer.writeShort(item_meta);
        }
        send(buffer);
    }

    void MinecraftClient::sendAnimation(uint8_t animation_id)
    {
        // Send player animation packet
        ByteBuffer buffer;
        buffer.writeByte(0x12);                                     // Packet ID
        buffer.writeInt(current_world->player.m_entity->entity_id); // Entity ID
        buffer.writeByte(animation_id);
        send(buffer);
    }

    void MinecraftClient::sendBlockItemSwitch(uint16_t item_id)
    {
        // Send player switch item packet
        ByteBuffer buffer;
        buffer.writeByte(0x10); // Packet ID
        buffer.writeShort(item_id);
        send(buffer);
    }

    void MinecraftClient::handleNamedEntitySpawn(ByteBuffer &buffer)
    {
        // Read named entity spawn
        int32_t entity_id = buffer.readInt();
        std::string player_name = buffer.readString();
        int32_t x = buffer.readInt();
        int32_t y = buffer.readInt();
        int32_t z = buffer.readInt();
        vfloat_t yaw = (buffer.readByte() * 360.0) / 256;
        vfloat_t pitch = (buffer.readByte() * 360.0) / 256;
        int16_t item = buffer.readShort();

        if (buffer.underflow)
            return;

        entity_player_mp *entity = new entity_player_mp(vec3f(x, y, z), player_name);
        entity->set_server_pos_rot(vec3i(x, y, z), vec3f(pitch, -yaw, 0), 0);
        entity->teleport(vec3f(x / 32.0, y / 32.0 + 1.0 / 64.0, z / 32.0));
        entity->holding_item = item;

        entity->entity_id = entity_id;
        add_entity(entity);
    }

    void MinecraftClient::handlePickupSpawn(ByteBuffer &buffer)
    {
        // Read pickup spawn
        int32_t entity_id = buffer.readInt();
        int16_t item = buffer.readShort();
        uint8_t count = buffer.readByte();
        int16_t meta = buffer.readShort();
        int32_t x = buffer.readInt();
        int32_t y = buffer.readInt();
        int32_t z = buffer.readInt();
        buffer.readByte(); // Rotation
        buffer.readByte(); // Pitch
        buffer.readByte(); // Roll

        if (buffer.underflow)
            return;

        entity_item *entity = new entity_item(vec3f(x, y, z), inventory::item_stack(item, count, meta));
        entity->set_server_position(vec3i(x, y, z));
        entity->teleport(vec3f(x / 32.0, y / 32.0 + 1.0 / 64.0, z / 32.0));
        entity->entity_id = entity_id;
        add_entity(entity);
    }

    void MinecraftClient::handleCollect(ByteBuffer &buffer)
    {
        // Read pickup
        int32_t item_entity_id = buffer.readInt();
        int32_t entity_id = buffer.readInt();

        if (buffer.underflow)
            return;

        entity_physical *pickup_entity = get_entity_by_id(item_entity_id);

        entity_physical *entity = get_entity_by_id(entity_id);

        // Make sure the entities exists and the picked up entity is an item entity
        if (!pickup_entity || pickup_entity->type != 1 || !entity)
            return;

        entity_item *item_entity = dynamic_cast<entity_item *>(pickup_entity);
        if (!item_entity)
            return;
        item_entity->pickup(entity->get_position(0) - vec3f(0, 0.5, 0));
    }

    void MinecraftClient::handleVehicleSpawn(ByteBuffer &buffer)
    {
        // Read vehicle spawn
        int32_t entity_id = buffer.readInt();
        uint8_t type = buffer.readByte();
        int32_t x = buffer.readInt();
        int32_t y = buffer.readInt();
        int32_t z = buffer.readInt();

        if (buffer.underflow)
            return;

        entity_physical *entity = nullptr;
        switch (type)
        {
        case 70:
            entity = new entity_falling_block(block_t{uint8_t(BlockID::sand), 0x7F, 0}, vec3i(x / 32, y / 32, z / 32));
            break;
        case 71:
            entity = new entity_falling_block(block_t{uint8_t(BlockID::gravel), 0x7F, 0}, vec3i(x / 32, y / 32, z / 32));
            break;

        default:
            entity = new entity_physical();
            dbgprintf("Generic vehicle %d spawned at %f %f %f with entity type %d\n", entity_id, x, y, z, type);
            break;
        }
        entity->set_server_position(vec3i(x, y, z));
        entity->teleport(vec3f(x / 32.0, y / 32.0 + 1.0 / 64.0, z / 32.0));
        entity->entity_id = entity_id;
        add_entity(entity);
    }

    void MinecraftClient::handleMobSpawn(ByteBuffer &buffer)
    {
        // Read mob spawn
        int32_t entity_id = buffer.readInt();
        uint8_t type = buffer.readByte();
        int32_t x = buffer.readInt();
        int32_t y = buffer.readInt();
        int32_t z = buffer.readInt();
        uint8_t yaw = buffer.readByte();
        uint8_t pitch = buffer.readByte();
        DataWatcher metadata;
        metadata.read(buffer);

        if (buffer.underflow)
            return;

        dbgprintf("Mob %d spawned at %f %f %f with entity type %d\n", entity_id, x / 32., y / 32., z / 32., int(type & 0xFF));
#ifdef DEBUGENTITIES
        // Print metadata
        for (size_t i = 0; i < metadata.metadata.size(); i++)
        {
            switch (metadata.metadata[i].type)
            {
            case 0:
                dbgprintf("Metadata %d: %d\n", i, metadata.getByte(i));
                break;
            case 1:
                dbgprintf("Metadata %d: %d\n", i, metadata.getShort(i));
                break;
            case 2:
                dbgprintf("Metadata %d: %d\n", i, metadata.getInt(i));
                break;
            case 3:
                dbgprintf("Metadata %d: %f\n", i, metadata.getFloat(i));
                break;
            case 4:
                dbgprintf("Metadata %d: %s\n", i, metadata.getString(i).c_str());
                break;
            case 5:
            {
                uint16_t item_id;
                uint8_t item_count;
                uint8_t item_meta;
                metadata.getItem(i, item_id, item_count, item_meta);
                dbgprintf("Metadata %d: %d %d %d\n", i, item_id, item_count, item_meta);
                break;
            }
            case 6:
            {
                int32_t mx, my, mz;
                metadata.getPosition(i, mx, my, mz);
                dbgprintf("Metadata %d: %d %d %d\n", i, mx, my, mz);
                break;
            }
            }
        }
#endif
        // Create entity based on type
        entity_physical *entity = nullptr;
        switch (type)
        {
        case 50:
            entity = new entity_creeper(vec3f(x, y, z) / 32.0);
            break;
        default:
            entity = new entity_living();
            break;
        }
        entity->width = 0.6;
        entity->height = 1.8;
        entity->set_server_pos_rot(vec3i(x, y, z), vec3f(pitch * (360.0 / 256), yaw * (-360.0 / 256), 0), 0);
        entity->teleport(vec3f(x / 32.0, y / 32.0 + 1.0 / 64.0, z / 32.0));
        entity->entity_id = entity_id;
        add_entity(entity);
    }

    void MinecraftClient::handlePaintingSpawn(ByteBuffer &buffer)
    {
        // Read painting spawn
        int32_t entity_id = buffer.readInt();
        std::string title = buffer.readString();
        int32_t x = buffer.readInt();
        int32_t y = buffer.readInt();
        int32_t z = buffer.readInt();
        int32_t direction = buffer.readInt();

        if (buffer.underflow)
            return;

        entity_physical *entity = new entity_physical();
        entity->teleport(vec3f(x, y, z));
        entity->entity_id = entity_id;
        add_entity(entity);

        dbgprintf("Painting %d named %s spawned at %d %d %d with facing value %d\n", entity_id, title.c_str(), x, y, z, direction);
    }

    void MinecraftClient::handleEntityMotion(ByteBuffer &buffer)
    {
        // Read entity motion
        int32_t entity_id = buffer.readInt();
        int16_t dx = buffer.readShort();
        int16_t dy = buffer.readShort();
        int16_t dz = buffer.readShort();

        if (buffer.underflow)
            return;

        entity_physical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        entity->velocity = vec3f(dx / 8000.0, dy / 8000.0, dz / 8000.0);
    }

    void MinecraftClient::handleDestroyEntity(ByteBuffer &buffer)
    {
        // Read destroy entity
        int32_t entity_id = buffer.readInt();

        if (buffer.underflow)
            return;

        remove_entity(entity_id);
    }

    void MinecraftClient::handleEntity(ByteBuffer &buffer)
    {
        // Read entity
        buffer.readInt(); // Entity ID

        if (buffer.underflow)
            return;

        // This packet indicates that the entity has not moved - we don't need to do anything
    }

    void MinecraftClient::handleEntityRelMove(ByteBuffer &buffer)
    {
        // Read relative move entity
        int32_t entity_id = buffer.readInt();
        int8_t dx = buffer.readByte();
        int8_t dy = buffer.readByte();
        int8_t dz = buffer.readByte();

        if (buffer.underflow)
            return;

        entity_physical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        entity->set_server_position(entity->server_pos + vec3i(dx, dy, dz), 3);
    }

    void MinecraftClient::handleEntityLook(ByteBuffer &buffer)
    {
        // Read look entity
        int32_t entity_id = buffer.readInt();
        int8_t yaw = buffer.readByte();
        int8_t pitch = buffer.readByte();

        if (buffer.underflow)
            return;

        entity_physical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        entity->set_server_pos_rot(entity->server_pos, vec3f(pitch * (360.0 / 256), yaw * (-360.0 / 256), 0), 3);
    }

    void MinecraftClient::handleEntityRelMoveLook(ByteBuffer &buffer)
    {
        // Read relative move and look entity
        int32_t entity_id = buffer.readInt();
        int8_t dx = buffer.readByte();
        int8_t dy = buffer.readByte();
        int8_t dz = buffer.readByte();
        int8_t yaw = buffer.readByte();
        int8_t pitch = buffer.readByte();

        if (buffer.underflow)
            return;

        entity_physical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        entity->set_server_pos_rot(entity->server_pos + vec3i(dx, dy, dz), vec3f(pitch * (360.0 / 256), yaw * (-360.0 / 256), 0), 3);
    }

    void MinecraftClient::handleEntityTeleport(ByteBuffer &buffer)
    {
        // Read teleport entity
        int32_t entity_id = buffer.readInt();
        int32_t x = buffer.readInt();
        int32_t y = buffer.readInt();
        int32_t z = buffer.readInt();
        int8_t yaw = buffer.readByte();
        int8_t pitch = buffer.readByte();

        if (buffer.underflow)
            return;

        entity_physical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        entity->set_server_pos_rot(vec3i(x, y, z), vec3f(pitch * (360.0 / 256), yaw * (-360.0 / 256), 0), 0);
    }

    void MinecraftClient::handleEntityStatus(ByteBuffer &buffer)
    {
        // Read entity status
        int32_t entity_id = buffer.readInt();
        uint8_t status = buffer.readByte();

        if (buffer.underflow)
            return;

        entity_physical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        if (status == 2)
        {
            entity_living *living_entity = dynamic_cast<entity_living *>(entity);
            if (living_entity)
                living_entity->hurt(0);
        }
        dbgprintf("Entity %d status: %d\n", entity_id, status);
    }

    void MinecraftClient::handleAttachEntity(ByteBuffer &buffer)
    {
        // Read attach entity
        int32_t entity_id = buffer.readInt();
        int32_t vehicle_id = buffer.readInt();

        if (buffer.underflow)
            return;

        entity_physical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            dbgprintf("Unknown entity %d\n", entity_id);
            return;
        }
        if (vehicle_id == -1)
        {
            dbgprintf("Entity %d stopped riding entity\n", entity_id);
            return;
        }
        entity_physical *vehicle = get_entity_by_id(vehicle_id);
        if (!vehicle)
        {
            dbgprintf("Entity %d started riding unknown entity %d\n", entity_id, vehicle_id);
            return;
        }
        dbgprintf("Entity %d started riding entity %d\n", entity_id, vehicle_id);
    }

    void MinecraftClient::handleMetadata(ByteBuffer &buffer)
    {
        // Read metadata
        int32_t entity_id = buffer.readInt();
        DataWatcher metadata;
        metadata.read(buffer);

        if (buffer.underflow)
            return;

        entity_physical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        dbgprintf("Received entity %d metadata\n", entity_id);
    }

    void MinecraftClient::handlePreChunk(ByteBuffer &buffer)
    {
        // Read pre chunk
        int32_t x = buffer.readInt();
        int32_t z = buffer.readInt();
        uint8_t mode = buffer.readByte();

        if (buffer.underflow)
            return;

        if (mode == 0)
        {
            lock_t chunk_lock(chunk_mutex);
            chunk_t *chunk = get_chunk(x, z);
            if (chunk)
            {
                // Mark chunk for removal
                current_world->remove_chunk(chunk);

                // Also trigger a cleanup immediately
                current_world->cleanup_chunks();
            }
        }
#ifdef PREALLOCATE_CHUNKS
        else
        {
            lock_t chunk_lock(chunk_mutex);
            try
            {
                chunk_t *chunk = new chunk_t;
                chunk->x = x;
                chunk->z = z;
                chunk->generation_stage = ChunkGenStage::empty;
                get_chunks().push_back(chunk);
            }
            catch (std::bad_alloc &e)
            {
                dbgprintf("Failed to allocate memory for chunk\n");
                dbgprintf("Chunk count: %d\n", get_chunks().size());
            }
        }
#endif
    }

    void MinecraftClient::handleMapChunk(ByteBuffer &buffer)
    {
        int32_t start_x = buffer.readInt();
        int16_t start_y = buffer.readShort();
        int32_t start_z = buffer.readInt();
        uint8_t size_x = buffer.readByte() + 1;
        uint8_t size_y = buffer.readByte() + 1;
        uint8_t size_z = buffer.readByte() + 1;
        int32_t compressed_size = buffer.readInt();

        if (buffer.underflow)
            return;

        uint8_t *compressed_data = new uint8_t[compressed_size];

        buffer.readBytes(compressed_data, compressed_size);

        if (buffer.underflow)
        {
            delete[] compressed_data;
            return;
        }

        // Decompress data
        uint32_t block_count = size_x * size_y * size_z;
        uint32_t decompressed_size = block_count * 5 / 2;
        uint8_t *decompressed_data = new uint8_t[decompressed_size];
        int decompressed_result_size = decompressed_size;
        int result = mz_uncompress(decompressed_data, (mz_ulong *)&decompressed_result_size, compressed_data, compressed_size);

        // Free compressed data
        delete[] compressed_data;

        if (result < 0 || uint32_t(decompressed_result_size) != decompressed_size)
        {
            dbgprintf("Wrong decompressed size: %s\n", mz_error(result));
            delete[] decompressed_data;
            return;
        }
        vec2i chunk_pos = block_to_chunk_pos(vec3i(start_x, start_y, start_z));

        chunk_t *chunk = get_chunk(chunk_pos);
        bool chunk_exists = chunk != nullptr;
        if (!chunk_exists)
        {
            try
            {
                chunk = new chunk_t;
                chunk->x = chunk_pos.x;
                chunk->z = chunk_pos.y;
            }
            catch (std::bad_alloc &e)
            {
                dbgprintf("Failed to allocate memory for chunk\n");
                dbgprintf("Chunk count: %d\n", get_chunks().size());
                delete[] decompressed_data;
                return;
            }
        }

        // Parse block data
        uint32_t data_index = 0;
        for (int x = 0; x < size_x; x++)
        {
            for (int z = 0; z < size_z; z++)
            {
                for (int y = 0; y < size_y; y++)
                {
                    uint8_t block_id = decompressed_data[data_index++];
                    chunk->set_block(vec3i(start_x + x, start_y + y, start_z + z), BlockID(block_id));
                }
            }
        }
        // Parse block metadata
        for (int x = 0; x < size_x; x++)
        {
            for (int z = 0; z < size_z; z++)
            {
                for (int y = 0; y < size_y; y += 2)
                {
                    uint8_t metadata = decompressed_data[data_index++];
                    chunk->get_block(vec3i(start_x + x, start_y + y, start_z + z))->meta = metadata & 0x0F;
                    chunk->get_block(vec3i(start_x + x, start_y + y + 1, start_z + z))->meta = metadata >> 4;
                }
            }
        }

        // Parse block light
        for (int x = 0; x < size_x; x++)
        {
            for (int z = 0; z < size_z; z++)
            {
                for (int y = 0; y < size_y; y += 2)
                {
                    uint8_t light = decompressed_data[data_index++];
                    chunk->get_block(vec3i(start_x + x, start_y + y, start_z + z))->block_light = light & 0x0F;
                    chunk->get_block(vec3i(start_x + x, start_y + y + 1, start_z + z))->block_light = light >> 4;
                }
            }
        }

        // Parse sky light
        for (int x = 0; x < size_x; x++)
        {
            for (int z = 0; z < size_z; z++)
            {
                for (int y = 0; y < size_y; y += 2)
                {
                    uint8_t light = decompressed_data[data_index++];
                    chunk->get_block(vec3i(start_x + x, start_y + y, start_z + z))->sky_light = light & 0x0F;
                    chunk->get_block(vec3i(start_x + x, start_y + y + 1, start_z + z))->sky_light = light >> 4;
                }
            }
        }

        // Free decompressed data
        delete[] decompressed_data;

        chunk->lit_state = 1;
        chunk->generation_stage = ChunkGenStage::done;
        chunk->recalculate_height_map();
        // Mark chunk as dirty
        for (int vbo_index = 0; vbo_index < VERTICAL_SECTION_COUNT; vbo_index++)
        {
            chunk->vbos[vbo_index].dirty = true;
        }

        lock_t chunk_lock(chunk_mutex);
        if (!chunk_exists)
        {
            get_chunks().push_back(chunk);
        }
    }

    void MinecraftClient::handleBlockChange(ByteBuffer &buffer)
    {
        // Read block change
        int32_t x = buffer.readInt();
        uint8_t y = buffer.readByte();
        int32_t z = buffer.readInt();
        uint8_t block_id = buffer.readByte();
        uint8_t block_meta = buffer.readByte();

        if (buffer.underflow)
            return;

        vec3i pos(x, y, z);
        chunk_t *chunk = get_chunk_from_pos(pos);
        if (!chunk)
        {
            return;
        }
        chunk->set_block(pos, BlockID(block_id));
        chunk->get_block(pos)->meta = block_meta;
        update_block_at(pos);
    }

    void MinecraftClient::handleMultiBlockChange(ByteBuffer &buffer)
    {
        // Read multi block change
        int32_t x = buffer.readInt();
        int32_t z = buffer.readInt();
        uint16_t count = buffer.readShort() & 0xFFFF;

        if (buffer.underflow)
            return;
        int16_t coords[count];
        uint8_t types[count];
        uint8_t metas[count];
        for (uint16_t i = 0; i < count; i++)
        {
            coords[i] = buffer.readShort();
            if (buffer.underflow)
                return;
        }
        for (uint16_t i = 0; i < count; i++)
        {
            types[i] = buffer.readByte();
            if (buffer.underflow)
                return;
        }
        for (uint16_t i = 0; i < count; i++)
        {
            metas[i] = buffer.readByte();
            if (buffer.underflow)
                return;
        }
        x *= 16;
        z *= 16;

        for (uint16_t i = 0; i < count; i++)
        {
            vec3i pos = vec3i(x + ((coords[i] >> 12) & 0xF), coords[i] & 0xFF, z + ((coords[i] >> 8) & 0xF));
            chunk_t *chunk = get_chunk_from_pos(pos);
            if (!chunk)
            {
                dbgprintf("Could not change block at %d %d %d as chunk doesn't exist!\n", pos.x, pos.y, pos.z);
                continue;
            }
            block_t *block = chunk->get_block(pos);
            block->set_blockid(BlockID(types[i]));
            block->meta = metas[i];
            update_block_at(pos);
        }
    }

    void MinecraftClient::handleExplosion(ByteBuffer &buffer)
    {
        // Read explosion
        double x = buffer.readDouble();
        double y = buffer.readDouble();
        double z = buffer.readDouble();
        float radius = buffer.readFloat();
        uint32_t record_count = buffer.readInt();

        if (buffer.underflow)
            return;

        for (uint32_t i = 0; i < record_count; i++)
        {
            // Ignore the records for now
            buffer.readByte();
            buffer.readByte();
            buffer.readByte();
        }

        if (buffer.underflow)
            return;

        // Create explosion effect
        current_world->create_explosion(vec3f(x, y, z), radius, nullptr);
    }

    int MinecraftClient::clientToNetworkSlot(int slot_id)
    {
        if (slot_id < 9)
            return slot_id + 36;
        return slot_id;
    }

    int MinecraftClient::networkToClientSlot(int slot_id)
    {
        if (slot_id >= 36 && slot_id < 45)
            return slot_id - 36;
        if (slot_id < 9)
            return -1; // Crafting and armor slots are not supported
        return slot_id;
    }

    void MinecraftClient::handleSetSlot(ByteBuffer &buffer)
    {
        // Read slot
        uint8_t window_id = buffer.readByte();
        int16_t slot_id = buffer.readShort();
        int16_t item_id = buffer.readShort();

        if (buffer.underflow)
            return;

        slot_id = networkToClientSlot(slot_id);
        inventory::item_stack item(0, 0, 0);
        if (item_id >= 0)
        {
            uint8_t count = buffer.readByte();
            uint16_t meta = buffer.readShort() & 0xFFFF;
            item = inventory::item_stack(item_id, count, meta);
        }
        if (buffer.underflow)
            return;
        if (window_id == 0 && slot_id < 36 && slot_id >= 0)
        {
            current_world->player.m_inventory[slot_id] = item;
        }
    }

    void MinecraftClient::disconnect()
    {
        // Send disconnect packet
        ByteBuffer buffer;
        buffer.writeByte(0xFF); // Packet ID
        buffer.writeString("Disconnected by client");
        send(buffer);
        status = ErrorStatus::SEND_ERROR;
        close();
    }

    void MinecraftClient::handleWindowItems(ByteBuffer &buffer)
    {
        // Read inventory
        std::vector<inventory::item_stack> items;

        uint8_t window_id = buffer.readByte();
        uint16_t count = buffer.readShort();
        if (buffer.underflow)
            return;

        for (uint16_t i = 0; i < count; i++)
        {
            int16_t item_id = buffer.readShort();
            if (item_id >= 0)
            {
                uint8_t count = buffer.readByte();
                uint16_t meta = buffer.readShort() & 0xFFFF;
                items.push_back(inventory::item_stack(item_id, count, meta));
            }
            if (buffer.underflow)
                return;
        }

        if (window_id == 0)
        {
            for (size_t i = 0; i < items.size(); i++)
            {
                int slot_id = networkToClientSlot(i);
                if (slot_id >= 0 && slot_id < 36)
                {
                    current_world->player.m_inventory[slot_id] = items[i];
                }
            }
        }
    }

    void MinecraftClient::handleUpdateSign(ByteBuffer &buffer)
    {
        // Read sign update
        int32_t x = buffer.readInt();
        int16_t y = buffer.readShort();
        int32_t z = buffer.readInt();
        std::string text1 = buffer.readString();
        std::string text2 = buffer.readString();
        std::string text3 = buffer.readString();
        std::string text4 = buffer.readString();

        if (buffer.underflow)
            return;

        dbgprintf("Sign at %d %d %d updated with text:\n%s\n%s\n%s\n%s\n", x, y, z, text1.c_str(), text2.c_str(), text3.c_str(), text4.c_str());
    }

    void MinecraftClient::handleKick(ByteBuffer &buffer)
    {
        // Read kick message
        std::string message = buffer.readString();
        if (buffer.underflow)
            return;

        gui_dirtscreen *dirtscreen = dynamic_cast<gui_dirtscreen *>(gui::get_gui());
        if (!dirtscreen)
            dirtscreen = new gui_dirtscreen(gertex::GXView());
        dirtscreen->set_text("Connection lost\n\n" + message);
        dirtscreen->set_progress(0, 0);
        gui::set_gui(dirtscreen);

        status = ErrorStatus::RECEIVE_ERROR;
    }

    void MinecraftClient::sendKeepAlive()
    {
        // Send keep alive packet
        ByteBuffer buffer;
        buffer.writeByte(0x00); // Packet ID
        send(buffer);
    }

    bool MinecraftClient::handlePacket(ByteBuffer &buffer)
    {
        if (status != ErrorStatus::OK)
        {
            return true;
        }

        buffer.offset = 0;

        // Read packet ID
        uint8_t packet_id = buffer.readByte();
        if (buffer.underflow)
        {
            buffer.underflow = false;
            return true;
        }

        // Handle packet
        switch (packet_id)
        {
        case 0x00:
        {
            // Handle packet 0x00 (keep alive)
            handleKeepAlive(buffer);
            break;
        }
        case 0x01:
        {
            // Handle packet 0x01 (login)
            handleLoginRequest(buffer);
            break;
        }
        case 0x02:
        {
            // Handle packet 0x02 (handshake)
            handleHandshake(buffer);
            break;
        }
        case 0x03:
        {
            // Handle packet 0x03 (chat message)
            handleChatMessage(buffer);
            break;
        }
        case 0x04:
        {
            // Handle packet 0x04 (time update)
            handleTimeUpdate(buffer);
            break;
        }
        case 0x05:
        {
            // Handle packet 0x05 (entity equipment)
            handlePlayerEquipment(buffer);
            break;
        }
        case 0x06:
        {
            // Handle packet 0x06 (spawn position)
            buffer.readInt(); // X
            buffer.readInt(); // Y
            buffer.readInt(); // Z
            break;
        }
        case 0x07:
        {
            // Handle packet 0x07 (use entity)
            buffer.readInt();  // Player ID
            buffer.readInt();  // Target ID
            buffer.readBool(); // Left click
            break;
        }
        case 0x08:
        {
            // Handle packet 0x08 (update health)
            handlePlayerHealth(buffer);
            break;
        }
        case 0x09:
        {
            // Handle packet 0x09 (respawn)
            handleRespawn(buffer);
            break;
        }
        case 0x0A:
        {
            // Handle packet 0x0A (fly)
            // This is a base packet for player movement
            buffer.readBool(); // on ground
            break;
        }
        case 0x0B:
        {
            // Handle packet 0x0B (player position)
            handlePlayerPosition(buffer);
            break;
        }
        case 0x0C:
        {
            // Handle packet 0x0C (player look)
            handlePlayerLook(buffer);
            break;
        }
        case 0x0D:
        {
            // Handle packet 0x0D (player position and look)
            handlePlayerPositionLook(buffer);
            break;
        }
        case 0x11:
        {
            // Handle packet 0x11 (use bed)
            handleUseBed(buffer);
            break;
        }
        case 0x12:
        {
            // Handle packet 0x12 (animation)
            buffer.readInt();  // Entity ID
            buffer.readByte(); // Animation
            break;
        }
        case 0x14:
        {
            // Handle packet 0x14 (named entity spawn)
            handleNamedEntitySpawn(buffer);
            break;
        }
        case 0x15:
        {
            // Handle packet 0x15 (pickup spawn)
            handlePickupSpawn(buffer);
            break;
        }
        case 0x16:
        {
            // Handle packet 0x16 (collect)
            handleCollect(buffer);
            break;
        }
        case 0x17:
        {
            // Handle packet 0x17 (vehicle spawn)
            handleVehicleSpawn(buffer);
            break;
        }
        case 0x18:
        {
            // Handle packet 0x18 (mob spawn)
            handleMobSpawn(buffer);
            break;
        }
        case 0x19:
        {
            // Handle packet 0x19 (painting spawn)
            handlePaintingSpawn(buffer);
            break;
        }
        case 0x1C:
        {
            // Handle packet 0x1C (entity velocity)
            handleEntityMotion(buffer);
            break;
        }
        case 0x1D:
        {
            // Handle packet 0x1D (destroy entity)
            handleDestroyEntity(buffer);
            break;
        }
        case 0x1E:
        {
            // Handle packet 0x1E (entity)
            handleEntity(buffer);
            break;
        }
        case 0x1F:
        {
            // Handle packet 0x1F (entity relative move)
            handleEntityRelMove(buffer);
            break;
        }
        case 0x20:
        {
            // Handle packet 0x20 (entity look)
            handleEntityLook(buffer);
            break;
        }
        case 0x21:
        {
            // Handle packet 0x21 (entity relative move and look)
            handleEntityRelMoveLook(buffer);
            break;
        }
        case 0x22:
        {
            // Handle packet 0x22 (entity teleport)
            handleEntityTeleport(buffer);
            break;
        }
        case 0x26:
        {
            // Handle packet 0x26 (entity status)
            handleEntityStatus(buffer);
            break;
        }
        case 0x27:
        {
            // Handle packet 0x27 (attach entity)
            handleAttachEntity(buffer);
            break;
        }
        case 0x28:
        {
            // Handle packet 0x28 (entity metadata)
            buffer.readInt(); // Entity ID
            DataWatcher metadata;
            metadata.read(buffer);
            break;
        }
        case 0x32:
        {
            // Handle packet 0x32 (pre chunk)
            handlePreChunk(buffer);
            break;
        }
        case 0x33:
        {
            // Handle packet 0x33 (map chunk)
            handleMapChunk(buffer);
            break;
        }
        case 0x34:
        {
            // Handle packet 0x34 (multi block change)
            handleMultiBlockChange(buffer);
            break;
        }
        case 0x35:
        {
            // Handle packet 0x35 (block change)
            handleBlockChange(buffer);
            break;
        }
        case 0x3C:
        {
            handleExplosion(buffer);
            break;
        }
        case 0x67:
        {
            // Handle packet 0x67 (set slot)
            handleSetSlot(buffer);
            break;
        }
        case 0x68:
        {
            // Handle packet 0x68 (inventory)
            handleWindowItems(buffer);
            break;
        }
        case 0x82:
        {
            // Handle packet 0x82 (update sign)
            handleUpdateSign(buffer);
            break;
        }
        case 0xFF:
        {
            // Handle packet 0xFF (disconnect)
            handleKick(buffer);
            break;
        }
        default:
        {
            dbgprintf("Unknown packet ID: 0x%02X\n", packet_id);

            gui_dirtscreen *dirtscreen = dynamic_cast<gui_dirtscreen *>(gui::get_gui());
            if (!dirtscreen)
                dirtscreen = new gui_dirtscreen(gertex::GXView());
            dirtscreen->set_text("Connection Lost\n\nReason: Received unknown packet " + std::to_string(packet_id));
            dirtscreen->set_progress(0, 0);
            gui::set_gui(dirtscreen);

            status = ErrorStatus::RECEIVE_ERROR;
            break;
        }
        }
        if (!buffer.underflow && status == ErrorStatus::OK)
        {
            buffer.erase(buffer.begin(), buffer.begin() + buffer.offset);
        }
        if (status != ErrorStatus::OK)
        {
            current_world->set_remote(false);
            buffer.clear();
        }
        bool last_message = buffer.underflow || status != ErrorStatus::OK;
        buffer.underflow = false;
        return last_message;
    }
} // namespace Crapper