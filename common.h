/* Copyright (C) 2024 Maxim Plekh - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPLv3 license.
 *
 * You should have received a copy of the GPLv3 license with this file.
 * If not, please visit : http://choosealicense.com/licenses/gpl-3.0/
 */
#pragma once

#include <exception>
#include <fstream>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>

#include <arpa/inet.h>
#include <unistd.h>
#define PORT 8080

const std::string CFG_FILE("../../../enbsim.cfg");

enum class UeMode {
    UL_ONLY,
    DL_ONLY,
    MIXED
};

enum ResourceType : uint32_t {
    UL = 0,
    DL
};

std::ostream& operator << (std::ostream& os, const ResourceType& obj) {
    os << (obj == ResourceType::UL ? "UPLINK" : "DOWNLINK");
    return os;
}

enum class AllocationStatus : uint32_t {
    SUCCESS,
    FAIL
};

std::ostream& operator << (std::ostream& os, const AllocationStatus& obj) {
    os << (obj == AllocationStatus::SUCCESS ? "SUCCESS" : "FAIL");
    return os;
}

struct ResourceRequest {
    uint32_t ue_id;
    ResourceType resource_type;
    uint32_t data_length;
};

struct SchedulerResponse {
    uint32_t ue_id;
    AllocationStatus status;
};

struct Configuration {
    bool DEBUGPRINTS = true;
    std::chrono::milliseconds SF_TIME = std::chrono::milliseconds(1U); // Subframe duration, milliseconds (wall-clock time delay in simulation)

    unsigned SIMULATION_PERIOD_SF = 200;
    UeMode UE_MODE = UeMode::MIXED;
    unsigned K; // maximum advance scheduling time
    unsigned L = 16; // data length
    unsigned M = 16; // number of UEs to simulate
    uint32_t N = 64; // number of resource blocks (indifidual frequency channels)

    Configuration() {
        LoadConfig();
        K = 10 * L;
    }

private:
    void LoadConfig() {
        std::ifstream cfg_stream(CFG_FILE);
        if (!cfg_stream.is_open()) {
            std::cerr << "Cannot open " << CFG_FILE << ", using defaults." << std::endl;
            return;
        }
        std::string line;
        while(getline(cfg_stream, line)) {
            auto delimiterPos = line.find("=");
            if( line.empty() || line[0] == '#' || delimiterPos == std::string::npos) continue;
            auto key = line.substr(0, delimiterPos);
            auto val = line.substr(delimiterPos + 1);
            ParseKeyValPair(key, val);
        }
    }

    void ParseKeyValPair(const std::string& key, const std::string& val) {
        try {
            if (key.compare("M") == 0) {
                M = std::stoul(val);
            } else if (key.compare("N") == 0) {
                N = std::stoul(val);
            } else if (key.compare("SF_TIME") == 0) {
                SF_TIME = std::chrono::milliseconds(std::stoul(val));
            } else {
                std::cerr << "Unknown key in config: "<< key << "=" << val << '\n';
                return;
            }
            std::cout << "Parameter " << key << "=" << val << '\n';
        } catch (std::exception& e) {
            std::cerr << "Failed to parse " << key << "=" << val << ", exception: " << e.what() << '\n';
        }
    }
} cfg;

template <typename T>
void SockSend(int sockfd, struct sockaddr_in& servaddr, socklen_t& len, const std::vector<T>& message) {
    const size_t bytes_to_send = message.size() * sizeof(T);
    int result = sendto(sockfd, message.data(), bytes_to_send,
               MSG_CONFIRM, (const struct sockaddr *) &servaddr,
               len);
    if(result == -1) {
        std::cerr << "error " << errno << ": " << strerror(errno);
    }
}

template <typename T>
void SockRecv(int sockfd, struct sockaddr_in& servaddr, socklen_t& len, std::vector<T>& message) {
    message.resize(cfg.M);
    const size_t bytes_to_receive = message.size() * sizeof(T);
    int result = recvfrom(sockfd, message.data(), bytes_to_receive,
               MSG_WAITALL, (struct sockaddr *) &servaddr,
               &len);
    if(result == -1) {
        std::cerr << "error " << errno << ": " << strerror(errno);
    }
    if(result % sizeof(T) != 0) {
        std::cerr << "received " << result << " bytes which is not multple of record size (" << sizeof(T) << ")\n";
    }
    message.resize(result / sizeof(T));
}
