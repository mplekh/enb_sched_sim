/* Copyright (C) 2024 Maxim Plekh - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPLv3 license.
 *
 * You should have received a copy of the GPLv3 license with this file.
 * If not, please visit : http://choosealicense.com/licenses/gpl-3.0/
 */

#include <iostream>
#include <sstream>
#include <thread>
#include <cassert>
#include <random>

#include "../common.h"

template<typename T>
class Fifo {
    std::queue<T> queue_{};
    std::mutex mtx_{};
    std::condition_variable cond_empty_{};
    bool done_ = false;

  public:
    Fifo() = default;
    Fifo(const Fifo&) {/*not really copyable*/};
    Fifo& operator = (const Fifo&) = delete;

    Fifo(Fifo&&) = default;
    Fifo& operator = (Fifo&&) = default;
    ~Fifo() {
        done();
    };

    void push(const T& item) {
        std::unique_lock guard(mtx_);
	queue_.push(item);
	cond_empty_.notify_one();
    }

    void push(T&& item) {
        std::unique_lock guard(mtx_);
        queue_.push(std::move(item));
	cond_empty_.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock guard(mtx_);
        cond_empty_.wait(guard, [&]() { return !queue_.empty() || done_; });
        if(done_)
            return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    std::size_t size() {
        std::unique_lock guard(mtx_);
        return queue_.size();
    }

    void done() {
         std::unique_lock guard(mtx_);
         done_ = true;
         cond_empty_.notify_all();
     }
};

class UE {
    uint32_t ue_id_;
    Fifo<ResourceRequest>& uplink_;
    std::vector<Fifo<SchedulerResponse>>& downlink_;

  public:
    UE() = default;
    UE(uint32_t id, Fifo<ResourceRequest>& uplink, std::vector<Fifo<SchedulerResponse>>& downlink)
      : ue_id_(id), uplink_(uplink), downlink_(downlink) {
    }

