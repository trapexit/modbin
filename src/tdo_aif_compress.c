/*
  3DO AIF executable compression / decompression.

  Wire format recap (see algorithm.md for the authoritative spec):

    - A compressed AIF has:   [AIF header | compressed stream | stub | sig?]
      with a BL at offset 0 pointing at the stub.
    - An uncompressed AIF has [AIF header | body              | sig?]
      with a NOP at offset 0.

  The compressed stream is:

     word_count (be32)                          -- uncompressed body / 4
     96 ctrl bytes gating 768 dict bytes        -- order-1 prediction tbl
     body: one ctrl byte per 4 output bytes     -- 2 bits / out byte

  For each output byte:  code=0 -> literal in stream; code=1..3 ->
  dict[zone-1][prev_byte]. `prev_byte` persists across word boundaries.
*/

#include "tdo_aif.h"
#include "tdo_aif_compress.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Short local aliases for the public header constants. */
#define AIF_HEADER_SIZE TDO_AIF_HEADER_SIZE
#define NOP_INSTR       TDO_AIF_NOP_INSTR
#define BL_OPCODE       TDO_AIF_BL_OPCODE

/* Wire-format constants. */
#define WORD_SIZE          4u
#define DICT_ZONES         3u
#define DICT_ZONE_SIZE     256u
#define DICT_SIZE          (DICT_ZONES * DICT_ZONE_SIZE)    /* 768 */
#define DICT_CTRL_BYTES    (DICT_SIZE / 8u)                 /* 96  */

#define PAD_UP(x, n) (((x) + ((n) - 1u)) & ~((n) - 1u))


/* -------------------------------------------------------------------------- */
/* Byte-swapping + ARM BL codec helpers.                                      */
/* -------------------------------------------------------------------------- */

static
uint32_t
rd_be32(const uint8_t *p)
{
  return (((uint32_t)p[0] << 24) |
          ((uint32_t)p[1] << 16) |
          ((uint32_t)p[2] <<  8) |
          ((uint32_t)p[3] <<  0));
}

static
void
wr_be32(uint8_t  *p,
        uint32_t  v)
{
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >>  8);
  p[3] = (uint8_t)(v >>  0);
}

/* Decode an ARM BL at image offset 0. Returns the absolute image
   offset of the target. Caller must have already verified that the
   top byte of instr_ is BL_OPCODE. */
static
size_t
arm_bl_target(uint32_t instr)
{
  int32_t imm = (int32_t)(instr & 0x00FFFFFFu);
  if(imm & 0x00800000)
    imm |= (int32_t)0xFF000000;  /* sign-extend 24 -> 32 */
  /* ARM BL: target = PC + 8 + imm*4, with PC = 0 for an instruction
     at image offset 0. */
  return (size_t)(8 + imm * 4);
}

/* Encode an ARM BL at image offset 0 targeting `target`. Target must
   be word aligned and within +/- 32MiB; both hold for AIF stubs. */
static
uint32_t
arm_bl_encode(size_t target)
{
  int32_t imm = (int32_t)(((int64_t)target - 8) / 4);
  return ((uint32_t)BL_OPCODE << 24) | ((uint32_t)imm & 0x00FFFFFFu);
}


/* -------------------------------------------------------------------------- */
/* Decompressor.                                                              */
/* -------------------------------------------------------------------------- */

/* Read the 768-byte prediction dictionary from the stream.
   Returns the byte offset just past the dictionary on success, or 0
   (impossible for a well-formed stream) on truncation. */
static
size_t
read_dict(const uint8_t *stream,
          size_t         stream_size,
          size_t         pos,
          uint8_t        dict[DICT_SIZE])
{
  size_t dp = 0;

  for(unsigned i = 0; i < DICT_CTRL_BYTES; i++)
    {
      uint8_t ctrl;

      if(pos >= stream_size)
        return 0;
      ctrl = stream[pos++];

      /* Each control bit gates one dict byte: 1 => implicit zero,
         0 => next literal byte from the stream. LSB first. */
      for(unsigned k = 0; k < 8; k++, ctrl >>= 1)
        {
          if(ctrl & 1u)
            {
              dict[dp++] = 0;
            }
          else
            {
              if(pos >= stream_size)
                return 0;
              dict[dp++] = stream[pos++];
            }
        }
    }
  return pos;
}


