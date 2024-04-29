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
#include "simple-opt.h"
#include "str.h"
#include "tdo_aif.h"
#include "tdo_aif_signing.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MODBIN_VERSION "1.3.1"

static
struct simple_opt*
simple_opt_options(void)
{
  static const char *key_set[] = {"app","3do",NULL};
  static struct simple_opt options[] =
    {
     {SIMPLE_OPT_FLAG,       'h',"help",       false,"print this help message and exit"},
     {SIMPLE_OPT_FLAG,       'V',NULL,         false,"print modbin version"},
     {SIMPLE_OPT_FLAG,      '\0',"debug",      false,"enable debugging"},
     {SIMPLE_OPT_FLAG,      '\0',"nodebug",    false,"disable debugging"},
     {SIMPLE_OPT_UNSIGNED,  '\0',"subsystype", true, "set folio subtype"},
     {SIMPLE_OPT_UNSIGNED,  '\0',"type",       true, "set folio node type"},
     {SIMPLE_OPT_UNSIGNED,  '\0',"pri",        true, "set priority"},
     {SIMPLE_OPT_UNSIGNED,  '\0',"version",    true, "set version number"},
     {SIMPLE_OPT_UNSIGNED,  '\0',"flags",      true, "set app flags"},
     {SIMPLE_OPT_UNSIGNED,  '\0',"osversion",  true, "set OS_version number"},
     {SIMPLE_OPT_UNSIGNED,  '\0',"osrevision", true, "set OS_revision number"},
     {SIMPLE_OPT_UNSIGNED,  '\0',"stack",      true, "set stack size"},
     {SIMPLE_OPT_UNSIGNED,  '\0',"freespace",  true, "set freespace"},
     {SIMPLE_OPT_UNSIGNED,  '\0',"maxusecs",   true, "set maximum usecs"},
     {SIMPLE_OPT_STRING,    '\0',"name",       true, "executable name"},
     {SIMPLE_OPT_FLAG,      '\0',"reset",      false,"resets all values to default"},
     {SIMPLE_OPT_STRING_SET,'\0',"sign",       true, "sign executable","app|3do", key_set},
     {SIMPLE_OPT_END}
    };

  return options;
}

int
main(int    argc_,
     char **argv_)
{
  int rv;
  void *file_buf;
  size_t file_size;
  const char *sign;
  const char *input_file;
  const char *output_file;
  struct simple_opt *options;
  struct simple_opt_result result;

  options = simple_opt_options();
  result  = simple_opt_parse(argc_,argv_,options);

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

  if(options[0].was_seen || (result.argc < 1))
    {
      simple_opt_print_usage(stdout,
                             80,
                             "modbin",
                             "[options]... <input-file> [<output-file>]",
                             "modbin is used to set 3DO AIF header values and sign executables.",
                             options);
      exit(EXIT_SUCCESS);
    }

  input_file  = result.argv[0];
  output_file = ((result.argc == 2) ? result.argv[1] : NULL);

  file_buf = fileio_read_all(input_file,&file_size);
  if(file_buf == NULL)
    {
      fprintf(stderr,"ERROR: unable to open file - %s\n",input_file);
      exit(EXIT_FAILURE);
    }

  if(!tdo_aif_is_aif(file_buf,file_size))
    {
      fprintf(stderr,"ERROR: does not appear to be a valid AIF file\n");
      exit(EXIT_FAILURE);
    }

  sign = NULL;
  for(int i = 0; options[i].type != SIMPLE_OPT_END; i++)
    {
      if(!options[i].was_seen)
        continue;

      if(streq(options[i].long_name,"debug"))
        tdo_aif_set_debug(file_buf);
      else if(streq(options[i].long_name,"nodebug"))
        tdo_aif_set_nodebug(file_buf);
      else if(streq(options[i].long_name,"subsystype"))
        tdo_aif_set_subsystype(file_buf,options[i].val.v_unsigned);
      else if(streq(options[i].long_name,"type"))
        tdo_aif_set_type(file_buf,options[i].val.v_unsigned);
      else if(streq(options[i].long_name,"pri"))
        tdo_aif_set_priority(file_buf,options[i].val.v_unsigned);
      else if(streq(options[i].long_name,"version"))
        tdo_aif_set_version(file_buf,options[i].val.v_unsigned);
      else if(streq(options[i].long_name,"flags"))
        tdo_aif_set_flags(file_buf,options[i].val.v_unsigned);
      else if(streq(options[i].long_name,"osversion"))
        tdo_aif_set_osversion(file_buf,options[i].val.v_unsigned);
      else if(streq(options[i].long_name,"osrevision"))
        tdo_aif_set_osrevision(file_buf,options[i].val.v_unsigned);
      else if(streq(options[i].long_name,"stack"))
        tdo_aif_set_stack(file_buf,options[i].val.v_unsigned);
      else if(streq(options[i].long_name,"freespace"))
        tdo_aif_set_freespace(file_buf,options[i].val.v_unsigned);
      else if(streq(options[i].long_name,"maxusecs"))
        tdo_aif_set_maxusecs(file_buf,options[i].val.v_unsigned);
      else if(streq(options[i].long_name,"name"))
        tdo_aif_set_name(file_buf,options[i].val.v_string);
      else if(streq(options[i].long_name,"reset"))
        tdo_aif_reset(file_buf,&file_size);
      else if(streq(options[i].long_name,"sign"))
        sign = options[i].string_set[options[i].val.v_string_set_idx];
    }

  if(sign != NULL)
    {
      rv = tdo_aif_sign(&file_buf,&file_size,sign);
      if(rv == -1)
        goto error;
    }

  tdo_aif_print(stdout,file_buf);

  if(output_file != NULL)
    rv = fileio_write_all(output_file,file_buf,file_size);

 error:
  free(file_buf);

  return ((rv == 0) ? 0 : 1);
}
