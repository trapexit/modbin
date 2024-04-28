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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define TDO_HEADER_OFFSET 0x2C
#define DEBUG_OFFSET      0x40
#define SUBSYSTYPE_OFFSET 0x88
#define TYPE_OFFSET       0x89
#define PRIORITY_OFFSET   0x8A
#define VERSION_OFFSET    0x94
#define FLAGS_OFFSET      0xA4
#define OSVERSION_OFFSET  0xA5
#define OSREVISION_OFFSET 0xA6
#define STACK_OFFSET      0xA8
#define FREESPACE_OFFSET  0xAC
#define SIG_OFFSET_OFFSET 0xB0
#define SIG_SIZE_OFFSET   0xB4
#define MAXUSECS_OFFSET   0xBC
#define NAME_OFFSET       0xC0
#define NAME_SIZE         32

#define TDO_HEADER_VALUE 0x40
#define DEBUG_VALUE      0xEF00010A
#define NODEBUG_VALUE    0xE1A02002

static
void
set_byte(uint8_t *buf_,
         long     off_,
         uint8_t  val_)
{
  buf_[off_] = val_;
}

static
void
set_word(uint8_t  *buf_,
         long      off_,
         uint32_t  val_)
{
  buf_[off_ + 0] = ((uint8_t)(val_ >> 24));
  buf_[off_ + 1] = ((uint8_t)(val_ >> 16));
  buf_[off_ + 2] = ((uint8_t)(val_ >>  8));
  buf_[off_ + 3] = ((uint8_t)(val_ >>  0));
}

static
uint8_t
get_byte(const uint8_t *buf_,
         long           off_)
{
  return buf_[off_];
}

static
uint32_t
get_word(const uint8_t *buf_,
         long           off_)
{
  return (((uint8_t)buf_[off_ + 0] << 24) |
          ((uint8_t)buf_[off_ + 1] << 16) |
          ((uint8_t)buf_[off_ + 2] <<  8) |
          ((uint8_t)buf_[off_ + 3] <<  0));
}

void
tdo_aif_set_3do_flag(void *buf_)
{
  set_byte(buf_,TDO_HEADER_OFFSET,TDO_HEADER_VALUE);
}

void
tdo_aif_reset_3do_flag(void *buf_)
{
  set_byte(buf_,TDO_HEADER_OFFSET,0x00);
}

void
tdo_aif_set_debug(void *buf_)
{
  tdo_aif_set_3do_flag(buf_);
  set_word(buf_,DEBUG_OFFSET,DEBUG_VALUE);
}

void
tdo_aif_set_nodebug(void *buf_)
{
  tdo_aif_set_3do_flag(buf_);
  set_word(buf_,DEBUG_OFFSET,NODEBUG_VALUE);
}

void
tdo_aif_set_priority(void    *buf_,
                     uint8_t  val_)
{
  tdo_aif_set_3do_flag(buf_);
  set_byte(buf_,PRIORITY_OFFSET,val_);
}

void
tdo_aif_set_subsystype(void    *buf_,
                       uint8_t  val_)
{
  tdo_aif_set_3do_flag(buf_);
  set_byte(buf_,SUBSYSTYPE_OFFSET,val_);
}

void
tdo_aif_set_type(void    *buf_,
                 uint8_t  val_)
{
  tdo_aif_set_3do_flag(buf_);
  set_byte(buf_,TYPE_OFFSET,val_);
}


void
tdo_aif_set_version(void    *buf_,
                    uint8_t  val_)
{
  tdo_aif_set_3do_flag(buf_);
  set_byte(buf_,VERSION_OFFSET,val_);
}

void
tdo_aif_set_flags(void    *buf_,
                  uint8_t  val_)
{
  tdo_aif_set_3do_flag(buf_);
  set_byte(buf_,FLAGS_OFFSET,val_);
}

void
tdo_aif_set_osversion(void    *buf_,
                      uint8_t  val_)
{
  tdo_aif_set_3do_flag(buf_);
  set_byte(buf_,OSVERSION_OFFSET,val_);
}

void
tdo_aif_set_osrevision(void    *buf_,
                       uint8_t  val_)
{
  tdo_aif_set_3do_flag(buf_);
  set_byte(buf_,OSREVISION_OFFSET,val_);
}

void
tdo_aif_set_stack(void     *buf_,
                  uint32_t  val_)
{
  tdo_aif_set_3do_flag(buf_);
  set_word(buf_,STACK_OFFSET,val_);
}

