//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "PageAllocatorDefines.h"

#ifdef PROFILE_MEM
struct PageMemoryData;
#endif

class CodeGenNumberThreadAllocator;

namespace Memory
{
typedef void* FunctionTableHandle;

#if DBG_DUMP && !defined(JD_PRIVATE)

#define GUARD_PAGE_TRACE(...) \
    if (Js::Configuration::Global.flags.PrintGuardPageBounds) \
{ \
    Output::Print(__VA_ARGS__); \
}

#define PAGE_ALLOC_TRACE(format, ...) PAGE_ALLOC_TRACE_EX(false, false, format, __VA_ARGS__)
#define PAGE_ALLOC_VERBOSE_TRACE(format, ...) PAGE_ALLOC_TRACE_EX(true, false, format, __VA_ARGS__)
#define PAGE_ALLOC_VERBOSE_TRACE_0(format) PAGE_ALLOC_TRACE_EX(true, false, format, "")

#define PAGE_ALLOC_TRACE_AND_STATS(format, ...) PAGE_ALLOC_TRACE_EX(false, true, format, __VA_ARGS__)
#define PAGE_ALLOC_TRACE_AND_STATS_0(format) PAGE_ALLOC_TRACE_EX(false, true, format, "")
#define PAGE_ALLOC_VERBOSE_TRACE_AND_STATS(format, ...) PAGE_ALLOC_TRACE_EX(true, true, format, __VA_ARGS__)
#define PAGE_ALLOC_VERBOSE_TRACE_AND_STATS_0(format) PAGE_ALLOC_TRACE_EX(true, true, format, "")

#define PAGE_ALLOC_TRACE_EX(verbose, stats, format, ...)                \
    if (this->pageAllocatorFlagTable.Trace.IsEnabled(Js::PageAllocatorPhase)) \
    { \
        if (!verbose || this->pageAllocatorFlagTable.Verbose) \
        {   \
            Output::Print(_u("%p : %p> PageAllocator(%p): "), GetCurrentThreadContextId(), ::GetCurrentThreadId(), this); \
            if (debugName != nullptr) \
            { \
                Output::Print(_u("[%s] "), this->debugName); \
            } \
            Output::Print(format, __VA_ARGS__);         \
            Output::Print(_u("\n")); \
            if (stats && this->pageAllocatorFlagTable.Stats.IsEnabled(Js::PageAllocatorPhase)) \
            { \
                this->DumpStats();         \
            }  \
            Output::Flush(); \
        } \
    }
#else
#define PAGE_ALLOC_TRACE(format, ...)
#define PAGE_ALLOC_VERBOSE_TRACE(format, ...)
#define PAGE_ALLOC_VERBOSE_TRACE_0(format)

#define PAGE_ALLOC_TRACE_AND_STATS(format, ...)
#define PAGE_ALLOC_VERBOSE_TRACE_AND_STATS(format, ...)
#define PAGE_ALLOC_TRACE_AND_STATS_0(format)
#define PAGE_ALLOC_VERBOSE_TRACE_AND_STATS_0(format)

#endif

#ifdef _M_X64
#define XDATA_RESERVE_PAGE_COUNT   (2)       // Number of pages per page segment (32 pages) reserved for xdata.
#else
#define XDATA_RESERVE_PAGE_COUNT   (0)       // ARM uses the heap, so it's not required.
#endif

//
// Allocation done by the secondary allocator
//
struct SecondaryAllocation
{
    BYTE* address;   // address of the allocation by the secondary allocator

