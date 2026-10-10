// Exercise fixed-width shifts and bit access with independent numeric vectors.

#include "bigdigits.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#define TEST_DIGITS 3
#define CROSS_SHIFT_BITS 4
#define TOP_BIT_INDEX (BITS_PER_DIGIT - 1)
#define OUT_OF_RANGE_BIT (TEST_DIGITS * BITS_PER_DIGIT)

typedef struct ShiftCase
{
  size_t shift;
  DIGIT_T left[TEST_DIGITS];
  DIGIT_T right[TEST_DIGITS];
  DIGIT_T left_carry;
  DIGIT_T right_carry;
} ShiftCase;

static const DIGIT_T g_INPUT[TEST_DIGITS] =
  {
    0x89abcdefU,
    0x01234567U,
    0xfedcba98U
  };

// Limbs are least-significant first. Right-shift carry is left-justified.
static const ShiftCase g_SHIFT_CASES[] =
  {
    {
      0,
      {
        0x89abcdefU,
        0x01234567U,
        0xfedcba98U
      },
      {
        0x89abcdefU,
        0x01234567U,
        0xfedcba98U
      },
      0,
      0
    },
    {
      BITS_PER_DIGIT,
      {
        0x00000000U,
        0x89abcdefU,
        0x01234567U
      },
      {
        0x01234567U,
        0xfedcba98U,
        0x00000000U
      },
      0xfedcba98U,
      0x89abcdefU
    },
    {
      (BITS_PER_DIGIT + CROSS_SHIFT_BITS),
      {
        0x00000000U,
        0x9abcdef0U,
        0x12345678U
      },
      {
        0x80123456U,
        0x0fedcba9U,
        0x00000000U
      },
      0xedcba980U,
      0x789abcdeU
    },
    {
      (2 * BITS_PER_DIGIT),
      {
        0x00000000U,
        0x00000000U,
        0x89abcdefU
      },
      {
        0xfedcba98U,
        0x00000000U,
        0x00000000U
      },
      0x01234567U,
      0x01234567U
    },
    {
      ((2 * BITS_PER_DIGIT) + CROSS_SHIFT_BITS),
      {
        0x00000000U,
        0x00000000U,
        0x9abcdef0U
      },
      {
        0x0fedcba9U,
        0x00000000U,
        0x00000000U
      },
      0x12345678U,
      0x80123456U
    },
    {
      (TEST_DIGITS * BITS_PER_DIGIT),
      {
        0x00000000U,
        0x00000000U,
        0x00000000U
      },
      {
        0x00000000U,
        0x00000000U,
        0x00000000U
      },
      0x89abcdefU,
      0xfedcba98U
    },
    {
      CROSS_SHIFT_BITS,
      {
        0x9abcdef0U,
        0x12345678U,
        0xedcba980U
      },
      {
        0x789abcdeU,
        0x80123456U,
        0x0fedcba9U
      },
      0x0000000fU,
      0xf0000000U
    }
  };

static const DIGIT_T g_BIT_INPUT[TEST_DIGITS] =
  {
    0x01234567U,
    0x76543210U,
    0x13579bdfU
  };

static
void
_check_shift_case(const ShiftCase *case_,
                  int              in_place_)
{
  DIGIT_T left[TEST_DIGITS];
  DIGIT_T right[TEST_DIGITS];
  DIGIT_T left_carry;
  DIGIT_T right_carry;
  const DIGIT_T *left_input = g_INPUT;
  const DIGIT_T *right_input = g_INPUT;

  memcpy(left, g_INPUT, sizeof(left));
  memcpy(right, g_INPUT, sizeof(right));
  if(in_place_)
    {
      left_input = left;
      right_input = right;
    }

  left_carry = mpShiftLeft(left, left_input, case_->shift, TEST_DIGITS);
  right_carry = mpShiftRight(right, right_input, case_->shift, TEST_DIGITS);
  assert(left_carry == case_->left_carry);
  assert(right_carry == case_->right_carry);
  assert(memcmp(left, case_->left, sizeof(left)) == 0);
  assert(memcmp(right, case_->right, sizeof(right)) == 0);
}


static
void
_check_top_bit(size_t digit_index_)
{
  DIGIT_T actual[TEST_DIGITS];
  DIGIT_T expected[TEST_DIGITS];
  size_t bit = ((digit_index_ * BITS_PER_DIGIT) + TOP_BIT_INDEX);
  int status;

  assert(digit_index_ < TEST_DIGITS);
  memcpy(actual, g_BIT_INPUT, sizeof(actual));
  memcpy(expected, g_BIT_INPUT, sizeof(expected));
  expected[digit_index_] = (expected[digit_index_] | (DIGIT_T)HIBITMASK);
  assert(mpGetBit(actual, TEST_DIGITS, bit) == 0);
  status = mpSetBit(actual, TEST_DIGITS, bit, 1);
  assert(status == 0);
  assert(mpGetBit(actual, TEST_DIGITS, bit) == 1);
  assert(memcmp(actual, expected, sizeof(actual)) == 0);
  status = mpSetBit(actual, TEST_DIGITS, bit, 0);
  assert(status == 0);
  assert(mpGetBit(actual, TEST_DIGITS, bit) == 0);
  assert(memcmp(actual, g_BIT_INPUT, sizeof(actual)) == 0);
}


static
void
_check_out_of_range(size_t bit_)
{
  DIGIT_T actual[TEST_DIGITS];
  int status;

  memcpy(actual, g_BIT_INPUT, sizeof(actual));
  status = mpSetBit(actual, TEST_DIGITS, bit_, 1);
  assert(status == -1);
  assert(memcmp(actual, g_BIT_INPUT, sizeof(actual)) == 0);
  status = mpSetBit(actual, TEST_DIGITS, bit_, 0);
  assert(status == -1);
  assert(memcmp(actual, g_BIT_INPUT, sizeof(actual)) == 0);
  assert(mpGetBit(actual, TEST_DIGITS, bit_) == -1);
}


int
main(void)
{
  size_t i;

  for(i = 0; i < (sizeof(g_SHIFT_CASES) / sizeof(g_SHIFT_CASES[0])); i++)
    {
      _check_shift_case(&g_SHIFT_CASES[i], 0);
      // Aliasing must preserve both the shifted limbs and the original-source carry.
      _check_shift_case(&g_SHIFT_CASES[i], 1);
    }

  for(i = 0; i < TEST_DIGITS; i++)
    _check_top_bit(i);
  _check_out_of_range(OUT_OF_RANGE_BIT);
  _check_out_of_range((OUT_OF_RANGE_BIT + TOP_BIT_INDEX));
  return 0;
}
