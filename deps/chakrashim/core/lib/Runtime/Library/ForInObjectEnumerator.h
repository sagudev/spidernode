//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class ForInObjectEnumerator : public WeakReferenceCache<ForInObjectEnumerator>
    {
    private:
        DynamicObjectSnapshotEnumeratorWPCache<true, false> embeddedEnumerator;
        JavascriptEnumerator * currentEnumerator;
        RecyclableObject *object;
        RecyclableObject *baseObject;
        BVSparse<Recycler>* propertyIds;
        RecyclableObject *firstPrototype;
        Var currentIndex;
        Type* baseObjectType;
        SListBase<Js::PropertyRecord const *> newPropertyStrings;
        ScriptContext * scriptContext;
        bool enumSymbols;

        BOOL TestAndSetEnumerated(PropertyId propertyId);
        BOOL GetCurrentEnumerator();

        // Only used by the vtable ctor for ForInObjectEnumeratorWrapper
        friend class ForInObjectEnumeratorWrapper;
        ForInObjectEnumerator() { }

    public:
        ForInObjectEnumerator(RecyclableObject* object, ScriptContext * requestContext, bool enumSymbols = false);
        ~ForInObjectEnumerator() { Clear(); }

        ScriptContext * GetScriptContext() const { return scriptContext; }
        BOOL CanBeReused();
        void Initialize(RecyclableObject* currentObject, ScriptContext * scriptContext);
        void Clear();
        Var GetCurrentIndex();        
        BOOL MoveNext();
        void Reset();
        Var MoveAndGetNext(PropertyId& propertyId);

        static uint32 GetOffsetOfCurrentEnumerator() { return offsetof(ForInObjectEnumerator, currentEnumerator); }
        static uint32 GetOffsetOfFirstPrototype() { return offsetof(ForInObjectEnumerator, firstPrototype); }

        static RecyclableObject* GetFirstPrototypeWithEnumerableProperties(RecyclableObject* object);
    };

    // Use when we want to use the ForInObject as if they are normal javascript enumerator
    class ForInObjectEnumeratorWrapper : public JavascriptEnumerator
    {
    public:
        ForInObjectEnumeratorWrapper(RecyclableObject* object, ScriptContext * requestContext)
            : JavascriptEnumerator(requestContext), forInObjectEnumerator(object, requestContext)
        {
        }

        virtual void Reset() override { forInObjectEnumerator.Reset(); }
        virtual Var MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes = nullptr)
        {
            return forInObjectEnumerator.MoveAndGetNext(propertyId);
        }
    protected:
        DEFINE_VTABLE_CTOR(ForInObjectEnumeratorWrapper, JavascriptEnumerator);
        virtual void MarshalToScriptContext(Js::ScriptContext * scriptContext) override
        {
            // Currently this wrapper is only used by ExtensionEnumeratorObject and doesn't support marshaling cross script context.
            AssertMsg(false, "ForInObjectEnumeratorWrapper should never get marshaled");
        }
    private:
        ForInObjectEnumerator forInObjectEnumerator;
    };
}
