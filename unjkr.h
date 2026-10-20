#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace mhf {

bool unjkr(const uint8_t* src, size_t src_size,
           uint8_t compression_byte,
           std::vector<uint8_t>& out, size_t out_size);

bool unjkr_auto(const uint8_t* src, size_t src_size,
                std::vector<uint8_t>& out);

}
