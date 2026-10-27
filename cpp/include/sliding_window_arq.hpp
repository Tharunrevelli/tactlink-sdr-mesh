#pragma once

#include "packet.hpp"
#include <vector>
#include <array>
#include <unordered_map>
#include <chrono>
#include <cstdint>
#include <iostream>

namespace tactlink {

constexpr size_t ARQ_WINDOW_SIZE = 16;
constexpr uint32_t DEFAULT_RETRANSMIT_TIMEOUT_TICKS = 5;

struct TxEntry {
    TacticalPacket packet;
    bool acked = false;
    uint32_t timeout_counter = 0;
    uint32_t retransmit_count = 0;
};

struct RxEntry {
    TacticalPacket packet;
    bool received = false;
};

/**
 * @brief Selective Repeat ARQ (Automatic Repeat reQuest) Flow Control
 * Provides guaranteed reliable delivery over lossy, fading tactical radio channels.
 */
class SelectiveRepeatARQ {
public:
    SelectiveRepeatARQ(size_t window_size = ARQ_WINDOW_SIZE, uint32_t rto_ticks = DEFAULT_RETRANSMIT_TIMEOUT_TICKS)
        : window_size_(window_size),
          rto_ticks_(rto_ticks),
          send_base_(0),
          next_seq_num_(0),
          recv_base_(0),
          total_retransmissions_(0),
          total_delivered_(0) {
        tx_window_.resize(window_size_);
        rx_window_.resize(window_size_);
    }

    /**
     * @brief Sender sends a packet (if window has capacity)
     */
    bool send(TacticalPacket pkt, std::vector<uint8_t>& wire_bytes) {
        if (next_seq_num_ >= send_base_ + window_size_) {
            return false; // Window full, backpressure
        }

        pkt.seq_num = next_seq_num_;
        size_t idx = next_seq_num_ % window_size_;

        TxEntry entry;
        entry.packet = pkt;
        entry.acked = false;
        entry.timeout_counter = rto_ticks_;
        entry.retransmit_count = 0;

        tx_window_[idx] = entry;
        wire_bytes = pkt.serialize();
        next_seq_num_++;
        return true;
    }

    /**
     * @brief Handle incoming ACK from remote receiver
     */
    void process_ack(uint32_t ack_seq) {
        if (ack_seq >= send_base_ && ack_seq < next_seq_num_) {
            size_t idx = ack_seq % window_size_;
            tx_window_[idx].acked = true;

            // Slide send window if base packet was acked
            while (send_base_ < next_seq_num_ && tx_window_[send_base_ % window_size_].acked) {
                send_base_++;
            }
        }
    }

    /**
     * @brief Handle incoming NACK (fast retransmission request)
     */
    bool process_nack(uint32_t nack_seq, std::vector<uint8_t>& retransmit_bytes) {
        if (nack_seq >= send_base_ && nack_seq < next_seq_num_) {
            size_t idx = nack_seq % window_size_;
            if (!tx_window_[idx].acked) {
                tx_window_[idx].retransmit_count++;
                tx_window_[idx].timeout_counter = rto_ticks_;
                total_retransmissions_++;
                retransmit_bytes = tx_window_[idx].packet.serialize();
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Tick sender timer: triggers retransmission for timed-out packets
     */
    std::vector<std::vector<uint8_t>> tick_timer() {
        std::vector<std::vector<uint8_t>> retransmissions;
        for (uint32_t seq = send_base_; seq < next_seq_num_; ++seq) {
            size_t idx = seq % window_size_;
            if (!tx_window_[idx].acked) {
                if (tx_window_[idx].timeout_counter > 0) {
                    tx_window_[idx].timeout_counter--;
                }
                if (tx_window_[idx].timeout_counter == 0) {
                    // Timeout expired, retransmit
                    tx_window_[idx].timeout_counter = rto_ticks_;
                    tx_window_[idx].retransmit_count++;
                    total_retransmissions_++;
                    retransmissions.push_back(tx_window_[idx].packet.serialize());
                }
            }
        }
        return retransmissions;
    }

    /**
     * @brief Receiver processes incoming packet from wire
     * Returns true if packet was valid and buffered/delivered, outputs delivery batch and ACK sequence.
     */
    bool receive(const TacticalPacket& pkt, std::vector<TacticalPacket>& delivered_packets, uint32_t& ack_to_send) {
        ack_to_send = pkt.seq_num;

        // Packet within current receive window
        if (pkt.seq_num >= recv_base_ && pkt.seq_num < recv_base_ + window_size_) {
            size_t idx = pkt.seq_num % window_size_;
            rx_window_[idx].packet = pkt;
            rx_window_[idx].received = true;

            // Deliver in-order contiguous packets
            while (rx_window_[recv_base_ % window_size_].received) {
                size_t base_idx = recv_base_ % window_size_;
                delivered_packets.push_back(rx_window_[base_idx].packet);
                rx_window_[base_idx].received = false; // reset slot
                recv_base_++;
                total_delivered_++;
            }
            return true;
        } else if (pkt.seq_num < recv_base_) {
            // Duplicate of already-delivered packet (ACK was likely lost)
            return true; // Re-send ACK
        }

        return false; // Out of window
    }

    uint32_t send_base() const { return send_base_; }
    uint32_t next_seq_num() const { return next_seq_num_; }
    uint32_t recv_base() const { return recv_base_; }
    uint32_t total_retransmissions() const { return total_retransmissions_; }
    uint32_t total_delivered() const { return total_delivered_; }

private:
    size_t window_size_;
    uint32_t rto_ticks_;
    uint32_t send_base_;
    uint32_t next_seq_num_;
    uint32_t recv_base_;
    uint32_t total_retransmissions_;
    uint32_t total_delivered_;

    std::vector<TxEntry> tx_window_;
    std::vector<RxEntry> rx_window_;
};

} // namespace tactlink
