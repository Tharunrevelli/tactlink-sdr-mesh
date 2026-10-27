#include "packet.hpp"
#include "crc32.hpp"
#include "sliding_window_arq.hpp"
#include "mesh_router.hpp"
#include "priority_scheduler.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>

using namespace tactlink;

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  TACTLINK-SDR: TACTICAL SDR MESH & PROTOCOL ENGINE       \n";
    std::cout << "  Resilient Defence Data Link // MIL-STD-188-220 Spec    \n";
    std::cout << "==========================================================\n\n";

    MeshRouter router;

    // 6 Tactical Nodes
    router.add_node({1, "HQ_CORPS_COMMAND", "HQ", 28.6139, 77.2090, {}});
    router.add_node({2, "RADAR_OUTPOST_NORTH", "RADAR", 29.1000, 77.5000, {}});
    router.add_node({3, "UAV_SURVEILLANCE_RELAY", "UAV", 28.9500, 77.3500, {}});
    router.add_node({4, "ARMORED_TASK_FORCE", "ARMOR", 29.4000, 77.6000, {}});
    router.add_node({5, "NAVAL_PATROL_COASTAL", "NAVAL", 28.2000, 77.0000, {}});
    router.add_node({6, "ARTILLERY_FIRE_BATTERY", "ARTILLERY", 29.2500, 77.4000, {}});

    // Initial RF Mesh Links (SNR in dBm, Loss Rate, Latency in ms)
    router.update_link(1, 2, 24.0, 0.03, 6.0);
    router.update_link(1, 3, 34.0, 0.01, 2.5); // Fast aerial line-of-sight
    router.update_link(1, 5, 26.0, 0.02, 5.0);
    router.update_link(2, 4, 18.0, 0.06, 8.0);
    router.update_link(2, 6, 22.0, 0.04, 7.0);
    router.update_link(3, 4, 30.0, 0.01, 3.0); // UAV to Forward Armor
    router.update_link(3, 6, 28.0, 0.02, 4.0);
    router.update_link(4, 6, 20.0, 0.05, 5.0);

    SelectiveRepeatARQ sender_arq(16, 4);
    SelectiveRepeatARQ receiver_arq(16, 4);
    PriorityPacketScheduler priority_scheduler;

    std::mt19937 rng(1337);
    std::uniform_real_distribution<double> drop_prob(0.0, 1.0);
    std::uniform_real_distribution<double> bit_flip_prob(0.0, 1.0);

    const int total_messages = 80;
    std::vector<std::string> sample_commands = {
        "ENGAGE_TARGET_T102_VECTOR_280",
        "RADAR_TRACK_UPDATE_IFF_HOSTILE",
        "DRONE_HEADING_WAYPOINT_BRAVO",
        "MEDICAL_EVAC_GRID_DELTA_8",
        "FIRE_AUTHORIZATION_CODE_ALPHA_9"
    };

    std::ofstream json_out("tactlink_telemetry.json");
    json_out << "{\n  \"events\": [\n";

    size_t successful_transmissions = 0;
    size_t crc_errors_detected = 0;
    size_t packets_dropped_in_air = 0;
    double total_serialization_time_us = 0.0;

    std::cout << "[SIMULATION] Dispatching tactical datalink telemetry & command stream...\n";

    for (int step = 0; step < total_messages; ++step) {
        // At step 35, Electronic Warfare (EW) jamming hits Node 3 (UAV Relay)
        if (step == 35) {
            std::cout << "\n[ALERT] Hostile RF Jamming detected on UAV_SURVEILLANCE_RELAY (Node 3)!\n";
            std::cout << "[ROUTING] Dynamic Dijkstra recalculating failover topological paths...\n\n";
            router.set_node_jammed(3, true);
        }

        // Generate packet
        TacticalPacket pkt;
        pkt.src_node = 1; // HQ Command
        pkt.dst_node = 4; // Armored Task Force

        if (step % 20 == 0) {
            pkt.priority = PacketPriority::EMERGENCY_DISTRESS;
        } else if (step % 5 == 0) {
            pkt.priority = PacketPriority::TACTICAL_COMMAND;
        } else if (step % 2 == 0) {
            pkt.priority = PacketPriority::SENSOR_TELEMETRY;
        } else {
            pkt.priority = PacketPriority::BULK_DATA;
        }

        std::string payload_str = sample_commands[step % sample_commands.size()] + "_SEQ_" + std::to_string(step);
        pkt.payload.assign(payload_str.begin(), payload_str.end());

        // Dynamic routing check
        auto route = router.find_route(pkt.src_node, pkt.dst_node);

        // Schedule packet
        priority_scheduler.enqueue(pkt, step * 1000);

        TacticalPacket scheduled_pkt;
        uint64_t ts;
        priority_scheduler.dequeue(scheduled_pkt, ts);

        // ARQ Send
        std::vector<uint8_t> wire_bytes;
        auto t_start = std::chrono::high_resolution_clock::now();
        sender_arq.send(scheduled_pkt, wire_bytes);
        auto t_end = std::chrono::high_resolution_clock::now();
        total_serialization_time_us += std::chrono::duration<double, std::micro>(t_end - t_start).count();

        // RF Channel Simulation: 4% packet drop, 3% bit flip noise
        bool dropped = (drop_prob(rng) < 0.05);
        bool bit_flip = (bit_flip_prob(rng) < 0.03);

        if (bit_flip && !wire_bytes.empty()) {
            wire_bytes[wire_bytes.size() / 2] ^= 0x40; // Flip single bit
        }

        bool delivered = false;
        std::string status_msg = "DELIVERED";

        if (dropped) {
            packets_dropped_in_air++;
            status_msg = "RF_DROP_RETRANSMIT";
        } else {
            TacticalPacket rx_pkt;
            bool crc_valid = TacticalPacket::deserialize(wire_bytes.data(), wire_bytes.size(), rx_pkt);
            if (!crc_valid) {
                crc_errors_detected++;
                status_msg = "CRC_ERROR_REJECTED";
            } else {
                std::vector<TacticalPacket> delivered_batch;
                uint32_t ack_seq = 0;
                receiver_arq.receive(rx_pkt, delivered_batch, ack_seq);
                sender_arq.process_ack(ack_seq);
                successful_transmissions++;
                delivered = true;
            }
        }

        // Tick ARQ retransmission timer
        auto retrans = sender_arq.tick_timer();
        for (const auto& r_bytes : retrans) {
            TacticalPacket retried;
            if (TacticalPacket::deserialize(r_bytes.data(), r_bytes.size(), retried)) {
                std::vector<TacticalPacket> batch;
                uint32_t ack;
                receiver_arq.receive(retried, batch, ack);
                sender_arq.process_ack(ack);
                successful_transmissions++;
            }
        }

        // Export event to JSON
        json_out << "    {\n";
        json_out << "      \"step\": " << step << ",\n";
        json_out << "      \"seq_num\": " << scheduled_pkt.seq_num << ",\n";
        json_out << "      \"priority\": \"" << priority_str(scheduled_pkt.priority) << "\",\n";
        json_out << "      \"src\": " << scheduled_pkt.src_node << ",\n";
        json_out << "      \"dst\": " << scheduled_pkt.dst_node << ",\n";
        json_out << "      \"path\": [";
        for (size_t p = 0; p < route.full_path.size(); ++p) {
            json_out << route.full_path[p] << (p + 1 < route.full_path.size() ? ", " : "");
        }
        json_out << "],\n";
        json_out << "      \"route_cost\": " << std::fixed << std::setprecision(2) << route.total_cost << ",\n";
        json_out << "      \"status\": \"" << status_msg << "\",\n";
        json_out << "      \"bytes\": " << wire_bytes.size() << ",\n";
        json_out << "      \"uav_jammed\": " << (step >= 35 ? "true" : "false") << "\n";
        json_out << "    }" << (step + 1 < total_messages ? "," : "") << "\n";

        if (step % 20 == 0 || step == total_messages - 1) {
            std::cout << "  Step " << std::setw(2) << step
                      << " | Seq: " << std::setw(2) << scheduled_pkt.seq_num
                      << " | Priority: " << std::setw(10) << priority_str(scheduled_pkt.priority)
                      << " | Path: [";
            for (size_t p = 0; p < route.full_path.size(); ++p) {
                std::cout << route.full_path[p] << (p + 1 < route.full_path.size() ? "->" : "");
            }
            std::cout << "] | Status: " << status_msg << "\n";
        }
    }

    json_out << "  ]\n}\n";
    json_out.close();

    std::cout << "\n------------------ BENCHMARK RESULTS ------------------\n";
    std::cout << "  Total Packets Dispatched    : " << total_messages << "\n";
    std::cout << "  Successful In-Order Delivery: " << receiver_arq.total_delivered() << "\n";
    std::cout << "  CRC-32 Corruptions Blocked  : " << crc_errors_detected << "\n";
    std::cout << "  ARQ Retransmissions Triggered: " << sender_arq.total_retransmissions() << "\n";
    std::cout << "  Avg Frame Serialize Latency : " << std::fixed << std::setprecision(3) 
              << (total_serialization_time_us / total_messages) << " microseconds\n";
    std::cout << "  Telemetry export saved to   : tactlink_telemetry.json\n";
    std::cout << "--------------------------------------------------------\n";

    return 0;
}
