// Exercise key selection with release-mode assertions disabled.

#include "tdo_keys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NDEBUG
#error This helper must exercise the NDEBUG key paths.
#endif

#define HEX_BUFFER_BYTES 129
#define ARGUMENT_COUNT 3

// Consumes the caller-owned BIGD and prints its exact hexadecimal value.
static
int
_print_value(BIGD value_)
{
  char hex[HEX_BUFFER_BYTES];
  size_t length;

  length = bdConvToHex(value_,hex,sizeof(hex));
  bdFree(&value_);
  if(length >= sizeof(hex))
    return EXIT_FAILURE;
  if(puts(hex) == EOF)
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}


int
main(int    argc_,
     char **argv_)
{
  md5_digest_t digest;
  size_t index;

  if(argc_ != ARGUMENT_COUNT)
    return EXIT_FAILURE;
  if(strcmp(argv_[1],"n") == 0)
    return _print_value(tdo_keys_n(argv_[2]));
  if(strcmp(argv_[1],"d") == 0)
    return _print_value(tdo_keys_d(argv_[2]));
  if(strcmp(argv_[1],"m") != 0)
    return EXIT_FAILURE;
  for(index = 0; index < sizeof(digest); index++)
    digest[index] = (md5_u8_t)index;
  return _print_value(tdo_keys_m(argv_[2],digest));
}