int
tdo_aif_decompress_body(const uint8_t  *stream_,
                        size_t          stream_size_,
                        uint8_t       **out_,
                        size_t         *out_size_)
{
  uint8_t  dict[DICT_SIZE];
  uint32_t word_count;
  size_t   pos;
  uint8_t *out;
  size_t   out_len;
  uint8_t  prev;

  if(stream_size_ < WORD_SIZE)
    return -1;

  word_count = rd_be32(stream_);
  pos        = WORD_SIZE;

  pos = read_dict(stream_, stream_size_, pos, dict);
  if(pos == 0)
    return -1;

  out = (uint8_t*)malloc(word_count ? (size_t)word_count * WORD_SIZE : 1);
  if(out == NULL)
    return -1;

  /* Body: per 4-byte output word, one control byte holds four 2-bit
     codes. code==0 => literal, code==1..3 => dict[(code-1)][prev]. */
  out_len = 0;
  prev    = 0;
  for(uint32_t w = 0; w < word_count; w++)
    {
      uint8_t ctrl;

      if(pos >= stream_size_)
        goto fail;
      ctrl = stream_[pos++];

      for(unsigned byte_idx = 0; byte_idx < 4; byte_idx++)
        {
          unsigned code = (ctrl >> (byte_idx * 2)) & 0x3u;
          uint8_t  b;

          if(code == 0)
            {
              if(pos >= stream_size_)
                goto fail;
              b = stream_[pos++];
            }
          else
            {
              b = dict[(code - 1) * DICT_ZONE_SIZE + prev];
            }

          out[out_len++] = b;
          prev           = b;
        }
    }

  *out_      = out;
  *out_size_ = out_len;
  return 0;

 fail:
  free(out);
  return -1;
}


/* Full-image decoder: verifies the BL at offset 0, decodes the
   compressed stream, and reassembles the AIF with the BL patched to
   a NOP and any trailing RSA signature relocated. */
int
tdo_aif_decompress(const uint8_t  *in_,
                   size_t          in_size_,
                   uint8_t       **out_,
                   size_t         *out_size_)
{
  uint32_t b0;
  size_t   decomp_start;
  uint8_t *body;
  size_t   body_size;
  uint32_t sig_offset;
  uint32_t sig_size;
  uint8_t *out;
  size_t   out_size;
  size_t   new_sig_offset;

  if(in_size_ < AIF_HEADER_SIZE)
    return -1;

  b0 = rd_be32(in_);
  if((b0 >> 24) != BL_OPCODE)
    return -1;

  decomp_start = arm_bl_target(b0);
  if(decomp_start <= AIF_HEADER_SIZE || decomp_start > in_size_)
    return -1;

  if(tdo_aif_decompress_body(in_       + AIF_HEADER_SIZE,
                             decomp_start - AIF_HEADER_SIZE,
                             &body,
                             &body_size) != 0)
    return -1;

  /* Preserve any appended RSA signature; relocate it to the new
     end-of-image after decompression. */
  sig_offset = tdo_aif_get_sig_offset((void*)in_);
  sig_size   = tdo_aif_get_sig_size((void*)in_);
  if(!sig_offset || !sig_size ||
     (size_t)sig_offset + (size_t)sig_size > in_size_)
    {
      sig_offset = 0;
      sig_size   = 0;
    }

  out_size = AIF_HEADER_SIZE + body_size + sig_size;
  out      = (uint8_t*)malloc(out_size ? out_size : 1);
  if(out == NULL)
    {
      free(body);
      return -1;
    }

  /* Header (with BL -> NOP), body, signature. */
  memcpy(out, in_, AIF_HEADER_SIZE);
  wr_be32(out, NOP_INSTR);
  memcpy(out + AIF_HEADER_SIZE, body, body_size);
  free(body);

  if(sig_size)
    {
      new_sig_offset = AIF_HEADER_SIZE + body_size;
      memcpy(out + new_sig_offset, in_ + sig_offset, sig_size);
      tdo_aif_set_sig_offset(out, (uint32_t)new_sig_offset);
      /* sig_size field unchanged. */
    }

  *out_      = out;
  *out_size_ = out_size;
  return 0;
}


/* -------------------------------------------------------------------------- */
/* Compressor.                                                                */
/* -------------------------------------------------------------------------- */

/*
  Build the 3-zone order-1 prediction dictionary.

  For each context byte c, we choose up to 3 "predicted" next bytes
  that, if seen, can be encoded with a 2-bit code instead of an 8-bit
  literal. Using a zone costs 1 dictionary byte per slot unless the
  picked byte is zero (implicit via the sparse bit).

  Score model: score(c,b) = freq(c,b) - cost(b)
                            where cost(b) = 0 if b==0 else 1.
  A slot is only worth filling when its score is strictly positive;
  otherwise we leave it as 0 (free via sparse encoding).

  Ties resolve to the smaller byte value (the `b` loop is ascending
  and the comparison is strict `>`, so the first maximum wins).
*/
static
void
build_dictionary(const uint8_t *body,
                 size_t         body_size,
                 uint8_t        zones[DICT_ZONES][DICT_ZONE_SIZE])
{
  static uint32_t freq[DICT_ZONE_SIZE][DICT_ZONE_SIZE]; /* 256 KiB; .bss */
  uint8_t prev;

  memset(freq, 0, sizeof(freq));

  prev = 0;
  for(size_t i = 0; i < body_size; i++)
    {
      freq[prev][body[i]]++;
      prev = body[i];
    }

  for(unsigned c = 0; c < DICT_ZONE_SIZE; c++)
    {
      uint8_t used[DICT_ZONE_SIZE] = {0};

      for(unsigned slot = 0; slot < DICT_ZONES; slot++)
        {
          int32_t best_score = 0; /* strictly positive required */
          int     best_b     = -1;

          for(unsigned b = 0; b < DICT_ZONE_SIZE; b++)
            {
              int32_t score;

              if(used[b])
                continue;

              score = (int32_t)freq[c][b] - (b != 0 ? 1 : 0);
              if(score > best_score)
                {
                  best_score = score;
                  best_b     = (int)b;
                }
            }

          if(best_b < 0)
            {
              zones[slot][c] = 0;
              continue;
            }

          zones[slot][c]    = (uint8_t)best_b;
          used[best_b]      = 1;
        }
    }
}