void
tdo_aif_set_freespace(void     *buf_,
                      uint32_t  val_)
{
  tdo_aif_set_3do_flag(buf_);
  set_word(buf_,FREESPACE_OFFSET,val_);
}

void
tdo_aif_set_maxusecs(void     *buf_,
                     uint32_t  val_)
{
  tdo_aif_set_3do_flag(buf_);
  set_word(buf_,MAXUSECS_OFFSET,val_);
}

void
tdo_aif_set_sig_offset(void     *buf_,
                       uint32_t  val_)
{
  tdo_aif_set_3do_flag(buf_);
  set_word(buf_,SIG_OFFSET_OFFSET,val_);
}

void
tdo_aif_set_sig_size(void     *buf_,
                     uint32_t  val_)
{
  tdo_aif_set_3do_flag(buf_);
  set_word(buf_,SIG_SIZE_OFFSET,val_);
}

void
tdo_aif_set_name(void       *buf_,
                 const char *val_)
{
  char *buf = buf_;
  size_t len;

  len = strlen(val_);
  if(len > (NAME_SIZE - 1))
    len = (NAME_SIZE - 1);

  memset(&buf[NAME_OFFSET],0,NAME_SIZE);
  strncpy(&buf[NAME_OFFSET],val_,len);
  for(size_t i = len; i < NAME_SIZE; i++)
    buf[NAME_OFFSET + i] = '\0';

  tdo_aif_set_3do_flag(buf_);
}

uint8_t
tdo_aif_get_3do_flag(void *buf_)
{
  return get_byte(buf_,TDO_HEADER_OFFSET);
}

uint32_t
tdo_aif_get_debug(void *buf_)
{
  return get_word(buf_,DEBUG_OFFSET);
}

uint8_t
tdo_aif_get_priority(void *buf_)
{
  return get_byte(buf_,PRIORITY_OFFSET);
}

uint8_t
tdo_aif_get_subsystype(void *buf_)
{
  return get_byte(buf_,SUBSYSTYPE_OFFSET);
}

uint8_t
tdo_aif_get_type(void *buf_)
{
  return get_byte(buf_,TYPE_OFFSET);
}

uint8_t
tdo_aif_get_version(void *buf_)
{
  return get_byte(buf_,VERSION_OFFSET);
}

uint8_t
tdo_aif_get_flags(void *buf_)
{
  return get_byte(buf_,FLAGS_OFFSET);
}

uint8_t
tdo_aif_get_osversion(void *buf_)
{
  return get_byte(buf_,OSVERSION_OFFSET);
}

uint8_t
tdo_aif_get_osrevision(void *buf_)
{
  return get_byte(buf_,OSREVISION_OFFSET);
}

uint32_t
tdo_aif_get_stack(void *buf_)
{
  return get_word(buf_,STACK_OFFSET);
}

uint32_t
tdo_aif_get_freespace(void *buf_)
{
  return get_word(buf_,FREESPACE_OFFSET);
}

uint32_t
tdo_aif_get_maxusecs(void *buf_)
{
  return get_word(buf_,MAXUSECS_OFFSET);
}

uint32_t
tdo_aif_get_sig_offset(void *buf_)
{
  return get_word(buf_,SIG_OFFSET_OFFSET);
}

uint32_t
tdo_aif_get_sig_size(void *buf_)
{
  return get_word(buf_,SIG_SIZE_OFFSET);
}

const
char*
tdo_aif_get_name(void *buf_)
{
  return &((const char*)buf_)[NAME_OFFSET];
}

void
tdo_aif_reset_debug(void *buf_)
{
  set_word(buf_,DEBUG_OFFSET,0xE1A00000);
}

void
tdo_aif_reset(void *buf_,
              long *size_)
{
  tdo_aif_reset_debug(buf_);
  tdo_aif_set_priority(buf_,0x00);
  tdo_aif_set_subsystype(buf_,0x00);
  tdo_aif_set_type(buf_,0x00);
  tdo_aif_set_version(buf_,0x00);
  tdo_aif_set_flags(buf_,0x00);
  tdo_aif_set_osversion(buf_,0x18);
  tdo_aif_set_osrevision(buf_,0x00);
  tdo_aif_set_stack(buf_,0x00000000);
  tdo_aif_set_freespace(buf_,0x00000000);
  tdo_aif_set_maxusecs(buf_,0x00000000);
  if(tdo_aif_get_sig_offset(buf_) && tdo_aif_get_sig_size(buf_))
    *size_ = tdo_aif_get_sig_offset(buf_);
  tdo_aif_set_sig_offset(buf_,0x00000000);
  tdo_aif_set_sig_size(buf_,0x00000000);
  tdo_aif_set_name(buf_,"");
  tdo_aif_reset_3do_flag(buf_);
}

