/* Copyright (C) 2024 Maxim Plekh - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPLv3 license.
 *
 * You should have received a copy of the GPLv3 license with this file.
 * If not, please visit : http://choosealicense.com/licenses/gpl-3.0/
 */
#pragma once
#include <vector>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <cassert>

class Scheduler {
    unsigned window_len_;
    unsigned rb_per_sf_;
    std::vector<unsigned> subframes;

  public:
    unsigned success = 0;
    unsigned total = 0;
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
        total += num;
        success += num_reserved;
        return num_reserved;
    }

    double avgBlockPerSf (unsigned from, unsigned len) {
        const auto first = subframes.cbegin() + from;
        assert(first + len <= subframes.cend());
        return std::accumulate(first, first + len, 0.0) / len;
    }

    void printWindow(unsigned from, unsigned len) {
        auto print = [](const unsigned& n) { std::cout << n << ' '; };
        const auto first = subframes.cbegin() + from;
        assert(first + len <= subframes.cend());
        std::for_each(subframes.cbegin(), first, print);
        std::cout << "[ ";
        std::for_each(first, first + len, print);
        std::cout << "] ";
        std::for_each(first + len, subframes.cend(), print);
        std::cout << "\n";
    }
};
