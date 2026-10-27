#pragma once

#include "crc32.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>

namespace tactlink {

constexpr uint16_t FRAME_PREAMBLE = 0xAA55;
constexpr uint8_t PROTOCOL_VERSION = 0x01;
constexpr size_t MAX_PAYLOAD_SIZE = 512;
constexpr size_t HEADER_SIZE = 15; // Preamble(2) + Ver(1) + Priority(1) + Src(2) + Dst(2) + Seq(4) + TTL(1) + Len(2)
constexpr size_t FOOTER_SIZE = 4; // CRC-32 (4 bytes)

enum class PacketPriority : uint8_t {
    EMERGENCY_DISTRESS = 0, // Highest priority: medical evacuation, radar attack warning
    TACTICAL_COMMAND   = 1, // Fire control, weapon engagement, waypoint dispatch
    SENSOR_TELEMETRY   = 2, // Drone GPS, radar strobes, soldier vitals
    BULK_DATA          = 3  // Diagnostic logs, map tile caching
};

inline const char* priority_str(PacketPriority p) {
    switch (p) {
        case PacketPriority::EMERGENCY_DISTRESS: return "EMERGENCY";
        case PacketPriority::TACTICAL_COMMAND:   return "TACTICAL";
        case PacketPriority::SENSOR_TELEMETRY:   return "TELEMETRY";
        case PacketPriority::BULK_DATA:          return "BULK";
        default: return "UNKNOWN";
    }
}

struct TacticalPacket {
    uint16_t preamble = FRAME_PREAMBLE;
    uint8_t version = PROTOCOL_VERSION;
    PacketPriority priority = PacketPriority::SENSOR_TELEMETRY;
    uint16_t src_node = 0;
    uint16_t dst_node = 0;
    uint32_t seq_num = 0;
    uint8_t ttl = 16;
    uint16_t payload_len = 0;
    std::vector<uint8_t> payload;
    uint32_t crc32 = 0;

    /**
     * @brief Serialize packet into raw wire binary format
     */
    std::vector<uint8_t> serialize() const {
        uint16_t len = static_cast<uint16_t>(payload.size());
        size_t total_size = HEADER_SIZE + len + FOOTER_SIZE;
        std::vector<uint8_t> buffer(total_size);

        size_t offset = 0;
        // Preamble (Big-Endian)
        buffer[offset++] = static_cast<uint8_t>((preamble >> 8) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>(preamble & 0xFF);

        buffer[offset++] = version;
        buffer[offset++] = static_cast<uint8_t>(priority);

        // Src Node
        buffer[offset++] = static_cast<uint8_t>((src_node >> 8) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>(src_node & 0xFF);

        // Dst Node
        buffer[offset++] = static_cast<uint8_t>((dst_node >> 8) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>(dst_node & 0xFF);

        // Sequence Number (Big-Endian)
        buffer[offset++] = static_cast<uint8_t>((seq_num >> 24) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>((seq_num >> 16) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>((seq_num >> 8) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>(seq_num & 0xFF);

        buffer[offset++] = ttl;

        // Payload Length
        buffer[offset++] = static_cast<uint8_t>((len >> 8) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>(len & 0xFF);

        // Payload
        if (len > 0) {
            std::memcpy(&buffer[offset], payload.data(), len);
            offset += len;
        }

        // Calculate CRC-32 over Header + Payload
        uint32_t computed_crc = CRC32::calculate(buffer.data(), offset);

        buffer[offset++] = static_cast<uint8_t>((computed_crc >> 24) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>((computed_crc >> 16) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>((computed_crc >> 8) & 0xFF);
        buffer[offset++] = static_cast<uint8_t>(computed_crc & 0xFF);

        return buffer;
    }

    /**
     * @brief Deserialize and verify incoming raw wire binary format
     */
    static bool deserialize(const uint8_t* data, size_t size, TacticalPacket& out_pkt) {
        if (size < HEADER_SIZE + FOOTER_SIZE) return false;

        size_t offset = 0;
        out_pkt.preamble = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        if (out_pkt.preamble != FRAME_PREAMBLE) return false;

        out_pkt.version = data[offset++];
        if (out_pkt.version != PROTOCOL_VERSION) return false;

        out_pkt.priority = static_cast<PacketPriority>(data[offset++]);

        out_pkt.src_node = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;

        out_pkt.dst_node = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;

        out_pkt.seq_num = (static_cast<uint32_t>(data[offset]) << 24) |
                          (static_cast<uint32_t>(data[offset + 1]) << 16) |
                          (static_cast<uint32_t>(data[offset + 2]) << 8) |
                          data[offset + 3];
        offset += 4;

        out_pkt.ttl = data[offset++];

        out_pkt.payload_len = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;

        if (out_pkt.payload_len > MAX_PAYLOAD_SIZE || offset + out_pkt.payload_len + FOOTER_SIZE > size) {
            return false;
        }

        out_pkt.payload.resize(out_pkt.payload_len);
        if (out_pkt.payload_len > 0) {
            std::memcpy(out_pkt.payload.data(), &data[offset], out_pkt.payload_len);
            offset += out_pkt.payload_len;
        }

        uint32_t received_crc = (static_cast<uint32_t>(data[offset]) << 24) |
                                (static_cast<uint32_t>(data[offset + 1]) << 16) |
                                (static_cast<uint32_t>(data[offset + 2]) << 8) |
                                data[offset + 3];
        out_pkt.crc32 = received_crc;

        // Verify CRC-32 integrity
        uint32_t computed_crc = CRC32::calculate(data, offset);
        return (computed_crc == received_crc);
    }
};

} // namespace tactlink
