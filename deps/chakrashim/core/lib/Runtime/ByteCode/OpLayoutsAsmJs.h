//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#ifndef TEMP_DISABLE_ASMJS
namespace Js
{
    ///----------------------------------------------------------------------------
    ///
    /// enum OpCodeAsmJs
    ///
    /// OpCodeAsmJs defines the set of p-code instructions available for byte-code in Asm.Js.
    ///
    ///----------------------------------------------------------------------------
    enum class OpCodeAsmJs : ushort {
#define DEF_OP(x, y, ...) x,
#include "OpCodeListAsmJs.h"
        MaxByteSizedOpcodes = 255,
#include "ExtendedOpCodeListAsmJs.h"
        ByteCodeLast,
#undef DEF_OP
        Count  // Number of operations
    };

    inline OpCodeAsmJs operator+(OpCodeAsmJs o1, OpCodeAsmJs o2) { return (OpCodeAsmJs)((uint)o1 + (uint)o2); }
    inline uint operator+(OpCodeAsmJs o1, uint i) { return ((uint)o1 + i); }
    inline uint operator+(uint i, OpCodeAsmJs &o2) { return (i + (uint)o2); }
    inline OpCodeAsmJs operator++(OpCodeAsmJs &o) { return o = (OpCodeAsmJs)(o + 1U); }
    inline OpCodeAsmJs operator++(OpCodeAsmJs &o, int) { OpCodeAsmJs prev_o = o;  o = (OpCodeAsmJs)(o + 1U); return prev_o; }
    inline OpCodeAsmJs operator-(OpCodeAsmJs o1, OpCodeAsmJs o2) { return (OpCodeAsmJs)((uint)o1 - (uint)o2); }
    inline uint operator-(OpCodeAsmJs o1, uint i) { return ((uint)o1 - i); }
    inline uint operator-(uint i, OpCodeAsmJs &o2) { return (i - (uint)o2); }
    inline OpCodeAsmJs operator--(OpCodeAsmJs &o) { return o = (OpCodeAsmJs)(o - 1U); }
    inline OpCodeAsmJs operator--(OpCodeAsmJs &o, int) { return o = (OpCodeAsmJs)(o - 1U); }
    inline uint operator<<(OpCodeAsmJs o1, uint i) { return ((uint)o1 << i); }
    inline OpCodeAsmJs& operator+=(OpCodeAsmJs &o, uint i) { return (o = (OpCodeAsmJs)(o + i)); }
    inline OpCodeAsmJs& operator-=(OpCodeAsmJs &o, uint i) { return (o = (OpCodeAsmJs)(o - i)); }
    inline bool operator==(OpCodeAsmJs &o, uint i) { return ((uint)(o) == i); }
    inline bool operator==(uint i, OpCodeAsmJs &o) { return (i == (uint)(o)); }
    inline bool operator!=(OpCodeAsmJs &o, uint i) { return ((uint)(o) != i); }
    inline bool operator!=(uint i, OpCodeAsmJs &o) { return (i != (uint)(o)); }
    inline bool operator<(OpCodeAsmJs &o, uint i) { return ((uint)(o) < i); }
    inline bool operator<(uint i, OpCodeAsmJs &o) { return (i < (uint)(o)); }
    inline bool operator<=(OpCodeAsmJs &o, uint i) { return ((uint)(o) <= i); }
    inline bool operator<=(uint i, OpCodeAsmJs &o) { return (i <= (uint)(o)); }
    inline bool operator<=(OpCodeAsmJs o1, OpCode o2) { return ((OpCode)o1 <= (o2)); }
    inline bool operator>(OpCodeAsmJs &o, uint i) { return ((uint)(o) > i); }
    inline bool operator>(uint i, OpCodeAsmJs &o) { return (i > (uint)(o)); }
    inline bool operator>=(OpCodeAsmJs &o, uint i) { return ((uint)(o) >= i); }
    inline bool operator>=(uint i, OpCodeAsmJs &o) { return (i >= (uint)(o)); }

    inline BOOL IsSimd128AsmJsOpcode(OpCodeAsmJs o)
    {
        return (o > Js::OpCodeAsmJs::Simd128_Start && o < Js::OpCodeAsmJs::Simd128_End) || (o > Js::OpCodeAsmJs::Simd128_Start_Extend && o < Js::OpCodeAsmJs::Simd128_End_Extend);
    }
    inline uint Simd128AsmJsOpcodeCount()
    {
        return (uint)(Js::OpCodeAsmJs::Simd128_End - Js::OpCodeAsmJs::Simd128_Start) + 1 + (uint)(Js::OpCodeAsmJs::Simd128_End_Extend - Js::OpCodeAsmJs::Simd128_Start_Extend) + 1;
    }

    ///----------------------------------------------------------------------------
    ///
    /// enum OpLayoutTypeAsmJs
    ///
    /// OpLayoutTypeAsmJs defines a set of layouts available for OpCodes.  These layouts
    /// correspond to "OpLayout" structs defined below, such as "OpLayoutReg1".
    ///
    ///----------------------------------------------------------------------------

    BEGIN_ENUM_UINT( OpLayoutTypeAsmJs )
        // This define only one enum for each layout type, but not for each layout variant
#define LAYOUT_TYPE(x) x,
#define LAYOUT_TYPE_WMS LAYOUT_TYPE
#include "LayoutTypesAsmJs.h"
        Count,
    END_ENUM_UINT()

#pragma pack(push, 1)
    /// Asm.js Layout

