/* Copyright (C) 2024 Maxim Plekh - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPLv3 license.
 *
 * You should have received a copy of the GPLv3 license with this file.
 * If not, please visit : http://choosealicense.com/licenses/gpl-3.0/
 */

#include <iostream>
#include <cstring>
#include <cassert>
#include <algorithm>

#include "../common.h"

class Scheduler {
    unsigned window_len_;
    unsigned rb_per_sf_;
    std::vector<unsigned> subframes;

  public:
    Scheduler(unsigned simulation_len, unsigned window_len, unsigned rb_per_sf)
      : window_len_(window_len),
        rb_per_sf_(rb_per_sf),
        subframes(simulation_len + window_len - 1) {
    }

    unsigned reserve(unsigned current_sf, unsigned data_len, unsigned num) {
        const auto window_begin = subframes.begin() + current_sf;
        const auto window_end = window_begin + window_len_;

        auto is_free = [this](unsigned n) { return n < rb_per_sf_; };
        auto inc = [](unsigned& n) { n++; };

        unsigned num_reserved;
        auto first = window_begin;
        for(num_reserved = 0; num_reserved < num; num_reserved++) {
            first = std::find_if(first, window_end, is_free);
            if(first + data_len > window_end) break;
            std::for_each(first, first + data_len, inc);
        }
        return num_reserved;
    }

    void printWindow(unsigned from, unsigned len) {
        auto print = [](const unsigned& n) { std::cout << n << ' '; };
        const auto first = subframes.cbegin() + from;
        std::for_each(subframes.cbegin(), first, print);
        std::cout << "[ ";
        std::for_each(first, first + len, print);
        std::cout << "] ";
        assert(first + len <= subframes.cend());
        std::for_each(first + len, subframes.cend(), print);
        std::cout << "\n";
    }
};

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
    close(sockfd);
    exit(EXIT_SUCCESS);
}