    SecondaryAllocation() : address(nullptr)
    {
    }
};


//
// For every page segment a page allocator can create a secondary allocator which can have a specified
// number of pages reserved for secondary allocations. These pages are always reserved at the end of the
// segment. The PageAllocator itself cannot allocate from the region demarcated for the secondary allocator.
// Currently this is used for xdata allocations.
//
class SecondaryAllocator
{
public:
    virtual bool Alloc(ULONG_PTR functionStart, DWORD functionSize, DECLSPEC_GUARD_OVERFLOW ushort pdataCount, DECLSPEC_GUARD_OVERFLOW ushort xdataSize, SecondaryAllocation* xdata) = 0;
    virtual void Release(const SecondaryAllocation& allocation) = 0;
    virtual void Delete() = 0;
    virtual bool CanAllocate() = 0;
    virtual ~SecondaryAllocator() {};
};

/*
 * A segment is a collection of pages. A page corresponds to the concept of an
 * OS memory page. Segments allocate memory using the OS VirtualAlloc call.
 * It'll allocate the pageCount * page size number of bytes, the latter being
 * a system-wide constant.
 */
template<typename TVirtualAlloc>
class SegmentBase
{
public:
    SegmentBase(PageAllocatorBase<TVirtualAlloc> * allocator, DECLSPEC_GUARD_OVERFLOW size_t pageCount);
    virtual ~SegmentBase();

    size_t GetPageCount() const { return segmentPageCount; }

    // Some pages are reserved upfront for secondary allocations
    // which are done by a secondary allocator as opposed to the PageAllocator
    size_t GetAvailablePageCount() const { return segmentPageCount - secondaryAllocPageCount; }

    char* GetSecondaryAllocStartAddress() const { return (this->address + GetAvailablePageCount() * AutoSystemInfo::PageSize); }
    uint GetSecondaryAllocSize() const { return this->secondaryAllocPageCount * AutoSystemInfo::PageSize; }

    char* GetAddress() const { return address; }
    char* GetEndAddress() const { return GetSecondaryAllocStartAddress(); }

    bool CanAllocSecondary() { Assert(secondaryAllocator); return secondaryAllocator->CanAllocate(); }

    PageAllocatorBase<TVirtualAlloc>* GetAllocator() const { return allocator; }
    bool IsInPreReservedHeapPageAllocator() const;

    bool Initialize(DWORD allocFlags, bool excludeGuardPages);

#if DBG
    virtual bool IsPageSegment() const
    {
        return false;
    }
#endif

    bool IsInSegment(void* address) const
    {
        void* start = static_cast<void*>(GetAddress());
        void* end = static_cast<void*>(GetEndAddress());
        return (address >= start && address < end);
    }

    bool IsInCustomHeapAllocator() const
    {
        return this->allocator->type == PageAllocatorType::PageAllocatorType_CustomHeap;
    }

    SecondaryAllocator* GetSecondaryAllocator() { return secondaryAllocator; }

#if defined(_M_X64_OR_ARM64) && defined(RECYCLER_WRITE_BARRIER)
    bool IsWriteBarrierAllowed()
    {
        return isWriteBarrierAllowed;
    }

#endif

protected:
#if _M_IX86_OR_ARM32
    static const uint VirtualAllocThreshold =  524288; // 512kb As per spec
#else // _M_X64_OR_ARM64
    static const uint VirtualAllocThreshold = 1048576; // 1MB As per spec : when we cross this threshold of bytes, we should add guard pages
#endif
    static const uint maxGuardPages = 15;
    static const uint minGuardPages =  1;

    SecondaryAllocator* secondaryAllocator;
    char * address;
    PageAllocatorBase<TVirtualAlloc> * allocator;
    size_t segmentPageCount;
    uint trailingGuardPageCount;
    uint leadingGuardPageCount;
    uint   secondaryAllocPageCount;
#if defined(_M_X64_OR_ARM64) && defined(RECYCLER_WRITE_BARRIER)
    bool   isWriteBarrierAllowed;
#endif
};

/*
 * Page Segments allows a client to deal with virtual memory on a page level
 * unlike Segment, which gives you access on a segment basis. Pages managed
 * by the page segment are initially in a "free list", and have the no access
 * bit set on them. When a client wants pages, we get them from the free list
 * and commit them into memory. When the client no longer needs those pages,
 * we simply decommit them- this means that the pages are still reserved for
 * the process but are not a part of its working set and has no physical
 * storage associated with it.
 */

template<typename TVirtualAlloc>
class PageSegmentBase : public SegmentBase<TVirtualAlloc>
{
    typedef SegmentBase<TVirtualAlloc> Base;
public:
    PageSegmentBase(PageAllocatorBase<TVirtualAlloc> * allocator, bool committed, bool allocated);
    // Maximum possible size of a PageSegment; may be smaller.
    static const uint MaxDataPageCount = 256;     // 1 MB
    static const uint MaxGuardPageCount = 16;
    static const uint MaxPageCount = MaxDataPageCount + MaxGuardPageCount;  // 272 Pages

