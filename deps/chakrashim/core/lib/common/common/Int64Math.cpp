//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCommonPch.h"
#include "Common/Int64Math.h"

#if defined(_M_X64)
#if defined(_MSC_VER) && !defined(__clang__)
    #pragma intrinsic(_mul128)
#else
    static int64 _mul128(const int64 left, const int64 right, int64 *high) noexcept
    {
        int64 low;
        __asm__
        (
            "imulq %[right]\n"
            // (I)MUL (Q/64) R[D/A]X <- RAX * r/m64
            : "=d"(*high), "=a"(low)
            : [right]"rm"(right), "1"(left)
        );

        return low;
    }
#endif
#endif

bool
Int64Math::Add(int64 left, int64 right, int64 *pResult)
{
    // Overflow occurs when the result has a different sign from both the left and right operands
    *pResult = left + right;
    return ((left ^ *pResult) & (right ^ *pResult)) < 0;
}

// Returns true if we overflowed, false if we didn't
bool
Int64Math::Mul(int64 left, int64 right, int64 *pResult)
{
#if defined(_M_X64)
    int64 high;
    *pResult = _mul128(left, right, &high);
    return ((*pResult > 0) && high != 0) || ((*pResult < 0) && (high != -1));
#else
    *pResult = left * right;
    return (left != 0 && right != 0 && (*pResult / left) != right);
#endif
}

bool
Int64Math::Shl(int64 left, int64 right, int64 *pResult)
{
    *pResult = left << (right & 63);
    return (left != (int64)((uint64)*pResult >> right));
}

bool
Int64Math::Sub(int64 left, int64 right, int64 *pResult)
{
    // Overflow occurs when the result has a different sign from the left operand, and the result has the same sign as the
    // right operand
    *pResult = left - right;
    return ((left ^ *pResult) & ~(right ^ *pResult)) < 0;
}

bool
Int64Math::Div(int64 left, int64 right, int64 *pResult)
{
    AssertMsg(right != 0, "Divide by zero...");

    if (right == -1 && left == INT64_MIN)
    {
        //Special check for INT64_MIN/-1
        return true;
    }

    *pResult = left / right;
    return false;
}

bool
Int64Math::Mod(int64 left, int64 right, int64 *pResult)
{
    AssertMsg(right != 0, "Mod by zero...");
    if (right == -1 && left == INT64_MIN)
    {
        //Special check for INT64_MIN/-1
        return true;
    }
    *pResult = left % right;
    return false;
}

bool
Int64Math::Shr(int64 left, int64 right, int64 *pResult)
{
    *pResult = left >> (right & 63);
    return false;
}

bool
Int64Math::ShrU(int64 left, int64 right, int64 *pResult)
{
    uint64 uResult = ((uint64)left) >> (right & 63);
    *pResult = uResult;
    return false;
}

bool
Int64Math::And(int64 left, int64 right, int64 *pResult)
{
    *pResult = left & right;
    return false;
}

bool
Int64Math::Or(int64 left, int64 right, int64 *pResult)
{
    *pResult = left | right;
    return false;
}

bool
Int64Math::Xor(int64 left, int64 right, int64 *pResult)
{
    *pResult = left ^ right;
    return false;
}

bool
Int64Math::Neg(int64 val, int64 *pResult)
{
    *pResult = -val;
    return *pResult == INT64_MIN;
}

bool
Int64Math::Not(int64 val, int64 *pResult)
{
    *pResult = ~val;
    return false;
}

bool
Int64Math::Inc(int64 val, int64 *pResult)
{
    *pResult = val + 1;
    // Overflow if result ends up less than input
    return *pResult <= val;
}

bool
Int64Math::Dec(int64 val, int64 *pResult)
{
    *pResult = val - 1;
    // Overflow if result ends up greater than input
    return *pResult >= val;
}

uint64
Int64Math::Log2(int64 val)
{
    uint64 uval = (uint64)val;
    uint64 ret;
    for (ret = 0; uval >>= 1; ret++);
    return ret;
}

int64
Int64Math::NearestInRangeTo(const int64 value, const int64 minimum, const int64 maximum) // inclusive
{
    Assert(minimum <= maximum);
    return minimum >= value ? minimum : maximum <= value ? maximum : value;
}
