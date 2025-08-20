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
#include "../util/debuglog.hpp"

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
            debug::print("Network configured, ip: %s, gw: %s, mask %s\n", localip, gateway, netmask);
            return true;
        }
        debug::print("Error: Network Configuration failed\n");
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
            throw std::runtime_error("Failed to create socket");
        }

        // Set socket to non-blocking using net_ioctl
        u32 nonBlocking = 1;
        if (net_ioctl(sockfd, FIONBIO, &nonBlocking) < 0)
        {
            throw std::runtime_error("Failed to set non-blocking mode");
        }

        // Set up server address
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);

        // Resolve hostname
        hostent *server = net_gethostbyname(host.c_str());
        if (!server)
        {
            debug::print("Unknown hostname. Fallback to plain IP\n");
            if (inet_aton(host.c_str(), &server_addr.sin_addr) == 0)
            {
                throw std::runtime_error("Unknown host");
            }
        }
        else
        {
            memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
        }

        // Attempt to connect to the server
        int result = net_connect(sockfd, (sockaddr *)&server_addr, sizeof(server_addr));
        if (result < 0 && result != -EINPROGRESS)
        {
            throw std::runtime_error("Failed to connect to server: " + std::string(strerror(-result)));
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
                debug::print("Error sending data: %d %s\n", sent, strerror(-sent));

                // We don't want to throw an error here, just set the status.
                status = ErrorStatus::ERROR;
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

                throw std::runtime_error("Error receiving data: " + std::string(strerror(-received)));
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

    bool Client::is_connected()
    {
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(sockfd, &write_fds);

        // We don't want to block here, so we use a zero timeout
        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        net_select(sockfd + 1, nullptr, &write_fds, nullptr, &timeout);

        if (FD_ISSET(sockfd, &write_fds))
        {
            // Socket is writable - assume we are connected
            return true;
        }
        return false; // Not connected
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
        if (buffer.underflow)
            return false;
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

    MinecraftClient::MinecraftClient(World *remote_world)
    {
        if (remote_world == nullptr)
            throw std::runtime_error("World cannot be null");
        this->remote_world = remote_world;
    }

    void MinecraftClient::joinServer(std::string host, uint16_t port)
    {
        debug::print("Connecting to %s:%d\n", host.c_str(), port);

        GuiDirtscreen *dirtscreen = dynamic_cast<GuiDirtscreen *>(Gui::get_gui());
        if (dirtscreen)
            dirtscreen->set_text("Connecting to\n\n\n" + host + ":" + std::to_string(port));

        status = ErrorStatus::OK;
        state = ConnectionState::CONNECT;
        connect(host, port);

        if (status != ErrorStatus::OK)
        {
            throw std::runtime_error("Failed to connect to server");
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
        buffer.writeString(remote_world->player.player_name);
        send(buffer);

        if (status != ErrorStatus::OK)
            throw std::runtime_error("Failed to connect to server");
    }

    void MinecraftClient::handleHandshake(ByteBuffer &buffer)
    {
        // Read response
        std::string server_hash = buffer.readString();
        if (buffer.underflow)
            return;

        debug::print("Server hash: %s\n", server_hash.c_str());
        GuiDirtscreen *dirtscreen = dynamic_cast<GuiDirtscreen *>(Gui::get_gui());
        if (dirtscreen)
            dirtscreen->set_text("Logging in");
        sendLoginRequest();
    }

    void MinecraftClient::sendLoginRequest()
    {
        // Send login packet
        ByteBuffer buffer;
        buffer.writeByte(0x01);                               // Packet ID
        buffer.writeInt(9);                                   // Protocol version
        buffer.writeString(remote_world->player.player_name); // Player name

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
            throw std::runtime_error("Incompatible server");
        }
        set_world_hell(dimension);

        debug::print("Player entity ID: %d\n", entity_id);
        debug::print("World seed: %lld\n", seed);

        if (remote_world->player.chunk)
        {
            debug::print("Player entity already exists in a chunk even though no chunks should be loaded yet. This must be a bug!!!\n");
            status = ErrorStatus::ERROR;
            return;
        }

        // Remove the old player entity
        remove_entity(remote_world->player.entity_id);

        // Create a new player entity
        remote_world->player = EntityPlayerLocal(Vec3f(0.5, -999, 0.5));
        remote_world->player.entity_id = entity_id;
        add_entity(&remote_world->player);

        if (!remote_world->loaded)
        {
            GuiDirtscreen *dirtscreen = dynamic_cast<GuiDirtscreen *>(Gui::get_gui());
            if (!dirtscreen)
                dirtscreen = new GuiDirtscreen(gertex::GXView());
            dirtscreen->set_text("Loading level\n\n\nDownloading terrain");
            dirtscreen->set_progress(0, 0);
            Gui::set_gui(dirtscreen);
        }
        login_success = true;
        state = ConnectionState::PLAY;
    }

    void MinecraftClient::sendChatMessage(std::string message)
    {
        if (message.size() > 100)
        {
            debug::print("Chat message too long\n");
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
        debug::print("[CHAT] %s\n", message.c_str());
    }

    void MinecraftClient::handleTimeUpdate(ByteBuffer &buffer)
    {
        // Read time update
        int64_t time = buffer.readLong();
        if (buffer.underflow)
            return;
        int32_t new_time = time % 24000;
        // Update the time of day if the difference is greater than half a second
        if (std::abs(new_time - remote_world->time_of_day) > 10)
        {
            remote_world->time_of_day = new_time;
        }
    }

    void MinecraftClient::handlePlayerEquipment(ByteBuffer &buffer)
    {
        // Read entity equipment
        int32_t entity_id = buffer.readInt(); // Entity ID
        int16_t slot = buffer.readShort();    // Slot
        int16_t item = buffer.readShort();    // Item ID
        int16_t meta = buffer.readShort();    // Item metadata

        if (buffer.underflow)
            return;

        EntityPhysical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            debug::print("Received player equipment for unknown entity %d\n", entity_id);
            return;
        }
        EntityPlayer *player = dynamic_cast<EntityPlayer *>(entity);
        if (!player)
        {
            debug::print("Received player equipment for non-player entity %d\n", entity_id);
            return;
        }
        // Update the player's equipment
        if (slot < 0 || slot >= 5)
        {
            debug::print("Invalid equipment slot %d for entity %d\n", slot, entity_id);
            return;
        }
        if (item == -1)
        {
            // Remove the item from the slot
            player->equipment[slot] = inventory::ItemStack(); // Clear the slot
            EntityPlayerLocal *local_player = dynamic_cast<EntityPlayerLocal *>(player);
            if (local_player)
            {
                local_player->items[slot - 5] = inventory::ItemStack();
            }
        }
        else
        {
            // Add the item to the slot
            player->equipment[slot] = inventory::ItemStack(item, 1, meta);
            EntityPlayerLocal *local_player = dynamic_cast<EntityPlayerLocal *>(player);
            if (local_player)
            {
                local_player->items[slot - 5] = inventory::ItemStack(item, 1, meta);
            }
        }
    }

    void MinecraftClient::handlePlayerHealth(ByteBuffer &buffer)
    {
        // Read entity health
        int16_t health = buffer.readShort();
        if (health < remote_world->player.health)
        {
            remote_world->player.hurt_ticks = 10;
        }
        if (remote_world->player.health != health)
            remote_world->player.health_update_tick = 10;
        remote_world->player.health = health;
        if (health <= 0)
        {
            sendRespawn();
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
        debug::print("Respawned\n");
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

        remote_world->player.teleport(Vec3f(x, y, z));
        remote_world->player.on_ground = on_ground;
    }

    void MinecraftClient::handlePlayerLook(ByteBuffer &buffer)
    {
        // Read entity look
        float yaw = buffer.readFloat();
        float pitch = buffer.readFloat();
        bool on_ground = buffer.readBool(); // On ground

        if (buffer.underflow)
            return;

        remote_world->player.rotation = Vec3f(pitch, yaw, 0);
        remote_world->player.on_ground = on_ground;
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

        remote_world->player.teleport(Vec3f(x, y, z));
        remote_world->player.rotation = Vec3f(pitch, yaw, 0);
        remote_world->player.on_ground = on_ground;

        // Mirror the packet back to the server
        sendPlayerPositionLook();
        debug::print("Player position: %f %f %f and look: %f %f\n", x, y, z, yaw, pitch);
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

        EntityPhysical *entity = get_entity_by_id(entity_id);
        EntityPlayer *player = dynamic_cast<EntityPlayer *>(entity);
        if (player)
        {
            player->in_bed = in_bed;
            player->bed_pos = Vec3i(x, y, z);
        }
    }

    void MinecraftClient::sendPlayerPositionLook()
    {
        // Send player position and look packet
        ByteBuffer buffer;
        buffer.writeByte(0x0D); // Packet ID
        buffer.writeDouble(remote_world->player.position.x);
        buffer.writeDouble(remote_world->player.aabb.min.y);
        buffer.writeDouble(remote_world->player.position.y);
        buffer.writeDouble(remote_world->player.position.z);
        buffer.writeFloat(180 - remote_world->player.rotation.y); // Yaw
        buffer.writeFloat(-remote_world->player.rotation.x);      // Pitch
        buffer.writeBool(remote_world->player.on_ground);
        send(buffer);
    }

    void MinecraftClient::sendPlayerPosition()
    {
        // Send player position packet
        ByteBuffer buffer;
        buffer.writeByte(0x0B); // Packet ID
        buffer.writeDouble(remote_world->player.position.x);
        buffer.writeDouble(remote_world->player.position.y);
        buffer.writeDouble(remote_world->player.position.z);
        buffer.writeBool(remote_world->player.on_ground);
        send(buffer);
    }

    void MinecraftClient::sendPlayerLook()
    {
        // Send player look packet
        ByteBuffer buffer;
        buffer.writeByte(0x0C);                                   // Packet ID
        buffer.writeFloat(180 - remote_world->player.rotation.y); // Yaw
        buffer.writeFloat(-remote_world->player.rotation.x);      // Pitch
        buffer.writeBool(remote_world->player.on_ground);
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
#ifdef ITEM_DESYNC_FIX
            /*
             * This is a workaround for the item desync issue.
             * The server expects the stack to be in sync with
             * the client. Sending an invalid stack will cause
             * the server to refresh the item item stack. For
             * this to work, we need to send an item with a
             * count of 0, which is not send by normal clients.
             * This only works for the vanilla server
             */
            buffer.writeByte(0);
#else
            buffer.writeByte(item_count);
#endif
            buffer.writeShort(item_meta);
        }
        send(buffer);
    }

    void MinecraftClient::sendAnimation(uint8_t animation_id)
    {
        // Send player animation packet
        ByteBuffer buffer;
        buffer.writeByte(0x12);                          // Packet ID
        buffer.writeInt(remote_world->player.entity_id); // Entity ID
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

        EntityPlayerMp *entity = new EntityPlayerMp(Vec3f(x, y, z), player_name);
        entity->set_server_pos_rot(Vec3i(x, y, z), Vec3f(pitch, -yaw, 0), 0);
        entity->teleport(Vec3f(x / 32.0, y / 32.0 + 1.0 / 64.0, z / 32.0));
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

        EntityItem *entity = new EntityItem(Vec3f(x, y, z), inventory::ItemStack(item, count, meta));
        entity->set_server_position(Vec3i(x, y, z));
        entity->teleport(Vec3f(x / 32.0, y / 32.0 + 1.0 / 64.0, z / 32.0));
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

        EntityPhysical *pickup_entity = get_entity_by_id(item_entity_id);

        EntityPhysical *entity = get_entity_by_id(entity_id);

        // Make sure the entities exists and the picked up entity is an item entity
        if (!pickup_entity || pickup_entity->type != 1 || !entity)
            return;

        EntityItem *item_entity = dynamic_cast<EntityItem *>(pickup_entity);
        if (!item_entity)
            return;
        item_entity->pickup(entity->get_position(0) - Vec3f(0, 0.5, 0));
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

        EntityPhysical *entity = nullptr;
        switch (type)
        {
        case 70:
            entity = new EntityFallingBlock(Block{uint8_t(BlockID::sand), 0x7F, 0}, Vec3i(x / 32, y / 32, z / 32));
            break;
        case 71:
            entity = new EntityFallingBlock(Block{uint8_t(BlockID::gravel), 0x7F, 0}, Vec3i(x / 32, y / 32, z / 32));
            break;

        default:
            entity = new EntityPhysical();
            debug::print("Generic vehicle %d spawned at %f %f %f with entity type %d\n", entity_id, x, y, z, type);
            break;
        }
        entity->set_server_position(Vec3i(x, y, z));
        entity->teleport(Vec3f(x / 32.0, y / 32.0 + 1.0 / 64.0, z / 32.0));
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

        debug::print("Mob %d spawned at %f %f %f with entity type %d\n", entity_id, x / 32., y / 32., z / 32., int(type & 0xFF));
#ifdef DEBUGENTITIES
        // Print metadata
        for (size_t i = 0; i < metadata.metadata.size(); i++)
        {
            switch (metadata.metadata[i].type)
            {
            case 0:
                debug::print("Metadata %d: %d\n", i, metadata.getByte(i));
                break;
            case 1:
                debug::print("Metadata %d: %d\n", i, metadata.getShort(i));
                break;
            case 2:
                debug::print("Metadata %d: %d\n", i, metadata.getInt(i));
                break;
            case 3:
                debug::print("Metadata %d: %f\n", i, metadata.getFloat(i));
                break;
            case 4:
                debug::print("Metadata %d: %s\n", i, metadata.getString(i).c_str());
                break;
            case 5:
            {
                uint16_t item_id;
                uint8_t item_count;
                uint8_t item_meta;
                metadata.getItem(i, item_id, item_count, item_meta);
                debug::print("Metadata %d: %d %d %d\n", i, item_id, item_count, item_meta);
                break;
            }
            case 6:
            {
                int32_t mx, my, mz;
                metadata.getPosition(i, mx, my, mz);
                debug::print("Metadata %d: %d %d %d\n", i, mx, my, mz);
                break;
            }
            }
        }
#endif
        // Create entity based on type
        EntityPhysical *entity = nullptr;
        switch (type)
        {
        case 50:
            entity = new EntityCreeper(Vec3f(x, y, z) / 32.0);
            break;
        default:
            entity = new EntityLiving();
            break;
        }
        entity->width = 0.6;
        entity->height = 1.8;
        entity->set_server_pos_rot(Vec3i(x, y, z), Vec3f(pitch * (360.0 / 256), yaw * (-360.0 / 256), 0), 0);
        entity->teleport(Vec3f(x / 32.0, y / 32.0 + 1.0 / 64.0, z / 32.0));
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

        EntityPhysical *entity = new EntityPhysical();
        entity->teleport(Vec3f(x, y, z));
        entity->entity_id = entity_id;
        add_entity(entity);

        debug::print("Painting %d named %s spawned at %d %d %d with facing value %d\n", entity_id, title.c_str(), x, y, z, direction);
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

        EntityPhysical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        entity->velocity = Vec3f(dx / 8000.0, dy / 8000.0, dz / 8000.0);
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

        EntityPhysical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        entity->set_server_position(entity->server_pos + Vec3i(dx, dy, dz), 3);
    }

    void MinecraftClient::handleEntityLook(ByteBuffer &buffer)
    {
        // Read look entity
        int32_t entity_id = buffer.readInt();
        int8_t yaw = buffer.readByte();
        int8_t pitch = buffer.readByte();

        if (buffer.underflow)
            return;

        EntityPhysical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        entity->set_server_pos_rot(entity->server_pos, Vec3f(pitch * (360.0 / 256), yaw * (-360.0 / 256), 0), 3);
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

        EntityPhysical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        entity->set_server_pos_rot(entity->server_pos + Vec3i(dx, dy, dz), Vec3f(pitch * (360.0 / 256), yaw * (-360.0 / 256), 0), 3);
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

        EntityPhysical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        entity->set_server_pos_rot(Vec3i(x, y, z), Vec3f(pitch * (360.0 / 256), yaw * (-360.0 / 256), 0), 0);
    }

    void MinecraftClient::handleEntityStatus(ByteBuffer &buffer)
    {
        // Read entity status
        int32_t entity_id = buffer.readInt();
        uint8_t status = buffer.readByte();

        if (buffer.underflow)
            return;

        EntityPhysical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        if (status == 2)
        {
            EntityLiving *living_entity = dynamic_cast<EntityLiving *>(entity);
            if (living_entity)
                living_entity->hurt(0);
        }
        debug::print("Entity %d status: %d\n", entity_id, status);
    }

    void MinecraftClient::handleAttachEntity(ByteBuffer &buffer)
    {
        // Read attach entity
        int32_t entity_id = buffer.readInt();
        int32_t vehicle_id = buffer.readInt();

        if (buffer.underflow)
            return;

        EntityPhysical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            debug::print("Unknown entity %d\n", entity_id);
            return;
        }
        if (vehicle_id == -1)
        {
            debug::print("Entity %d stopped riding entity\n", entity_id);
            return;
        }
        EntityPhysical *vehicle = get_entity_by_id(vehicle_id);
        if (!vehicle)
        {
            debug::print("Entity %d started riding unknown entity %d\n", entity_id, vehicle_id);
            return;
        }
        debug::print("Entity %d started riding entity %d\n", entity_id, vehicle_id);
    }

    void MinecraftClient::handleMetadata(ByteBuffer &buffer)
    {
        // Read metadata
        int32_t entity_id = buffer.readInt();
        DataWatcher metadata;
        metadata.read(buffer);

        if (buffer.underflow)
            return;

        EntityPhysical *entity = get_entity_by_id(entity_id);
        if (!entity)
        {
            return;
        }
        debug::print("Received entity %d metadata\n", entity_id);
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
            Chunk *chunk = get_chunk(x, z);
            if (chunk)
            {
                // Mark chunk for removal
                remote_world->remove_chunk(chunk);

                // Also trigger a cleanup immediately
                remote_world->cleanup_chunks();
            }
        }
#ifdef PREALLOCATE_CHUNKS
        else
        {
            lock_t chunk_lock(chunk_mutex);
            try
            {
                Chunk *chunk = new Chunk(x, z);
                chunk->generation_stage = ChunkGenStage::empty;
                get_chunks().push_back(chunk);
            }
            catch (...)
            {
                debug::print("Chunk initialization failed\n");
                debug::print("Chunk count: %d\n", get_chunks().size());

                std::rethrow_exception(std::current_exception());
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
            debug::print("Wrong decompressed size: %s\n", mz_error(result));
            delete[] decompressed_data;
            return;
        }
        Vec2i chunk_pos = block_to_chunk_pos(Vec3i(start_x, start_y, start_z));

        Chunk *chunk = get_chunk(chunk_pos);
        bool chunk_exists = chunk != nullptr;
        if (!chunk_exists)
        {
            try
            {
                chunk = new Chunk(chunk_pos.x, chunk_pos.y);
            }
            catch (std::bad_alloc &e)
            {
                // This is usually caused by a vanilla server attempting to send all chunks
                // within the default view distance of 10 chunks. The custom server has a
                // smaller view distance which is why this is not an issue on such servers.
                // This can cause the client to run out of memory, especially on the Wii.

                // We will log the error and rethrow an exception to indicate failure.
                debug::print("Failed to allocate memory for chunk\n");
                debug::print("Chunk count: %d\n", get_chunks().size());

                // We need to free the decompressed data before throwing an exception
                delete[] decompressed_data;

                // Rethrow the exception
                throw std::runtime_error("Failed to allocate memory for chunk");
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
                    chunk->set_block(Vec3i(start_x + x, start_y + y, start_z + z), BlockID(block_id));
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
                    chunk->get_block(Vec3i(start_x + x, start_y + y, start_z + z))->meta = metadata & 0x0F;
                    chunk->get_block(Vec3i(start_x + x, start_y + y + 1, start_z + z))->meta = metadata >> 4;
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
                    chunk->get_block(Vec3i(start_x + x, start_y + y, start_z + z))->block_light = light & 0x0F;
                    chunk->get_block(Vec3i(start_x + x, start_y + y + 1, start_z + z))->block_light = light >> 4;
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
                    chunk->get_block(Vec3i(start_x + x, start_y + y, start_z + z))->sky_light = light & 0x0F;
                    chunk->get_block(Vec3i(start_x + x, start_y + y + 1, start_z + z))->sky_light = light >> 4;
                }
            }
        }

        // Free decompressed data
        delete[] decompressed_data;

        chunk->lit_state = 1;
        chunk->generation_stage = ChunkGenStage::done;
        chunk->recalculate_height_map();

        // Update the VBOs
        for (int32_t x = chunk->x - 1; x <= chunk->x + 1; x++)
        {
            for (int32_t z = chunk->z - 1; z <= chunk->z + 1; z++)
            {
                // Skip corners
                if ((x - chunk->x) && (z - chunk->z))
                    continue;

                // Mark neighbor as dirty
                Chunk *neighbor = get_chunk(x, z);
                if (neighbor)
                {
                    // Update the VBOs of the neighboring chunks
                    for (int i = 0; i < VERTICAL_SECTION_COUNT; i++)
                    {
                        neighbor->sections[i].dirty = true;
                    }
                }
            }
        }

        Lock chunk_lock(chunk_mutex);
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

        Vec3i pos(x, y, z);
        Chunk *chunk = get_chunk_from_pos(pos);
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
            Vec3i pos = Vec3i(x + ((coords[i] >> 12) & 0xF), coords[i] & 0xFF, z + ((coords[i] >> 8) & 0xF));
            Chunk *chunk = get_chunk_from_pos(pos);
            if (!chunk)
            {
                debug::print("Could not change block at %d %d %d as chunk doesn't exist!\n", pos.x, pos.y, pos.z);
                continue;
            }
            Block *block = chunk->get_block(pos);
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
        remote_world->create_explosion(Vec3f(x, y, z), radius, nullptr);
    }

    void MinecraftClient::handleSetSlot(ByteBuffer &buffer)
    {
        // Read slot
        uint8_t window_id = buffer.readByte();
        int16_t slot_id = buffer.readShort();
        int16_t item_id = buffer.readShort();

        if (buffer.underflow)
            return;

        inventory::ItemStack item(0, 0, 0);
        if (item_id >= 0)
        {
            uint8_t count = buffer.readByte();
            uint16_t meta = buffer.readShort() & 0xFFFF;
            item = inventory::ItemStack(item_id, count, meta);
        }
        if (buffer.underflow)
            return;
        if (window_id == 0 && slot_id >= 0 && (slot_id & 0xFFFF) < remote_world->player.items.size())
        {
            remote_world->player.items[slot_id] = item;
        }
    }

    void MinecraftClient::disconnect()
    {
        // Send disconnect packet
        ByteBuffer buffer;
        buffer.writeByte(0xFF); // Packet ID
        buffer.writeString("Disconnected by client");
        send(buffer);
        state = ConnectionState::DISCONNECTED;
        close();
    }

    void MinecraftClient::handleWindowItems(ByteBuffer &buffer)
    {
        // Read inventory
        std::vector<inventory::ItemStack> items;

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
                items.push_back(inventory::ItemStack(item_id, count, meta));
            }
            if (buffer.underflow)
                return;
        }

        if (window_id == 0)
        {
            // Update player inventory
            inventory::Container &container = remote_world->player.items;
            for (size_t i = 0; i < items.size() && i < container.size(); i++)
            {
                container[i] = items[i];
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

        debug::print("Sign at %d %d %d updated with text:\n%s\n%s\n%s\n%s\n", x, y, z, text1.c_str(), text2.c_str(), text3.c_str(), text4.c_str());
    }

    void MinecraftClient::handleKick(ByteBuffer &buffer)
    {
        // Read kick message
        std::string message = buffer.readString();
        if (buffer.underflow)
            return;

        throw std::runtime_error("Connection lost\n\n\n" + message);
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
            handleMetadata(buffer);
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
            debug::print("Unknown packet ID: 0x%02X\n", packet_id);
            throw std::runtime_error("Received unknown packet " + std::to_string(packet_id));
        }
        }
        if (!buffer.underflow && status == ErrorStatus::OK)
        {
            buffer.erase(buffer.begin(), buffer.begin() + buffer.offset);
        }
        bool last_message = buffer.underflow || status != ErrorStatus::OK;
        buffer.underflow = false;
        return last_message;
    }

    void MinecraftClient::tick()
    {
        try
        {
            if ((state == ConnectionState::HANDSHAKE || state == ConnectionState::PLAY) && is_connected())
            {
                // Receive packets
                receive(receive_buffer);

                // Handle up to 100 packets
                for (int i = 0; i < 100 && status == Crapper::ErrorStatus::OK; i++)
                {
                    if (handlePacket(receive_buffer))
                        break;
                }
            }
            switch (state)
            {
            case ConnectionState::CONNECT:
            case ConnectionState::HANDSHAKE:
                if (is_connected() && state == ConnectionState::CONNECT)
                {
                    sendHandshake();
                    state = ConnectionState::HANDSHAKE;
                }
                else
                {
                    // Check if the connection has timed out
                    if (watchdog_timer-- <= 0)
                        throw std::runtime_error("Failed to connect to server");
                }
                break;
            case ConnectionState::PLAY:
            {
                // Send keep alive every 900 ticks (every 45 seconds)
                if (remote_world->ticks % 900 == 0)
                    sendKeepAlive();

                // Send the player's position and look every 3 ticks
                if (remote_world->ticks % 3 == 0)
                    sendPlayerPositionLook();

                // Send the player's grounded status if it has changed
                bool on_ground = remote_world->player.on_ground;
                if (on_ground != this->on_ground)
                    sendGrounded(on_ground);

                if (status != ErrorStatus::OK)
                    throw std::runtime_error("Something went wrong");
                break;
            }
            default:
                break;
            }
        }
        catch (...)
        {
            // Update state
            status = ErrorStatus::ERROR;
            state = ConnectionState::DISCONNECTED;

            close();

            // Just clear the buffer as it's no longer usable
            receive_buffer.clear();

            // We'll let the caller handle the error
            std::rethrow_exception(std::current_exception());
        }
    }
} // namespace Crapper