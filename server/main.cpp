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

    unsigned subframes_left = cfg.SIMULATION_PERIOD_SF;
    while (subframes_left--) {
        std::cout << "subframe " << cfg.SIMULATION_PERIOD_SF - subframes_left << "\n";

        std::vector<ResourceRequest> aggregated_reqests;
        socklen_t len = sizeof(servaddr);
        SockRecv(sockfd, servaddr, len, aggregated_reqests);
        for (auto req : aggregated_reqests) {
            if (cfg.DEBUGPRINTS) {
                std::cout << "Request from " << req.ue_id << " for " << req.data_length << " blocks in " << req.resource_type << "\n";
            }
        }
        if(aggregated_reqests.empty()) continue;
        std::vector<SchedulerResponse> scheduler_response;

        for (auto req : aggregated_reqests) {
            scheduler_response.push_back({req.ue_id, AllocationStatus::SUCCESS});
        }
        SockSend(sockfd, servaddr, len, scheduler_response);
    }
    close(sockfd);
    exit(EXIT_SUCCESS);
}