int
tdo_aif_compress_body(const uint8_t  *body_,
                      size_t          body_size_,
                      uint8_t       **out_,
                      size_t         *out_size_)
{
  uint8_t  zones[DICT_ZONES][DICT_ZONE_SIZE];
  uint8_t  match[DICT_ZONE_SIZE][DICT_ZONE_SIZE]; /* code 1..3, 0=literal */
  uint8_t *out;
  size_t   out_cap;
  size_t   out_len;
  uint8_t  prev;

  if(body_size_ % WORD_SIZE != 0)
    return -1;

  build_dictionary(body_, body_size_, zones);

  /* Upper bound on stream size:
       4   word-count header
     + 96  dictionary control bytes
     + 768 dictionary literals (worst case: every dict slot non-zero)
     + (body_size/4) * 5 body bytes (worst case: 1 ctrl + 4 literals). */
  out_cap = WORD_SIZE + DICT_CTRL_BYTES + DICT_SIZE +
            (body_size_ / WORD_SIZE) * 5u;
  out     = (uint8_t*)malloc(out_cap ? out_cap : 1);
  if(out == NULL)
    return -1;

  /* --- Header --------------------------------------------------- */
  out_len = 0;
  wr_be32(&out[out_len], (uint32_t)(body_size_ / WORD_SIZE));
  out_len += WORD_SIZE;

  /* --- Dictionary ----------------------------------------------- */
  /* Logically zones[0]..zones[2] concatenated = 768 bytes, emitted
     as 96 ctrl bytes of 8 slots each; a 1-bit marks an implicit
     zero, a 0-bit means "next literal comes from the stream". */
  for(unsigned i = 0; i < DICT_CTRL_BYTES; i++)
    {
      uint8_t ctrl = 0;
      size_t  ctrl_at;
      size_t  base = (size_t)i * 8u;

      ctrl_at = out_len++;
      for(unsigned k = 0; k < 8; k++)
        {
          size_t  idx  = base + k;
          uint8_t byte = zones[idx / DICT_ZONE_SIZE][idx % DICT_ZONE_SIZE];

          if(byte == 0)
            ctrl |= (uint8_t)(1u << k);
          else
            out[out_len++] = byte;
        }
      out[ctrl_at] = ctrl;
    }

  /* --- Body ----------------------------------------------------- */
  /* Per-context code lookup: match[c][b] = smallest zone index+1
     (1..3) whose zones[z][c] == b, or 0 if no zone predicts b.
     Smaller zone index is preferred because the inverse is already
     fixed by the algorithm (decoder uses dict[(code-1)*256+prev]). */
  memset(match, 0, sizeof(match));
  for(unsigned c = 0; c < DICT_ZONE_SIZE; c++)
    {
      for(unsigned z = 0; z < DICT_ZONES; z++)
        {
          uint8_t v = zones[z][c];
          if(match[c][v] == 0)
            match[c][v] = (uint8_t)(z + 1);
        }
    }

  /* Emit body, 4 bytes per control byte. */
  prev = 0;
  for(size_t i = 0; i < body_size_; i += WORD_SIZE)
    {
      uint8_t ctrl    = 0;
      size_t  ctrl_at = out_len++;

      for(unsigned pos = 0; pos < 4; pos++)
        {
          uint8_t b    = body_[i + pos];
          uint8_t code = match[prev][b];

          if(code == 0)
            out[out_len++] = b;
          ctrl |= (uint8_t)(code << (pos * 2));
          prev  = b;
        }
      out[ctrl_at] = ctrl;
    }

  *out_      = out;
  *out_size_ = out_len;
  return 0;
}


/* -------------------------------------------------------------------------- */
/* Full-image compressor.                                                     */
/* -------------------------------------------------------------------------- */

