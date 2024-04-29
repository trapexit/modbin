/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "fileio.h"
#include "md5.h"
#include "tdo_aif.h"
#include "tdo_keys.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RSA512_SIG_SIZE 64

typedef unsigned char rsa512_sig_t[RSA512_SIG_SIZE];

static
void
calculate_md5(const char   *data_,
              md5_size_t    size_,
              md5_digest_t  digest_)
{
  md5_ctx_t ctx;

  md5_init(&ctx);
  md5_update(&ctx,data_,size_);
  md5_finalize(&ctx,digest_);
}

static
void
sign_md5_digest(const char   *key_,
                md5_digest_t  digest_,
                rsa512_sig_t  sig_)
{
  BIGD n;
  BIGD d;
  BIGD m;
  BIGD s;

  n = tdo_keys_n(key_);
  d = tdo_keys_d(key_);
  m = tdo_keys_m(key_,digest_);
  s = bdNew();

  bdModExp(s,m,d,n);

  bdConvToOctets(s,sig_,sizeof(rsa512_sig_t));

  bdFree(&s);
  bdFree(&m);
  bdFree(&d);
  bdFree(&n);
}

static
bool
end_of_buffer_0xFFFFFFFF(void   *buf_,
                         size_t  size_)
{
  size_t offset;
  uint8_t *buf;

  buf    = buf_;
  offset = (size_ - 4);

  return ((buf[offset + 0] == 0xFF) &&
          (buf[offset + 1] == 0xFF) &&
          (buf[offset + 2] == 0xFF) &&
          (buf[offset + 3] == 0xFF));
}

int
tdo_aif_sign(void       **buf_,
             size_t      *size_,
             const char  *key_)
{
  size_t size;
  char *buf;
  rsa512_sig_t sig;
  md5_digest_t digest;

  buf  = *buf_;
  size = *size_;

  if(tdo_aif_has_sig(buf))
    {
      fprintf(stderr,"WARNING: file already has signature. Ignoring.\n");
      size -= RSA512_SIG_SIZE;
      tdo_aif_set_sig_size(buf,0);
    }

  if(!end_of_buffer_0xFFFFFFFF(buf,size))
    fprintf(stderr,"WARNING: file doesn't appear to be an ARM executable. File last 4 bytes != 0xFF.\n");

  tdo_aif_set_sig_offset(buf,size);

  calculate_md5(buf,size,digest);

  sign_md5_digest(key_,digest,sig);

  tdo_aif_set_sig_size(buf,RSA512_SIG_SIZE);

  buf = realloc(buf,(size+RSA512_SIG_SIZE));
  if(buf == NULL)
    {
      fprintf(stderr,"ERROR: failed to allocate memory - %s",strerror(errno));
      return -1;
    }

  memcpy(&buf[size],sig,RSA512_SIG_SIZE);

  *buf_  = buf;
  *size_ = size + RSA512_SIG_SIZE;

  return 0;
}