    typedef BVStatic<MaxPageCount> PageBitVector;

    uint GetAvailablePageCount() const
    {
        size_t availablePageCount = Base::GetAvailablePageCount();
        Assert(availablePageCount < MAXUINT32);
        return static_cast<uint>(availablePageCount);
    }

    void Prime();
#ifdef PAGEALLOCATOR_PROTECT_FREEPAGE
    bool Initialize(DWORD allocFlags, bool excludeGuardPages);
#endif
    uint GetFreePageCount() const { return freePageCount; }
    uint GetDecommitPageCount() const { return decommitPageCount; }

    static bool IsAllocationPageAligned(__in char* address, size_t pageCount);

    template <typename T, bool notPageAligned>
    char * AllocDecommitPages(DECLSPEC_GUARD_OVERFLOW uint pageCount, T freePages, T decommitPages);

    template <bool notPageAligned>
    char * AllocPages(DECLSPEC_GUARD_OVERFLOW uint pageCount);

    void ReleasePages(__in void * address, uint pageCount);
    template <bool onlyUpdateState>
    void DecommitPages(__in void * address, uint pageCount);

    uint GetCountOfFreePages() const;
    uint GetNextBitInFreePagesBitVector(uint index) const;
    BOOLEAN TestRangeInFreePagesBitVector(uint index, uint pageCount) const;
    BOOLEAN TestInFreePagesBitVector(uint index) const;
    void ClearAllInFreePagesBitVector();
    void ClearRangeInFreePagesBitVector(uint index, uint pageCount);
    void SetRangeInFreePagesBitVector(uint index, uint pageCount);
    void ClearBitInFreePagesBitVector(uint index);

    uint GetCountOfDecommitPages() const;
    BOOLEAN TestInDecommitPagesBitVector(uint index) const;
    BOOLEAN TestRangeInDecommitPagesBitVector(uint index, uint pageCount) const;
    void SetRangeInDecommitPagesBitVector(uint index, uint pageCount);
    void SetBitInDecommitPagesBitVector(uint index);
    void ClearRangeInDecommitPagesBitVector(uint index, uint pageCount);

    template <bool notPageAligned>
    char * DoAllocDecommitPages(DECLSPEC_GUARD_OVERFLOW uint pageCount);
    uint GetMaxPageCount();

    size_t DecommitFreePages(size_t pageToDecommit);

    bool IsEmpty() const
    {
        return this->freePageCount == this->GetAvailablePageCount();
    }

    //
    // If a segment has decommitted pages - then it's not considered full as allocations can take place from it
    // However, if secondary allocations cannot be made from it - it's considered full nonetheless
    //
    bool IsFull() const
    {
        return (this->freePageCount == 0 && !ShouldBeInDecommittedList()) ||
            (this->secondaryAllocator != nullptr && !this->secondaryAllocator->CanAllocate());
    }

    bool IsAllDecommitted() const
    {
        return this->GetAvailablePageCount() == this->decommitPageCount;
    }

    bool ShouldBeInDecommittedList() const
    {
        return this->decommitPageCount != 0;
    }

    bool IsFreeOrDecommitted(void* address, uint pageCount) const
    {
        Assert(this->IsInSegment(address));

        uint base = GetBitRangeBase(address);
        return this->TestRangeInDecommitPagesBitVector(base, pageCount) || this->TestRangeInFreePagesBitVector(base, pageCount);
    }

