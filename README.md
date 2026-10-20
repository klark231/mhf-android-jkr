# MHF JKR Decompressor (C++)

Native C++17 decompressor for Monster Hunter Frontier's JKR-compressed payloads
(`JKR\x1a` magic). Part of a larger effort to bring MHF to Android without
Wine/Winlator/Box64 — this is the first portable module.

## Status

Byte-identical against real game data:

- **LZ** (type 3) — verified on `0002_00019E4C.jkr` (12,896 bytes)
- **HFI** (type 4, Huffman + LZ77) — verified on `0001_00000014.jkr` (326,256 bytes)
- **Raw** (type 0) — memcpy passthrough
- **HFIRW** (type 2) — not implemented (big-endian, PC MHF doesn't use it)

## Header layout (verified)

    offset  size  meaning
    0       4     magic "JKR\x1a"
    4       2     version (0x0108 observed)
    6       2     compression type (u16 LE): 0=raw 3=LZ 4=HFI
    8       4     constant 16 (unknown)
    12      4     decompressed size (u32 LE)
    16      ...   payload

## Build

    clang++ -std=c++17 -O2 -o test_unjkr test_unjkr.cpp unjkr.cpp

## Usage

    ./test_unjkr <compressed.jkr>                      # writes decoded.bin
    ./test_unjkr <compressed.jkr> <reference.bin>      # byte-compares, exits 0 on match

## Android portability

`unjkr.cpp` has no platform dependencies — pure C++17, `<cstring>` + `<vector>` + `<cstdint>` only.
Compiles unchanged with the Android NDK for arm64-v8a / armeabi-v7a.

## Credits

Algorithm reference: [ReFrontier](https://github.com/Houmgaor/ReFrontier) (MIT) — C# reference implementation.
This is an independent C++ reimplementation validated by byte comparison against real game data.
