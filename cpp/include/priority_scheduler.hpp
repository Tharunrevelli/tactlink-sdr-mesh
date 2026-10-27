#pragma once

#include "packet.hpp"
#include <queue>
#include <vector>
#include <cstdint>

namespace tactlink {

struct QueuedPacket {
    TacticalPacket packet;
    uint64_t enqueue_timestamp_us = 0;

    bool operator>(const QueuedPacket& o) const {
        if (packet.priority != o.packet.priority) {
            return static_cast<uint8_t>(packet.priority) > static_cast<uint8_t>(o.packet.priority); // lower value = higher priority
        }
        return packet.seq_num > o.packet.seq_num; // FIFO within same priority
    }
};

/**
 * @brief Strict Multilevel Priority Scheduler with Strict QoS guarantees
 */
class PriorityPacketScheduler {
public:
    PriorityPacketScheduler(size_t max_queue_capacity = 1024)
        : max_capacity_(max_queue_capacity) {}

    bool enqueue(TacticalPacket pkt, uint64_t timestamp_us = 0) {
        if (heap_.size() >= max_capacity_) {
            // Drop lowest priority if full, or reject
            if (pkt.priority == PacketPriority::BULK_DATA) return false;
        }
        heap_.push({std::move(pkt), timestamp_us});
        return true;
    }

    bool dequeue(TacticalPacket& out_pkt, uint64_t& out_timestamp_us) {
        if (heap_.empty()) return false;
        out_pkt = std::move(const_cast<TacticalPacket&>(heap_.top().packet));
        out_timestamp_us = heap_.top().enqueue_timestamp_us;
        heap_.pop();
        return true;
    }

    size_t size() const { return heap_.size(); }
    bool empty() const { return heap_.empty(); }

    void clear() {
        while (!heap_.empty()) heap_.pop();
    }

private:
    size_t max_capacity_;
    std::priority_queue<QueuedPacket, std::vector<QueuedPacket>, std::greater<QueuedPacket>> heap_;
};

} // namespace tactlink
