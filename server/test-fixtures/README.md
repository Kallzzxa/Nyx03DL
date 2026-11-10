# Test fixtures — NOT real models

Files in this directory are TEST-ONLY fixtures for the Linux harness
(`../test-crc32.cpp`). They contain known/pattern bytes so the CRC32,
registration and manifest logic can be verified without a real
.dff/.txd:

- `skin_a.dff` — 256 bytes: 0x00..0xFF
- `skin_a.txd` — 90 bytes: "123456789" x10
- `skin_b.dff` — 1024 bytes: deterministic LCG pseudo-random bytes
- `skin_b.txd` — 80 bytes: "NYX03DL-TEST-FIXTURE" x4

They are NOT GTA model files and must never be shipped as models or
claimed to be real skins.