    bool IsFreeOrDecommitted(void* address) const
    {
        Assert(this->IsInSegment(address));

        uint base = GetBitRangeBase(address);
        return this->TestInDecommitPagesBitVector(base) || this->TestInFreePagesBitVector(base);
    }

    PageBitVector GetUnAllocatedPages() const
    {
        PageBitVector unallocPages = freePages;
        unallocPages.Or(&decommitPages);
        return unallocPages;
    }

    void ChangeSegmentProtection(DWORD protectFlags, DWORD expectedOldProtectFlags);

#if DBG
    bool IsPageSegment() const override
    {
        return true;
    }
#endif

//---------- Private members ---------------/
private:
    uint GetBitRangeBase(void* address) const
    {
        uint base = ((uint)(((char *)address) - this->address)) / AutoSystemInfo::PageSize;
        return base;
    }

    PageBitVector freePages;
    PageBitVector decommitPages;

    uint     freePageCount;
    uint     decommitPageCount;
};

template<typename TVirtualAlloc = VirtualAllocWrapper>
class HeapPageAllocator;

/*
 * A Page Allocation is an allocation made by a page allocator
 * This has a base address, and tracks the number of pages that
 * were allocated from the segment
 */
class PageAllocation
{
public:
    char * GetAddress() const { return ((char *)this) + sizeof(PageAllocation); }
    size_t GetSize() const { return pageCount * AutoSystemInfo::PageSize - sizeof(PageAllocation); }
    size_t GetPageCount() const { return pageCount; }
    void* GetSegment() const { return segment; }

private:
    size_t pageCount;
    void * segment;

    friend class PageAllocatorBase<VirtualAllocWrapper>;
    friend class PageAllocatorBase<PreReservedVirtualAllocWrapper>;
    friend class HeapPageAllocator<>;
};

/*
 * This allocator is responsible for allocating and freeing pages. It does
 * so by virtue of allocating segments for groups of pages, and then handing
 * out memory in these segments. It's also responsible for free segments.
 * This class also controls the idle decommit thread, which decommits pages
 * when they're no longer needed
 */

template<typename TVirtualAlloc>
class PageAllocatorBase
{
    friend class CodeGenNumberThreadAllocator;
    // Allowing recycler to report external memory allocation.
    friend class Recycler;
public:
    static uint const DefaultMaxFreePageCount = 0x400;       // 4 MB
    static uint const DefaultLowMaxFreePageCount = 0x100;    // 1 MB for low-memory process

    static uint const MinPartialDecommitFreePageCount = 0x1000;  // 16 MB

    static uint const DefaultMaxAllocPageCount = 32;        // 128K
    static uint const DefaultSecondaryAllocPageCount = 0;

    static size_t GetProcessUsedBytes();

    static size_t GetAndResetMaxUsedBytes();

#if ENABLE_BACKGROUND_PAGE_FREEING
    struct BackgroundPageQueue
    {
        BackgroundPageQueue();

        SLIST_HEADER freePageList;

        CriticalSection backgroundPageQueueCriticalSection;
#if DBG
        bool isZeroPageQueue;
#endif
    };

#if ENABLE_BACKGROUND_PAGE_ZEROING
    struct ZeroPageQueue : BackgroundPageQueue
    {
        ZeroPageQueue();

        SLIST_HEADER pendingZeroPageList;
    };
#endif
#endif

    PageAllocatorBase(AllocationPolicyManager * policyManager,
#ifndef JD_PRIVATE
        Js::ConfigFlagsTable& flags = Js::Configuration::Global.flags,
#endif
        PageAllocatorType type = PageAllocatorType_Max,
        uint maxFreePageCount = DefaultMaxFreePageCount,
        bool zeroPages = false,
#if ENABLE_BACKGROUND_PAGE_FREEING
        BackgroundPageQueue * backgroundPageQueue = nullptr,
#endif
        uint maxAllocPageCount = DefaultMaxAllocPageCount,
        uint secondaryAllocPageCount = DefaultSecondaryAllocPageCount,
        bool stopAllocationOnOutOfMemory = false,
        bool excludeGuardPages = false);