/*
  Canonical decompressor stub, 0x188 bytes = 98 big-endian ARM words,
  extracted from `audio.privfolio` in the 3DO devkit
  `takeme/System/Folios/` tree and identical across every known
  compressed 3DO AIF executable.

  Layout (see algorithm.md for the full description):

    +0x00          1 word   B  over the 4 data words
    +0x04 .. 0x14  4 words  data table (magic / decomp_offset /
                            decompressed_end / scratch_delta),
                            patched per file in tdo_aif_compress()
    +0x14 .. 0x5C  18 words in-place setup (runs at decomp_start)
    +0x5C .. 0xC0  25 words relocated driver (orchestrates decode)
    +0xC0 .. 0x110 20 words dictionary reader (builds 768-byte dict)
    +0x110 .. 0x188 30 words body decoder (emits output words)

  The array is stored in host byte order; the on-wire stream is
  produced by emitting each word through wr_be32(). ARM mnemonics are
  from the 3DO SDT `decaof -c` disassembler (i.e. ARM Ltd SDT2.51
  syntax). The first 5 entries are sentinels and are always
  overwritten by the per-file header patches in tdo_aif_compress().
*/
static const uint32_t TDO_AIF_DECOMP_STUB[] = {
  /* ---- Entry (word 0): branch over the 4 data words below. Always ---- */
  /*      overwritten with 0xEA000003 by tdo_aif_compress().          ---- */
  0xea000003u, /* 0x000: B 0x14 */

  /* ---- Data table (words 1..4). All overwritten per file by       ---- */
  /*      tdo_aif_compress(): magic=1, decomp_offset,                ---- */
  /*      decompressed_end, scratch_delta. Mnemonics shown for       ---- */
  /*      reference only; these slots are not executed.              ---- */
  0x00000001u, /* 0x004: ANDEQ r0,r0,r1               (= magic, 1)          */
  0x00009a6cu, /* 0x008: ANDEQ r9,r0,r12,ROR #20      (= decomp_offset)     */
  0x0000d554u, /* 0x00C: ANDEQ r13,r0,r4,ASR r5       (= decompressed_end)  */
  0x00003dccu, /* 0x010: ANDEQ r3,r0,r12,ASR #27      (= scratch_delta)     */

  /* ---- In-place setup (runs at original decomp_start). Copies the ---- */
  /*      stub tail (bytes 0x5C..0x188) to decomp_start+scratch_delta,---- */
  /*      computes how many bytes of compressed stream to relocate,  ---- */
  /*      then jumps to the relocated copy.                          ---- */
  0xe92d00e0u, /* 0x014: STMDB r13!,{r5-r7} */
  0xe24e0004u, /* 0x018: SUB   r0,r14,#4              ; r0 = decomp_start   */
  0xe51f101cu, /* 0x01C: LDR   r1,[pc,#-0x1c]         ; decomp_offset       */
  0xe0800001u, /* 0x020: ADD   r0,r0,r1               ; image_base+decomp   */
  0xe51f101cu, /* 0x024: LDR   r1,[pc,#-0x1c]         ; scratch_delta       */
  0xe0800001u, /* 0x028: ADD   r0,r0,r1               ; dst = +scratch      */
  0xe1a04000u, /* 0x02C: MOV   r4,r0                  ; save scratch base   */
  0xe24f5038u, /* 0x030: SUB   r5,pc,#0x38            ; r5 = &stub[0]       */
  0xe2455004u, /* 0x034: SUB   r5,r5,#4                                      */
  0xe28f201cu, /* 0x038: ADD   r2,pc,#0x1c            ; r2 = &stub[0x5c]    */
  0xe28f3f51u, /* 0x03C: ADD   r3,pc,#0x144           ; r3 = &stub[0x188]   */
  0xe4921004u, /* 0x040: LDR   r1,[r2],#4             ; copy tail word-by-  */
  0xe4801004u, /* 0x044: STR   r1,[r0],#4             ;   word to scratch   */
  0xe1520003u, /* 0x048: CMP   r2,r3                                         */
  0x1afffffbu, /* 0x04C: BNE   0x40                                          */
  0xe51f6050u, /* 0x050: LDR   r6,[pc,#-0x50]         ; decomp_offset       */
  0xe2466c01u, /* 0x054: SUB   r6,r6,#0x100           ; comp_size = ofs-100 */
  0xe1a0f004u, /* 0x058: MOV   pc,r4                  ; jump to relocated   */

  /* ---- Relocated driver. Copies the compressed stream backward    ---- */
  /*      into scratch, allocates 0x300 bytes of stack scratch for   ---- */
  /*      the dictionary, calls the dict reader, then the body       ---- */
  /*      decoder, patches the BL at image+0 to a NOP, and returns.  ---- */
  0xe24eb004u, /* 0x05C: SUB   r11,r14,#4             ; r11 = image_base    */
  0xe92d4000u, /* 0x060: STMDB r13!,{r14} */
  0xe2444004u, /* 0x064: SUB   r4,r4,#4                                      */
  0xe4150004u, /* 0x068: LDR   r0,[r5],#-4            ; copy compressed     */
  0xe4040004u, /* 0x06C: STR   r0,[r4],#-4            ;   stream backward   */
  0xe2566004u, /* 0x070: SUBS  r6,r6,#4                                      */
  0x1afffffbu, /* 0x074: BNE   0x68                                          */
  0xe2840004u, /* 0x078: ADD   r0,r4,#4               ; r0 = &stream[0]     */
  0xe24ddc03u, /* 0x07C: SUB   r13,r13,#0x300         ; reserve 768 B dict  */
  0xe4907004u, /* 0x080: LDR   r7,[r0],#4             ; r7 = word_count     */
  0xe3a01060u, /* 0x084: MOV   r1,#0x60               ; 96 ctrl bytes       */
  0xe1a0200du, /* 0x088: MOV   r2,r13                 ; dict dst = sp       */
  0xeb00000bu, /* 0x08C: BL    0xc0                   ; read_dict(r0,r1,r2) */
  0xe1a01000u, /* 0x090: MOV   r1,r0                  ; r1 = stream pos     */
  0xe1a02007u, /* 0x094: MOV   r2,r7                  ; r2 = word_count     */
  0xe1a0000du, /* 0x098: MOV   r0,r13                 ; r0 = dict base      */
  0xe28b3c01u, /* 0x09C: ADD   r3,r11,#0x100          ; r3 = &output[0x100] */
  0xeb00001au, /* 0x0A0: BL    0x110                  ; decode_body(...)    */
  0xe28ddc03u, /* 0x0A4: ADD   r13,r13,#0x300         ; release dict scratch*/
  0xe8bd4000u, /* 0x0A8: LDMIA r13!,{r14} */
  0xe59f0008u, /* 0x0AC: LDR   r0,[pc,#8]             ; NOP instruction     */
  0xe50e0004u, /* 0x0B0: STR   r0,[r14,#-4]           ; patch BL@image+0    */
  0xe8bd00e0u, /* 0x0B4: LDMIA r13!,{r5-r7} */
  0xe1a0f00eu, /* 0x0B8: MOV   pc,r14                 ; return to caller    */
  0xe1a00000u, /* 0x0BC: NOP                          ; literal pool (MOV r0,r0) */

  /* ---- Dictionary reader. Reads 96 control bytes, each gating 2    ---- */
  /*      4-byte words (8 slots). Bit=1 => emit 0, bit=0 => copy a    ---- */
  /*      literal byte from the stream. Produces 768 bytes on stack.  ---- */
  0xe92d5870u, /* 0x0C0: STMDB r13!,{r4-r6,r11,r12,r14} */
  0xe3a0c000u, /* 0x0C4: MOV    r12,#0                 ; constant 0         */
  0xe4d03001u, /* 0x0C8: LDRB   r3,[r0],#1             ; ctrl byte          */
  0xe3a05002u, /* 0x0CC: MOV    r5,#2                  ; 2 words / ctrl     */
  0xe1b030a3u, /* 0x0D0: MOVS   r3,r3,LSR #1           ; shift out bit      */
  0x34d04001u, /* 0x0D4: LDRCCB r4,[r0],#1             ;   literal byte     */
  0x21a0400cu, /* 0x0D8: MOVCS  r4,r12                 ;   or zero          */
  0xe3a06003u, /* 0x0DC: MOV    r6,#3                  ; 3 more bytes       */
  0xe1b030a3u, /* 0x0E0: MOVS   r3,r3,LSR #1 */
  0x34d0e001u, /* 0x0E4: LDRCCB r14,[r0],#1 */
  0x308e4404u, /* 0x0E8: ADDCC  r4,r14,r4,LSL #8       ; shift byte into    */
  0x21a04404u, /* 0x0EC: MOVCS  r4,r4,LSL #8           ;   accumulator      */
  0xe2566001u, /* 0x0F0: SUBS   r6,r6,#1 */
  0x1afffff9u, /* 0x0F4: BNE    0xe0                                         */
  0xe4824004u, /* 0x0F8: STR    r4,[r2],#4             ; store dict word    */
  0xe2555001u, /* 0x0FC: SUBS   r5,r5,#1 */
  0x1afffff2u, /* 0x100: BNE    0xd0                                         */
  0xe2511001u, /* 0x104: SUBS   r1,r1,#1               ; 96 ctrl iters      */
  0x1affffeeu, /* 0x108: BNE    0xc8                                         */
  0xe8bd9870u, /* 0x10C: LDMIA r13!,{r4-r6,r11,r12,pc} */

  /* ---- Body decoder. Per 4-byte output word: 1 control byte whose ---- */
  /*      4 2-bit codes select literal or one of the 3 prediction    ---- */
  /*      zones keyed by the previous emitted byte. r0=dict base,    ---- */
  /*      r1=stream, r2=word_count, r3=&output[0x100].               ---- */
  0xe92d58f0u, /* 0x110: STMDB r13!,{r4-r7,r11,r12,r14} */
  0xe3a0c000u, /* 0x114: MOV    r12,#0                 ; scratch            */
  0xe2400c01u, /* 0x118: SUB    r0,r0,#0x100           ; prebias dict ptr   */
  0xe4d15001u, /* 0x11C: LDRB   r5,[r1],#1             ; control byte       */
  0xe215e003u, /* 0x120: ANDS   r14,r5,#3              ; byte 0 code        */
  0x108cc40eu, /* 0x124: ADDNE  r12,r12,r14,LSL #8     ;  dict[code*256+prev]*/
  0x17d0400cu, /* 0x128: LDRNEB r4,[r0,r12]            ;  predicted byte    */
  0x04d14001u, /* 0x12C: LDREQB r4,[r1],#1             ;  or literal        */
  0xe215e00cu, /* 0x130: ANDS   r14,r5,#0xc            ; byte 1 code        */
  0x1084c30eu, /* 0x134: ADDNE  r12,r4,r14,LSL #6 */
  0x17d0c00cu, /* 0x138: LDRNEB r12,[r0,r12] */
  0x04d1c001u, /* 0x13C: LDREQB r12,[r1],#1 */
  0xe08c4404u, /* 0x140: ADD    r4,r12,r4,LSL #8       ; shift into accum    */
  0xe215e030u, /* 0x144: ANDS   r14,r5,#0x30           ; byte 2 code        */
  0x108cc20eu, /* 0x148: ADDNE  r12,r12,r14,LSL #4 */
  0x17d0c00cu, /* 0x14C: LDRNEB r12,[r0,r12] */
  0x04d1c001u, /* 0x150: LDREQB r12,[r1],#1 */
  0xe08c4404u, /* 0x154: ADD    r4,r12,r4,LSL #8 */
  0xe215e0c0u, /* 0x158: ANDS   r14,r5,#0xc0           ; byte 3 code        */
  0x108cc10eu, /* 0x15C: ADDNE  r12,r12,r14,LSL #2 */
  0x17d0c00cu, /* 0x160: LDRNEB r12,[r0,r12] */
  0x04d1c001u, /* 0x164: LDREQB r12,[r1],#1 */
  0xe08c4404u, /* 0x168: ADD    r4,r12,r4,LSL #8 */
  0xe3520001u, /* 0x16C: CMP    r2,#1                  ; last word?         */
  0x1a000000u, /* 0x170: BNE    0x178 */
  0xe1a00000u, /* 0x174: NOP                           ; delay slot         */
  0xe4834004u, /* 0x178: STR    r4,[r3],#4             ; emit output word   */
  0xe2522001u, /* 0x17C: SUBS   r2,r2,#1 */
  0x1affffe5u, /* 0x180: BNE    0x11c */
  0xe8bd98f0u, /* 0x184: LDMIA r13!,{r4-r7,r11,r12,pc} */
};