    template <typename SizePolicy>
    struct OpLayoutT_AsmTypedArr
    {
        // force encode 4 bytes because it can be a value
        uint32                               SlotIndex;
        typename SizePolicy::RegSlotType     Value;
        int8                                 ViewType;
    };

    template <typename SizePolicy>
    struct OpLayoutT_AsmCall
    {
        typename SizePolicy::ArgSlotType     ArgCount;
        typename SizePolicy::RegSlotSType    Return;
        typename SizePolicy::RegSlotType     Function;
        int8                                 ReturnType;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg1
    {
        typename SizePolicy::RegSlotType     R0;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg2
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg3
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg4
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::RegSlotType     R3;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg5
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::RegSlotType     R3;
        typename SizePolicy::RegSlotType     R4;
    };

    template <typename SizePolicy>
    struct OpLayoutT_AsmReg6
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::RegSlotType     R3;
        typename SizePolicy::RegSlotType     R4;
        typename SizePolicy::RegSlotType     R5;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg7
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::RegSlotType     R3;
        typename SizePolicy::RegSlotType     R4;
        typename SizePolicy::RegSlotType     R5;
        typename SizePolicy::RegSlotType     R6;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg9
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::RegSlotType     R3;
        typename SizePolicy::RegSlotType     R4;
        typename SizePolicy::RegSlotType     R5;
        typename SizePolicy::RegSlotType     R6;
        typename SizePolicy::RegSlotType     R7;
        typename SizePolicy::RegSlotType     R8;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg10
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::RegSlotType     R3;
        typename SizePolicy::RegSlotType     R4;
        typename SizePolicy::RegSlotType     R5;
        typename SizePolicy::RegSlotType     R6;
        typename SizePolicy::RegSlotType     R7;
        typename SizePolicy::RegSlotType     R8;
        typename SizePolicy::RegSlotType     R9;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg11
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::RegSlotType     R3;
        typename SizePolicy::RegSlotType     R4;
        typename SizePolicy::RegSlotType     R5;
        typename SizePolicy::RegSlotType     R6;
        typename SizePolicy::RegSlotType     R7;
        typename SizePolicy::RegSlotType     R8;
        typename SizePolicy::RegSlotType     R9;
        typename SizePolicy::RegSlotType     R10;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg17
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::RegSlotType     R3;
        typename SizePolicy::RegSlotType     R4;
        typename SizePolicy::RegSlotType     R5;
        typename SizePolicy::RegSlotType     R6;
        typename SizePolicy::RegSlotType     R7;
        typename SizePolicy::RegSlotType     R8;
        typename SizePolicy::RegSlotType     R9;
        typename SizePolicy::RegSlotType     R10;
        typename SizePolicy::RegSlotType     R11;
        typename SizePolicy::RegSlotType     R12;
        typename SizePolicy::RegSlotType     R13;
        typename SizePolicy::RegSlotType     R14;
        typename SizePolicy::RegSlotType     R15;
        typename SizePolicy::RegSlotType     R16;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg18
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::RegSlotType     R3;
        typename SizePolicy::RegSlotType     R4;
        typename SizePolicy::RegSlotType     R5;
        typename SizePolicy::RegSlotType     R6;
        typename SizePolicy::RegSlotType     R7;
        typename SizePolicy::RegSlotType     R8;
        typename SizePolicy::RegSlotType     R9;
        typename SizePolicy::RegSlotType     R10;
        typename SizePolicy::RegSlotType     R11;
        typename SizePolicy::RegSlotType     R12;
        typename SizePolicy::RegSlotType     R13;
        typename SizePolicy::RegSlotType     R14;
        typename SizePolicy::RegSlotType     R15;
        typename SizePolicy::RegSlotType     R16;
        typename SizePolicy::RegSlotType     R17;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg19
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
        typename SizePolicy::RegSlotType     R2;
        typename SizePolicy::RegSlotType     R3;
        typename SizePolicy::RegSlotType     R4;
        typename SizePolicy::RegSlotType     R5;
        typename SizePolicy::RegSlotType     R6;
        typename SizePolicy::RegSlotType     R7;
        typename SizePolicy::RegSlotType     R8;
        typename SizePolicy::RegSlotType     R9;
        typename SizePolicy::RegSlotType     R10;
        typename SizePolicy::RegSlotType     R11;
        typename SizePolicy::RegSlotType     R12;
        typename SizePolicy::RegSlotType     R13;
        typename SizePolicy::RegSlotType     R14;
        typename SizePolicy::RegSlotType     R15;
        typename SizePolicy::RegSlotType     R16;
        typename SizePolicy::RegSlotType     R17;
        typename SizePolicy::RegSlotType     R18;
    };
    template <typename SizePolicy>
    struct OpLayoutT_AsmReg2IntConst1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     R1;
                 int                         C2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Double1
    {
        typename SizePolicy::RegSlotType     I0;
        typename SizePolicy::RegSlotType     D1;
    };
    template <typename SizePolicy>
    struct OpLayoutT_Int1Float1
    {
        typename SizePolicy::RegSlotType     I0;
        typename SizePolicy::RegSlotType     F1;
    };
    template <typename SizePolicy>
    struct OpLayoutT_Double1Int1
    {
        typename SizePolicy::RegSlotType     D0;
        typename SizePolicy::RegSlotType     I1;
    };
    template <typename SizePolicy>
    struct OpLayoutT_Double1Float1
    {
        typename SizePolicy::RegSlotType     D0;
        typename SizePolicy::RegSlotType     F1;
    };
    template <typename SizePolicy>
    struct OpLayoutT_Double1Reg1
    {
        typename SizePolicy::RegSlotType     D0;
        typename SizePolicy::RegSlotType     R1;
    };
    template <typename SizePolicy>
    struct OpLayoutT_Float1Reg1
    {
        typename SizePolicy::RegSlotType     F0;
        typename SizePolicy::RegSlotType     R1;
    };
    template <typename SizePolicy>
    struct OpLayoutT_Int1Reg1
    {
        typename SizePolicy::RegSlotType     I0;
        typename SizePolicy::RegSlotType     R1;
    };
    template <typename SizePolicy>
    struct OpLayoutT_Reg1Double1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     D1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Float1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     F1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Int1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     I1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Double2
    {
        typename SizePolicy::RegSlotType     I0;
        typename SizePolicy::RegSlotType     D1;
        typename SizePolicy::RegSlotType     D2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Float2
    {
        typename SizePolicy::RegSlotType     I0;
        typename SizePolicy::RegSlotType     F1;
        typename SizePolicy::RegSlotType     F2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int2
    {
        typename SizePolicy::RegSlotType     I0;
        typename SizePolicy::RegSlotType     I1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Const1
    {
        typename SizePolicy::RegSlotType     I0;
                 int                         C1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int3
    {
        typename SizePolicy::RegSlotType     I0;
        typename SizePolicy::RegSlotType     I1;
        typename SizePolicy::RegSlotType     I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Double2
    {
        typename SizePolicy::RegSlotType     D0;
        typename SizePolicy::RegSlotType     D1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float2
    {
        typename SizePolicy::RegSlotType     F0;
        typename SizePolicy::RegSlotType     F1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float3
    {
        typename SizePolicy::RegSlotType     F0;
        typename SizePolicy::RegSlotType     F1;
        typename SizePolicy::RegSlotType     F2;
    };
    template <typename SizePolicy>
    struct OpLayoutT_Float1Double1
    {
        typename SizePolicy::RegSlotType     F0;
        typename SizePolicy::RegSlotType     D1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float1Int1
    {
        typename SizePolicy::RegSlotType     F0;
        typename SizePolicy::RegSlotType     I1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Double3

    {
        typename SizePolicy::RegSlotType     D0;
        typename SizePolicy::RegSlotType     D1;
        typename SizePolicy::RegSlotType     D2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_AsmUnsigned1
    {
        typename SizePolicy::UnsignedType C1;
    };

    struct OpLayoutAsmBr
    {
        int32  RelativeJumpOffset;
    };

    template <typename SizePolicy>
    struct OpLayoutT_BrInt1
    {
        int32  RelativeJumpOffset;
        typename SizePolicy::RegSlotType     I1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_BrInt2
    {
        int32  RelativeJumpOffset;
        typename SizePolicy::RegSlotType     I1;
        typename SizePolicy::RegSlotType     I2;
    };


    /* Float32x4 layouts */
    //--------------------
    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_2
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    F4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_3
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    F4_1;
        typename SizePolicy::RegSlotType    F4_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool32x4_1Float32x4_2
    {
        typename SizePolicy::RegSlotType    B4_0;
        typename SizePolicy::RegSlotType    F4_1;
        typename SizePolicy::RegSlotType    F4_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_4
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    F4_1;
        typename SizePolicy::RegSlotType    F4_2;
        typename SizePolicy::RegSlotType    F4_3;
    };

    // 4 floats -> float32x4.
    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_1Float4
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    F1;
        typename SizePolicy::RegSlotType    F2;
        typename SizePolicy::RegSlotType    F3;
        typename SizePolicy::RegSlotType    F4;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_2Int4
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    F4_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_3Int4
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    F4_1;
        typename SizePolicy::RegSlotType    F4_2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_1Float1
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    F1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_2Float1
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    F4_1;
        typename SizePolicy::RegSlotType    F2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_1Float64x2_1
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    D2_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_1Int32x4_1
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    I4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_1Uint32x4_1
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    U4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_1Int16x8_1
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    I8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_1Uint16x8_1
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    U8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_1Int8x16_1
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    I16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_1Uint8x16_1
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    U16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_1Bool32x4_1Float32x4_2
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    B4_1;
        typename SizePolicy::RegSlotType    F4_2;
        typename SizePolicy::RegSlotType    F4_3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Float32x4_1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     F4_1;
    };

    /* Int32x4 layouts */
    //--------------------
    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_2
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    I4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_3
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    I4_1;
        typename SizePolicy::RegSlotType    I4_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool32x4_1Int32x4_2
    {
        typename SizePolicy::RegSlotType    B4_0;
        typename SizePolicy::RegSlotType    I4_1;
        typename SizePolicy::RegSlotType    I4_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_1Bool32x4_1Int32x4_2
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    B4_1;
        typename SizePolicy::RegSlotType    I4_2;
        typename SizePolicy::RegSlotType    I4_3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_1Int1
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    I1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_1Int4
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    I1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_2Int4
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    I4_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_3Int4
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    I4_1;
        typename SizePolicy::RegSlotType    I4_2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_2Int1
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    I4_1;
        typename SizePolicy::RegSlotType    I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_2Int2
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    I4_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Int32x4_1Int1
    {
        typename SizePolicy::RegSlotType    I0;
        typename SizePolicy::RegSlotType    I4_1;
        typename SizePolicy::RegSlotType    I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float32x4_2Int1Float1
    {
        typename SizePolicy::RegSlotType    F4_0;
        typename SizePolicy::RegSlotType    F4_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    F3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float1Float32x4_1Int1
    {
        typename SizePolicy::RegSlotType    F0;
        typename SizePolicy::RegSlotType    F4_1;
        typename SizePolicy::RegSlotType    I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Int32x4_1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     I4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_1Float64x2_1
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    D2_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_1Float32x4_1
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    F4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_1Uint32x4_1
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    U4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_1Int16x8_1
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    I8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_1Uint16x8_1
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    U8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_1Int8x16_1
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    I16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int32x4_1Uint8x16_1
    {
        typename SizePolicy::RegSlotType    I4_0;
        typename SizePolicy::RegSlotType    U16_1;
    };

    /* Bool32x4 layouts */
    //--------------------
    template <typename SizePolicy>
    struct OpLayoutT_Bool32x4_1Int4
    {
        typename SizePolicy::RegSlotType    B4_0;
        typename SizePolicy::RegSlotType    I1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Bool32x4_1
    {
        typename SizePolicy::RegSlotType    I0;
        typename SizePolicy::RegSlotType    B4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool32x4_2Int2
    {
        typename SizePolicy::RegSlotType    B4_0;
        typename SizePolicy::RegSlotType    B4_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Bool32x4_1Int1
    {
        typename SizePolicy::RegSlotType    I0;
        typename SizePolicy::RegSlotType    B4_1;
        typename SizePolicy::RegSlotType    I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool32x4_2
    {
        typename SizePolicy::RegSlotType    B4_0;
        typename SizePolicy::RegSlotType    B4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool32x4_3
    {
        typename SizePolicy::RegSlotType    B4_0;
        typename SizePolicy::RegSlotType    B4_1;
        typename SizePolicy::RegSlotType    B4_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Bool32x4_1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     B4_1;
    };

    /* Bool16x8 layouts */
    //--------------------
    template <typename SizePolicy>
    struct OpLayoutT_Bool16x8_1Int8
    {
        typename SizePolicy::RegSlotType    B8_0;
        typename SizePolicy::RegSlotType    I1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
        typename SizePolicy::RegSlotType    I7;
        typename SizePolicy::RegSlotType    I8;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Bool16x8_1
    {
        typename SizePolicy::RegSlotType    I0;
        typename SizePolicy::RegSlotType    B8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool16x8_2
    {
        typename SizePolicy::RegSlotType    B8_0;
        typename SizePolicy::RegSlotType    B8_1;
    };
    
    template <typename SizePolicy>
    struct OpLayoutT_Bool16x8_2Int2
    {
        typename SizePolicy::RegSlotType    B8_0;
        typename SizePolicy::RegSlotType    B8_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Bool16x8_1Int1
    {
    typename SizePolicy::RegSlotType    I0;
    typename SizePolicy::RegSlotType    B8_1;
    typename SizePolicy::RegSlotType    I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool16x8_3
    {
        typename SizePolicy::RegSlotType    B8_0;
        typename SizePolicy::RegSlotType    B8_1;
        typename SizePolicy::RegSlotType    B8_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Bool16x8_1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     B8_1;
    };

    /* Bool8x16 layouts */
    //--------------------
    template <typename SizePolicy>
    struct OpLayoutT_Bool8x16_1Int16
    {
        typename SizePolicy::RegSlotType    B16_0;
        typename SizePolicy::RegSlotType    I1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
        typename SizePolicy::RegSlotType    I7;
        typename SizePolicy::RegSlotType    I8;
        typename SizePolicy::RegSlotType    I9;
        typename SizePolicy::RegSlotType    I10;
        typename SizePolicy::RegSlotType    I11;
        typename SizePolicy::RegSlotType    I12;
        typename SizePolicy::RegSlotType    I13;
        typename SizePolicy::RegSlotType    I14;
        typename SizePolicy::RegSlotType    I15;
        typename SizePolicy::RegSlotType    I16;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Bool8x16_1
    {
        typename SizePolicy::RegSlotType    I0;
        typename SizePolicy::RegSlotType    B16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool8x16_2
    {
        typename SizePolicy::RegSlotType    B16_0;
        typename SizePolicy::RegSlotType    B16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool8x16_2Int2
    {
        typename SizePolicy::RegSlotType    B16_0;
        typename SizePolicy::RegSlotType    B16_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Bool8x16_1Int1
    {
        typename SizePolicy::RegSlotType    I0;
        typename SizePolicy::RegSlotType    B16_1;
        typename SizePolicy::RegSlotType    I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool8x16_3
    {
        typename SizePolicy::RegSlotType    B16_0;
        typename SizePolicy::RegSlotType    B16_1;
        typename SizePolicy::RegSlotType    B16_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Bool8x16_1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     B16_1;
    };

    /* Int8x16 layouts */
    //--------------------
    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_2
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    I16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_3
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    I16_1;
        typename SizePolicy::RegSlotType    I16_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool8x16_1Int8x16_2
    {
        typename SizePolicy::RegSlotType    B16_0;
        typename SizePolicy::RegSlotType    I16_1;
        typename SizePolicy::RegSlotType    I16_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_1Bool8x16_1Int8x16_2
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    B16_1;
        typename SizePolicy::RegSlotType    I16_2;
        typename SizePolicy::RegSlotType    I16_3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_4
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    I16_1;
        typename SizePolicy::RegSlotType    I16_2;
        typename SizePolicy::RegSlotType    I16_3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_1Int1
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    I1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_1Int16
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    I1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
        typename SizePolicy::RegSlotType    I7;
        typename SizePolicy::RegSlotType    I8;
        typename SizePolicy::RegSlotType    I9;
        typename SizePolicy::RegSlotType    I10;
        typename SizePolicy::RegSlotType    I11;
        typename SizePolicy::RegSlotType    I12;
        typename SizePolicy::RegSlotType    I13;
        typename SizePolicy::RegSlotType    I14;
        typename SizePolicy::RegSlotType    I15;
        typename SizePolicy::RegSlotType    I16;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_2Int16
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    I16_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
        typename SizePolicy::RegSlotType    I7;
        typename SizePolicy::RegSlotType    I8;
        typename SizePolicy::RegSlotType    I9;
        typename SizePolicy::RegSlotType    I10;
        typename SizePolicy::RegSlotType    I11;
        typename SizePolicy::RegSlotType    I12;
        typename SizePolicy::RegSlotType    I13;
        typename SizePolicy::RegSlotType    I14;
        typename SizePolicy::RegSlotType    I15;
        typename SizePolicy::RegSlotType    I16;
        typename SizePolicy::RegSlotType    I17;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_3Int16
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    I16_1;
        typename SizePolicy::RegSlotType    I16_2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
        typename SizePolicy::RegSlotType    I7;
        typename SizePolicy::RegSlotType    I8;
        typename SizePolicy::RegSlotType    I9;
        typename SizePolicy::RegSlotType    I10;
        typename SizePolicy::RegSlotType    I11;
        typename SizePolicy::RegSlotType    I12;
        typename SizePolicy::RegSlotType    I13;
        typename SizePolicy::RegSlotType    I14;
        typename SizePolicy::RegSlotType    I15;
        typename SizePolicy::RegSlotType    I16;
        typename SizePolicy::RegSlotType    I17;
        typename SizePolicy::RegSlotType    I18;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_3Int4
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    I16_1;
        typename SizePolicy::RegSlotType    I16_2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_2Int1
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    I16_1;
        typename SizePolicy::RegSlotType    I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_2Int2
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    I16_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Int8x16_1Int1
    {
        typename SizePolicy::RegSlotType    I0;
        typename SizePolicy::RegSlotType    I16_1;
        typename SizePolicy::RegSlotType    I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Int8x16_1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     I16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_1Float32x4_1
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    F4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_1Int32x4_1
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    I4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_1Int16x8_1
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    I8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_1Uint32x4_1
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    U4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_1Uint16x8_1
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    U8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int8x16_1Uint8x16_1
    {
        typename SizePolicy::RegSlotType    I16_0;
        typename SizePolicy::RegSlotType    U16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Int8x16_1
    {
        typename SizePolicy::RegSlotType    I0;
        typename SizePolicy::RegSlotType    I16_1;
    };
    
    // Disabled for now
    /* Float64x2 layouts */
    //--------------------
    template <typename SizePolicy>
    struct OpLayoutT_Float64x2_2
    {
        typename SizePolicy::RegSlotType    D2_0;
        typename SizePolicy::RegSlotType    D2_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float64x2_3
    {
        typename SizePolicy::RegSlotType    D2_0;
        typename SizePolicy::RegSlotType    D2_1;
        typename SizePolicy::RegSlotType    D2_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float64x2_4
    {
        typename SizePolicy::RegSlotType    D2_0;
        typename SizePolicy::RegSlotType    D2_1;
        typename SizePolicy::RegSlotType    D2_2;
        typename SizePolicy::RegSlotType    D2_3;
    };


    template <typename SizePolicy>
    struct OpLayoutT_Float64x2_1Double2
    {
        typename SizePolicy::RegSlotType    D2_0;
        typename SizePolicy::RegSlotType    D1;
        typename SizePolicy::RegSlotType    D2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float64x2_1Double1
    {
        typename SizePolicy::RegSlotType    D2_0;
        typename SizePolicy::RegSlotType    D1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float64x2_2Double1
    {
        typename SizePolicy::RegSlotType    D2_0;
        typename SizePolicy::RegSlotType    D2_1;
        typename SizePolicy::RegSlotType    D2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float64x2_2Int2
    {
        typename SizePolicy::RegSlotType    D2_0;
        typename SizePolicy::RegSlotType    D2_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float64x2_3Int2
    {
        typename SizePolicy::RegSlotType    D2_0;
        typename SizePolicy::RegSlotType    D2_1;
        typename SizePolicy::RegSlotType    D2_2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float64x2_1Float32x4_1
    {
        typename SizePolicy::RegSlotType    D2_0;
        typename SizePolicy::RegSlotType    F4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float64x2_1Int32x4_1
    {
        typename SizePolicy::RegSlotType    D2_0;
        typename SizePolicy::RegSlotType    I4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Float64x2_1Int32x4_1Float64x2_2
    {
        typename SizePolicy::RegSlotType    D2_0;
        typename SizePolicy::RegSlotType    I4_1;
        typename SizePolicy::RegSlotType    D2_2;
        typename SizePolicy::RegSlotType    D2_3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Float64x2_1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     D2_1;
    };

    /* Int16x8 layouts */
    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_1Int8
    {
        typename SizePolicy::RegSlotType    I8_0;
        typename SizePolicy::RegSlotType    I1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
        typename SizePolicy::RegSlotType    I7;
        typename SizePolicy::RegSlotType    I8;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Int16x8_1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     I8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_2
    {
        typename SizePolicy::RegSlotType    I8_0;
        typename SizePolicy::RegSlotType    I8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Int16x8_1Int1
    {
        typename SizePolicy::RegSlotType I0;
        typename SizePolicy::RegSlotType I8_1;
        typename SizePolicy::RegSlotType I2;
    };

    template <typename SizePolicy> 
    struct OpLayoutT_Int16x8_2Int8
    {
        typename SizePolicy::RegSlotType I8_0;
        typename SizePolicy::RegSlotType I8_1;
        typename SizePolicy::RegSlotType I2;
        typename SizePolicy::RegSlotType I3;
        typename SizePolicy::RegSlotType I4;
        typename SizePolicy::RegSlotType I5;
        typename SizePolicy::RegSlotType I6;
        typename SizePolicy::RegSlotType I7;
        typename SizePolicy::RegSlotType I8;
        typename SizePolicy::RegSlotType I9;
    };

    template <typename SizePolicy> 
    struct OpLayoutT_Int16x8_3Int8
    {
        typename SizePolicy::RegSlotType I8_0;
        typename SizePolicy::RegSlotType I8_1;
        typename SizePolicy::RegSlotType I8_2;
        typename SizePolicy::RegSlotType I3;
        typename SizePolicy::RegSlotType I4;
        typename SizePolicy::RegSlotType I5;
        typename SizePolicy::RegSlotType I6;
        typename SizePolicy::RegSlotType I7;
        typename SizePolicy::RegSlotType I8;
        typename SizePolicy::RegSlotType I9;
        typename SizePolicy::RegSlotType I10;
    };
    
    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_1Int1
    {
        typename SizePolicy::RegSlotType I8_0;
        typename SizePolicy::RegSlotType I1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_2Int2
    {
        typename SizePolicy::RegSlotType I8_0;
        typename SizePolicy::RegSlotType I8_1;
        typename SizePolicy::RegSlotType I2;
        typename SizePolicy::RegSlotType I3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_3
    {
        typename SizePolicy::RegSlotType I8_0;
        typename SizePolicy::RegSlotType I8_1;
        typename SizePolicy::RegSlotType I8_2;
    };
    
    template <typename SizePolicy>
    struct OpLayoutT_Bool16x8_1Int16x8_2
    {
        typename SizePolicy::RegSlotType    B8_0;
        typename SizePolicy::RegSlotType    I8_1;
        typename SizePolicy::RegSlotType    I8_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_1Bool16x8_1Int16x8_2
    {
        typename SizePolicy::RegSlotType    I8_0;
        typename SizePolicy::RegSlotType    B8_1;
        typename SizePolicy::RegSlotType    I8_2;
        typename SizePolicy::RegSlotType    I8_3;
    };
  
    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_2Int1
    {
        typename SizePolicy::RegSlotType I8_0;
        typename SizePolicy::RegSlotType I8_1;
        typename SizePolicy::RegSlotType I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_1Float32x4_1
    {
        typename SizePolicy::RegSlotType I8_0;
        typename SizePolicy::RegSlotType F4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_1Int32x4_1
    {
        typename SizePolicy::RegSlotType I8_0;
        typename SizePolicy::RegSlotType I4_1;

    };

    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_1Int8x16_1
    {
        typename SizePolicy::RegSlotType I8_0;
        typename SizePolicy::RegSlotType I16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_1Uint32x4_1
    {
        typename SizePolicy::RegSlotType I8_0;
        typename SizePolicy::RegSlotType U4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_1Uint16x8_1
    {
        typename SizePolicy::RegSlotType I8_0;
        typename SizePolicy::RegSlotType U8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int16x8_1Uint8x16_1
    {
        typename SizePolicy::RegSlotType I8_0;
        typename SizePolicy::RegSlotType U16_1;
    };

    /* Uint32x4 layouts */
    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_1Int4
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    I1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Uint32x4_1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     U4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_2
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    U4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Uint32x4_1Int1
    {
        typename SizePolicy::RegSlotType    I0;
        typename SizePolicy::RegSlotType    U4_1;
        typename SizePolicy::RegSlotType    I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_2Int4
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    U4_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_3Int4
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    U4_1;
        typename SizePolicy::RegSlotType    U4_2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_1Int1
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    I1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_2Int2
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    U4_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_3
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    U4_1;
        typename SizePolicy::RegSlotType    U4_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool32x4_1Uint32x4_2
    {
        typename SizePolicy::RegSlotType    B4_0;
        typename SizePolicy::RegSlotType    U4_1;
        typename SizePolicy::RegSlotType    U4_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_1Bool32x4_1Uint32x4_2
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    B4_1;
        typename SizePolicy::RegSlotType    U4_2;
        typename SizePolicy::RegSlotType    U4_3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_2Int1
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    U4_1;
        typename SizePolicy::RegSlotType    I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_1Float32x4_1
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    F4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_1Int32x4_1
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    I4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_1Int16x8_1
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    I8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_1Int8x16_1
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    I16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_1Uint16x8_1
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    U8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint32x4_1Uint8x16_1
    {
        typename SizePolicy::RegSlotType    U4_0;
        typename SizePolicy::RegSlotType    U16_1;
    };

    /* Uint16x8 layouts */
    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_1Int8
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    I1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
        typename SizePolicy::RegSlotType    I7;
        typename SizePolicy::RegSlotType    I8;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Uint16x8_1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     U8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_2
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    U8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Uint16x8_1Int1
    {
        typename SizePolicy::RegSlotType    I0;
        typename SizePolicy::RegSlotType    U8_1;
        typename SizePolicy::RegSlotType    I2;
    };
        template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_2Int8
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    U8_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
        typename SizePolicy::RegSlotType    I7;
        typename SizePolicy::RegSlotType    I8;
        typename SizePolicy::RegSlotType    I9;
    };
        
    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_3Int8
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    U8_1;
        typename SizePolicy::RegSlotType    U8_2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
        typename SizePolicy::RegSlotType    I7;
        typename SizePolicy::RegSlotType    I8;
        typename SizePolicy::RegSlotType    I9;
        typename SizePolicy::RegSlotType    I10;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_1Int1
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    I1;
    };
    
    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_2Int2
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    U8_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
    };
    
    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_3
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    U8_1;
        typename SizePolicy::RegSlotType    U8_2;
    };
    
    template <typename SizePolicy>
    struct OpLayoutT_Bool16x8_1Uint16x8_2
    {
        typename SizePolicy::RegSlotType    B8_0;
        typename SizePolicy::RegSlotType    U8_1;
        typename SizePolicy::RegSlotType    U8_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_1Bool16x8_1Uint16x8_2
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    B8_1;
        typename SizePolicy::RegSlotType    U8_2;
        typename SizePolicy::RegSlotType    U8_3;
    };
        
    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_2Int1
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    U8_1;
        typename SizePolicy::RegSlotType    I2;
    };
        
    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_1Float32x4_1
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    F4_1;
    };
    
    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_1Int32x4_1
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    I4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_1Int16x8_1
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    I8_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_1Int8x16_1
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    I16_1;
    };
    
    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_1Uint32x4_1
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    U4_1;
    };
    
    template <typename SizePolicy>
    struct OpLayoutT_Uint16x8_1Uint8x16_1
    {
        typename SizePolicy::RegSlotType    U8_0;
        typename SizePolicy::RegSlotType    U16_1;
    };

    /* Uint8x16 layouts */
    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_1Int16
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    I1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
        typename SizePolicy::RegSlotType    I7;
        typename SizePolicy::RegSlotType    I8;
        typename SizePolicy::RegSlotType    I9;
        typename SizePolicy::RegSlotType    I10;
        typename SizePolicy::RegSlotType    I11;
        typename SizePolicy::RegSlotType    I12;
        typename SizePolicy::RegSlotType    I13;
        typename SizePolicy::RegSlotType    I14;
        typename SizePolicy::RegSlotType    I15;
        typename SizePolicy::RegSlotType    I16;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Reg1Uint8x16_1
    {
        typename SizePolicy::RegSlotType     R0;
        typename SizePolicy::RegSlotType     U16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_2
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    U16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Int1Uint8x16_1Int1
    {
        typename SizePolicy::RegSlotType    I0;
        typename SizePolicy::RegSlotType    U16_1;
        typename SizePolicy::RegSlotType    I2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_2Int16
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    U16_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
        typename SizePolicy::RegSlotType    I7;
        typename SizePolicy::RegSlotType    I8;
        typename SizePolicy::RegSlotType    I9;
        typename SizePolicy::RegSlotType    I10;
        typename SizePolicy::RegSlotType    I11;
        typename SizePolicy::RegSlotType    I12;
        typename SizePolicy::RegSlotType    I13;
        typename SizePolicy::RegSlotType    I14;
        typename SizePolicy::RegSlotType    I15;
        typename SizePolicy::RegSlotType    I16;
        typename SizePolicy::RegSlotType    I17;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_3Int16
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    U16_1;
        typename SizePolicy::RegSlotType    U16_2;
        typename SizePolicy::RegSlotType    I3;
        typename SizePolicy::RegSlotType    I4;
        typename SizePolicy::RegSlotType    I5;
        typename SizePolicy::RegSlotType    I6;
        typename SizePolicy::RegSlotType    I7;
        typename SizePolicy::RegSlotType    I8;
        typename SizePolicy::RegSlotType    I9;
        typename SizePolicy::RegSlotType    I10;
        typename SizePolicy::RegSlotType    I11;
        typename SizePolicy::RegSlotType    I12;
        typename SizePolicy::RegSlotType    I13;
        typename SizePolicy::RegSlotType    I14;
        typename SizePolicy::RegSlotType    I15;
        typename SizePolicy::RegSlotType    I16;
        typename SizePolicy::RegSlotType    I17;
        typename SizePolicy::RegSlotType    I18;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_1Int1
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    I1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_2Int2
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    U16_1;
        typename SizePolicy::RegSlotType    I2;
        typename SizePolicy::RegSlotType    I3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_3
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    U16_1;
        typename SizePolicy::RegSlotType    U16_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Bool8x16_1Uint8x16_2
    {
        typename SizePolicy::RegSlotType    B16_0;
        typename SizePolicy::RegSlotType    U16_1;
        typename SizePolicy::RegSlotType    U16_2;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_1Bool8x16_1Uint8x16_2
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    B16_1;
        typename SizePolicy::RegSlotType    U16_2;
        typename SizePolicy::RegSlotType    U16_3;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_2Int1
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    U16_1;
        typename SizePolicy::RegSlotType    I2;
        
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_1Float32x4_1
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    F4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_1Int32x4_1
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    I4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_1Int16x8_1
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    I8_1;
    };


    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_1Int8x16_1
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    I16_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_1Uint32x4_1
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    U4_1;
    };

    template <typename SizePolicy>
    struct OpLayoutT_Uint8x16_1Uint16x8_1
    {
        typename SizePolicy::RegSlotType    U16_0;
        typename SizePolicy::RegSlotType    U8_1;
    };

    /* bool32x4 layout */
    template <typename SizePolicy>
    struct OpLayoutT_Bool32x4_1Int1
    {
        typename SizePolicy::RegSlotType    B4_0;
        typename SizePolicy::RegSlotType    I1;
    };

    /* bool16x8 layout */
    template <typename SizePolicy>
    struct OpLayoutT_Bool16x8_1Int1
    {
        typename SizePolicy::RegSlotType    B8_0;
        typename SizePolicy::RegSlotType    I1;
    };

    /* bool8x16 layout */
    template <typename SizePolicy>
    struct OpLayoutT_Bool8x16_1Int1
    {
        typename SizePolicy::RegSlotType    B16_0;
        typename SizePolicy::RegSlotType    I1;
    };
    
    template <typename SizePolicy>
    struct OpLayoutT_AsmSimdTypedArr
    {
        // force encode 4 bytes because it can be a value
        uint32                               SlotIndex;
        typename SizePolicy::RegSlotType     Value;
        int8                                 ViewType;
        int8                                 DataWidth; // # of bytes to load/store
    };


    // Generate the multi size layout type defs
#define LAYOUT_TYPE_WMS(layout) \
    typedef OpLayoutT_##layout<LargeLayoutSizePolicy> OpLayout##layout##_Large; \
    typedef OpLayoutT_##layout<MediumLayoutSizePolicy> OpLayout##layout##_Medium; \
    typedef OpLayoutT_##layout<SmallLayoutSizePolicy> OpLayout##layout##_Small;
#include "LayoutTypesAsmJs.h"

#pragma pack(pop)

    // Generate structure to automatically map layout to its info
    template <OpLayoutTypeAsmJs::_E layout> struct OpLayoutInfoAsmJs;

#define LAYOUT_TYPE(layout) \
    CompileAssert(sizeof(OpLayout##layout) <= MaxLayoutSize); \
    template <> struct OpLayoutInfoAsmJs<OpLayoutTypeAsmJs::layout> \
        {  \
        static const bool HasMultiSizeLayout = false; \
        };

#define LAYOUT_TYPE_WMS(layout) \
    CompileAssert(sizeof(OpLayout##layout##_Large) <= MaxLayoutSize); \
    template <> struct OpLayoutInfoAsmJs<OpLayoutTypeAsmJs::layout> \
        {  \
        static const bool HasMultiSizeLayout = true; \
        };
#include "LayoutTypesAsmJs.h"

    // Generate structure to automatically map opcode to its info
    // Also generate assert to make sure the layout and opcode use the same macro with and without multiple size layout
    template <OpCodeAsmJs opcode> struct OpCodeInfoAsmJs;

#define DEFINE_OPCODEINFO(op, layout, extended) \
    CompileAssert(!OpLayoutInfoAsmJs<OpLayoutTypeAsmJs::layout>::HasMultiSizeLayout); \
    template <> struct OpCodeInfoAsmJs<OpCodeAsmJs::op> \
        { \
        static const OpLayoutTypeAsmJs::_E Layout = OpLayoutTypeAsmJs::layout; \
        static const bool HasMultiSizeLayout = false; \
        static const bool IsExtendedOpcode = extended; \
        typedef OpLayout##layout LayoutType; \
        };
#define DEFINE_OPCODEINFO_WMS(op, layout, extended) \
    CompileAssert(OpLayoutInfoAsmJs<OpLayoutTypeAsmJs::layout>::HasMultiSizeLayout); \
    template <> struct OpCodeInfoAsmJs<OpCodeAsmJs::op> \
        { \
        static const OpLayoutTypeAsmJs::_E Layout = OpLayoutTypeAsmJs::layout; \
        static const bool HasMultiSizeLayout = true; \
        static const bool IsExtendedOpcode = extended; \
        typedef OpLayout##layout##_Large LayoutType_Large; \
        typedef OpLayout##layout##_Medium LayoutType_Medium; \
        typedef OpLayout##layout##_Small LayoutType_Small; \
        };
#define MACRO(op, layout, ...) DEFINE_OPCODEINFO(op, layout, false)
#define MACRO_WMS(op, layout, ...) DEFINE_OPCODEINFO_WMS(op, layout, false)
#define MACRO_EXTEND(op, layout, ...) DEFINE_OPCODEINFO(op, layout, true)
#define MACRO_EXTEND_WMS(op, layout, ...) DEFINE_OPCODEINFO_WMS(op, layout, true)
#include "OpCodesAsmJs.h"
#undef DEFINE_OPCODEINFO
#undef DEFINE_OPCODEINFO_WMS
}
#endif
