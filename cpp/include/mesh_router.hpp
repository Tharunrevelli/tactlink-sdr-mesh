#pragma once

#include "packet.hpp"
#include <vector>
#include <queue>
#include <unordered_map>
#include <string>
#include <limits>
#include <algorithm>
#include <iomanip>
#include <iostream>

namespace tactlink {

constexpr double LINK_INF = 1e9;

struct LinkMetric {
    uint16_t neighbor_id;
    double snr_dbm;      // RF Signal-to-Noise Ratio (dBm)
    double packet_loss;  // [0.0, 1.0]
    double latency_ms;   // round-trip latency
    bool is_jammed = false;

    double compute_cost() const {
        if (is_jammed || packet_loss >= 0.85 || snr_dbm <= 3.0) {
            return LINK_INF; // Link severed or jammed by EW
        }
        // Lower SNR, higher loss, higher latency -> higher cost
        return (100.0 / std::max(snr_dbm, 1.0)) * (1.0 + 4.0 * packet_loss) + (latency_ms * 0.1);
    }
};

struct TacticalNode {
    uint16_t node_id;
    std::string callsign;
    std::string role; // e.g., "HQ_COMMAND", "RADAR_OUTPOST", "UAV_RELAY", "ARMOR_UNIT"
    double latitude;
    double longitude;
    std::vector<LinkMetric> links;
};

struct RouteInfo {
    uint16_t next_hop = 0;
    double total_cost = LINK_INF;
    std::vector<uint16_t> full_path;
};

/**
 * @brief Dynamic Tactical Mesh Router using Dijkstra's Shortest Path Algorithm
 * Optimized for mobile ad-hoc tactical battlefield topologies with RF electronic warfare jamming.
 */
class MeshRouter {
public:
    MeshRouter() = default;

    void add_node(const TacticalNode& node) {
        nodes_[node.node_id] = node;
    }

    void update_link(uint16_t u, uint16_t v, double snr_dbm, double packet_loss, double latency_ms, bool is_jammed = false) {
        set_directed_link(u, v, snr_dbm, packet_loss, latency_ms, is_jammed);
        set_directed_link(v, u, snr_dbm, packet_loss, latency_ms, is_jammed);
    }

    void set_node_jammed(uint16_t node_id, bool jammed) {
        if (nodes_.find(node_id) == nodes_.end()) return;
        for (auto& link : nodes_[node_id].links) {
            link.is_jammed = jammed;
        }
        // Also update inbound links to this node
        for (auto& kv : nodes_) {
            for (auto& l : kv.second.links) {
                if (l.neighbor_id == node_id) {
                    l.is_jammed = jammed;
                }
            }
        }
    }

    /**
     * @brief Compute Shortest Path via Dijkstra's Algorithm
     */
    RouteInfo find_route(uint16_t src, uint16_t dst) const {
        RouteInfo route;
        if (nodes_.find(src) == nodes_.end() || nodes_.find(dst) == nodes_.end()) {
            return route;
        }

        if (src == dst) {
            route.next_hop = src;
            route.total_cost = 0.0;
            route.full_path = {src};
            return route;
        }

        // Min-heap for Dijkstra: (cost, node_id)
        using PQElement = std::pair<double, uint16_t>;
        std::priority_queue<PQElement, std::vector<PQElement>, std::greater<PQElement>> pq;

        std::unordered_map<uint16_t, double> dist;
        std::unordered_map<uint16_t, uint16_t> parent;

        for (const auto& kv : nodes_) {
            dist[kv.first] = LINK_INF;
        }

        dist[src] = 0.0;
        pq.push({0.0, src});

        while (!pq.empty()) {
            auto top = pq.top();
            pq.pop();

            double d = top.first;
            uint16_t u = top.second;

            if (d > dist[u]) continue;
            if (u == dst) break; // Found target

            const auto& node = nodes_.at(u);
            for (const auto& link : node.links) {
                uint16_t v = link.neighbor_id;
                double cost = link.compute_cost();
                if (cost >= LINK_INF) continue;

                if (dist[u] + cost < dist[v]) {
                    dist[v] = dist[u] + cost;
                    parent[v] = u;
                    pq.push({dist[v], v});
                }
            }
        }

        if (dist[dst] >= LINK_INF) {
            return route; // Unreachable / network partitioned
        }

        // Reconstruct path
        std::vector<uint16_t> path;
        uint16_t curr = dst;
        while (curr != src) {
            path.push_back(curr);
            curr = parent[curr];
        }
        path.push_back(src);
        std::reverse(path.begin(), path.end());

        route.total_cost = dist[dst];
        route.full_path = path;
        route.next_hop = (path.size() > 1) ? path[1] : src;

        return route;
    }

    const std::unordered_map<uint16_t, TacticalNode>& get_nodes() const {
        return nodes_;
    }

private:
    void set_directed_link(uint16_t u, uint16_t v, double snr, double loss, double lat, bool jammed) {
        if (nodes_.find(u) == nodes_.end()) return;
        auto& links = nodes_[u].links;
        for (auto& l : links) {
            if (l.neighbor_id == v) {
                l.snr_dbm = snr;
                l.packet_loss = loss;
                l.latency_ms = lat;
                l.is_jammed = jammed;
                return;
            }
        }
        links.push_back({v, snr, loss, lat, jammed});
    }

    std::unordered_map<uint16_t, TacticalNode> nodes_;
};

} // namespace tactlink