    virtual ~PageAllocatorBase();

    bool IsClosed() const { return isClosed; }
    void Close() { Assert(!isClosed); isClosed = true; }

    AllocationPolicyManager * GetAllocationPolicyManager() { return policyManager; }

    uint GetMaxAllocPageCount();

    //VirtualAllocator APIs
    TVirtualAlloc * GetVirtualAllocator() { return virtualAllocator; }
    bool IsPreReservedPageAllocator() { return virtualAllocator != nullptr; }


    PageAllocation * AllocPagesForBytes(DECLSPEC_GUARD_OVERFLOW size_t requestedBytes);
    PageAllocation * AllocAllocation(DECLSPEC_GUARD_OVERFLOW size_t pageCount);

    void ReleaseAllocation(PageAllocation * allocation);
    void ReleaseAllocationNoSuspend(PageAllocation * allocation);

    char * Alloc(size_t * pageCount, SegmentBase<TVirtualAlloc> ** segment);

    void Release(void * address, size_t pageCount, void * segment);

    char * AllocPages(DECLSPEC_GUARD_OVERFLOW uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment);
    char * AllocPagesPageAligned(DECLSPEC_GUARD_OVERFLOW uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment);

    void ReleasePages(__in void * address, uint pageCount, __in void * pageSegment);
#if ENABLE_BACKGROUND_PAGE_FREEING
    void BackgroundReleasePages(void * address, uint pageCount, PageSegmentBase<TVirtualAlloc> * pageSegment);
#endif

    // Decommit
    void DecommitNow(bool all = true);
    void SuspendIdleDecommit();
    void ResumeIdleDecommit();

#if ENABLE_BACKGROUND_PAGE_ZEROING
    void StartQueueZeroPage();
    void StopQueueZeroPage();
    void ZeroQueuedPages();
    void BackgroundZeroQueuedPages();
#endif
#if ENABLE_BACKGROUND_PAGE_FREEING
    void FlushBackgroundPages();
#endif

    bool DisableAllocationOutOfMemory() const { return disableAllocationOutOfMemory; }
    void ResetDisableAllocationOutOfMemory() { disableAllocationOutOfMemory = false; }

#ifdef RECYCLER_MEMORY_VERIFY
    void EnableVerify() { verifyEnabled = true; }
#endif
#if defined(RECYCLER_NO_PAGE_REUSE) || defined(ARENA_MEMORY_VERIFY)
    void ReenablePageReuse() { Assert(disablePageReuse); disablePageReuse = false; }
    bool DisablePageReuse() { bool wasDisablePageReuse = disablePageReuse; disablePageReuse = true; return wasDisablePageReuse; }
#endif

#if DBG
    bool HasZeroQueuedPages() const;
    virtual void SetDisableThreadAccessCheck() { disableThreadAccessCheck = true;}
    virtual void SetEnableThreadAccessCheck() { disableThreadAccessCheck = false; }

    virtual bool IsIdleDecommitPageAllocator() const { return false; }
    virtual bool HasMultiThreadAccess() const { return false; }
    bool ValidThreadAccess()
    {
        DWORD currentThreadId = ::GetCurrentThreadId();
        return disableThreadAccessCheck ||
            (this->concurrentThreadId == -1 && this->threadContextHandle == NULL) ||  // JIT thread after close
            (this->concurrentThreadId != -1 && this->concurrentThreadId == currentThreadId) ||
            this->threadContextHandle == GetCurrentThreadContextId();
    }
    virtual void UpdateThreadContextHandle(ThreadContextId updatedThreadContextHandle) { threadContextHandle = updatedThreadContextHandle; }
    void SetConcurrentThreadId(DWORD threadId) { this->concurrentThreadId = threadId; }
    void ClearConcurrentThreadId() { this->concurrentThreadId = (DWORD)-1; }
    DWORD GetConcurrentThreadId() { return this->concurrentThreadId;  }
    DWORD HasConcurrentThreadId() { return this->concurrentThreadId != -1; }

#endif

#if DBG_DUMP
    char16 const * debugName;
#endif
protected:
    SegmentBase<TVirtualAlloc> * AllocSegment(DECLSPEC_GUARD_OVERFLOW size_t pageCount);
    void ReleaseSegment(SegmentBase<TVirtualAlloc> * segment);

