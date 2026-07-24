#include "mass_math.h"

#include <limits.h>
#include <stddef.h>

typedef struct
{
    uint64_t high;
    uint64_t low;
} WideUnsigned;

static uint64_t Magnitude(int64_t value)
{
    uint64_t bits = (uint64_t)value;
    return (value < 0) ? (UINT64_C(0) - bits) : bits;
}

static WideUnsigned MultiplyWide(uint64_t first, uint64_t second)
{
    WideUnsigned product = {0U, 0U};
    WideUnsigned addend = {0U, first};
    uint8_t bit;

    for (bit = 0U; bit < 64U; ++bit)
    {
        if ((second & UINT64_C(1)) != 0U)
        {
            uint64_t old_low = product.low;
            product.low += addend.low;
            product.high += addend.high;
            if (product.low < old_low) ++product.high;
        }
        second >>= 1U;
        addend.high = (addend.high << 1U) | (addend.low >> 63U);
        addend.low <<= 1U;
    }
    return product;
}

static bool DivideWideRounded(WideUnsigned numerator, uint64_t denominator,
                              uint64_t *quotient)
{
    uint64_t result = 0U;
    uint64_t remainder = 0U;
    int16_t bit;
    bool overflow = false;

    if ((denominator == 0U) || (quotient == NULL)) return false;
    for (bit = 127; bit >= 0; --bit)
    {
        uint64_t input_bit = (bit >= 64) ?
            ((numerator.high >> (uint8_t)(bit - 64)) & UINT64_C(1)) :
            ((numerator.low >> (uint8_t)bit) & UINT64_C(1));
        bool carry = (remainder >> 63U) != 0U;
        bool subtract;

        remainder = (remainder << 1U) | input_bit;
        subtract = carry || (remainder >= denominator);
        if (subtract)
        {
            remainder -= denominator;
            if (bit >= 64) overflow = true;
            else result |= UINT64_C(1) << (uint8_t)bit;
        }
    }
    if (overflow) return false;
    if ((remainder != 0U) && (remainder >= denominator - remainder))
    {
        if (result == UINT64_MAX) return false;
        ++result;
    }
    *quotient = result;
    return true;
}

bool MassMath_Add(MassValueUg first, MassValueUg second, MassValueUg *result)
{
    if ((result == NULL) || ((second > 0) && (first > INT64_MAX - second)) ||
        ((second < 0) && (first < INT64_MIN - second))) return false;
    *result = first + second;
    return true;
}

bool MassMath_Subtract(MassValueUg first, MassValueUg second,
                       MassValueUg *result)
{
    if ((result == NULL) || ((second < 0) && (first > INT64_MAX + second)) ||
        ((second > 0) && (first < INT64_MIN + second))) return false;
    *result = first - second;
    return true;
}

bool MassMath_Abs(MassValueUg value, uint64_t *magnitude)
{
    if (magnitude == NULL) return false;
    *magnitude = Magnitude(value);
    return true;
}

bool MassMath_MulDivRound(MassValueUg value, int64_t multiplier,
                          int64_t divisor, MassValueUg *result)
{
    WideUnsigned product;
    uint64_t quotient;
    bool negative;

    if ((result == NULL) || (divisor == 0)) return false;
    negative = ((value < 0) != (multiplier < 0)) != (divisor < 0);
    product = MultiplyWide(Magnitude(value), Magnitude(multiplier));
    if (!DivideWideRounded(product, Magnitude(divisor), &quotient)) return false;
    if (negative)
    {
        if (quotient > (UINT64_C(1) << 63U)) return false;
        *result = (quotient == (UINT64_C(1) << 63U)) ?
            INT64_MIN : -(int64_t)quotient;
    }
    else
    {
        if (quotient > (uint64_t)INT64_MAX) return false;
        *result = (int64_t)quotient;
    }
    return true;
}

bool MassMath_Quantize(MassValueUg value, uint64_t division,
                       MassValueUg *result)
{
    MassValueUg units;
    if ((division == 0U) || (division > (uint64_t)INT64_MAX) ||
        !MassMath_MulDivRound(value, 1, (int64_t)division, &units))
        return false;
    return MassMath_MulDivRound(units, (int64_t)division, 1, result);
}
