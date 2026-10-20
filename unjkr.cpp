#include "unjkr.h"
#include <cstring>

namespace mhf {

struct BitReader {
    const uint8_t* data; size_t size; size_t pos = 0;
    uint8_t current = 0; int bit_pos = 0;
    BitReader(const uint8_t* d, size_t s) : data(d), size(s) {}
    bool get_bit() {
        if (bit_pos <= 0) {
            if (pos >= size) return false;
            current = data[pos++]; bit_pos = 7;
        } else bit_pos--;
        return ((current >> bit_pos) & 1) != 0;
    }
    uint8_t get_byte() {
        if (pos >= size) return 0;
        return data[pos++];
    }
};

static void lz_copy(std::vector<uint8_t>& buf, int offset, int length, int& out_index) {
    if (length <= 0 || out_index + length > (int)buf.size()) return;
    if (out_index - offset - 1 < 0) return;
    int no_overlap = (length < offset) ? length : offset;
    int src_start = out_index - offset - 1;
    std::memmove(buf.data() + out_index, buf.data() + src_start, (size_t)no_overlap);
    for (int i = out_index + no_overlap; i < out_index + length; ++i)
        buf[i] = buf[i - offset - 1];
    out_index += length;
}

static bool decode_lz(const uint8_t* src, size_t src_size,
                      std::vector<uint8_t>& out, size_t out_size) {
    BitReader br(src, src_size);
    int idx = 0; int target = (int)out_size;
    while (br.pos < br.size && idx < target) {
        if (!br.get_bit()) { out[idx++] = br.get_byte(); continue; }
        if (!br.get_bit()) {
            int length = (br.get_bit() ? 2 : 0) + (br.get_bit() ? 1 : 0) + 3;
            int offset = br.get_byte();
            lz_copy(out, offset, length, idx); continue;
        }
        uint8_t hi = br.get_byte();
        uint8_t lo = br.get_byte();
        int length = (hi & 0xE0) >> 5;
        int offset = ((hi & 0x1F) << 8) | lo;
        if (length != 0) { lz_copy(out, offset, length + 2, idx); continue; }
        if (!br.get_bit()) {
            int len2 = 0;
            for (int i = 3; i >= 0; --i) len2 += br.get_bit() ? (1 << i) : 0;
            lz_copy(out, offset, len2 + 2 + 8, idx); continue;
        }
        uint8_t temp = br.get_byte();
        if (temp == 0xFF) {
            int n = offset + 0x1B;
            for (int i = 0; i < n; ++i) out[idx++] = br.get_byte();
            continue;
        }
        lz_copy(out, offset, (int)temp + 0x1A, idx);
    }
    return idx == target;
}

static bool decode_hfi(const uint8_t* src, size_t src_size,
                       std::vector<uint8_t>& out, size_t out_size) {
    if (src_size < 2) return false;
    int16_t table_len = (int16_t)(src[0] | (src[1] << 8));
    if (table_len <= 0 || table_len > 0x400) return false;
    size_t table_off = 2;
    size_t data_off = table_off + (size_t)table_len * 2;
    if (data_off > src_size) return false;
    const uint8_t* table = src + table_off;
    const uint8_t* data = src + data_off;
    size_t data_size = src_size - data_off;

    struct Huff {
        const uint8_t* data; size_t size; size_t pos = 0;
        uint8_t flag_byte = 0; int flag_shift = 0;
        uint8_t hf_byte = 0; int hf_shift = 0;
        const uint8_t* table; int table_len;
        bool hbyte(uint8_t& result) {
            int node = table_len; int safety = 256;
            while (node >= 0x100 && safety-- > 0) {
                hf_shift--;
                if (hf_shift < 0) {
                    if (pos >= size) return false;
                    hf_byte = data[pos++]; hf_shift = 7;
                }
                int bit = (hf_byte >> hf_shift) & 1;
                size_t off = (size_t)(node * 2 - 0x200 + bit) * 2;
                if (off + 1 >= (size_t)table_len * 2) return false;
                node = (int16_t)(table[off] | (table[off+1] << 8));
            }
            if (safety < 0) return false;
            result = (uint8_t)node; return true;
        }
        bool get_bit(bool& b) {
            if (flag_shift <= 0) {
                uint8_t v; if (!hbyte(v)) return false;
                flag_byte = v; flag_shift = 7;
            } else flag_shift--;
            b = ((flag_byte >> flag_shift) & 1) != 0; return true;
        }
        bool get_byte(uint8_t& b) { return hbyte(b); }
    };
    Huff br{data, data_size, 0, 0, 0, 0, 0, table, table_len};

    int idx = 0; int target = (int)out_size;
    while (idx < target) {
        bool b1; if (!br.get_bit(b1)) return false;
        if (!b1) { uint8_t v; if (!br.get_byte(v)) return false; out[idx++] = v; continue; }
        bool b2; if (!br.get_bit(b2)) return false;
        if (!b2) {
            bool a, b; if (!br.get_bit(a) || !br.get_bit(b)) return false;
            int length = (a?2:0)+(b?1:0)+3;
            uint8_t off; if (!br.get_byte(off)) return false;
            lz_copy(out, off, length, idx); continue;
        }
        uint8_t hi, lo; if (!br.get_byte(hi) || !br.get_byte(lo)) return false;
        int length = (hi & 0xE0) >> 5;
        int offset = ((hi & 0x1F) << 8) | lo;
        if (length != 0) { lz_copy(out, offset, length+2, idx); continue; }
        bool b3; if (!br.get_bit(b3)) return false;
        if (!b3) {
            int len2 = 0;
            for (int i = 3; i >= 0; --i) { bool b; if (!br.get_bit(b)) return false; len2 += b?(1<<i):0; }
            lz_copy(out, offset, len2 + 10, idx); continue;
        }
        uint8_t temp; if (!br.get_byte(temp)) return false;
        if (temp == 0xFF) {
            int n = offset + 0x1B;
            for (int i = 0; i < n; ++i) { uint8_t v; if (!br.get_byte(v)) return false; out[idx++] = v; }
            continue;
        }
        lz_copy(out, offset, (int)temp + 0x1A, idx);
    }
    return idx == target;
}

bool unjkr(const uint8_t* src, size_t src_size, uint8_t compression_byte,
           std::vector<uint8_t>& out, size_t out_size) {
    out.assign(out_size, 0);
    if (compression_byte == 0) {
        if (src_size < out_size) return false;
        std::memcpy(out.data(), src, out_size);
        return true;
    }
    if (compression_byte == 3) return decode_lz(src, src_size, out, out_size);
    if (compression_byte == 4) return decode_hfi(src, src_size, out, out_size);
    return false;
}

bool unjkr_auto(const uint8_t* src, size_t src_size, std::vector<uint8_t>& out) {
    if (src_size < 16) return false;
    if (!(src[0]=='J' && src[1]=='K' && src[2]=='R' && src[3]==0x1A)) return false;
    uint16_t cbyte = (uint16_t)(src[6] | (src[7] << 8));
    uint32_t dsize = (uint32_t)(src[12] | (src[13]<<8) | (src[14]<<16) | (src[15]<<24));
    return unjkr(src + 16, src_size - 16, (uint8_t)cbyte, out, dsize);
}

}