/* Size check: 98 words * 4 bytes = 0x188. */
typedef char TDO_AIF_DECOMP_STUB_SIZE_CHECK
  [(sizeof(TDO_AIF_DECOMP_STUB) == 0x188) ? 1 : -1];
#define TDO_AIF_DECOMP_STUB_SIZE ((size_t)sizeof(TDO_AIF_DECOMP_STUB))


/* -------------------------------------------------------------------------- */
/* Full-image compressor.                                                     */
/* -------------------------------------------------------------------------- */

/* Stub header-word offsets (patched per file). */
#define STUB_BRANCH_OFS         0x00u  /* B past the data table */
#define STUB_MAGIC_OFS          0x04u  /* compressor magic = 1   */
#define STUB_DECOMP_OFS_OFS     0x08u  /* absolute offset of stub */
#define STUB_DECOMP_END_OFS     0x0Cu  /* AIF_HEADER_SIZE + body_size */
#define STUB_SCRATCH_DELTA_OFS  0x10u  /* relocation scratch size */

/* Stub offset of the code the stub copies to scratch. The body decoder
   writes upward from 0x100, so decompressed_end must stay below
   decomp_start + scratch_delta + this offset. */
#define STUB_RELOC_OFS          0x5Cu

#define STUB_BRANCH_INSTR       0xEA000003u  /* B (skip 4 data words) */
#define STUB_MAGIC_VALUE        0x00000001u

