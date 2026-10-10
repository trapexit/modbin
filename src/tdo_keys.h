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

#include "bigd.h"
#include "md5.h"

BIGD tdo_keys_m1_retail_3do_n(void);
BIGD tdo_keys_m1_retail_3do_d(void);
BIGD tdo_keys_m1_retail_app_n(void);
BIGD tdo_keys_m1_retail_app_d(void);

BIGD tdo_keys_m1_retail_message(md5_digest_t digest);

// key must be non-NULL and exactly "app" or "3do"; other names assert and abort,
// including when assertions are disabled. Each result is a newly allocated
// BIGD owned by the caller, which must release it with bdFree().
// m requires a valid MD5 digest and uses the same retail encoding for both keys.
BIGD tdo_keys_n(const char *key);
BIGD tdo_keys_d(const char *key);
BIGD tdo_keys_m(const char *key, md5_digest_t digest);
