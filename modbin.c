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

#include "simple-opt.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MODBIN_VERSION "1.0.0"

#define WORK_SPACE_OFFSET 0x2C
#define DEBUG_OFFSET      0x40
#define PRIORITY_OFFSET   0x8A
#define VERSION_OFFSET    0x94
#define FLAGS_OFFSET      0xA4
#define OSVERSION_OFFSET  0xA5
#define OSREVISION_OFFSET 0xA6
#define STACK_OFFSET      0xA8
#define FREESPACE_OFFSET  0xAC
#define MAXUSECS_OFFSET   0xBC

#define WORK_SPACE_VALUE 0x40
#define DEBUG_VALUE      0xEF00010A
#define NODEBUG_VALUE    0xE1A02002


static
uint32_t
bswap32(const uint32_t i_)
{
  return (((i_ >> 24) & 0x000000FF) |
          ((i_ >>  8) & 0x0000FF00) |
          ((i_ <<  8) & 0x00FF0000) |
          ((i_ << 24) & 0xFF000000));
}

static
void
set_3do_flag(FILE *file_)
{
  uint8_t b;
  size_t rv;

  b = WORK_SPACE_VALUE;
  fseek(file_,WORK_SPACE_OFFSET,SEEK_SET);
  rv = fwrite(&b,1,1,file_);
  assert(rv == 1);
}

static
void
set_byte(FILE    *file_,
         int      off_,
         uint8_t  val_)
{
  size_t rv;

  set_3do_flag(file_);

  fseek(file_,off_,SEEK_SET);
  rv = fwrite(&val_,1,1,file_);
  assert(rv == 1);
}

static
void
set_word(FILE     *file_,
         int       off_,
         uint32_t  val_)
{
  size_t rv;

  set_3do_flag(file_);

  val_ = bswap32(val_);
  fseek(file_,off_,SEEK_SET);
  rv = fwrite(&val_,4,1,file_);
  assert(rv == 1);
}

static
uint8_t
get_byte(FILE *file_,
         int   off_)
{
  uint8_t b;
  size_t rv;

  fseek(file_,off_,SEEK_SET);
  rv = fread(&b,1,1,file_);
  assert(rv == 1);

  return b;
}

static
uint32_t
get_word(FILE *file_,
         int   off_)
{
  size_t rv;
  uint32_t w;

  fseek(file_,off_,SEEK_SET);
  rv = fread(&w,4,1,file_);
  assert(rv == 1);

  return bswap32(w);
}

static
void
print_values(FILE *file_)
{
  uint8_t b;
  uint32_t w;

  b = get_byte(file_,WORK_SPACE_OFFSET);
  if(b != 0x40)
    {
      printf("3DO header information has not been set\n");
      return;
    }

  printf("3DO header is:\n");

  w = get_word(file_,DEBUG_OFFSET);
  if(w == DEBUG_VALUE)
    printf("  debugging disabled: 0x%.8x\n",w);
  else if(w == NODEBUG_VALUE)
    printf("  debugging enabled: 0x%.8x\n",w);
  else
    printf("  debugging not set: 0x%.8x\n",w);

  b = get_byte(file_,PRIORITY_OFFSET);
  printf("  priority: 0x%.2x (%d)\n",b,b);

  b = get_byte(file_,VERSION_OFFSET);
  printf("  version: 0x%.2x (%d)\n",b,b);

  b = get_byte(file_,FLAGS_OFFSET);
  printf("  flags: 0x%.2x (%d)\n",b,b);

  b = get_byte(file_,OSVERSION_OFFSET);
  printf("  osversion: 0x%.2x (%d)\n",b,b);

  b = get_byte(file_,OSREVISION_OFFSET);
  printf("  osrevision: 0x%.2x (%d)\n",b,b);

  w = get_word(file_,STACK_OFFSET);
  printf("  stack size: 0x%.8x (%d)\n",w,w);

  w = get_word(file_,FREESPACE_OFFSET);
  printf("  freespace: 0x%.8x (%d)\n",w,w);

  w = get_word(file_,MAXUSECS_OFFSET);
  printf("  max usecs: 0x%.8x (%d)\n",w,w);
}

static
void
reset_values(FILE *file_)
{
  set_byte(file_,WORK_SPACE_OFFSET,0x00);
  set_word(file_,DEBUG_OFFSET,0xE1A00000);
  set_byte(file_,PRIORITY_OFFSET,0x00);
  set_byte(file_,VERSION_OFFSET,0x00);
  set_byte(file_,FLAGS_OFFSET,0x00);
  set_byte(file_,OSVERSION_OFFSET,0x18);
  set_byte(file_,OSREVISION_OFFSET,0x00);
  set_word(file_,STACK_OFFSET,0x00000000);
  set_word(file_,FREESPACE_OFFSET,0x00000000);
  set_word(file_,MAXUSECS_OFFSET,0x00000000);
}

