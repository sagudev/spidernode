//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//---------------------------------------------------------------------------------
// One allocation chunk from CustomHeap + PData if needed, tracked as a linked list
//---------------------------------------------------------------------------------
struct EmitBufferAllocation
{
    CustomHeap::Allocation* allocation;
    size_t bytesUsed;
    size_t bytesCommitted;
    bool   recorded;
    EmitBufferAllocation * nextAllocation;

    BYTE * GetUnused() const            { return (BYTE*) allocation->address + bytesUsed; }
    BYTE * GetUncommitted() const       { return (BYTE*) allocation->address + bytesCommitted; }

    // Truncation to DWORD okay here
    DWORD BytesFree() const    { return static_cast<DWORD>(this->bytesCommitted - this->bytesUsed); }
};
typedef void* NativeMethod;

//----------------------------------------------------------------------------
// Emit buffer manager - manages allocation chunks from VirtualAlloc
//----------------------------------------------------------------------------
template <class SyncObject = FakeCriticalSection>
class EmitBufferManager
{
public:
    EmitBufferManager(ArenaAllocator * allocator, CustomHeap::CodePageAllocators * codePageAllocators, Js::ScriptContext * scriptContext, LPCWSTR name);
    ~EmitBufferManager();

    // All the following methods are guarded with the SyncObject
    void Decommit();
    void Clear();

    EmitBufferAllocation* AllocateBuffer(DECLSPEC_GUARD_OVERFLOW __in size_t bytes, __deref_bcount(bytes) BYTE** ppBuffer, ushort pdataCount = 0, ushort xdataSize = 0, bool canAllocInPreReservedHeapPageSegment = false, bool isAnyJittedCode = false);
    bool CommitBuffer(EmitBufferAllocation* allocation, __out_bcount(bytes) BYTE* destBuffer, __in size_t bytes, __in_bcount(bytes) const BYTE* sourceBuffer, __in DWORD alignPad = 0);
    bool ProtectBufferWithExecuteReadWriteForInterpreter(EmitBufferAllocation* allocation);
    bool CommitReadWriteBufferForInterpreter(EmitBufferAllocation* allocation, _In_reads_bytes_(bufferSize) BYTE* pBuffer, _In_ size_t bufferSize);
    void CompletePreviousAllocation(EmitBufferAllocation* allocation);
    bool FreeAllocation(void* address);
    //Ends here

    bool IsInHeap(void* address);

#if DBG_DUMP
    void DumpAndResetStats(char16 const * source);
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void CheckBufferPermissions(EmitBufferAllocation *allocation);
#endif

#if DBG
    bool IsBufferExecuteReadOnly(EmitBufferAllocation * allocation);
#endif

    EmitBufferAllocation * allocations;

private:
    void FreeAllocations(bool release);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    bool CheckCommitFaultInjection();

    int commitCount;
#endif
    ArenaAllocator * allocator;
    Js::ScriptContext * scriptContext;

    EmitBufferAllocation * NewAllocation(DECLSPEC_GUARD_OVERFLOW size_t bytes, ushort pdataCount, ushort xdataSize, bool canAllocInPreReservedHeapPageSegment, bool isAnyJittedCode);
    EmitBufferAllocation* GetBuffer(EmitBufferAllocation *allocation, DECLSPEC_GUARD_OVERFLOW __in size_t bytes, __deref_bcount(bytes) BYTE** ppBuffer);

    bool FinalizeAllocation(EmitBufferAllocation *allocation);
    CustomHeap::Heap allocationHeap;

    SyncObject  criticalSection;
#if DBG_DUMP

public:
    LPCWSTR name;
    size_t totalBytesCode;
    size_t totalBytesLoopBody;
    size_t totalBytesAlignment;
    size_t totalBytesCommitted;
    size_t totalBytesReserved;
#endif
};

