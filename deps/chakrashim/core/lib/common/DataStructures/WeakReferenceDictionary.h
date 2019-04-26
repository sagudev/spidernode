//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    interface IWeakReferenceDictionary
    {
        virtual void Cleanup() = 0;
    };

    template <
        class TKey,
        class TValue,
        class SizePolicy = PowerOf2SizePolicy,
        template <typename ValueOrKey> class Comparer = DefaultComparer
    >
    class WeakReferenceDictionary: public BaseDictionary<TKey, RecyclerWeakReference<TValue>*, RecyclerNonLeafAllocator, SizePolicy, Comparer, WeakRefValueDictionaryEntry>,
                                   public IWeakReferenceDictionary
    {
        typedef BaseDictionary<TKey, RecyclerWeakReference<TValue>*, RecyclerNonLeafAllocator, SizePolicy, Comparer, WeakRefValueDictionaryEntry> Base;
        typedef typename Base::EntryType EntryType;

    public:
        WeakReferenceDictionary(Recycler* recycler, int capacity = 0):
          Base(recycler, capacity)
        {
            Assert(reinterpret_cast<void*>(this) == reinterpret_cast<void*>((IWeakReferenceDictionary*) this));
        }

        virtual void Cleanup() override
        {
            this->MapAndRemoveIf([](typename Base::EntryType &entry)
            {
                return (Base::EntryType::NeedsCleanup(entry));
            });
        }

    private:
        using Base::Clone;
        using Base::Copy;

        PREVENT_COPY(WeakReferenceDictionary);
    };
};