/* Safety margin for the scratch area in which the stub relocates
   itself and the compressed stream during decompression. Not
   load-bearing: the value it produces is verified per file against
   stub_scratch_needed() and the image is declined when it falls
   short. The observed Opera-stock padding is 0x20c..0x2e4. */
#define SCRATCH_DELTA_MARGIN    0x400u


/*
  Minimum scratch_delta the decompressor stub needs to decode `stream_`.

  The stub relocates the stream to
  [scratch_delta + AIF_HEADER_SIZE, decomp_start + scratch_delta) and then decodes it
  forward while writing the body forward from AIF_HEADER_SIZE. After m body words it
  has written 4*m bytes and consumed i(m) stream bytes (word count, dictionary, one
  control byte per word, every literal byte), so the next byte it reads sits at
  scratch_delta + AIF_HEADER_SIZE + i(m). Every byte it has not read yet must stay
  above the write cursor:

      scratch_delta >= max over m of (4*m - i(m))

  Below that the decoder reads stream bytes it has already overwritten, which decodes
  the body to garbage or walks the stub into code the body overwrote.

  Returns 0 with *need_ set, or -1 if `stream_` is truncated (impossible for a stream
  produced by tdo_aif_compress_body()).
*/
static
int
stub_scratch_needed(const uint8_t *stream_,
                    size_t         stream_size_,
                    size_t        *need_)
{
  size_t   pos = WORD_SIZE;
  uint32_t word_count;
  int64_t  deficit;
  int64_t  max_deficit;

  if(stream_size_ < WORD_SIZE)
    return -1;

  word_count = rd_be32(stream_);

  /* Dictionary: 96 control bytes, each gating 8 slots; a 0 bit means the
     slot's byte is present in the stream as a literal. */
  for(unsigned i = 0; i < DICT_CTRL_BYTES; i++)
    {
      uint8_t ctrl;

      if(pos >= stream_size_)
        return -1;
      ctrl = stream_[pos++];

      for(unsigned k = 0; k < 8; k++, ctrl >>= 1)
        {
          if((ctrl & 1u) == 0)
            {
              if(pos >= stream_size_)
                return -1;
              pos++;
            }
        }
    }

  /* Nothing has been written yet, so the deficit is only what the word
     count and the dictionary have consumed. */
  max_deficit = deficit = -(int64_t)pos;

  for(uint32_t w = 0; w < word_count; w++)
    {
      uint8_t ctrl;
      int64_t literals = 0;

      if(pos >= stream_size_)
        return -1;
      ctrl = stream_[pos++];

      for(unsigned byte_idx = 0; byte_idx < 4; byte_idx++)
        {
          if(((ctrl >> (byte_idx * 2)) & 0x3u) == 0)
            {
              if(pos >= stream_size_)
                return -1;
              pos++;
              literals++;
            }
        }

      /* One word written, one control byte plus its literals read. */
      deficit += ((int64_t)WORD_SIZE - (1 + literals));
      if(deficit > max_deficit)
        max_deficit = deficit;
    }

  *need_ = ((max_deficit > 0) ? (size_t)max_deficit : 0u);
  return 0;
}


