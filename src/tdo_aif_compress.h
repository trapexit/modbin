/*
  3DO AIF executable compression / decompression.

  The 3DO AIF "compressed" format is an order-1 byte predictor with a
  768-byte dictionary (three 256-byte prediction zones indexed by the
  previously emitted byte). See algorithm.md for the full wire format
  and a description of the in-executable decompressor stub.

  All entry points follow the same convention:
    *out_ is set to a malloc()ed buffer owned by the caller (free() to
    release). Returns 0 on success, -1 on failure; tdo_aif_compress()
    returns a TdoAifCompressStatus instead.
*/

#ifndef TDO_AIF_COMPRESS_H
#define TDO_AIF_COMPRESS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
  Status returned by tdo_aif_compress().

  The DECLINED_* values mean the operation succeeded but nothing was compressed:
  *out_ is an owned copy of the input, unchanged, and the tool reports the image
  as uncompressed.
*/
typedef enum TdoAifCompressStatus
{
  TdoAifCompressStatus_OK              =  0,  /* *out_ is a compressed AIF   */
  TdoAifCompressStatus_DECLINED_UNSAFE =  1,  /* stub scratch too small      */
  TdoAifCompressStatus_DECLINED_GROWTH =  2,  /* image would not be smaller  */
  TdoAifCompressStatus_ERROR           = -1   /* no output, *out_ untouched  */
} TdoAifCompressStatus;

/*
  Full-image compressor.

  `in_` / `in_size_` is an uncompressed 3DO AIF (header at offset 0,
  body beginning at offset 0x100, optional RSA signature appended). On
  success *out_ is a freshly-allocated compressed AIF: header +
  compressed stream + 0x188-byte decompressor stub + optional
  signature, with the BL at offset 0 and sig_offset patched. The input
  body is zero-padded to a 4-byte multiple if needed.

  The image is emitted only when the stub's scratch_delta covers the
  decoder's write-ahead (see stub_scratch_needed() in the .c) and the
  compressed image is strictly smaller than `in_`; otherwise the result
  is declined and *out_ is an unchanged copy of the input. *out_ is
  always set on a non-error return and is malloc()ed (caller frees).

  If the input is already compressed, it is decompressed first so the
  operation is idempotent.
*/
TdoAifCompressStatus tdo_aif_compress(const uint8_t  *in_,
                                      size_t          in_size_,
                                      uint8_t       **out_,
                                      size_t         *out_size_);

/*
  Decompress a full 3DO AIF image.

  On success *out_ is a freshly-allocated buffer containing the
  reconstructed uncompressed AIF: the original 256-byte header with
  word 0 patched to a NOP (0xE1A00000), followed by the decompressed
  body. If the original image carried an RSA signature (via the 3DO
  header sig_offset/sig_size at 0xB0/0xB4) it is appended to the output
  and sig_offset is rewritten to its new location.
*/
int tdo_aif_decompress(const uint8_t  *in_,
                       size_t          in_size_,
                       uint8_t       **out_,
                       size_t         *out_size_);

/*
  Compress the body of a 3DO AIF image.

  `body_` is the raw, uncompressed body bytes (everything at file
  offset 0x100 and beyond, excluding any appended signature). Length
  must be a multiple of 4. Output is only the compressed stream itself
  (4-byte big-endian word count + 96+non-zero-dict-bytes + body
  tokens); it does not include the AIF/3DO header or the 0x188-byte
  decompressor stub.
*/
int tdo_aif_compress_body(const uint8_t  *body_,
                          size_t          body_size_,
                          uint8_t       **out_,
                          size_t         *out_size_);

/*
  Decode a raw compressed stream (no AIF header, no decompressor stub)
  as produced by tdo_aif_compress_body into its original body bytes.
  Rejects decoded byte counts that cannot be represented by size_t.
*/
int tdo_aif_decompress_body(const uint8_t  *stream_,
                            size_t          stream_size_,
                            uint8_t       **out_,
                            size_t         *out_size_);

#ifdef __cplusplus
}
#endif

#endif
