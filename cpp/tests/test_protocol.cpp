#include "packet.hpp"
#include "crc32.hpp"
#include "sliding_window_arq.hpp"
#include "mesh_router.hpp"
#include "priority_scheduler.hpp"

#include <iostream>
#include <cassert>
#include <string>

using namespace tactlink;

void test_crc32_and_framing() {
    TacticalPacket pkt;
    pkt.priority = PacketPriority::TACTICAL_COMMAND;
    pkt.src_node = 1;
    pkt.dst_node = 5;
    pkt.seq_num = 100;
    std::string msg = "ENGAGE_HOSTILE_GRID_BRAVO_44";
    pkt.payload.assign(msg.begin(), msg.end());

    auto wire = pkt.serialize();
    assert(wire.size() == HEADER_SIZE + msg.size() + FOOTER_SIZE);

    TacticalPacket parsed;
    assert(TacticalPacket::deserialize(wire.data(), wire.size(), parsed));
    assert(parsed.src_node == 1);
    assert(parsed.dst_node == 5);
    assert(parsed.seq_num == 100);
    assert(parsed.payload.size() == msg.size());

    // Inject single-bit corruption into payload
    wire[HEADER_SIZE + 2] ^= 0x01;
    TacticalPacket corrupt_pkt;
    assert(!TacticalPacket::deserialize(wire.data(), wire.size(), corrupt_pkt)); // CRC check must fail

    std::cout << "[PASS] CRC-32 and bit-level framing tests passed.\n";
}

void test_selective_repeat_arq() {
    SelectiveRepeatARQ sender(8, 3);
    SelectiveRepeatARQ receiver(8, 3);

    // Send 4 packets
    std::vector<std::vector<uint8_t>> sent_bytes;
    for (int i = 0; i < 4; ++i) {
        TacticalPacket p;
        p.src_node = 10;
        p.dst_node = 20;
        std::string payload = "PACKET_" + std::to_string(i);
        p.payload.assign(payload.begin(), payload.end());
        std::vector<uint8_t> wire;
        assert(sender.send(p, wire));
        sent_bytes.push_back(wire);
    }

    // Deliver packets out-of-order: 0, 2, 3 (packet 1 dropped over RF)
    std::vector<TacticalPacket> delivered;
    uint32_t ack_seq;

    TacticalPacket p0, p2, p3;
    assert(TacticalPacket::deserialize(sent_bytes[0].data(), sent_bytes[0].size(), p0));
    assert(TacticalPacket::deserialize(sent_bytes[2].data(), sent_bytes[2].size(), p2));
    assert(TacticalPacket::deserialize(sent_bytes[3].data(), sent_bytes[3].size(), p3));

    receiver.receive(p0, delivered, ack_seq);
    sender.process_ack(ack_seq);
    assert(delivered.size() == 1); // Only packet 0 delivered immediately

    receiver.receive(p2, delivered, ack_seq);
    sender.process_ack(ack_seq);
    assert(delivered.size() == 1); // Packet 2 buffered, waiting for packet 1

    receiver.receive(p3, delivered, ack_seq);
    sender.process_ack(ack_seq);
    assert(delivered.size() == 1); // Packet 3 buffered

    // Simulate timer ticks on sender -> packet 1 timeout retransmission
    auto retransmissions = sender.tick_timer();
    sender.tick_timer();
    retransmissions = sender.tick_timer();
    assert(retransmissions.size() == 1); // Packet 1 retransmitted!

    // Receive retransmitted packet 1
    TacticalPacket p1;
    assert(TacticalPacket::deserialize(retransmissions[0].data(), retransmissions[0].size(), p1));
    receiver.receive(p1, delivered, ack_seq);
    sender.process_ack(ack_seq);

    // Contiguous stream [0, 1, 2, 3] now delivered!
    assert(delivered.size() == 4);
    assert(delivered[1].seq_num == 1);
    assert(delivered[2].seq_num == 2);
    assert(delivered[3].seq_num == 3);

    std::cout << "[PASS] SelectiveRepeatARQ flow control tests passed.\n";
}

void test_mesh_router_dijkstra() {
    MeshRouter router;

    // 4 Nodes in diamond: 1 (HQ), 2 (Radar), 3 (UAV Relay), 4 (Forward Unit)
    router.add_node({1, "HQ_CENTRAL", "HQ", 0.0, 0.0, {}});
    router.add_node({2, "RADAR_OUTPOST", "RADAR", 10.0, 5.0, {}});
    router.add_node({3, "UAV_RELAY_ALPHA", "UAV", 5.0, 15.0, {}});
    router.add_node({4, "STRIKE_GROUP", "ARMOR", 20.0, 20.0, {}});

    // Link 1 <-> 2 (Good SNR 28dBm, low loss 0.02)
    router.update_link(1, 2, 28.0, 0.02, 5.0);
    // Link 2 <-> 4 (Moderate SNR 15dBm, loss 0.05)
    router.update_link(2, 4, 15.0, 0.05, 8.0);
    // Link 1 <-> 3 (Excellent SNR 35dBm, loss 0.01)
    router.update_link(1, 3, 35.0, 0.01, 3.0);
    // Link 3 <-> 4 (High SNR 32dBm, loss 0.01)
    router.update_link(3, 4, 32.0, 0.01, 4.0);

    // Route from 1 to 4 should prefer 1 -> 3 -> 4 (UAV Relay has better SNR)
    auto route = router.find_route(1, 4);
    assert(route.full_path.size() == 3);
    assert(route.full_path[0] == 1 && route.full_path[1] == 3 && route.full_path[2] == 4);

    // Now simulate EW Jamming on UAV Relay (Node 3)
    router.set_node_jammed(3, true);

    // Route must dynamically failover to 1 -> 2 -> 4!
    auto failover_route = router.find_route(1, 4);
    assert(failover_route.full_path.size() == 3);
    assert(failover_route.full_path[0] == 1 && failover_route.full_path[1] == 2 && failover_route.full_path[2] == 4);

    std::cout << "[PASS] MeshRouter Dijkstra and dynamic failover tests passed.\n";
}

void test_priority_scheduler() {
    PriorityPacketScheduler scheduler;

    TacticalPacket bulk;
    bulk.priority = PacketPriority::BULK_DATA;
    bulk.seq_num = 1;

    TacticalPacket telemetry;
    telemetry.priority = PacketPriority::SENSOR_TELEMETRY;
    telemetry.seq_num = 2;

    TacticalPacket emergency;
    emergency.priority = PacketPriority::EMERGENCY_DISTRESS;
    emergency.seq_num = 3;

    scheduler.enqueue(bulk);
    scheduler.enqueue(telemetry);
    scheduler.enqueue(emergency);

    TacticalPacket out;
    uint64_t ts;
    assert(scheduler.dequeue(out, ts) && out.priority == PacketPriority::EMERGENCY_DISTRESS);
    assert(scheduler.dequeue(out, ts) && out.priority == PacketPriority::SENSOR_TELEMETRY);
    assert(scheduler.dequeue(out, ts) && out.priority == PacketPriority::BULK_DATA);

    std::cout << "[PASS] PriorityPacketScheduler strict QoS tests passed.\n";
}

int main() {
    std::cout << "--- Running TactLink-SDR Unit Test Suite ---\n";
    test_crc32_and_framing();
    test_selective_repeat_arq();
    test_mesh_router_dijkstra();
    test_priority_scheduler();
    std::cout << "--- ALL TACTLINK-SDR UNIT TESTS PASSED ---\n";
    return 0;
}