    template <bool doPageAlign>
    char * AllocInternal(size_t * pageCount, SegmentBase<TVirtualAlloc> ** segment);

    template <bool notPageAligned>
    char * SnailAllocPages(DECLSPEC_GUARD_OVERFLOW uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment);
    void OnAllocFromNewSegment(DECLSPEC_GUARD_OVERFLOW uint pageCount, __in void* pages, SegmentBase<TVirtualAlloc>* segment);

    template <bool notPageAligned>
    char * TryAllocFreePages(DECLSPEC_GUARD_OVERFLOW uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment);
    char * TryAllocFromZeroPagesList(DECLSPEC_GUARD_OVERFLOW uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment, SLIST_HEADER& zeroPagesList, bool isPendingZeroList);
    char * TryAllocFromZeroPages(DECLSPEC_GUARD_OVERFLOW uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment);

    template <bool notPageAligned>
    char * TryAllocDecommittedPages(DECLSPEC_GUARD_OVERFLOW uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment);

    DListBase<PageSegmentBase<TVirtualAlloc>> * GetSegmentList(PageSegmentBase<TVirtualAlloc> * segment);
    void TransferSegment(PageSegmentBase<TVirtualAlloc> * segment, DListBase<PageSegmentBase<TVirtualAlloc>> * fromSegmentList);

    void FillAllocPages(__in void * address, uint pageCount);
    void FillFreePages(__in void * address, uint pageCount);

    struct FreePageEntry : public SLIST_ENTRY
    {
        PageSegmentBase<TVirtualAlloc> * segment;
        uint pageCount;
    };

    bool IsPageSegment(SegmentBase<TVirtualAlloc>* segment)
    {
        return segment->GetAvailablePageCount() <= maxAllocPageCount;
    }

#if DBG_DUMP
    virtual void DumpStats() const;
#endif
    virtual PageSegmentBase<TVirtualAlloc> * AddPageSegment(DListBase<PageSegmentBase<TVirtualAlloc>>& segmentList);
    static PageSegmentBase<TVirtualAlloc> * AllocPageSegment(DListBase<PageSegmentBase<TVirtualAlloc>>& segmentList, 
        PageAllocatorBase<TVirtualAlloc> * pageAllocator, bool committed, bool allocated);

    // Zero Pages
#if ENABLE_BACKGROUND_PAGE_ZEROING
    void AddPageToZeroQueue(__in void * address, uint pageCount, __in PageSegmentBase<TVirtualAlloc> * pageSegment);
    bool HasZeroPageQueue() const;
#endif

    bool ZeroPages() const { return zeroPages; }
#if ENABLE_BACKGROUND_PAGE_ZEROING
    bool QueueZeroPages() const { return queueZeroPages; }
#endif

    FreePageEntry * PopPendingZeroPage();
#if DBG
    void Check();
    bool disableThreadAccessCheck;
#endif

protected:
    // Data
    DListBase<PageSegmentBase<TVirtualAlloc>> segments;
    DListBase<PageSegmentBase<TVirtualAlloc>> fullSegments;
    DListBase<PageSegmentBase<TVirtualAlloc>> emptySegments;
    DListBase<PageSegmentBase<TVirtualAlloc>> decommitSegments;

    DListBase<SegmentBase<TVirtualAlloc>> largeSegments;

