/* Copyright (C) 2024 Maxim Plekh - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPLv3 license.
 *
 * You should have received a copy of the GPLv3 license with this file.
 * If not, please visit : http://choosealicense.com/licenses/gpl-3.0/
 */

#include <iostream>

#include "scheduler.h"
#include "../common.h"

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    if(bind(sockfd, reinterpret_cast<const struct sockaddr *>(&servaddr), sizeof(servaddr)) < 0 ) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    Scheduler schUplink(cfg.SIMULATION_PERIOD_SF, cfg.K, cfg.N);
    Scheduler schDownlink(cfg.SIMULATION_PERIOD_SF, cfg.K, cfg.N);

    unsigned current_sf;
    for (current_sf = 0; current_sf < cfg.SIMULATION_PERIOD_SF; current_sf++) {
        std::cout << "subframe " << current_sf << "\n";

        std::vector<ResourceRequest> aggregated_reqests;
        socklen_t len = sizeof(servaddr);
        SockRecv(sockfd, servaddr, len, aggregated_reqests);
        if(aggregated_reqests.empty()) continue;

        unsigned ul_requests = 0;
        unsigned dl_requests = 0;
        for (auto req : aggregated_reqests) {
            if (cfg.DEBUGPRINTS) {
                std::cout << "Request from " << req.ue_id << " for " << req.data_length << " blocks in " << req.resource_type << "\n";
            }
            if (req.resource_type == ResourceType::UL) {
                ul_requests++;
            } else if (req.resource_type == ResourceType::DL) {
                dl_requests++;
            } else {
                std::cerr << "Invalid resource_type requested\n";
            }

        }

        unsigned ul_allocated_count = schUplink.reserve(current_sf, cfg.L, ul_requests);
        unsigned dl_allocated_count = schDownlink.reserve(current_sf, cfg.L, dl_requests);
        if (cfg.DEBUGPRINTS) {
            if (ul_requests) std::cout << "UL: allocated " << ul_allocated_count << " of requested " << ul_requests << "\n";
            if (dl_requests) std::cout << "DL: allocated " << dl_allocated_count << " of requested " << dl_requests << "\n";
        }

        std::vector<SchedulerResponse> scheduler_response;
        for (auto req : aggregated_reqests) {
            assert(req.data_length == cfg.L);
            AllocationStatus status = AllocationStatus::FAIL;
            if (req.resource_type == ResourceType::UL && ul_allocated_count > 0) {
                ul_allocated_count--;
                status = AllocationStatus::SUCCESS;
            } else if (req.resource_type == ResourceType::DL && dl_allocated_count > 0) {
                dl_allocated_count--;
                status = AllocationStatus::SUCCESS;
            }
            scheduler_response.push_back({req.ue_id, status});
        }

        SockSend(sockfd, servaddr, len, scheduler_response);
    }
    if (cfg.DEBUGPRINTS) {
        /* Print num of reserved blocks in each subframe,
         * reservation window of last subframe is in square brackets
         */
        std::cout << "Reserved blocks in UL subframes:\n";
        schUplink.printWindow(cfg.SIMULATION_PERIOD_SF - 1, cfg.K);
        std::cout << "Reserved blocks in DL subframes:\n";
        schDownlink.printWindow(cfg.SIMULATION_PERIOD_SF - 1, cfg.K);
    }
    const double success_rate = 100.0 * (schUplink.success + schDownlink.success) / (schUplink.total + schDownlink.total);
    const double ul_blk_per_sf = schUplink.avgBlockPerSf(0, cfg.SIMULATION_PERIOD_SF - 1);
    const double dl_blk_per_sf = schUplink.avgBlockPerSf(0, cfg.SIMULATION_PERIOD_SF - 1);
    std::cout << "\nSuccess rate: " << success_rate << "%\n";
    // Throughput calculation assumes 1000 sf/sec regardless of SF_TIME value.
    std::cout << "Uplink throughput: " << 1000.0 * ul_blk_per_sf << " bytes/sec\n";
    std::cout << "Downlink throughput: " << 1000.0 * dl_blk_per_sf << " bytes/sec\n";
    std::cout << "Uplink utilization: " << 100.0 * ul_blk_per_sf / cfg.N << " %\n";
    std::cout << "Downlink utilization: " << 100.0 * dl_blk_per_sf / cfg.N << " %\n";
    close(sockfd);
    exit(EXIT_SUCCESS);
}
