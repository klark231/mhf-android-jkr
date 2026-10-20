#include "unjkr.h"
#include <cstdio>
#include <cstring>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <jkr_file> [reference_out]\n", argv[0]); return 1; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("open input"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> in(sz);
    if (fread(in.data(), 1, sz, f) != (size_t)sz) { fclose(f); return 1; }
    fclose(f);

    std::vector<uint8_t> out;
    if (!mhf::unjkr_auto(in.data(), in.size(), out)) {
        fprintf(stderr, "decode failed\n"); return 1;
    }
    printf("decoded %zu bytes\n", out.size());

    if (argc < 3) {
        FILE* o = fopen("decoded.bin", "wb");
        fwrite(out.data(), 1, out.size(), o); fclose(o);
        printf("wrote decoded.bin\n");
        return 0;
    }
    FILE* r = fopen(argv[2], "rb");
    if (!r) { perror("open ref"); return 1; }
    fseek(r, 0, SEEK_END); long rsz = ftell(r); fseek(r, 0, SEEK_SET);
    std::vector<uint8_t> ref(rsz);
    fread(ref.data(), 1, rsz, r); fclose(r);
    if (out.size() != ref.size()) { fprintf(stderr, "SIZE MISMATCH: %zu vs %zu\n", out.size(), ref.size()); return 1; }
    if (memcmp(out.data(), ref.data(), out.size()) != 0) { fprintf(stderr, "CONTENT MISMATCH\n"); return 1; }
    printf("OK: byte-identical to reference\n");
    return 0;
}