    uint maxAllocPageCount;
    DWORD allocFlags;
    uint maxFreePageCount;
    size_t freePageCount;
    uint secondaryAllocPageCount;
    bool isClosed;
    bool stopAllocationOnOutOfMemory;
    bool disableAllocationOutOfMemory;
    bool excludeGuardPages;
    AllocationPolicyManager * policyManager;
    TVirtualAlloc * virtualAllocator;

#ifndef JD_PRIVATE
    Js::ConfigFlagsTable& pageAllocatorFlagTable;
#endif

    // zero pages
    bool zeroPages;
#if ENABLE_BACKGROUND_PAGE_FREEING
    BackgroundPageQueue * backgroundPageQueue;
#if ENABLE_BACKGROUND_PAGE_ZEROING
    bool queueZeroPages;
    bool hasZeroQueuedPages;
#endif
#endif

    // Idle Decommit
    bool isUsed;
    size_t minFreePageCount;
    uint idleDecommitEnterCount;

    void UpdateMinFreePageCount();
    void ResetMinFreePageCount();
    void ClearMinFreePageCount();
    void AddFreePageCount(uint pageCount);

    static uint GetFreePageLimit() { return 0; }

#if DBG
    size_t debugMinFreePageCount;
    ThreadContextId threadContextHandle;
    DWORD concurrentThreadId;
#endif
#if DBG_DUMP
    size_t decommitPageCount;
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    bool verifyEnabled;
#endif
#if defined(RECYCLER_NO_PAGE_REUSE) || defined(ARENA_MEMORY_VERIFY)
    bool disablePageReuse;
#endif

    friend class SegmentBase<TVirtualAlloc>;
    friend class PageSegmentBase<TVirtualAlloc>;
    friend class IdleDecommit;

protected:
    virtual bool CreateSecondaryAllocator(SegmentBase<TVirtualAlloc>* segment, bool committed, SecondaryAllocator** allocator)
    {
        *allocator = nullptr;
        return true;
    }

    bool IsAddressInSegment(__in void* address, const PageSegmentBase<TVirtualAlloc>& segment);
    bool IsAddressInSegment(__in void* address, const SegmentBase<TVirtualAlloc>& segment);

private:
    uint GetSecondaryAllocPageCount() const { return this->secondaryAllocPageCount; }
    void IntegrateSegments(DListBase<PageSegmentBase<TVirtualAlloc>>& segmentList, uint segmentCount, size_t pageCount);
#if ENABLE_BACKGROUND_PAGE_FREEING
    void QueuePages(void * address, uint pageCount, PageSegmentBase<TVirtualAlloc> * pageSegment);
#endif

    template <bool notPageAligned>
    char* AllocPagesInternal(DECLSPEC_GUARD_OVERFLOW uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment);

#ifdef PROFILE_MEM
    PageMemoryData * memoryData;
#endif

    size_t usedBytes;
    PageAllocatorType type;

    size_t reservedBytes;
    size_t committedBytes;
    size_t numberOfSegments;

#ifdef PERF_COUNTERS
    PerfCounter::Counter& GetReservedSizeCounter() const
    {
        return PerfCounter::PageAllocatorCounterSet::GetReservedSizeCounter(type);
    }
    PerfCounter::Counter& GetCommittedSizeCounter() const
    {
        return PerfCounter::PageAllocatorCounterSet::GetCommittedSizeCounter(type);
    }
    PerfCounter::Counter& GetUsedSizeCounter() const
    {
        return PerfCounter::PageAllocatorCounterSet::GetUsedSizeCounter(type);
    }
    PerfCounter::Counter& GetTotalReservedSizeCounter() const
    {
        return PerfCounter::PageAllocatorCounterSet::GetTotalReservedSizeCounter();
    }
    PerfCounter::Counter& GetTotalCommittedSizeCounter() const
    {
        return PerfCounter::PageAllocatorCounterSet::GetTotalCommittedSizeCounter();
    }
    PerfCounter::Counter& GetTotalUsedSizeCounter() const
    {
        return PerfCounter::PageAllocatorCounterSet::GetTotalUsedSizeCounter();
    }
#endif
    void AddReservedBytes(size_t bytes);
    void SubReservedBytes(size_t bytes);
    void AddCommittedBytes(size_t bytes);
    void SubCommittedBytes(size_t bytes);
    void AddUsedBytes(size_t bytes);
    void SubUsedBytes(size_t bytes);
    void AddNumberOfSegments(size_t segmentCount);
    void SubNumberOfSegments(size_t segmentCount);