/* Hand back an owned copy of the image that was passed in and report the
   reason compression was declined. Only sets *out_ on success; a returned
   ERROR means the caller got no output buffer. */
static
TdoAifCompressStatus
declined_image(const uint8_t        *in_,
               size_t                in_size_,
               uint8_t             **out_,
               size_t               *out_size_,
               TdoAifCompressStatus  reason_)
{
  uint8_t *out;

  out = (uint8_t*)malloc(in_size_ ? in_size_ : 1);
  if(out == NULL)
    return TdoAifCompressStatus_ERROR;

  if(in_size_)
    memcpy(out, in_, in_size_);

  *out_      = out;
  *out_size_ = in_size_;
  return reason_;
}


TdoAifCompressStatus
tdo_aif_compress(const uint8_t  *in_,
                 size_t          in_size_,
                 uint8_t       **out_,
                 size_t         *out_size_)
{
  const uint8_t *orig_in   = in_;
  size_t         orig_size = in_size_;
  uint32_t  sig_offset;
  uint32_t  sig_size;
  size_t    body_end;
  size_t    raw_body_size;
  size_t    body_size;
  uint8_t  *body_buf   = NULL;
  uint8_t  *decomp_buf = NULL;
  size_t    decomp_buf_size = 0;
  uint8_t  *stream     = NULL;
  size_t    stream_size;
  size_t    decomp_start;
  size_t    decompressed_end;
  uint32_t  scratch_delta;
  uint8_t  *out;
  size_t    out_size;
  size_t    stub_at;
  size_t    scratch_needed;
  TdoAifCompressStatus status;

  if(in_size_ < AIF_HEADER_SIZE)
    return TdoAifCompressStatus_ERROR;

  /* --- Normalise input ----------------------------------------- */
  /* --compress is idempotent: if the input is already compressed
     (BL at offset 0), decompress it in-place first. */
  if((rd_be32(in_) >> 24) == BL_OPCODE)
    {
      if(tdo_aif_decompress(in_, in_size_, &decomp_buf, &decomp_buf_size) != 0)
        return TdoAifCompressStatus_ERROR;
      in_      = decomp_buf;
      in_size_ = decomp_buf_size;
    }

  /* --- Locate body & (optional) trailing signature ------------- */
  sig_offset = tdo_aif_get_sig_offset((void*)in_);
  sig_size   = tdo_aif_get_sig_size((void*)in_);
  if(sig_offset && sig_size &&
     (size_t)sig_offset >= AIF_HEADER_SIZE &&
     (size_t)sig_offset + (size_t)sig_size <= in_size_)
    {
      body_end = sig_offset;
    }
  else
    {
      body_end   = in_size_;
      sig_offset = 0;
      sig_size   = 0;
    }

  if(body_end < AIF_HEADER_SIZE)
    goto fail;

  /* --- Prepare padded body (must be a multiple of 4) ----------- */
  raw_body_size = body_end - AIF_HEADER_SIZE;
  body_size     = PAD_UP(raw_body_size, WORD_SIZE);

  body_buf = (uint8_t*)malloc(body_size ? body_size : 1);
  if(body_buf == NULL)
    goto fail;
  if(raw_body_size)
    memcpy(body_buf, in_ + AIF_HEADER_SIZE, raw_body_size);
  if(body_size > raw_body_size)
    memset(body_buf + raw_body_size, 0, body_size - raw_body_size);

  /* --- Compress body ------------------------------------------- */
  if(tdo_aif_compress_body(body_buf, body_size, &stream, &stream_size) != 0)
    goto fail;

  /* Pad the compressed stream to a 4-byte boundary so the stub
     that follows (and the BL at offset 0 that targets it) remains
     word aligned. Padding bytes are never read by the decoder. */
  {
    size_t padded = PAD_UP(stream_size, WORD_SIZE);
    if(padded != stream_size)
      {
        uint8_t *p = (uint8_t*)realloc(stream, padded);
        if(p == NULL)
          goto fail;
        memset(p + stream_size, 0, padded - stream_size);
        stream      = p;
        stream_size = padded;
      }
  }

  /* --- Compute stub header fields ------------------------------ */
  decomp_start     = AIF_HEADER_SIZE + stream_size;
  decompressed_end = AIF_HEADER_SIZE + body_size;
  {
    size_t delta = decompressed_end > decomp_start
                     ? (decompressed_end - decomp_start)
                     : 0u;
    delta += SCRATCH_DELTA_MARGIN;
    scratch_delta = (uint32_t)PAD_UP(delta, WORD_SIZE);
  }

  /* --- Assemble output image ----------------------------------- */
  /*   [AIF header | compressed stream | stub | signature]        */
  out_size = decomp_start + TDO_AIF_DECOMP_STUB_SIZE + sig_size;

  /* --- Refuse images the stub cannot decompress, or that do not ---- */
  /*     shrink: the caller gets the input back unchanged instead of   */
  /*     a file that would corrupt itself at load.                    */
  if(stub_scratch_needed(stream, stream_size, &scratch_needed) != 0)
    goto fail;

  if(decompressed_end > (decomp_start + STUB_RELOC_OFS))
    {
      size_t tail_need = (decompressed_end - decomp_start - STUB_RELOC_OFS);
      if(tail_need > scratch_needed)
        scratch_needed = tail_need;
    }

  status = TdoAifCompressStatus_OK;
  if((size_t)scratch_delta < scratch_needed)
    status = TdoAifCompressStatus_DECLINED_UNSAFE;
  else if(out_size >= in_size_)
    status = TdoAifCompressStatus_DECLINED_GROWTH;

  if(status != TdoAifCompressStatus_OK)
    {
      free(body_buf);
      free(stream);
      free(decomp_buf);
      return declined_image(orig_in, orig_size, out_, out_size_, status);
    }

  out      = (uint8_t*)malloc(out_size ? out_size : 1);
  if(out == NULL)
    goto fail;

  /* Header + compressed stream. */
  memcpy(out, in_, AIF_HEADER_SIZE);
  memcpy(out + AIF_HEADER_SIZE, stream, stream_size);

  /* Stub body, then overwrite the five per-file header words. */
  stub_at = decomp_start;
  for(size_t i = 0; i < TDO_AIF_DECOMP_STUB_SIZE / WORD_SIZE; i++)
    wr_be32(out + stub_at + i * WORD_SIZE, TDO_AIF_DECOMP_STUB[i]);

  wr_be32(out + stub_at + STUB_BRANCH_OFS,        STUB_BRANCH_INSTR);
  wr_be32(out + stub_at + STUB_MAGIC_OFS,         STUB_MAGIC_VALUE);
  wr_be32(out + stub_at + STUB_DECOMP_OFS_OFS,    (uint32_t)decomp_start);
  wr_be32(out + stub_at + STUB_DECOMP_END_OFS,    (uint32_t)decompressed_end);
  wr_be32(out + stub_at + STUB_SCRATCH_DELTA_OFS, scratch_delta);

  /* BL at offset 0 -> stub entry. */
  wr_be32(out, arm_bl_encode(decomp_start));

  /* Relocate signature (if any) to the new end-of-image. */
  if(sig_size)
    {
      size_t new_sig_offset = stub_at + TDO_AIF_DECOMP_STUB_SIZE;
      memcpy(out + new_sig_offset, in_ + sig_offset, sig_size);
      tdo_aif_set_sig_offset(out, (uint32_t)new_sig_offset);
      /* sig_size field unchanged. */
    }

  free(body_buf);
  free(stream);
  free(decomp_buf);

  *out_      = out;
  *out_size_ = out_size;
  return TdoAifCompressStatus_OK;

 fail:
  free(body_buf);
  free(stream);
  free(decomp_buf);
  return TdoAifCompressStatus_ERROR;
}
