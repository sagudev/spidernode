//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class JavascriptArrayEnumeratorBase _ABSTRACT : public JavascriptEnumerator
    {
    protected:
        JavascriptArray* arrayObject;
        uint32 index;
        bool doneArray;
        bool doneObject;
        BOOL enumNonEnumerable;
        bool enumSymbols;
        JavascriptEnumerator* objectEnumerator;

        DEFINE_VTABLE_CTOR_ABSTRACT(JavascriptArrayEnumeratorBase, JavascriptEnumerator)

        JavascriptArrayEnumeratorBase(JavascriptArray* arrayObject, ScriptContext* scriptContext, BOOL enumNonEnumerable, bool enumSymbols = false);
        virtual uint32 GetCurrentItemIndex()  override { return index; }
    };
}