bool
tdo_aif_has_sig(void *buf_)
{
  return (tdo_aif_get_sig_size(buf_) != 0);
}

bool
tdo_aif_is_aif(void *buf_,
               long  size_)
{
  if(size_ < 256)
    return false;

  if(get_word(buf_,0) != 0xE1A00000)
    return false;

  if(tdo_aif_get_sig_offset(buf_) && tdo_aif_get_sig_size(buf_))
    {
      uint32_t size;

      size  = tdo_aif_get_sig_offset(buf_);
      size += tdo_aif_get_sig_size(buf_);
      if(size < size_)
        return false;
    }

  return true;
}

void
tdo_aif_print_signature(FILE *output_,
                        void *buf_)
{
  uint8_t *buf;
  uint32_t offset;
  uint32_t size;

  buf    = buf_;
  offset = tdo_aif_get_sig_offset(buf_);
  size   = tdo_aif_get_sig_size(buf_);

  fprintf(output_,"  signature: ");

  if((offset != 0) && (size != 0))
    {
      for(uint8_t *p = &buf[offset], *e = p + size; p < e;)
        {
          for(uint32_t i = 0; i < 16; i++, p++)
            fprintf(output_,"%.2x",*p);
          fprintf(output_,"\n             ");
        }
      fprintf(output_,"\n");
    }
  else
    {
      fprintf(output_, "none\n");
    }
}

void
tdo_aif_print(FILE *output_,
              void *buf_)
{
  uint8_t b;
  uint32_t w;

  b = tdo_aif_get_3do_flag(buf_);
  if(b != TDO_HEADER_VALUE)
    {
      fprintf(output_,"3DO header information has not been set\n");
      return;
    }

  fprintf(output_,"3DO header is:\n");

  fprintf(output_,"  name: %s\n",tdo_aif_get_name(buf_));

  w = tdo_aif_get_debug(buf_);
  switch(w)
    {
    case DEBUG_VALUE:
      fprintf(output_,"  debugging disabled: 0x%.8x\n",w);
      break;
    case NODEBUG_VALUE:
      fprintf(output_,"  debugging enabled: 0x%.8x\n",w);
      break;
    default:
      fprintf(output_,"  debugging not set: 0x%.8x\n",w);
      break;
    }

  b = tdo_aif_get_subsystype(buf_);
  fprintf(output_,"  subsystype: 0x%.2x (%d)\n",b,b);

  b = tdo_aif_get_type(buf_);
  fprintf(output_,"  type: 0x%.2x (%d)\n",b,b);

  b = tdo_aif_get_priority(buf_);
  fprintf(output_,"  priority: 0x%.2x (%d)\n",b,b);

  b = tdo_aif_get_version(buf_);
  fprintf(output_,"  version: 0x%.2x (%d)\n",b,b);

  b = tdo_aif_get_flags(buf_);
  fprintf(output_,"  flags: 0x%.2x (%d)\n",b,b);

  b = tdo_aif_get_osversion(buf_);
  fprintf(output_,"  osversion: 0x%.2x (%d)\n",b,b);

  b = tdo_aif_get_osrevision(buf_);
  fprintf(output_,"  osrevision: 0x%.2x (%d)\n",b,b);

  w = tdo_aif_get_stack(buf_);
  fprintf(output_,"  stack size: 0x%.8x (%d)\n",w,w);

  w = tdo_aif_get_freespace(buf_);
  fprintf(output_,"  freespace: 0x%.8x (%d)\n",w,w);

  w = tdo_aif_get_maxusecs(buf_);
  fprintf(output_,"  max usecs: 0x%.8x (%d)\n",w,w);

  w = tdo_aif_get_sig_offset(buf_);
  fprintf(output_,"  sig offset: 0x%.8x (%d)\n",w,w);

  w = tdo_aif_get_sig_size(buf_);
  fprintf(output_,"  sig size: 0x%.8x (%d)\n",w,w);

  tdo_aif_print_signature(output_,buf_);
}
