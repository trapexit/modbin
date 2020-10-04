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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


void tdo_aif_set_3do_flag(void *buf);
void tdo_aif_set_debug(void *buf);
void tdo_aif_set_nodebug(void *buf);
void tdo_aif_set_priority(void *buf, uint8_t val);
void tdo_aif_set_version(void *buf, uint8_t val);
void tdo_aif_set_flags(void *buf, uint8_t val);
void tdo_aif_set_osversion(void *buf, uint8_t val);
void tdo_aif_set_osrevision(void *buf, uint8_t val);
void tdo_aif_set_stack(void *buf, uint32_t val);
void tdo_aif_set_freespace(void *buf, uint32_t val);
void tdo_aif_set_maxusecs(void *buf, uint32_t val);
void tdo_aif_set_sig_offset(void *buf, uint32_t val);
void tdo_aif_set_sig_size(void *buf, uint32_t val);
void tdo_aif_set_name(void *buf, const char *val);

uint8_t     tdo_aif_get_3do_flag(void *buf);
uint32_t    tdo_aif_get_debug(void *buf);
uint8_t     tdo_aif_get_priority(void *buf);
uint8_t     tdo_aif_get_version(void *buf);
uint8_t     tdo_aif_get_flags(void *buf);
uint8_t     tdo_aif_get_osversion(void *buf);
uint8_t     tdo_aif_get_osrevision(void *buf);
uint32_t    tdo_aif_get_stack(void *buf);
uint32_t    tdo_aif_get_freespace(void *buf);
uint32_t    tdo_aif_get_maxusecs(void *buf);
uint32_t    tdo_aif_get_sig_offset(void *buf);
uint32_t    tdo_aif_get_sig_size(void *buf);
const char *tdo_aif_get_name(void *buf);

bool tdo_aif_is_aif(void *buf, long size);

void tdo_aif_reset(void *buf);

bool tdo_aif_has_sig(void *buf);

void tdo_aif_print(FILE *output, void *buf);
void tdo_aif_print_signature(FILE *output, void *buf);
