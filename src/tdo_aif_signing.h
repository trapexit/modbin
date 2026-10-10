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

#pragma once

#include <stddef.h>
#include <stdint.h>

// Signs a caller-owned heap AIF buffer with the borrowed key name.
// workspace_ is a borrowed optional full-word override for both hashing and
// output; NULL preserves the signature metadata setters' workspace marker.
// Returns 0 after updating *buf_ (which may move) and *size_; the caller retains
// ownership. Returns -1 on allocation failure, leaving *buf_ and *size_ unchanged
// but possibly modifying header metadata in the original buffer.
int
tdo_aif_sign(void           **buf_,
             size_t          *size_,
             const char      *key_,
             const uint32_t  *workspace_);
