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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *
fileio_read_all(const char *filepath_,
                size_t     *size_)
{
  void *buf;
  size_t size;
  FILE *file;
  size_t rv;

  file = fopen(filepath_,"rb");
  if(file == NULL)
    {
      fprintf(stderr,
              "ERROR: failed to open input file '%s' - %s\n",
              filepath_,
              strerror(errno));
      return NULL;
    }

  fseek(file,0,SEEK_END);
  size = ftell(file);
  if(size > (1024*1024*16))
    {
      fprintf(stderr,
              "ERROR: input file too large and unlikely to be legitimate - '%s'\n",
              filepath_);
      fclose(file);
      return NULL;
    }

  fseek(file,0,SEEK_SET);

  buf = malloc(size);
  if(buf == NULL)
    {
      fprintf(stderr,
              "ERROR: failed allocate memory - %s\n",
              strerror(errno));
      fclose(file);
      return NULL;
    }

  rv = fread(buf,1,size,file);
  if(rv != size)
    {
      fprintf(stderr,
              "ERROR: failed to read file fully - %zu / %zu bytes - '%s'\n",
              rv,
              size,
              filepath_);
      fclose(file);
      return NULL;
    }

  *size_ = size;

  fclose(file);

  return buf;
}

int
fileio_write_all(const char   *filepath_,
                 const void   *data_,
                 const size_t  size_)
{
  FILE *file;
  size_t rv;

  file = fopen(filepath_,"wb");
  if(file == NULL)
    {
      fprintf(stderr,
              "ERROR: failed to open input file '%s' - %s\n",
              filepath_,
              strerror(errno));
      return -1;
    }

  rv = fwrite(data_,1,size_,file);
  if(rv != size_)
    {
      fprintf(stderr,
              "ERROR: failed to write file fully - %zu / %zu bytes - %s\n",
              rv,
              size_,
              filepath_);
    }

  fclose(file);

  return 0;
}
