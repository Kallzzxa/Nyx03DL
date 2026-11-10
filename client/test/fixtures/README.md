# Test fixtures — NOT real models

Copied from `../../server/test-fixtures/` (same bytes, same CRCs), plus
`manifest.json` built from those REAL CRCs (decimal, as the M1 plugin
emits them — nothing here is a made-up number):

- `skin_a.dff` — 256 bytes: 0x00..0xFF; CRC32 0x29058C73 = 688229491
- `skin_a.txd` — 90 bytes: "123456789" x10; CRC32 0xDF68476A = 3748153194
- `skin_b.dff` — 1024 bytes: deterministic LCG pseudo-random;
  CRC32 0xF5E54AD0 = 4125444816
- `skin_b.txd` — 80 bytes: "NYX03DL-TEST-FIXTURE" x4;
  CRC32 0x0F151593 = 253040019
- `manifest.json` — the exact JSON the M1 plugin would emit for skins
  20000 (base 264) and 20001 (base 265) from these files.

TEST-ONLY. They are NOT GTA model files and must never be shipped as
models or claimed to be real skins.