    bool RequestAlloc(size_t byteCount)
    {
        if (disableAllocationOutOfMemory)
        {
            return false;
        }

        if (policyManager != nullptr)
        {
            return policyManager->RequestAlloc(byteCount);
        }

        return true;
    }

    void ReportFree(size_t byteCount)
    {
        if (policyManager != nullptr)
        {
            policyManager->ReportFree(byteCount);
        }
    }

    template <typename T>
    void ReleaseSegmentList(DListBase<T> * segmentList);

protected:
    // Instrumentation
    void LogAllocSegment(SegmentBase<TVirtualAlloc> * segment);
    void LogAllocSegment(uint segmentCount, size_t pageCount);
    void LogFreeSegment(SegmentBase<TVirtualAlloc> * segment);
    void LogFreeDecommittedSegment(SegmentBase<TVirtualAlloc> * segment);
    void LogFreePartiallyDecommittedPageSegment(PageSegmentBase<TVirtualAlloc> * pageSegment);

    void LogAllocPages(size_t pageCount);
    void LogFreePages(size_t pageCount);
    void LogCommitPages(size_t pageCount);
    void LogRecommitPages(size_t pageCount);
    void LogDecommitPages(size_t pageCount);

    void ReportFailure(size_t byteCount)
    {
        if (this->stopAllocationOnOutOfMemory)
        {
            this->disableAllocationOutOfMemory = true;
        }

        if (policyManager != nullptr)
        {
            policyManager->ReportFailure(byteCount);
        }
    }
};

template <>
char *
PageAllocatorBase<VirtualAllocWrapper>::AllocPages(uint pageCount, PageSegmentBase<VirtualAllocWrapper> ** pageSegment);

template<typename TVirtualAlloc>
class HeapPageAllocator : public PageAllocatorBase<TVirtualAlloc>
{
    typedef PageAllocatorBase<TVirtualAlloc> Base;
public:
    HeapPageAllocator(AllocationPolicyManager * policyManager, bool allocXdata, bool excludeGuardPages, TVirtualAlloc * virtualAllocator);

    BOOL ProtectPages(__in char* address, size_t pageCount, __in void* segment, DWORD dwVirtualProtectFlags, DWORD desiredOldProtectFlag);
    bool AllocSecondary(void* segment, ULONG_PTR functionStart, DWORD functionSize, DECLSPEC_GUARD_OVERFLOW ushort pdataCount, DECLSPEC_GUARD_OVERFLOW ushort xdataSize, SecondaryAllocation* allocation);
    bool ReleaseSecondary(const SecondaryAllocation& allocation, void* segment);
    void TrackDecommittedPages(void * address, uint pageCount, __in void* segment);
    void DecommitPages(__in char* address, size_t pageCount = 1);

    // Release pages that has already been decommitted
    void ReleaseDecommitted(void * address, size_t pageCount, __in void * segment);
    bool IsAddressFromAllocator(__in void* address);

    bool AllocXdata() { return allocXdata; }
private:
    bool         allocXdata;
    void         ReleaseDecommittedSegment(__in SegmentBase<TVirtualAlloc>* segment);
#if PDATA_ENABLED
    virtual bool CreateSecondaryAllocator(SegmentBase<TVirtualAlloc>* segment, bool committed, SecondaryAllocator** allocator) override;
#endif

};

}
