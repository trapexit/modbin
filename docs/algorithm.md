# 3DO AIF Compression - Reverse Engineering Summary

## Result
20/20 compressed 3DO AIF executables in this repo decompress cleanly with the
reference Python implementation at `files/3do_aif_decompress.py`. Every
reconstructed file passes `modbin`'s AIF validation, reports `compressed: no`,
and preserves its 3DO header (name, priority, flags, type, time, stack, ...) and
RSA signature (for the 6 signed files).

## Algorithm

The AIF decompressor stub is a constant 0x188-byte blob (identical across all
files) pointed to by the BL at file offset 0. It reads the payload at
`[0x100 .. decompressor_start)` as a big-endian byte stream structured as:

1. **Word count** - a single `uint32_t` big-endian: the number of 4-byte words
   of uncompressed body (body begins at offset 0x100; its length is
   `word_count * 4`).

2. **Prediction dictionary (768 bytes, "sparse" encoded)** - 96 control bytes;
   each control byte describes 2 consecutive 4-byte dictionary words. For each
   of those 8 bytes, a bit (LSB-first) says: `1` -> the byte is 0 (omitted from
   the stream), `0` -> the byte is read from the stream as a literal. Result is
   a flat 768-byte array interpreted as three 256-byte "zones".

3. **Body** - a stream of `(control_byte, payload...)` groups. One control byte
   per output 4-byte word. Each output byte uses 2 bits of the control
   (position 0 = bits 0-1, pos 1 = bits 2-3, pos 2 = bits 4-5, pos 3 = bits 6-7):
   - `code == 0` -> read literal byte from stream
   - `code == 1` -> byte = `zone0[prev_byte]`
   - `code == 2` -> byte = `zone1[prev_byte]`
   - `code == 3` -> byte = `zone2[prev_byte]`

   `prev_byte` is the last byte emitted (persists across word boundaries;
   initial value 0). This is an order-1 Markov predictor with 3 candidate
   "next bytes" per context.

Worst-case expansion: a word of 4 all-literal bytes costs 1 + 4 = 5 bytes.
Best case: a fully-predicted word costs 1 byte (the control byte).

## Decompressor stub (what the ARM code actually does)

- Header words immediately after the decompressor's entry BL:
  `0x00 B over_data`, `0x04 = 1` (magic), `0x08 = decomp_offset` (self),
  `0x0C = decompressed_end` (= `image_base + 0x100 + word_count*4`),
  `0x10 = scratch_delta` (offset past `decomp_start` where the stub relocates
  its body before running, so the output can be written in place).
  `scratch_delta` must satisfy `scratch_delta >= max over m of (4*m - i(m))`,
  where `i(m)` is the number of stream bytes consumed after emitting `m` body
  words (word count + dictionary bytes + one control byte per word + every
  literal byte); it must also cover `decompressed_end - decomp_start - 0x5C`
  when that is positive. The stub reads the relocated stream forward from
  `scratch_delta + 0x100` while writing the body forward from `0x100`, so every
  byte it has not read yet has to stay above the write cursor - a smaller value
  makes the decoder read stream bytes it has already overwritten and decode the
  body to garbage. The 1995 stock folios ship with a hand-chosen
  `scratch_delta` (e.g. `0xF50` for `jstring.folio`, where a size-derived
  formula gives `0x400`); `modbin --compress` checks the requirement per file
  and declines instead of emitting an image that falls short.
- The stub:
  1. Copies its tail (everything from offset 0x5C of the stub to its end) to
     `decomp_start + scratch_delta`, jumps to the relocated copy.
  2. Copies the compressed stream from `[0x100 .. decomp_start)` *backward*
     down to `[scratch_delta+0x54 .. scratch_delta + 0xB00ish)` so the output
     can grow forward from 0x100 without trampling input.
  3. Reads the word count, builds the 768-byte dictionary on stack.
  4. Runs the order-1 decoder writing words forward starting at 0x100.
  5. Overwrites the BL at offset 0 with a NOP so subsequent loads don't
     redecompress, then returns to the original caller (which continues to the
     self-reloc BL at offset 4).

The 6 signed files (`audio.privfolio`, `FMVVIDEODEVICE.PRIVDEVICE`,
`debugger.privfolio`, `graphics.privfolio`, `international.privfolio`,
`operamath.privfolio`) append a 64-byte RSA signature immediately after the
decompressor stub, with `sig_offset`/`sig_size` set in the 3DO header at
offsets 0xB0/0xB4. The signature is separate from the compressed stream and
is not involved in decompression.

## Files produced

- `files/3do_aif_decompress.py`
  - `decompress_aif_bytes(data) -> bytes`: full reference decompressor.
    Relocates the signature to the new end of file and fixes `sig_offset`
    so the result is a valid, modbin-recognized AIF.
  - `_decompress_body(compressed_stream) -> bytes`: pure stream decoder.
  - `compress_body(body) -> bytes`: encoder (optimal for the format: picks
    the three dictionary bytes per context that maximise net savings, leaving
    any slot whose marginal savings would be <= 0 as the free zero slot).
    Round-trips correctly through the stock decompressor against all 20
    files in this repo, and produces output slightly smaller than the
    original Opera compressor did (by ~27 bytes total across the 20 files).

## How this could slot into modbin

- Add a `--decompress` mode: invoke `decompress_aif_bytes` on input.
- Add a `--compress` mode: strip any existing decompressor stub / BL at 0x00,
  take the body (`[0x100 .. end or sig_offset)`), run `compress_body`, then
  glue: `header + compressed_stream + stock_decompressor_stub (0x188 bytes) +
  optional signature` and patch the BL at 0x00 so its displacement lands on
  the new decompressor (`(new_decomp_offset - 8) / 4`).