int
main(int    argc_,
     char **argv_)
{
  bool show;
  bool verbose;
  FILE *file;
  uint32_t v;
  struct simple_opt_result result;
  struct simple_opt options[] =
    {
     {SIMPLE_OPT_FLAG, 'h',"help",      false,"print this help message and exit"},
     {SIMPLE_OPT_FLAG, 'V',NULL,        false,"print modbin version"},
     {SIMPLE_OPT_FLAG,'\0',"debug",     false,"enable debugging"},
     {SIMPLE_OPT_FLAG,'\0',"nodebug",   false,"disable debugging"},
     {SIMPLE_OPT_INT, '\0',"pri",       true, "set priority"},
     {SIMPLE_OPT_INT, '\0',"version",   true, "set version number"},
     {SIMPLE_OPT_INT, '\0',"flags",     true, "set app flags"},
     {SIMPLE_OPT_INT, '\0',"osversion", true, "set OS_version number"},
     {SIMPLE_OPT_INT, '\0',"osrevision",true, "set OS_revision number"},
     {SIMPLE_OPT_INT, '\0',"stack",     true, "set stack size"},
     {SIMPLE_OPT_INT, '\0',"freespace", true, "set freespace"},
     {SIMPLE_OPT_INT, '\0',"maxusecs",  true, "set maximum usecs"},
     {SIMPLE_OPT_FLAG,'\0',"verbose",   false,"turn on verbose mode"},
     {SIMPLE_OPT_FLAG,'\0',"reset",     false,"resets all values to default"},
     {SIMPLE_OPT_END}
    };

  result = simple_opt_parse(argc_,argv_,options);

  if(result.result_type != SIMPLE_OPT_RESULT_SUCCESS)
    {
      simple_opt_print_error(stderr,80,argv_[0],result);
      exit(EXIT_FAILURE);
    }

  if(options[1].was_seen)
    {
      printf("modbin version: %s\n",MODBIN_VERSION);
      exit(EXIT_SUCCESS);
    }

  if(options[0].was_seen || (result.argc != 1))
    {
      simple_opt_print_usage(stdout,
                             80,
                             argv_[0],
                             "[options]... <file>",
                             "modbin is used to set 3DO AIF header values.",
                             options);
      exit(EXIT_SUCCESS);
    }

  file = fopen(result.argv[0],"r+");
  if(file == NULL)
    {
      fprintf(stderr,"error: unable to open files - %s\n",result.argv[0]);
      exit(EXIT_FAILURE);
    }

  show = true;
  verbose = false;
  for(int i = 0; options[i].type != SIMPLE_OPT_END; i++)
    {
      if(!options[i].was_seen)
        continue;

      show = false;
      if(!strcmp(options[i].long_name,"debug"))
        set_word(file,DEBUG_OFFSET,DEBUG_VALUE);
      else if(!strcmp(options[i].long_name,"nodebug"))
        set_word(file,DEBUG_OFFSET,NODEBUG_VALUE);
      else if(!strcmp(options[i].long_name,"pri"))
        set_byte(file,PRIORITY_OFFSET,options[i].val.v_int);
      else if(!strcmp(options[i].long_name,"version"))
        set_byte(file,VERSION_OFFSET,options[i].val.v_int);
      else if(!strcmp(options[i].long_name,"flags"))
        set_byte(file,FLAGS_OFFSET,options[i].val.v_int);
      else if(!strcmp(options[i].long_name,"osversion"))
        set_byte(file,OSVERSION_OFFSET,options[i].val.v_int);
      else if(!strcmp(options[i].long_name,"osrevision"))
        set_byte(file,OSREVISION_OFFSET,options[i].val.v_int);
      else if(!strcmp(options[i].long_name,"stack"))
        set_word(file,STACK_OFFSET,options[i].val.v_int);
      else if(!strcmp(options[i].long_name,"freespace"))
        set_word(file,FREESPACE_OFFSET,options[i].val.v_int);
      else if(!strcmp(options[i].long_name,"maxusecs"))
        set_word(file,MAXUSECS_OFFSET,options[i].val.v_int);
      else if(!strcmp(options[i].long_name,"verbose"))
        verbose = true;
      else if(!strcmp(options[i].long_name,"reset"))
        reset_values(file);
    }

  if(verbose || show)
    print_values(file);

  fclose(file);

  return 0;
}