    void operator()() {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<uint32_t> resource_type(0, 1);
        while(true) {
            const auto dir = cfg.UE_MODE == UeMode::DL_ONLY ? ResourceType::DL
                           : cfg.UE_MODE == UeMode::UL_ONLY ? ResourceType::UL
                           : static_cast<ResourceType>(resource_type(rng));
            const uint32_t L = cfg.L;
            uplink_.push({ue_id_, dir, L});
            // After generating a request, the UE waits for a response message
            SchedulerResponse resp;
            if(!downlink_.at(ue_id_).pop(resp))
                return;
            assert(ue_id_ == resp.ue_id);
            if (cfg.DEBUGPRINTS) {
                std::ostringstream outstr;
                outstr << "UE " << ue_id_ << " received response with status " << resp.status << "\n";
                std::cout << outstr.str();
            }
            // After receiving the response, the UE first sleeps for L subframes
            std::this_thread::sleep_for(L * cfg.SF_TIME_SCALE);
            // If UE receives a success response, it continues generating the next request message
            if(resp.status == AllocationStatus::SUCCESS) continue;
            std::uniform_int_distribution<std::mt19937::result_type> unif_dist(1, L);
            const unsigned sleep_on_busy = unif_dist(rng);
            std::this_thread::sleep_for( sleep_on_busy * cfg.SF_TIME_SCALE);
        }
    }
};

int main() {
    int sockfd;

    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    Fifo<ResourceRequest> uplink_channel;
    std::vector<Fifo<SchedulerResponse>> downlink_channels;
    for(unsigned i = 0; i < cfg.M; i++) {
        downlink_channels.emplace_back();
    }

    std::vector<UE> connected_ues;
    for(unsigned i = 0; i < cfg.M; i++) {
        connected_ues.emplace_back(i, uplink_channel, downlink_channels);
    }

    std::vector<std::thread> ueThreads;
    for(const auto& ue : connected_ues) {
        ueThreads.emplace_back(ue);
    }

    unsigned success_ul{}, success_dl{}, total_ul{}, total_dl{};
    std::vector<unsigned> last_success_sf(cfg.M);
    std::vector<double> avg_success_times(cfg.M);

    for(unsigned i = 1; i <= cfg.SIMULATION_PERIOD_SF; ++i) {
        std::this_thread::sleep_for(cfg.SF_TIME_SCALE);
        const size_t num_requests = uplink_channel.size();
        if (cfg.DEBUGPRINTS) {
            std::ostringstream outstr;
            outstr << "Subframe " << i;
            if (num_requests) outstr << ": aggregating " << num_requests << " requests";
            outstr << "\n";
            std::cout << outstr.str();
        }
        std::vector<ResourceRequest> aggregated_reqests(num_requests);
        for (auto& req : aggregated_reqests) {
            if (!uplink_channel.pop(req)) {
                std::cout << "interrupted" << std::endl;
                break;
            }
        }

        socklen_t len = sizeof(servaddr);
        SockSend(sockfd, servaddr, len, aggregated_reqests);
        if (aggregated_reqests.empty()) continue;

        std::vector<SchedulerResponse> scheduler_response;
        SockRecv(sockfd, servaddr, len, scheduler_response);

        for (auto resp : scheduler_response) {
            downlink_channels.at(resp.ue_id).push(resp);
        }
        // collect statistics after dispatch
        unsigned resp_num = 0;
        for (auto resp : scheduler_response) {
            assert(aggregated_reqests.at(resp_num).ue_id == resp.ue_id);
            const bool is_ul = aggregated_reqests.at(resp_num).resource_type == ResourceType::UL;
            resp_num++;
            if (is_ul) total_ul++; else total_dl++;
            if (resp.status != AllocationStatus::SUCCESS) continue;
            if (is_ul) success_ul++; else success_dl++;
            if (last_success_sf.at(resp.ue_id) != 0) {
                if (avg_success_times.at(resp.ue_id) == 0)
                    avg_success_times.at(resp.ue_id) = (i - last_success_sf.at(resp.ue_id));
                else
                    avg_success_times.at(resp.ue_id) = (avg_success_times.at(resp.ue_id) + (i - last_success_sf.at(resp.ue_id))) / 2.0;
            }
            last_success_sf.at(resp.ue_id) = i;
        }
    }

    for(auto& ch : downlink_channels) {
        ch.done();
    }
    for(auto& t : ueThreads) {
        t.join();
    }
    close(sockfd);

    const double success_rate = 100.0 * (success_ul + success_dl) / (total_ul + total_dl);
    /* Throughput calculation based on number of successful allocations, therefore it have to
     * take into account subframes after simulation end. With short simulation period, throughput
     * numbers will be lower than reported on server side.
     */
    const double ul_blk_per_sf = 1.0 * success_ul * cfg.L / (cfg.SIMULATION_PERIOD_SF + cfg.K - 1);
    const double dl_blk_per_sf = 1.0 * success_dl * cfg.L / (cfg.SIMULATION_PERIOD_SF + cfg.K - 1);
    std::cout << "\nSuccess rate: " << success_rate << "%\n";
    // Throughput calculation assumes 1000 sf/sec regardless of SF_TIME_SCALE value.
    std::cout << "Uplink throughput: " << 1000.0 * ul_blk_per_sf << " bytes/sec\n";
    std::cout << "Downlink throughput: " << 1000.0 * dl_blk_per_sf << " bytes/sec\n";

    std::vector<double> nz_avg_success_times;
    for(auto t : avg_success_times) {
        if (t > 0.0) nz_avg_success_times.push_back(t);
    }
    if (nz_avg_success_times.size() > 0) {
        const double avg_delay = std::accumulate(nz_avg_success_times.cbegin(), nz_avg_success_times.cend(), 0.0) / nz_avg_success_times.size();
        std::cout << "Average delay: " << avg_delay << " ms\n";
    }
    const unsigned num_unserved_ues = avg_success_times.size() - nz_avg_success_times.size();
    if (num_unserved_ues > 0) {
        std::cerr << "Insufficient simulation time, increase SIMULATION_PERIOD_SF parameter\n";
        std::cout << "Number of unserved UEs: " << num_unserved_ues << " (" << 100.0 * num_unserved_ues / avg_success_times.size() << " %)\n";
    }
    exit(EXIT_SUCCESS);
}
