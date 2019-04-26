//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeBasePch.h"
#include "BackendApi.h"
#include "ThreadServiceWrapper.h"
#include "Types/TypePropertyCache.h"
#include "Debug/DebuggingFlags.h"
#include "Debug/DiagProbe.h"
#include "Debug/DebugManager.h"
#include "Chars.h"
#include "CaseInsensitive.h"
#include "CharSet.h"
#include "CharMap.h"
#include "StandardChars.h"
#include "Base/ThreadContextTlsEntry.h"
#include "Base/ThreadBoundThreadContextManager.h"
#include "Language/SourceDynamicProfileManager.h"
#include "Language/CodeGenRecyclableData.h"
#include "Language/InterpreterStackFrame.h"
#include "Language/JavascriptStackWalker.h"
#include "Base/ScriptMemoryDumper.h"

// SIMD_JS
#include "Library/SimdLib.h"

#if DBG
#include "Memory/StressTest.h"
#endif

#ifdef DYNAMIC_PROFILE_MUTATOR
#include "Language/DynamicProfileMutator.h"
#endif


#ifdef ENABLE_BASIC_TELEMETRY
#include "Telemetry.h"
#endif

int TotalNumberOfBuiltInProperties = Js::PropertyIds::_countJSOnlyProperty;

/*
 * When we aren't adding any additional properties
 */
void DefaultInitializeAdditionalProperties(ThreadContext *threadContext)
{

}

/*
 *
 */
void (*InitializeAdditionalProperties)(ThreadContext *threadContext) = DefaultInitializeAdditionalProperties;

// To make sure the marker function doesn't get inlined, optimized away, or merged with other functions we disable optimization.
// If this method ends up causing a perf problem in the future, we should replace it with asm versions which should be lighter.
#pragma optimize("g", off)
_NOINLINE extern "C" void* MarkerForExternalDebugStep()
{
    // We need to return something here to prevent this function from being merged with other empty functions by the linker.
    static int __dummy;
    return &__dummy;
}
#pragma optimize("", on)

CriticalSection ThreadContext::s_csThreadContext;
size_t ThreadContext::processNativeCodeSize = 0;
ThreadContext * ThreadContext::globalListFirst = nullptr;
ThreadContext * ThreadContext::globalListLast = nullptr;
__declspec(thread) uint ThreadContext::activeScriptSiteCount = 0;

const Js::PropertyRecord * const ThreadContext::builtInPropertyRecords[] =
{
    Js::BuiltInPropertyRecords::EMPTY,
#define ENTRY_INTERNAL_SYMBOL(n) Js::BuiltInPropertyRecords::n,
#define ENTRY_SYMBOL(n, d) Js::BuiltInPropertyRecords::n,
#define ENTRY(n) Js::BuiltInPropertyRecords::n,
#define ENTRY2(n, s) ENTRY(n)
#include "Base/JnDirectFields.h"
};

ThreadContext::RecyclableData::RecyclableData(Recycler *const recycler) :
    soErrorObject(nullptr, nullptr, nullptr, true),
    oomErrorObject(nullptr, nullptr, nullptr, true),
    terminatedErrorObject(nullptr, nullptr, nullptr),
    typesWithProtoPropertyCache(recycler),
    propertyGuards(recycler, 128),
    oldEntryPointInfo(nullptr),
    returnedValueList(nullptr),
    constructorCacheInvalidationCount(0)
{
}

#if PDATA_ENABLED
#define ALLOC_XDATA (true)
#else
#define ALLOC_XDATA (false)
#endif

ThreadContext::ThreadContext(AllocationPolicyManager * allocationPolicyManager, JsUtil::ThreadService::ThreadServiceCallback threadServiceCallback, bool enableExperimentalFeatures) :
    currentThreadId(::GetCurrentThreadId()),
    stackLimitForCurrentThread(0),
    stackProber(nullptr),
    isThreadBound(false),
    hasThrownPendingException(false),
    noScriptScope(false),
    heapEnum(nullptr),
    threadContextFlags(ThreadContextFlagNoFlag),
    JsUtil::DoublyLinkedListElement<ThreadContext>(),
    allocationPolicyManager(allocationPolicyManager),
    threadService(threadServiceCallback),
    isOptimizedForManyInstances(Js::Configuration::Global.flags.OptimizeForManyInstances),
    bgJit(Js::Configuration::Global.flags.BgJit),
    pageAllocator(allocationPolicyManager, PageAllocatorType_Thread, Js::Configuration::Global.flags, 0, PageAllocator::DefaultMaxFreePageCount,
        false
#if ENABLE_BACKGROUND_PAGE_FREEING
        , &backgroundPageQueue
#endif
        ),
    recycler(nullptr),
    hasCollectionCallBack(false),
    callDispose(true),
#if ENABLE_NATIVE_CODEGEN
    jobProcessor(nullptr),
#endif
    interruptPoller(nullptr),
    expirableCollectModeGcCount(-1),
    expirableObjectList(nullptr),
    expirableObjectDisposeList(nullptr),
    numExpirableObjects(0),
    disableExpiration(false),
    callRootLevel(0),
    nextTypeId((Js::TypeId)Js::Constants::ReservedTypeIds),
    entryExitRecord(nullptr),
    leafInterpreterFrame(nullptr),
    threadServiceWrapper(nullptr),
    temporaryArenaAllocatorCount(0),
    temporaryGuestArenaAllocatorCount(0),
    crefSContextForDiag(0),
    scriptContextList(nullptr),
    scriptContextEverRegistered(false),
#if DBG_DUMP || defined(PROFILE_EXEC)
    topLevelScriptSite(nullptr),
#endif
    polymorphicCacheState(0),
    stackProbeCount(0),
#ifdef BAILOUT_INJECTION
    bailOutByteCodeLocationCount(0),
#endif
    sourceCodeSize(0),
    nativeCodeSize(0),
    threadAlloc(_u("TC"), GetPageAllocator(), Js::Throw::OutOfMemory),
    inlineCacheThreadInfoAllocator(_u("TC-InlineCacheInfo"), GetPageAllocator(), Js::Throw::OutOfMemory),
    isInstInlineCacheThreadInfoAllocator(_u("TC-IsInstInlineCacheInfo"), GetPageAllocator(), Js::Throw::OutOfMemory),
    equivalentTypeCacheInfoAllocator(_u("TC-EquivalentTypeCacheInfo"), GetPageAllocator(), Js::Throw::OutOfMemory),
    protoInlineCacheByPropId(&inlineCacheThreadInfoAllocator, 512),
    storeFieldInlineCacheByPropId(&inlineCacheThreadInfoAllocator, 256),
    isInstInlineCacheByFunction(&isInstInlineCacheThreadInfoAllocator, 128),
    registeredInlineCacheCount(0),
    unregisteredInlineCacheCount(0),
    prototypeChainEnsuredToHaveOnlyWritableDataPropertiesAllocator(_u("TC-ProtoWritableProp"), GetPageAllocator(), Js::Throw::OutOfMemory),
    standardUTF8Chars(0),
    standardUnicodeChars(0),
    hasUnhandledException(FALSE),
    hasCatchHandler(FALSE),
    disableImplicitFlags(DisableImplicitNoFlag),
    hasCatchHandlerToUserCode(false),
    propertyMap(nullptr),
    caseInvariantPropertySet(nullptr),
    entryPointToBuiltInOperationIdCache(&threadAlloc, 0),
#if ENABLE_NATIVE_CODEGEN
    codeGenNumberThreadAllocator(nullptr),
#if DYNAMIC_INTERPRETER_THUNK || defined(ASMJS_PLAT)
    thunkPageAllocators(allocationPolicyManager, /* allocXData */ false, /* virtualAllocator */ nullptr),
#endif
    codePageAllocators(allocationPolicyManager, ALLOC_XDATA, GetPreReservedVirtualAllocator()),
#endif
    dynamicObjectEnumeratorCacheMap(&HeapAllocator::Instance, 16),
    //threadContextFlags(ThreadContextFlagNoFlag),
    telemetryBlock(&localTelemetryBlock),
    configuration(enableExperimentalFeatures),
    jsrtRuntime(nullptr),
    rootPendingClose(nullptr),
    wellKnownHostTypeHTMLAllCollectionTypeId(Js::TypeIds_Undefined),
    isProfilingUserCode(true),
    loopDepth(0),
    maxGlobalFunctionExecTime(0.0),
    isAllJITCodeInPreReservedRegion(true),
    tridentLoadAddress(nullptr),
    debugManager(nullptr)
#if ENABLE_TTD
    , IsTTRecordRequested(false)
    , IsTTDebugRequested(false)
    , TTDUri()
    , TTSnapInterval(2000)
    , TTSnapHistoryLength(UINT32_MAX)
    , TTDLog(nullptr)
    , TTDWriteInitializeFunction(nullptr)
    , TTDStreamFunctions({ 0 })
#endif
#ifdef ENABLE_DIRECTCALL_TELEMETRY
    , directCallTelemetry(this)
#endif
{
    pendingProjectionContextCloseList = JsUtil::List<IProjectionContext*, ArenaAllocator>::New(GetThreadAlloc());
    hostScriptContextStack = Anew(GetThreadAlloc(), JsUtil::Stack<HostScriptContext*>, GetThreadAlloc());

    functionCount = 0;
    sourceInfoCount = 0;
    scriptContextCount=0;

    isScriptActive = false;

#ifdef ENABLE_CUSTOM_ENTROPY
    entropy.Initialize();
#endif
    
#if ENABLE_NATIVE_CODEGEN
    this->bailOutRegisterSaveSpace = AnewArrayZ(this->GetThreadAlloc(), Js::Var, GetBailOutRegisterSaveSlotCount());
#endif

#if defined(ENABLE_SIMDJS) && ENABLE_NATIVE_CODEGEN
    simdFuncInfoToOpcodeMap = Anew(this->GetThreadAlloc(), FuncInfoToOpcodeMap, this->GetThreadAlloc());
    simdOpcodeToSignatureMap = AnewArrayZ(this->GetThreadAlloc(), SimdFuncSignature, Js::Simd128OpcodeCount());
    {
#define MACRO_SIMD_WMS(op, LayoutAsmJs, OpCodeAttrAsmJs, OpCodeAttr, ...) \
    AddSimdFuncToMaps(Js::OpCode::##op, __VA_ARGS__);

#define MACRO_SIMD_EXTEND_WMS(op, LayoutAsmJs, OpCodeAttrAsmJs, OpCodeAttr, ...) MACRO_SIMD_WMS(op, LayoutAsmJs, OpCodeAttrAsmJs, OpCodeAttr, __VA_ARGS__)

#include "ByteCode/OpCodesSimd.h"
    }
#endif // defined(ENABLE_SIMDJS) && ENABLE_NATIVE_CODEGEN

#if DBG_DUMP
    scriptSiteCount = 0;
    pageAllocator.debugName = _u("Thread");
#endif
#ifdef DYNAMIC_PROFILE_MUTATOR
    this->dynamicProfileMutator = DynamicProfileMutator::GetMutator();
#endif

    PERF_COUNTER_INC(Basic, ThreadContext);

#ifdef LEAK_REPORT
    this->rootTrackerScriptContext = nullptr;
    this->threadId = ::GetCurrentThreadId();
#endif

    memset(&localTelemetryBlock, 0, sizeof(localTelemetryBlock));

    AutoCriticalSection autocs(ThreadContext::GetCriticalSection());
    ThreadContext::LinkToBeginning(this, &ThreadContext::globalListFirst, &ThreadContext::globalListLast);
#if DBG
    // Since we created our page allocator while we were constructing this thread context
    // it will pick up the thread context id that is current on the thread. We need to update
    // that now.
    pageAllocator.UpdateThreadContextHandle((ThreadContextId)this);
#endif

#ifdef ENABLE_PROJECTION
#if DBG_DUMP
    this->projectionMemoryInformation = nullptr;
#endif
#endif
    this->InitAvailableCommit();
}

void ThreadContext::InitAvailableCommit()
{
    // Once per process: get the available commit for the process from the OS and push it to the AutoSystemInfo.
    // (This must be done lazily, outside DllMain. And it must be done from the Runtime, since the common lib
    // doesn't have access to the DelayLoadLibrary stuff.)
    ULONG64 commit;
    BOOL success = AutoSystemInfo::Data.GetAvailableCommit(&commit);
    if (!success)
    {
        commit = (ULONG64)-1;
#ifdef NTBUILD
        APP_MEMORY_INFORMATION AppMemInfo;
        success = GetWinCoreProcessThreads()->GetProcessInformation(
            GetCurrentProcess(),
            ProcessAppMemoryInfo,
            &AppMemInfo,
            sizeof(AppMemInfo));
        if (success)
        {
            commit = AppMemInfo.AvailableCommit;
        }
#endif
        AutoSystemInfo::Data.SetAvailableCommit(commit);
    }
}

void ThreadContext::SetStackProber(StackProber * stackProber)
{
    this->stackProber = stackProber;

    if (stackProber != NULL && this->stackLimitForCurrentThread != Js::Constants::StackLimitForScriptInterrupt)
    {
        this->stackLimitForCurrentThread = stackProber->GetScriptStackLimit();
    }
}

PBYTE ThreadContext::GetScriptStackLimit() const
{
    return stackProber->GetScriptStackLimit();
}

IActiveScriptProfilerHeapEnum* ThreadContext::GetHeapEnum()
{
    return heapEnum;
}

void ThreadContext::SetHeapEnum(IActiveScriptProfilerHeapEnum* newHeapEnum)
{
    Assert((newHeapEnum != nullptr && heapEnum == nullptr) || (newHeapEnum == nullptr && heapEnum != nullptr));
    heapEnum = newHeapEnum;
}

void ThreadContext::ClearHeapEnum()
{
    Assert(heapEnum != nullptr);
    heapEnum = nullptr;
}

void ThreadContext::GlobalInitialize()
{
    for (int i = 0; i < _countof(builtInPropertyRecords); i++)
    {
        builtInPropertyRecords[i]->SetHash(JsUtil::CharacterBuffer<WCHAR>::StaticGetHashCode(builtInPropertyRecords[i]->GetBuffer(), builtInPropertyRecords[i]->GetLength()));
    }
}

ThreadContext::~ThreadContext()
{
    {
        AutoCriticalSection autocs(ThreadContext::GetCriticalSection());
        ThreadContext::Unlink(this, &ThreadContext::globalListFirst, &ThreadContext::globalListLast);
    }

#if ENABLE_TTD
    if(this->TTDLog != nullptr)
    {
        TT_HEAP_DELETE(TTD::EventLog, this->TTDLog);
        this->TTDLog = nullptr;
    }
#endif

#ifdef LEAK_REPORT
    if (Js::Configuration::Global.flags.IsEnabled(Js::LeakReportFlag))
    {
        AUTO_LEAK_REPORT_SECTION(Js::Configuration::Global.flags, _u("Thread Context (%p): %s (TID: %d)"), this,
            this->GetRecycler()->IsInDllCanUnloadNow()? _u("DllCanUnloadNow") :
            this->GetRecycler()->IsInDetachProcess()? _u("DetachProcess") : _u("Destructor"), this->threadId);
        LeakReport::DumpUrl(this->threadId);
    }
#endif
    if (interruptPoller)
    {
        HeapDelete(interruptPoller);
        interruptPoller = nullptr;
    }

#if DBG
    // ThreadContext dtor may be running on a different thread.
    // Recycler may call finalizer that free temp Arenas, which will free pages back to
    // the page Allocator, which will try to suspend idle on a different thread.
    // So we need to disable idle decommit asserts.
    pageAllocator.ShutdownIdleDecommit();
#endif

    // Allocating memory during the shutdown codepath is not preferred
    // so we'll close the page allocator before we release the GC
    // If any dispose is allocating memory during shutdown, that is a bug
    pageAllocator.Close();

    // The recycler need to delete before the background code gen thread
    // because that might run finalizer which need access to the background code gen thread.
    if (recycler != nullptr)
    {
        for (Js::ScriptContext *scriptContext = scriptContextList; scriptContext; scriptContext = scriptContext->next)
        {
            if (!scriptContext->IsActuallyClosed())
            {
                // We close ScriptContext here because anyhow HeapDelete(recycler) when disposing the
                // JavaScriptLibrary will close ScriptContext. Explicit close gives us chance to clear
                // other things to which ScriptContext holds reference to
                AssertMsg(!IsInScript(), "Can we be in script here?");
                scriptContext->MarkForClose();
            }
        }

        // If all scriptContext's have been closed, then the sourceProfileManagersByUrl
        // should have been released
        AssertMsg(this->recyclableData->sourceProfileManagersByUrl == nullptr ||
            this->recyclableData->sourceProfileManagersByUrl->Count() == 0, "There seems to have been a refcounting imbalance.");

        this->recyclableData->sourceProfileManagersByUrl = nullptr;
        this->recyclableData->oldEntryPointInfo = nullptr;

        if (this->recyclableData->symbolRegistrationMap != nullptr)
        {
            this->recyclableData->symbolRegistrationMap->Clear();
            this->recyclableData->symbolRegistrationMap = nullptr;
        }

        if (this->recyclableData->returnedValueList != nullptr)
        {
            this->recyclableData->returnedValueList->Clear();
            this->recyclableData->returnedValueList = nullptr;
        }

        if (this->propertyMap != nullptr)
        {
            HeapDelete(this->propertyMap);
            this->propertyMap = nullptr;
        }

        // Unpin the memory for leak report so we don't report this as a leak.
        recyclableData.Unroot(recycler);

#if defined(LEAK_REPORT) || defined(CHECK_MEMORY_LEAK)
        for (Js::ScriptContext *scriptContext = scriptContextList; scriptContext; scriptContext = scriptContext->next)
        {
            scriptContext->ClearSourceContextInfoMaps();
            scriptContext->ShutdownClearSourceLists();
        }

#ifdef LEAK_REPORT
        // heuristically figure out which one is the root tracker script engine
        // and force close on it
        if (this->rootTrackerScriptContext != nullptr)
        {
            this->rootTrackerScriptContext->Close(false);
        }
#endif
#endif
#if ENABLE_NATIVE_CODEGEN
        HeapDelete(this->codeGenNumberThreadAllocator);
        this->codeGenNumberThreadAllocator = nullptr;
#endif

        Assert(this->debugManager == nullptr);

        HeapDelete(recycler);
    }

#if ENABLE_NATIVE_CODEGEN
    if(jobProcessor)
    {
        if(this->bgJit)
        {
            HeapDelete(static_cast<JsUtil::BackgroundJobProcessor *>(jobProcessor));
        }
        else
        {
            HeapDelete(static_cast<JsUtil::ForegroundJobProcessor *>(jobProcessor));
        }
        jobProcessor = nullptr;
    }
#endif

    // Do not require all GC callbacks to be revoked, because Trident may not revoke if there
    // is a leak, and we don't want the leak to be masked by an assert

#ifdef ENABLE_PROJECTION
    externalWeakReferenceCacheList.Clear(&HeapAllocator::Instance);
#endif

    this->collectCallBackList.Clear(&HeapAllocator::Instance);
    this->protoInlineCacheByPropId.Reset();
    this->storeFieldInlineCacheByPropId.Reset();
    this->isInstInlineCacheByFunction.Reset();
    this->equivalentTypeCacheEntryPoints.Reset();
    this->prototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext.Reset();

    this->registeredInlineCacheCount = 0;
    this->unregisteredInlineCacheCount = 0;

    AssertMsg(this->GetHeapEnum() == nullptr, "Heap enumeration should have been cleared/closed by the ScriptSite.");
    if (this->GetHeapEnum() != nullptr)
    {
        this->ClearHeapEnum();
    }

#ifdef BAILOUT_INJECTION
    if (Js::Configuration::Global.flags.IsEnabled(Js::BailOutByteCodeFlag)
        && Js::Configuration::Global.flags.BailOutByteCode.Empty())
    {
        Output::Print(_u("Bail out byte code location count: %d"), this->bailOutByteCodeLocationCount);
    }
#endif

    Assert(processNativeCodeSize >= nativeCodeSize);
    ::InterlockedExchangeSubtract(&processNativeCodeSize, nativeCodeSize);

    PERF_COUNTER_DEC(Basic, ThreadContext);

#ifdef DYNAMIC_PROFILE_MUTATOR
    if (this->dynamicProfileMutator != nullptr)
    {
        this->dynamicProfileMutator->Delete();
    }
#endif

#ifdef ENABLE_PROJECTION
#if DBG_DUMP
    if (this->projectionMemoryInformation)
    {
        this->projectionMemoryInformation->Release();
        this->projectionMemoryInformation = nullptr;
    }
#endif
#endif
}

void
ThreadContext::SetJSRTRuntime(void* runtime)
{
    Assert(jsrtRuntime == nullptr); jsrtRuntime = runtime;
#ifdef ENABLE_BASIC_TELEMETRY
    Telemetry::EnsureInitializeForJSRT();
#endif
}

void ThreadContext::CloseForJSRT()
{
    // This is used for JSRT APIs only.
    Assert(this->jsrtRuntime);
#ifdef ENABLE_BASIC_TELEMETRY
    // log any relevant telemetry before disposing the current thread for cases which are properly shutdown
    Telemetry::OnJSRTThreadContextClose();
#endif
    ShutdownThreads();
}


ThreadContext* ThreadContext::GetContextForCurrentThread()
{
    ThreadContextTLSEntry * tlsEntry = ThreadContextTLSEntry::GetEntryForCurrentThread();
    if (tlsEntry != nullptr)
    {
        return static_cast<ThreadContext *>(tlsEntry->GetThreadContext());
    }
    return nullptr;
}

void ThreadContext::ValidateThreadContext()
    {
#if DBG
    // verify the runtime pointer is valid.
        {
            BOOL found = FALSE;
            AutoCriticalSection autocs(ThreadContext::GetCriticalSection());
            ThreadContext* currentThreadContext = GetThreadContextList();
            while (currentThreadContext)
            {
                if (currentThreadContext == this)
                {
                    return;
                }
                currentThreadContext = currentThreadContext->Next();
            }
            AssertMsg(found, "invalid thread context");
        }

#endif
}

#if ENABLE_NATIVE_CODEGEN && defined(ENABLE_SIMDJS)
void ThreadContext::AddSimdFuncToMaps(Js::OpCode op, ...)
{
    Assert(simdFuncInfoToOpcodeMap != nullptr);
    Assert(simdOpcodeToSignatureMap != nullptr);

    va_list arguments;
    va_start(arguments, op);

    int argumentsCount = va_arg(arguments, int);
    AssertMsg(argumentsCount >= 0 && argumentsCount <= 20, "Invalid arguments count for SIMD opcode");
    if (argumentsCount == 0)
    {
        // no info to add
        return;
    }
    Js::FunctionInfo *funcInfo = va_arg(arguments, Js::FunctionInfo*);
    AddSimdFuncInfo(op, funcInfo);

    SimdFuncSignature simdFuncSignature;
    simdFuncSignature.valid = true;
    simdFuncSignature.argCount = argumentsCount - 2; // arg count to Simd func = argumentsCount - FuncInfo and return Type fields.
    simdFuncSignature.returnType = va_arg(arguments, ValueType);
    simdFuncSignature.args = AnewArrayZ(this->GetThreadAlloc(), ValueType, simdFuncSignature.argCount);
    for (uint iArg = 0; iArg < simdFuncSignature.argCount; iArg++)
    {
        simdFuncSignature.args[iArg] = va_arg(arguments, ValueType);
    }

    simdOpcodeToSignatureMap[Js::SIMDUtils::SimdOpcodeAsIndex(op)] = simdFuncSignature;

    va_end(arguments);
}

void ThreadContext::AddSimdFuncInfo(Js::OpCode op, Js::FunctionInfo *funcInfo)
{
    // primary funcInfo
    simdFuncInfoToOpcodeMap->AddNew(funcInfo, op);
    // Entry points of SIMD loads/stores of non-full width all map to the same opcode. This is not captured in the opcode table, so add additional entry points here.
    switch (op)
    {
    case Js::OpCode::Simd128_LdArr_F4:
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDFloat32x4Lib::EntryInfo::Load1, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDFloat32x4Lib::EntryInfo::Load2, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDFloat32x4Lib::EntryInfo::Load3, op);
        break;
    case Js::OpCode::Simd128_StArr_F4:
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDFloat32x4Lib::EntryInfo::Store1, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDFloat32x4Lib::EntryInfo::Store2, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDFloat32x4Lib::EntryInfo::Store3, op);
        break;
    case Js::OpCode::Simd128_LdArr_I4:
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDInt32x4Lib::EntryInfo::Load1, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDInt32x4Lib::EntryInfo::Load2, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDInt32x4Lib::EntryInfo::Load3, op);
        break;
    case Js::OpCode::Simd128_StArr_I4:
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDInt32x4Lib::EntryInfo::Store1, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDInt32x4Lib::EntryInfo::Store2, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDInt32x4Lib::EntryInfo::Store3, op);
        break;
    }
}

Js::OpCode ThreadContext::GetSimdOpcodeFromFuncInfo(Js::FunctionInfo * funcInfo)
{
    Assert(simdFuncInfoToOpcodeMap != nullptr);
    if (simdFuncInfoToOpcodeMap->ContainsKey(funcInfo))
    {
        return simdFuncInfoToOpcodeMap->Item(funcInfo);

    }
    return (Js::OpCode) 0;
}

void ThreadContext::GetSimdFuncSignatureFromOpcode(Js::OpCode op, SimdFuncSignature &funcSignature)
{
    Assert(simdOpcodeToSignatureMap != nullptr);
    funcSignature = simdOpcodeToSignatureMap[Js::SIMDUtils::SimdOpcodeAsIndex(op)];
}
#endif

class AutoRecyclerPtr : public AutoPtr<Recycler>
{
public:
    AutoRecyclerPtr(Recycler * ptr) : AutoPtr<Recycler>(ptr) {}
    ~AutoRecyclerPtr()
    {
#if ENABLE_CONCURRENT_GC
        if (ptr != nullptr)
        {
            ptr->ShutdownThread();
        }
#endif
    }
};

Recycler* ThreadContext::EnsureRecycler()
{
    if (recycler == NULL)
    {
        AutoRecyclerPtr newRecycler(HeapNew(Recycler, GetAllocationPolicyManager(), &pageAllocator, Js::Throw::OutOfMemory, Js::Configuration::Global.flags));
        newRecycler->Initialize(isOptimizedForManyInstances, &threadService); // use in-thread GC when optimizing for many instances
        newRecycler->SetCollectionWrapper(this);

#if ENABLE_NATIVE_CODEGEN
        // This may throw, so it needs to be after the recycler is initialized,
        // otherwise, the recycler dtor may encounter problems
        AutoPtr<CodeGenNumberThreadAllocator> localCodeGenNumberThreadAllocator(
            HeapNew(CodeGenNumberThreadAllocator, newRecycler));
#endif

        this->recyclableData.Root(RecyclerNewZ(newRecycler, RecyclableData, newRecycler), newRecycler);

        if (this->GetIsThreadBound())
        {
            newRecycler->SetIsThreadBound();
        }

        // Assign the recycler to the ThreadContext after everything is initialized, because an OOM during initialization would
        // result in only partial initialization, so the 'recycler' member variable should remain null to cause full
        // reinitialization when requested later. Anything that happens after the Detach must have special cleanup code.
        this->recycler = newRecycler.Detach();

        try
        {
#ifdef RECYCLER_WRITE_BARRIER
#ifdef _M_X64_OR_ARM64
            if (!RecyclerWriteBarrierManager::OnThreadInit())
            {
                Js::Throw::OutOfMemory();
            }
#endif
#endif

            this->expirableObjectList = Anew(&this->threadAlloc, ExpirableObjectList, &this->threadAlloc);
            this->expirableObjectDisposeList = Anew(&this->threadAlloc, ExpirableObjectList, &this->threadAlloc);

            InitializePropertyMaps(); // has many dependencies on the recycler and other members of the thread context
#if ENABLE_NATIVE_CODEGEN
            this->codeGenNumberThreadAllocator = localCodeGenNumberThreadAllocator.Detach();
#endif
        }
        catch(...)
        {
            // Initialization failed, undo what was done above. Callees that throw must clean up after themselves.
            if (this->recyclableData != nullptr)
            {
                this->recyclableData.Unroot(this->recycler);
            }

            {
                // AutoRecyclerPtr's destructor takes care of shutting down the background thread and deleting the recycler
                AutoRecyclerPtr recyclerToDelete(this->recycler);
                this->recycler = nullptr;
            }

            throw;
        }

        JS_ETW(EventWriteJSCRIPT_GC_INIT(this->recycler, this->GetHiResTimer()->Now()));
    }

#if DBG
    if (CONFIG_FLAG(RecyclerTest))
    {
        StressTester test(recycler);
        test.Run();
    }
#endif

    return recycler;
}

Js::PropertyRecord const *
ThreadContext::GetPropertyName(Js::PropertyId propertyId)
{
    // This API should only be use on the main thread
    Assert(GetCurrentThreadContextId() == (ThreadContextId)this);
    return this->GetPropertyNameImpl<false>(propertyId);
}

Js::PropertyRecord const *
ThreadContext::GetPropertyNameLocked(Js::PropertyId propertyId)
{
    return GetPropertyNameImpl<true>(propertyId);
}

template <bool locked>
Js::PropertyRecord const *
ThreadContext::GetPropertyNameImpl(Js::PropertyId propertyId)
{
    //TODO: Remove this when completely transformed to use PropertyRecord*. Currently this is only partially done,
    // and there are calls to GetPropertyName with InternalPropertyId.

    if (propertyId >= 0 && Js::IsInternalPropertyId(propertyId))
    {
        return Js::InternalPropertyRecords::GetInternalPropertyName(propertyId);
    }

    int propertyIndex = propertyId - Js::PropertyIds::_none;

    if (propertyIndex < 0 || propertyIndex > propertyMap->GetLastIndex())
    {
        propertyIndex = 0;
    }

    const Js::PropertyRecord * propertyRecord = nullptr;
    if (locked) { propertyMap->LockResize(); }
    bool found = propertyMap->TryGetValueAt(propertyIndex, &propertyRecord);
    if (locked) { propertyMap->UnlockResize(); }

    AssertMsg(found && propertyRecord != nullptr, "using invalid propertyid");
    return propertyRecord;
}

void
ThreadContext::FindPropertyRecord(Js::JavascriptString *pstName, Js::PropertyRecord const ** propertyRecord)
{
    LPCWSTR psz = pstName->GetSz();
    FindPropertyRecord(psz, pstName->GetLength(), propertyRecord);
}

void
ThreadContext::FindPropertyRecord(__in LPCWSTR propertyName, __in int propertyNameLength, Js::PropertyRecord const ** propertyRecord)
{
    EnterPinnedScope((volatile void **)propertyRecord);
    *propertyRecord = FindPropertyRecord(propertyName, propertyNameLength);
    LeavePinnedScope();
}

const Js::PropertyRecord *
ThreadContext::FindPropertyRecord(const char16 * propertyName, int propertyNameLength)
{
    Js::PropertyRecord const * propertyRecord = nullptr;

    if (IsDirectPropertyName(propertyName, propertyNameLength))
    {
        propertyRecord = propertyNamesDirect[propertyName[0]];
        Assert(propertyRecord == propertyMap->LookupWithKey(Js::HashedCharacterBuffer<char16>(propertyName, propertyNameLength)));
    }
    else
    {
        propertyRecord = propertyMap->LookupWithKey(Js::HashedCharacterBuffer<char16>(propertyName, propertyNameLength));
    }

    return propertyRecord;
}

Js::PropertyRecord const *
ThreadContext::UncheckedAddPropertyId(__in LPCWSTR propertyName, __in int propertyNameLength, bool bind, bool isSymbol)
{
    return UncheckedAddPropertyId(JsUtil::CharacterBuffer<WCHAR>(propertyName, propertyNameLength), bind, isSymbol);
}

void ThreadContext::InitializePropertyMaps()
{
    Assert(this->recycler != nullptr);
    Assert(this->recyclableData != nullptr);
    Assert(this->propertyMap == nullptr);
    Assert(this->caseInvariantPropertySet == nullptr);

    try
    {
        this->propertyMap = HeapNew(PropertyMap, &HeapAllocator::Instance, TotalNumberOfBuiltInProperties + 700);

        this->recyclableData->boundPropertyStrings = RecyclerNew(this->recycler, JsUtil::List<Js::PropertyRecord const*>, this->recycler);

        memset(propertyNamesDirect, 0, 128*sizeof(Js::PropertyRecord *));

        Js::JavascriptLibrary::InitializeProperties(this);
        InitializeAdditionalProperties(this);
        //Js::JavascriptLibrary::InitializeDOMProperties(this);
    }
    catch(...)
    {
        // Initialization failed, undo what was done above. Callees that throw must clean up after themselves. The recycler will
        // be trashed, so clear members that point to recyclable memory. Stuff in 'recyclableData' will be taken care of by the
        // recycler, and the 'recyclableData' instance will be trashed as well.
        if (this->propertyMap != nullptr)
        {
            HeapDelete(this->propertyMap);
        }
        this->propertyMap = nullptr;

        this->caseInvariantPropertySet = nullptr;
        memset(propertyNamesDirect, 0, 128*sizeof(Js::PropertyRecord *));
        throw;
    }
}

void ThreadContext::UncheckedAddBuiltInPropertyId()
{
    for (int i = 0; i < _countof(builtInPropertyRecords); i++)
    {
        AddPropertyRecordInternal(builtInPropertyRecords[i]);
    }
}

bool
ThreadContext::IsDirectPropertyName(const char16 * propertyName, int propertyNameLength)
{
    return ((propertyNameLength == 1) && ((propertyName[0] & 0xFF80) == 0));
}

RecyclerWeakReference<const Js::PropertyRecord> *
ThreadContext::CreatePropertyRecordWeakRef(const Js::PropertyRecord * propertyRecord)
{
    RecyclerWeakReference<const Js::PropertyRecord> * propertyRecordWeakRef;

    if (propertyRecord->IsBound())
    {
        // Create a fake weak ref
        propertyRecordWeakRef = RecyclerNewLeaf(this->recycler, StaticPropertyRecordReference, propertyRecord);
    }
    else
    {
        propertyRecordWeakRef = recycler->CreateWeakReferenceHandle(propertyRecord);
    }

    return propertyRecordWeakRef;
}

Js::PropertyRecord const *
ThreadContext::UncheckedAddPropertyId(JsUtil::CharacterBuffer<WCHAR> const& propertyName, bool bind, bool isSymbol)
{
#if ENABLE_TTD
    if(this->TTDLog != nullptr && this->TTDLog->ShouldPerformDebugAction_SymbolCreation())
    {
        //We reload all properties that occour in the trace so they only way we get here in TTD mode is:
        //(1) if the program is creating a new symbol (which always gets a fresh id) and we should recreate it or 
        //(2) if it is forcing arguments in debug parse mode (instead of regular which we recorded in)
        if(isSymbol)
        {
            Js::PropertyId propertyId = Js::Constants::NoProperty;
            this->TTDLog->ReplaySymbolCreationEvent(&propertyId);

            //Don't recreate the symbol below, instead return the known symbol by looking up on the pid
            const Js::PropertyRecord* res = this->GetPropertyName(propertyId);
            AssertMsg(res != nullptr, "This should never happen!!!");

            return res;
        }
    }
#endif

    this->propertyMap->EnsureCapacity();

    // Automatically bind direct (single-character) property names, so that they can be
    // stored in the direct property table
    if (IsDirectPropertyName(propertyName.GetBuffer(), propertyName.GetLength()))
    {
        bind = true;
    }

    // Create the PropertyRecord

    int length = propertyName.GetLength();
    uint bytelength = sizeof(char16) * length;

    uint32 indexVal = 0;

    // Symbol properties cannot be numeric since their description is not to be used!
    bool isNumeric = !isSymbol && Js::PropertyRecord::IsPropertyNameNumeric(propertyName.GetBuffer(), propertyName.GetLength(), &indexVal);

    uint hash = JsUtil::CharacterBuffer<WCHAR>::StaticGetHashCode(propertyName.GetBuffer(), propertyName.GetLength());

    size_t allocLength = bytelength + sizeof(char16) + (isNumeric ? sizeof(uint32) : 0);

    // If it's bound, create it in the thread arena, along with a fake weak ref
    Js::PropertyRecord * propertyRecord;
    if (bind)
    {
        propertyRecord = AnewPlus(GetThreadAlloc(), allocLength, Js::PropertyRecord, bytelength, isNumeric, hash, isSymbol);
        propertyRecord->isBound = true;
    }
    else
    {
        propertyRecord = RecyclerNewFinalizedLeafPlus(recycler, allocLength, Js::PropertyRecord, bytelength, isNumeric, hash, isSymbol);
    }

    // Copy string and numeric info
    char16* buffer = (char16 *)(propertyRecord + 1);
    js_memcpy_s(buffer, bytelength, propertyName.GetBuffer(), bytelength);
    buffer[length] = _u('\0');

    if (isNumeric)
    {
        *(uint32 *)(buffer + length + 1) = indexVal;
        Assert(propertyRecord->GetNumericValue() == indexVal);
    }

    Js::PropertyId propertyId = this->GetNextPropertyId();

#if ENABLE_TTD
    if(isSymbol & (this->TTDLog != nullptr && this->TTDLog->ShouldPerformRecordAction_SymbolCreation()))
    {
        this->TTDLog->RecordSymbolCreationEvent(propertyId);
    }
#endif

    propertyRecord->pid = propertyId;

    AddPropertyRecordInternal(propertyRecord);

    return propertyRecord;
}

void
ThreadContext::AddPropertyRecordInternal(const Js::PropertyRecord * propertyRecord)
{
    // At this point the PropertyRecord is constructed but not added to the map.

    const char16 * propertyName = propertyRecord->GetBuffer();
    int propertyNameLength = propertyRecord->GetLength();
    Js::PropertyId propertyId = propertyRecord->GetPropertyId();

    Assert(propertyId == GetNextPropertyId());
    Assert(!IsActivePropertyId(propertyId));

#if DBG
    // Only Assert we can't find the property if we are not adding a symbol.
    // For a symbol, the propertyName is not used and may collide with something in the map already.
    if (!propertyRecord->IsSymbol())
    {
        Assert(FindPropertyRecord(propertyName, propertyNameLength) == nullptr);
    }
#endif

#if ENABLE_TTD
    if(this->TTDLog != nullptr)
    {
        this->TTDLog->AddPropertyRecord(propertyRecord);
    }
#endif

    // Add to the map
    propertyMap->Add(propertyRecord);

    PropertyRecordTrace(_u("Added property '%s' at 0x%08x, pid = %d\n"), propertyName, propertyRecord, propertyId);

    // Do not store the pid for symbols in the direct property name table.
    // We don't want property ids for symbols to be searchable anyway.
    if (!propertyRecord->IsSymbol() && IsDirectPropertyName(propertyName, propertyNameLength))
    {
        // Store the pids for single character properties in the propertyNamesDirect array.
        // This property record should have been created as bound by the caller.
        Assert(propertyRecord->IsBound());
        Assert(propertyNamesDirect[propertyName[0]] == nullptr);
        propertyNamesDirect[propertyName[0]] = propertyRecord;
    }

    if (caseInvariantPropertySet)
    {
        AddCaseInvariantPropertyRecord(propertyRecord);
    }

    // Check that everything was added correctly
#if DBG
    // Only Assert we can find the property if we are not adding a symbol.
    // For a symbol, the propertyName is not used and we won't be able to look the pid up via name.
    if (!propertyRecord->IsSymbol())
    {
        Assert(FindPropertyRecord(propertyName, propertyNameLength) == propertyRecord);
    }
    // We will still be able to lookup the symbol property by the property id, so go ahead and check that.
    Assert(GetPropertyName(propertyRecord->GetPropertyId()) == propertyRecord);
#endif
    JS_ETW(EventWriteJSCRIPT_HOSTING_PROPERTYID_LIST(propertyRecord, propertyRecord->GetBuffer()));
}

void
ThreadContext::AddCaseInvariantPropertyRecord(const Js::PropertyRecord * propertyRecord)
{
    Assert(this->caseInvariantPropertySet != nullptr);

    // Create a weak reference to the property record here (since we no longer use weak refs in the property map)
    RecyclerWeakReference<const Js::PropertyRecord> * propertyRecordWeakRef = CreatePropertyRecordWeakRef(propertyRecord);

    JsUtil::CharacterBuffer<WCHAR> newPropertyName(propertyRecord->GetBuffer(), propertyRecord->GetLength());
    Js::CaseInvariantPropertyListWithHashCode* list;
    if (!FindExistingPropertyRecord(newPropertyName, &list))
    {
        // This binds all the property string that is key in this map with no hope of reclaiming them
        // TODO: do better
        list = RecyclerNew(recycler, Js::CaseInvariantPropertyListWithHashCode, recycler, 1);
        // Do the add first so that the list is non-empty and we can calculate its hashcode correctly
        list->Add(propertyRecordWeakRef);

        // This will calculate the hashcode
        caseInvariantPropertySet->Add(list);
    }
    else
    {
        list->Add(propertyRecordWeakRef);
    }
}

void
ThreadContext::BindPropertyRecord(const Js::PropertyRecord * propertyRecord)
{
    if (!propertyRecord->IsBound())
    {
        Assert(!this->recyclableData->boundPropertyStrings->Contains(propertyRecord));

        this->recyclableData->boundPropertyStrings->Add(propertyRecord);

        // Cast around constness to set propertyRecord as bound
        const_cast<Js::PropertyRecord *>(propertyRecord)->isBound = true;
    }
}

void ThreadContext::GetOrAddPropertyId(__in LPCWSTR propertyName, __in int propertyNameLength, Js::PropertyRecord const ** propertyRecord)
{
    GetOrAddPropertyId(JsUtil::CharacterBuffer<WCHAR>(propertyName, propertyNameLength), propertyRecord);
}

void ThreadContext::GetOrAddPropertyId(JsUtil::CharacterBuffer<WCHAR> const& propertyName, Js::PropertyRecord const ** propRecord)
{
    EnterPinnedScope((volatile void **)propRecord);
    *propRecord = GetOrAddPropertyRecord(propertyName);
    LeavePinnedScope();
}

const Js::PropertyRecord *
ThreadContext::GetOrAddPropertyRecordImpl(JsUtil::CharacterBuffer<char16> propertyName, bool bind)
{
    // Make sure the recyclers around so that we can take weak references to the property strings
    EnsureRecycler();

    const Js::PropertyRecord * propertyRecord;
    FindPropertyRecord(propertyName.GetBuffer(), propertyName.GetLength(), &propertyRecord);

    if (propertyRecord == nullptr)
    {
        propertyRecord = UncheckedAddPropertyId(propertyName, bind);
    }
    else
    {
        // PropertyRecord exists, but may not be bound.  Bind now if requested.
        if (bind)
        {
            BindPropertyRecord(propertyRecord);
        }
    }

    Assert(propertyRecord != nullptr);
    Assert(!bind || propertyRecord->IsBound());

    return propertyRecord;
}

void ThreadContext::AddBuiltInPropertyRecord(const Js::PropertyRecord *propertyRecord)
{
    this->AddPropertyRecordInternal(propertyRecord);
}

BOOL ThreadContext::IsNumericPropertyId(Js::PropertyId propertyId, uint32* value)
{
    Js::PropertyRecord const * propertyRecord = this->GetPropertyName(propertyId);
    Assert(propertyRecord != nullptr);
    if (propertyRecord == nullptr || !propertyRecord->IsNumeric())
    {
        return false;
    }
    *value = propertyRecord->GetNumericValue();
    return true;
}

bool ThreadContext::IsActivePropertyId(Js::PropertyId pid)
{
    Assert(pid != Js::Constants::NoProperty);
    if (Js::IsInternalPropertyId(pid))
    {
        return true;
    }

    int propertyIndex = pid - Js::PropertyIds::_none;

    const Js::PropertyRecord * propertyRecord;
    if (propertyMap->TryGetValueAt(propertyIndex, &propertyRecord) && propertyRecord != nullptr)
    {
        return true;
    }

    return false;
}

void ThreadContext::InvalidatePropertyRecord(const Js::PropertyRecord * propertyRecord)
{
    InternalInvalidateProtoTypePropertyCaches(propertyRecord->GetPropertyId());     // use the internal version so we don't check for active property id

    this->propertyMap->Remove(propertyRecord);

    PropertyRecordTrace(_u("Reclaimed property '%s' at 0x%08x, pid = %d\n"),
        propertyRecord->GetBuffer(), propertyRecord, propertyRecord->GetPropertyId());
}

Js::PropertyId ThreadContext::GetNextPropertyId()
{
    return this->propertyMap->GetNextIndex() + Js::PropertyIds::_none;
}

Js::PropertyId ThreadContext::GetMaxPropertyId()
{
    auto maxPropertyId = this->propertyMap->Count() + Js::InternalPropertyIds::Count;
    return maxPropertyId;
}

void ThreadContext::CreateNoCasePropertyMap()
{
    Assert(caseInvariantPropertySet == nullptr);
    caseInvariantPropertySet = RecyclerNew(recycler, PropertyNoCaseSetType, recycler, 173);

    // Prevent the set from being reclaimed
    // Individual items in the set can be reclaimed though since they're lists of weak references
    // The lists themselves can be reclaimed when all the weak references in them are cleared
    this->recyclableData->caseInvariantPropertySet = caseInvariantPropertySet;

    // Note that we are allocating from the recycler below, so we may cause a GC at any time, which
    // could cause PropertyRecords to be collected and removed from the propertyMap.
    // Thus, don't use BaseDictionary::Map here, as it cannot tolerate changes while mapping.
    // Instead, walk the PropertyRecord entries in index order.  This will work even if a GC occurs.

    for (int propertyIndex = 0; propertyIndex <= this->propertyMap->GetLastIndex(); propertyIndex++)
    {
        const Js::PropertyRecord * propertyRecord;
        if (this->propertyMap->TryGetValueAt(propertyIndex, &propertyRecord) && propertyRecord != nullptr)
        {
            AddCaseInvariantPropertyRecord(propertyRecord);
        }
    }
}

JsUtil::List<const RecyclerWeakReference<Js::PropertyRecord const>*>*
ThreadContext::FindPropertyIdNoCase(Js::ScriptContext * scriptContext, LPCWSTR propertyName, int propertyNameLength)
{
    return ThreadContext::FindPropertyIdNoCase(scriptContext, JsUtil::CharacterBuffer<WCHAR>(propertyName,  propertyNameLength));
}

JsUtil::List<const RecyclerWeakReference<Js::PropertyRecord const>*>*
ThreadContext::FindPropertyIdNoCase(Js::ScriptContext * scriptContext, JsUtil::CharacterBuffer<WCHAR> const& propertyName)
{
    if (caseInvariantPropertySet == nullptr)
    {
        this->CreateNoCasePropertyMap();
    }
    Js::CaseInvariantPropertyListWithHashCode* list;
    if (FindExistingPropertyRecord(propertyName, &list))
    {
        return list;
    }
    return nullptr;
}

bool
ThreadContext::FindExistingPropertyRecord(_In_ JsUtil::CharacterBuffer<WCHAR> const& propertyName, Js::CaseInvariantPropertyListWithHashCode** list)
{
    Js::CaseInvariantPropertyListWithHashCode* l = this->caseInvariantPropertySet->LookupWithKey(propertyName);

    (*list) = l;

    return (l != nullptr);
}

void ThreadContext::CleanNoCasePropertyMap()
{
    if (this->caseInvariantPropertySet != nullptr)
    {
        this->caseInvariantPropertySet->MapAndRemoveIf([](Js::CaseInvariantPropertyListWithHashCode* value) -> bool {
            if (value && value->Count() == 0)
            {
                // Remove entry
                return true;
            }

            // Keep entry
            return false;
        });
    }
}

void
ThreadContext::ForceCleanPropertyMap()
{
    // No-op now that we no longer use weak refs
}

#if ENABLE_NATIVE_CODEGEN
JsUtil::JobProcessor *
ThreadContext::GetJobProcessor()
{
    if(bgJit && isOptimizedForManyInstances)
    {
        return ThreadBoundThreadContextManager::GetSharedJobProcessor();
    }

    if (!jobProcessor)
    {
        if(bgJit && !isOptimizedForManyInstances)
        {
            jobProcessor = HeapNew(JsUtil::BackgroundJobProcessor, GetAllocationPolicyManager(), &threadService, false /*disableParallelThreads*/);
        }
        else
        {
            jobProcessor = HeapNew(JsUtil::ForegroundJobProcessor);
        }
    }
    return jobProcessor;
}
#endif

void
ThreadContext::RegisterCodeGenRecyclableData(Js::CodeGenRecyclableData *const codeGenRecyclableData)
{
    Assert(codeGenRecyclableData);
    Assert(recyclableData);

    // Linking must not be done concurrently with unlinking (caller must use lock)
    recyclableData->codeGenRecyclableDatas.LinkToEnd(codeGenRecyclableData);
}

void
ThreadContext::UnregisterCodeGenRecyclableData(Js::CodeGenRecyclableData *const codeGenRecyclableData)
{
    Assert(codeGenRecyclableData);

    if(!recyclableData)
    {
        // The thread context's recyclable data may have already been released to the recycler if we're shutting down
        return;
    }

    // Unlinking may be done from a background thread, but not concurrently with linking (caller must use lock).  Partial unlink
    // does not zero the previous and next links for the unlinked node so that the recycler can scan through the node from the
    // main thread.
    recyclableData->codeGenRecyclableDatas.UnlinkPartial(codeGenRecyclableData);
}



uint
ThreadContext::EnterScriptStart(Js::ScriptEntryExitRecord * record, bool doCleanup)
{
    Recycler * recycler = this->GetRecycler();
    Assert(recycler->IsReentrantState());
    JS_ETW(EventWriteJSCRIPT_RUN_START(this,0));

    // Increment the callRootLevel early so that Dispose ran during FinishConcurrent will not close the current scriptContext
    uint oldCallRootLevel = this->callRootLevel++;

    if (oldCallRootLevel == 0)
    {
        Assert(!this->hasThrownPendingException);
        RECORD_TIMESTAMP(lastScriptStartTime);
        InterruptPoller *poller = this->interruptPoller;
        if (poller)
        {
            poller->StartScript();
        }

        recycler->SetIsInScript(true);
        if (doCleanup)
        {
            recycler->EnterIdleDecommit();
#if ENABLE_CONCURRENT_GC
            recycler->FinishConcurrent<FinishConcurrentOnEnterScript>();
#endif
            if (threadServiceWrapper == NULL)
            {
                // Reschedule the next collection at the start of the script.
                recycler->ScheduleNextCollection();
            }
        }
    }

    this->PushEntryExitRecord(record);

    AssertMsg(!this->IsScriptActive(),
              "Missing EnterScriptEnd or LeaveScriptStart");
    this->isScriptActive = true;
    recycler->SetIsScriptActive(true);

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::RunPhase))
    {
        Output::Trace(Js::RunPhase, _u("%p> EnterScriptStart(%p): Level %d\n"), ::GetCurrentThreadId(), this, this->callRootLevel);
        Output::Flush();
    }
#endif

    return oldCallRootLevel;
}


void
ThreadContext::EnterScriptEnd(Js::ScriptEntryExitRecord * record, bool doCleanup)
{
#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::RunPhase))
    {
        Output::Trace(Js::RunPhase, _u("%p> EnterScriptEnd  (%p): Level %d\n"), ::GetCurrentThreadId(), this, this->callRootLevel);
        Output::Flush();
    }
#endif
    this->PopEntryExitRecord(record);
    AssertMsg(this->IsScriptActive(),
              "Missing EnterScriptStart or LeaveScriptEnd");
    this->isScriptActive = false;
    this->GetRecycler()->SetIsScriptActive(false);
    this->callRootLevel--;
#ifdef EXCEPTION_CHECK
    ExceptionCheck::SetHandledExceptionType(record->handledExceptionType);
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    recycler->Verify(Js::RunPhase);
#endif

    if (this->callRootLevel == 0)
    {
        RECORD_TIMESTAMP(lastScriptEndTime);
        this->GetRecycler()->SetIsInScript(false);
        InterruptPoller *poller = this->interruptPoller;
        if (poller)
        {
            poller->EndScript();
        }
        ClosePendingProjectionContexts();
        ClosePendingScriptContexts();
        Assert(rootPendingClose == nullptr);

        if (this->hasThrownPendingException)
        {
            // We have some cases where the thread instant of JavascriptExceptionObject
            // are ignored and not clear. To avoid leaks, clear it here when
            // we are not in script, where no one should be using these JavascriptExceptionObject
            this->ClearPendingOOMError();
            this->ClearPendingSOError();
            this->hasThrownPendingException = false;
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.FreeRejittedCode)
#endif
        {
            // Since we're no longer in script, old entry points can now be collected
            Js::FunctionEntryPointInfo* current = this->recyclableData->oldEntryPointInfo;
            this->recyclableData->oldEntryPointInfo = nullptr;

            // Clear out the next pointers so older entry points wont be held on
            // as a false positive

            while (current != nullptr)
            {
                Js::FunctionEntryPointInfo* next = current->nextEntryPoint;
                current->nextEntryPoint = nullptr;
                current = next;
            }
        }

        if (doCleanup)
        {
            ThreadServiceWrapper* threadServiceWrapper = GetThreadServiceWrapper();
            if (!threadServiceWrapper || !threadServiceWrapper->ScheduleNextCollectOnExit())
            {
                // Do the idle GC now if we fail schedule one.
                recycler->CollectNow<CollectOnScriptExit>();
            }
            recycler->LeaveIdleDecommit();
        }
    }

    if (doCleanup)
    {
        PHASE_PRINT_TRACE1(Js::DisposePhase, _u("[Dispose] NeedDispose in EnterScriptEnd: %d\n"), this->recycler->NeedDispose());

        if (this->recycler->NeedDispose())
        {
            this->recycler->FinishDisposeObjectsNow<FinishDispose>();
        }
    }

    JS_ETW(EventWriteJSCRIPT_RUN_STOP(this,0));
}

void
ThreadContext::SetForceOneIdleCollection()
{
    ThreadServiceWrapper* threadServiceWrapper = GetThreadServiceWrapper();
    if (threadServiceWrapper)
    {
        threadServiceWrapper->SetForceOneIdleCollection();
    }

}

BOOLEAN
ThreadContext::IsOnStack(void const *ptr)
{
#if defined(_M_IX86) && defined(_MSC_VER)
    return ptr < (void*)__readfsdword(0x4) && ptr >= (void*)__readfsdword(0xE0C);
#elif defined(_M_AMD64) && defined(_MSC_VER)
    return ptr < (void*)__readgsqword(0x8) && ptr >= (void*)__readgsqword(0x1478);
#elif defined(_M_ARM)
    ULONG lowLimit, highLimit;
    ::GetCurrentThreadStackLimits(&lowLimit, &highLimit);
    bool isOnStack = (void*)lowLimit <= ptr && ptr < (void*)highLimit;
    return isOnStack;
#elif defined(_M_ARM64)
    ULONG64 lowLimit, highLimit;
    ::GetCurrentThreadStackLimits(&lowLimit, &highLimit);
    bool isOnStack = (void*)lowLimit <= ptr && ptr < (void*)highLimit;
    return isOnStack;
#elif !defined(_MSC_VER)
    return ::IsAddressOnStack((ULONG_PTR) ptr);
#else
    AssertMsg(FALSE, "IsOnStack -- not implemented yet case");
    Js::Throw::NotImplemented();
    return false;
#endif
}

 PBYTE
 ThreadContext::GetStackLimitForCurrentThread() const
{
    FAULTINJECT_SCRIPT_TERMINATION;
    PBYTE limit = this->stackLimitForCurrentThread;
    Assert(limit == Js::Constants::StackLimitForScriptInterrupt
        || !this->GetStackProber()
        || limit == this->GetStackProber()->GetScriptStackLimit());
    return limit;
}
void
ThreadContext::SetStackLimitForCurrentThread(PBYTE limit)
{
    this->stackLimitForCurrentThread = limit;
}

_NOINLINE //Win8 947081: might use wrong _AddressOfReturnAddress() if this and caller are inlined
bool
ThreadContext::IsStackAvailable(size_t size)
{
    PBYTE sp = (PBYTE)_AddressOfReturnAddress();
    PBYTE stackLimit = this->GetStackLimitForCurrentThread();
    bool stackAvailable = ((size_t)sp > size && (sp - size) > stackLimit);

    // Verify that JIT'd frames didn't mess up the ABI stack alignment
    Assert(((uintptr_t)sp & (AutoSystemInfo::StackAlign - 1)) == (sizeof(void*) & (AutoSystemInfo::StackAlign - 1)));

#if DBG
    this->GetStackProber()->AdjustKnownStackLimit(sp, size);
#endif

    FAULTINJECT_STACK_PROBE
    if (stackAvailable)
    {
        return true;
    }

    if (sp <= stackLimit)
    {
        if (stackLimit == Js::Constants::StackLimitForScriptInterrupt)
        {
            if (sp <= this->GetStackProber()->GetScriptStackLimit())
            {
                // Take down the process if we cant recover from the stack overflow
                Js::Throw::FatalInternalError();
            }
        }
    }

    return false;
}

_NOINLINE //Win8 947081: might use wrong _AddressOfReturnAddress() if this and caller are inlined
bool
ThreadContext::IsStackAvailableNoThrow(size_t size)
{
    PBYTE sp = (PBYTE)_AddressOfReturnAddress();
    PBYTE stackLimit = this->GetStackLimitForCurrentThread();
    bool stackAvailable = (sp > stackLimit) && ((size_t)sp > size) && ((sp - size) > stackLimit);

    FAULTINJECT_STACK_PROBE

    return stackAvailable;
}

/* static */ bool
ThreadContext::IsCurrentStackAvailable(size_t size)
{
    ThreadContext *currentContext = GetContextForCurrentThread();
    Assert(currentContext);

    return currentContext->IsStackAvailable(size);
}

/*
    returnAddress will be passed in the stackprobe call at the beginning of interpreter frame.
    We need to probe the stack before we link up the InterpreterFrame structure in threadcontext,
    and if we throw there, the stack walker might get confused when trying to identify a frame
    is interpreter frame by comparing the current ebp in ebp chain with return address specified
    in the last InterpreterFrame linked in threadcontext. We need to pass in the return address
    of the probing frame to skip the right one (we need to skip first match in a->a->a recursion,
    but not in a->b->a recursion).
*/
void
ThreadContext::ProbeStackNoDispose(size_t size, Js::ScriptContext *scriptContext, PVOID returnAddress)
{
    AssertCanHandleStackOverflow();
    if (!this->IsStackAvailable(size))
    {
        if (this->IsExecutionDisabled())
        {
            // The probe failed because we hammered the stack limit to trigger script interrupt.
            Assert(this->DoInterruptProbe());
            throw Js::ScriptAbortException();
        }

        Js::Throw::StackOverflow(scriptContext, returnAddress);
    }

    // Use every Nth stack probe as a QC trigger.
    if (AutoSystemInfo::ShouldQCMoreFrequently() && this->HasInterruptPoller() && this->IsScriptActive())
    {
        ++(this->stackProbeCount);
        if (this->stackProbeCount > ThreadContext::StackProbePollThreshold)
        {
            this->stackProbeCount = 0;
            this->CheckInterruptPoll();
        }
    }
}

void
ThreadContext::ProbeStack(size_t size, Js::ScriptContext *scriptContext, PVOID returnAddress)
{
    this->ProbeStackNoDispose(size, scriptContext, returnAddress);

    // BACKGROUND-GC TODO: If we're stuck purely in JITted code, we should have the
    // background GC thread modify the threads stack limit to trigger the runtime stack probe
    if (this->callDispose && this->recycler->NeedDispose())
    {
        PHASE_PRINT_TRACE1(Js::DisposePhase, _u("[Dispose] NeedDispose in ProbeStack: %d\n"), this->recycler->NeedDispose());
        this->recycler->FinishDisposeObjectsNow<FinishDisposeTimed>();
    }
}

void
ThreadContext::ProbeStack(size_t size, Js::RecyclableObject * obj, Js::ScriptContext *scriptContext)
{
    AssertCanHandleStackOverflowCall(obj->IsExternal() ||
        (Js::JavascriptOperators::GetTypeId(obj) == Js::TypeIds_Function &&
        Js::JavascriptFunction::FromVar(obj)->IsExternalFunction()));
    if (!this->IsStackAvailable(size))
    {
        if (this->IsExecutionDisabled())
        {
            // The probe failed because we hammered the stack limit to trigger script interrupt.
            Assert(this->DoInterruptProbe());
            throw Js::ScriptAbortException();
        }

        if (obj->IsExternal() ||
            (Js::JavascriptOperators::GetTypeId(obj) == Js::TypeIds_Function &&
            Js::JavascriptFunction::FromVar(obj)->IsExternalFunction()))
        {
            Js::JavascriptError::ThrowStackOverflowError(scriptContext);
        }
        Js::Throw::StackOverflow(scriptContext, NULL);
    }

}

void
ThreadContext::ProbeStack(size_t size)
{
    Assert(this->IsScriptActive());

    Js::ScriptEntryExitRecord *entryExitRecord = this->GetScriptEntryExit();
    Assert(entryExitRecord);

    Js::ScriptContext *scriptContext = entryExitRecord->scriptContext;
    Assert(scriptContext);

    this->ProbeStack(size, scriptContext);
}

/* static */ void
ThreadContext::ProbeCurrentStack(size_t size, Js::ScriptContext *scriptContext)
{
    Assert(scriptContext != nullptr);
    Assert(scriptContext->GetThreadContext() == GetContextForCurrentThread());
    scriptContext->GetThreadContext()->ProbeStack(size, scriptContext);
}

/* static */ void
ThreadContext::ProbeCurrentStackNoDispose(size_t size, Js::ScriptContext *scriptContext)
{
    Assert(scriptContext != nullptr);
    Assert(scriptContext->GetThreadContext() == GetContextForCurrentThread());
    scriptContext->GetThreadContext()->ProbeStackNoDispose(size, scriptContext);
}

template <bool leaveForHost>
void
ThreadContext::LeaveScriptStart(void * frameAddress)
{
    Assert(this->IsScriptActive());

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::RunPhase))
    {
        Output::Trace(Js::RunPhase, _u("%p> LeaveScriptStart(%p): Level %d\n"), ::GetCurrentThreadId(), this, this->callRootLevel);
        Output::Flush();
    }
#endif

    Js::ScriptEntryExitRecord * entryExitRecord = this->GetScriptEntryExit();

    AssertMsg(entryExitRecord && entryExitRecord->frameIdOfScriptExitFunction == nullptr,
              "Missing LeaveScriptEnd or EnterScriptStart");

    entryExitRecord->frameIdOfScriptExitFunction = frameAddress;

    this->isScriptActive = false;
    this->GetRecycler()->SetIsScriptActive(false);

    AssertMsg(!(leaveForHost && this->IsDisableImplicitCall()),
        "Disable implicit call should have been caught before leaving script for host");

    // Save the implicit call flags
    entryExitRecord->savedImplicitCallFlags = this->GetImplicitCallFlags();

    // clear the hasReentered to detect if we have reentered into script
    entryExitRecord->hasReentered = false;
#if DBG || defined(PROFILE_EXEC)
    entryExitRecord->leaveForHost = leaveForHost;
#endif
#if DBG
    entryExitRecord->leaveForAsyncHostOperation = false;
#endif

#ifdef PROFILE_EXEC
    if (leaveForHost)
    {
        entryExitRecord->scriptContext->ProfileEnd(Js::RunPhase);
    }
#endif
}

void ThreadContext::DisposeOnLeaveScript()
{
    PHASE_PRINT_TRACE1(Js::DisposePhase, _u("[Dispose] NeedDispose in LeaveScriptStart: %d\n"), this->recycler->NeedDispose());

    if (this->callDispose && this->recycler->NeedDispose())
    {
        this->recycler->FinishDisposeObjectsNow<FinishDispose>();
    }
}


template <bool leaveForHost>
void
ThreadContext::LeaveScriptEnd(void * frameAddress)
{
    Assert(!this->IsScriptActive());

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::RunPhase))
    {
        Output::Trace(Js::RunPhase, _u("%p> LeaveScriptEnd(%p): Level %d\n"), ::GetCurrentThreadId(), this, this->callRootLevel);
        Output::Flush();
    }
#endif

    Js::ScriptEntryExitRecord * entryExitRecord = this->GetScriptEntryExit();

    AssertMsg(entryExitRecord && entryExitRecord->frameIdOfScriptExitFunction,
              "LeaveScriptEnd without LeaveScriptStart");
    AssertMsg(frameAddress == nullptr || frameAddress == entryExitRecord->frameIdOfScriptExitFunction,
              "Mismatched script exit frames");
    Assert(!!entryExitRecord->leaveForHost == leaveForHost);

    entryExitRecord->frameIdOfScriptExitFunction = nullptr;

    AssertMsg(!this->IsScriptActive(), "Missing LeaveScriptStart or LeaveScriptStart");
    this->isScriptActive = true;
    this->GetRecycler()->SetIsScriptActive(true);

    Js::ImplicitCallFlags savedImplicitCallFlags = entryExitRecord->savedImplicitCallFlags;
    if (leaveForHost)
    {
        savedImplicitCallFlags = (Js::ImplicitCallFlags)(savedImplicitCallFlags | Js::ImplicitCall_External);
    }
    else if (entryExitRecord->hasReentered)
    {
        savedImplicitCallFlags = (Js::ImplicitCallFlags)(savedImplicitCallFlags | Js::ImplicitCall_AsyncHostOperation);
    }
    // Restore the implicit call flags
    this->SetImplicitCallFlags(savedImplicitCallFlags);

#ifdef PROFILE_EXEC
    if (leaveForHost)
    {
        entryExitRecord->scriptContext->ProfileBegin(Js::RunPhase);
    }
#endif
}

// explicit instantiations
template void ThreadContext::LeaveScriptStart<true>(void * frameAddress);
template void ThreadContext::LeaveScriptStart<false>(void * frameAddress);
template void ThreadContext::LeaveScriptEnd<true>(void * frameAddress);
template void ThreadContext::LeaveScriptEnd<false>(void * frameAddress);

void
ThreadContext::PushInterpreterFrame(Js::InterpreterStackFrame *interpreterFrame)
{
    interpreterFrame->SetPreviousFrame(this->leafInterpreterFrame);
    this->leafInterpreterFrame = interpreterFrame;
}

Js::InterpreterStackFrame *
ThreadContext::PopInterpreterFrame()
{
    Js::InterpreterStackFrame *interpreterFrame = this->leafInterpreterFrame;
    Assert(interpreterFrame);
    this->leafInterpreterFrame = interpreterFrame->GetPreviousFrame();
    return interpreterFrame;
}

BOOL
ThreadContext::ExecuteRecyclerCollectionFunctionCommon(Recycler * recycler, CollectionFunction function, CollectionFlags flags)
{
    return  __super::ExecuteRecyclerCollectionFunction(recycler, function, flags);
}

#if DBG
bool
ThreadContext::IsInAsyncHostOperation() const
{
    if (!this->IsScriptActive())
    {
        Js::ScriptEntryExitRecord * lastRecord = this->entryExitRecord;
        if (lastRecord != NULL)
        {
            return !!lastRecord->leaveForAsyncHostOperation;
        }
    }
    return false;
}
#endif

#if ENABLE_TTD
void ThreadContext::InitTimeTravel(bool doRecord, bool doReplay)
{
    AssertMsg(this->TTDLog == nullptr, "We should only init once.");
    AssertMsg((doRecord & !doReplay) || (!doRecord && doReplay), "Should be exactly 1 of record or replay.");

    this->TTDLog = HeapNewNoThrow(TTD::EventLog, this);

    if(doRecord)
    {
        this->TTDLog->InitForTTDRecord();
    }

    if(doReplay)
    {
        this->TTDLog->InitForTTDReplay();
    }
}

void ThreadContext::BeginCtxTimeTravel(Js::ScriptContext* ctx, const HostScriptContextCallbackFunctor& callbackFunctor)
{
    AssertMsg(!ctx->IsTTDDetached(), "We don't want to run time travel on multiple contexts yet.");

    this->TTDLog->StartTimeTravelOnScript(ctx, callbackFunctor);
}

void ThreadContext::EndCtxTimeTravel(Js::ScriptContext* ctx)
{
    AssertMsg(!ctx->IsTTDDetached(), "We don't want to run time travel on multiple contexts yet.");

    this->TTDLog->SetGlobalMode(TTD::TTDMode::Detached);
    this->TTDLog->StopTimeTravelOnScript(ctx);
}

void ThreadContext::EmitTTDLogIfNeeded()
{
    this->TTDLog->EmitLogIfNeeded();
}
#endif

BOOL
ThreadContext::ExecuteRecyclerCollectionFunction(Recycler * recycler, CollectionFunction function, CollectionFlags flags)
{
    // If the thread context doesn't have an associated Recycler set, don't do anything
    if (this->recycler == nullptr)
    {
        return FALSE;
    }

    // Take etw rundown lock on this thread context. We can't collect entryPoints if we are in etw rundown.
    AutoCriticalSection autocs(this->GetEtwRundownCriticalSection());

    // Disable calling dispose from leave script or the stack probe
    // while we're executing the recycler wrapper
    AutoRestoreValue<bool> callDispose(&this->callDispose, false);

    BOOL ret = FALSE;

    if (!this->IsScriptActive())
    {
        Assert(!this->IsDisableImplicitCall() || this->IsInAsyncHostOperation());
        ret = this->ExecuteRecyclerCollectionFunctionCommon(recycler, function, flags);

        // Make sure that we finish a collect that is activated outside of script, since
        // we won't have exit script to schedule it
        if (!this->IsInScript() && recycler->CollectionInProgress()
            && ((flags & CollectOverride_DisableIdleFinish) == 0) && threadServiceWrapper)
        {
            threadServiceWrapper->ScheduleFinishConcurrent();
        }
    }
    else
    {
        void * frameAddr = nullptr;
        GET_CURRENT_FRAME_ID(frameAddr);

        // We may need stack to call out from Dispose or QC
        if (!this->IsDisableImplicitCall()) // otherwise Dispose/QC disabled
        {
            // If we don't have stack space to call out from Dispose or QC,
            // don't throw, simply return false. This gives SnailAlloc a better
            // chance of allocating in low stack-space situations (like allocating
            // a StackOverflowException object)
            if (!this->IsStackAvailableNoThrow(Js::Constants::MinStackCallout))
            {
                return false;
            }
        }

#if ENABLE_TTD
        //
        //TODO: We leak any references that are JsReleased by the host in collection callbacks. Later we should defer these events to the end of the 
        //      top-level call or the next external call and then append them to the log.
        //

        bool preventRecording = (this->entryExitRecord != nullptr) && this->entryExitRecord->scriptContext->ShouldPerformRecordAction();
        if(preventRecording)
        {
            this->TTDLog->PushMode(TTD::TTDMode::ExcludedExecution);
        }
#endif

        this->LeaveScriptStart<false>(frameAddr);
        ret = this->ExecuteRecyclerCollectionFunctionCommon(recycler, function, flags);
        this->LeaveScriptEnd<false>(frameAddr);

#if ENABLE_TTD
        if(preventRecording)
        {
            this->TTDLog->PopMode(TTD::TTDMode::ExcludedExecution);
        }
#endif

        if (this->callRootLevel != 0)
        {
            this->CheckScriptInterrupt();
        }
    }

    return ret;
}

void
ThreadContext::DisposeObjects(Recycler * recycler)
{
    if(this->IsDisableImplicitCall())
    {
        // Don't dispose objects when implicit calls are disabled, since disposing may cause implicit calls. Objects will remain
        // in the dispose queue and will be disposed later when implicit calls are not disabled.
        return;
    }

    // we shouldn't dispose in noscriptscope as it might lead to script execution.
    Assert(!this->IsNoScriptScope());

    if (!this->IsScriptActive())
    {
        __super::DisposeObjects(recycler);
    }
    else
    {
        void * frameAddr = nullptr;
        GET_CURRENT_FRAME_ID(frameAddr);

        // We may need stack to call out from Dispose
        this->ProbeStack(Js::Constants::MinStackCallout);

        this->LeaveScriptStart<false>(frameAddr);
        __super::DisposeObjects(recycler);
        this->LeaveScriptEnd<false>(frameAddr);
    }
}

void
ThreadContext::PushEntryExitRecord(Js::ScriptEntryExitRecord * record)
{
    AssertMsg(record, "Didn't provide a script entry record to push");
    Assert(record->next == nullptr);


    Js::ScriptEntryExitRecord * lastRecord = this->entryExitRecord;
    if (lastRecord != nullptr)
    {
        // If we enter script again, we should have leave with leaveForHost or leave for dispose.
        Assert(lastRecord->leaveForHost || lastRecord->leaveForAsyncHostOperation);
        lastRecord->hasReentered = true;
        record->next = lastRecord;

        // these are on stack, which grows down. if this condition doesn't hold, then the list somehow got messed up
        if (!IsOnStack(lastRecord) || (uintptr_t)record >= (uintptr_t)lastRecord)
        {
            EntryExitRecord_Corrupted_fatal_error();
        }
    }

    this->entryExitRecord = record;
}

void ThreadContext::PopEntryExitRecord(Js::ScriptEntryExitRecord * record)
{
    AssertMsg(record && record == this->entryExitRecord, "Mismatch script entry/exit");

    // these are on stack, which grows down. if this condition doesn't hold, then the list somehow got messed up
    Js::ScriptEntryExitRecord * next = this->entryExitRecord->next;
    if (next && (!IsOnStack(next) || (uintptr_t)this->entryExitRecord >= (uintptr_t)next))
    {
        EntryExitRecord_Corrupted_fatal_error();
    }

    this->entryExitRecord = next;
}

BOOL ThreadContext::ReserveStaticTypeIds(__in int first, __in int last)
{
    if ( nextTypeId <= first )
    {
        nextTypeId = (Js::TypeId) last;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

Js::TypeId ThreadContext::ReserveTypeIds(int count)
{
    Js::TypeId firstTypeId = nextTypeId;
    nextTypeId = (Js::TypeId)(nextTypeId + count);
    return firstTypeId;
}

Js::TypeId ThreadContext::CreateTypeId()
{
    return nextTypeId = (Js::TypeId)(nextTypeId + 1);
}

WellKnownHostType ThreadContext::GetWellKnownHostType(Js::TypeId typeId)
{
    if (this->wellKnownHostTypeHTMLAllCollectionTypeId == typeId)
    {
        return WellKnownHostType_HTMLAllCollection;
    }

    return WellKnownHostType_Invalid;
}

void ThreadContext::SetWellKnownHostTypeId(WellKnownHostType wellKnownType, Js::TypeId typeId)
{
    AssertMsg(WellKnownHostType_HTMLAllCollection == wellKnownType, "ThreadContext::SetWellKnownHostTypeId called on type other than HTMLAllCollection");

    if (WellKnownHostType_HTMLAllCollection == wellKnownType)
    {
        this->wellKnownHostTypeHTMLAllCollectionTypeId = typeId;
    }
}

bool
ThreadContext::CanBeFalsy(Js::TypeId typeId)
{
    // Declare all the type ID that can be falsy so we can avoid the falsy check in the JIT
    return typeId == this->wellKnownHostTypeHTMLAllCollectionTypeId;
}

void ThreadContext::EnsureDebugManager()
{
    if (this->debugManager == nullptr)
    {
        this->debugManager = HeapNew(Js::DebugManager, this, this->GetAllocationPolicyManager());
    }
    InterlockedIncrement(&crefSContextForDiag);
    Assert(this->debugManager != nullptr);
}

void ThreadContext::ReleaseDebugManager()
{
    Assert(crefSContextForDiag > 0);
    Assert(this->debugManager != nullptr);

    LONG lref = InterlockedDecrement(&crefSContextForDiag);

    if (lref == 0)
    {
        if (this->recyclableData != nullptr)
        {
            this->recyclableData->returnedValueList = nullptr;
        }

        if (this->debugManager != nullptr)
        {
            this->debugManager->Close();
            HeapDelete(this->debugManager);
            this->debugManager = nullptr;
        }
    }
}



Js::TempArenaAllocatorObject *
ThreadContext::GetTemporaryAllocator(LPCWSTR name)
{
    AssertCanHandleOutOfMemory();

    if (temporaryArenaAllocatorCount != 0)
    {
        temporaryArenaAllocatorCount--;
        Js::TempArenaAllocatorObject * allocator = recyclableData->temporaryArenaAllocators[temporaryArenaAllocatorCount];
        recyclableData->temporaryArenaAllocators[temporaryArenaAllocatorCount] = nullptr;
        return allocator;
    }

    return Js::TempArenaAllocatorObject::Create(this);
}

void
ThreadContext::ReleaseTemporaryAllocator(Js::TempArenaAllocatorObject * tempAllocator)
{
    if (temporaryArenaAllocatorCount < MaxTemporaryArenaAllocators)
    {
        tempAllocator->GetAllocator()->Reset();
        recyclableData->temporaryArenaAllocators[temporaryArenaAllocatorCount] = tempAllocator;
        temporaryArenaAllocatorCount++;
        return;
    }

    tempAllocator->Dispose(false);
}


Js::TempGuestArenaAllocatorObject *
ThreadContext::GetTemporaryGuestAllocator(LPCWSTR name)
{
    AssertCanHandleOutOfMemory();

    if (temporaryGuestArenaAllocatorCount != 0)
    {
        temporaryGuestArenaAllocatorCount--;
        Js::TempGuestArenaAllocatorObject * allocator = recyclableData->temporaryGuestArenaAllocators[temporaryGuestArenaAllocatorCount];
        recyclableData->temporaryGuestArenaAllocators[temporaryGuestArenaAllocatorCount] = nullptr;
        return allocator;
    }

    return Js::TempGuestArenaAllocatorObject::Create(this);
}

void
ThreadContext::ReleaseTemporaryGuestAllocator(Js::TempGuestArenaAllocatorObject * tempGuestAllocator)
{
    if (temporaryGuestArenaAllocatorCount < MaxTemporaryArenaAllocators)
    {
        tempGuestAllocator->GetAllocator()->Reset();
        recyclableData->temporaryGuestArenaAllocators[temporaryGuestArenaAllocatorCount] = tempGuestAllocator;
        temporaryGuestArenaAllocatorCount++;
        return;
    }

    tempGuestAllocator->Dispose(false);
}

void
ThreadContext::AddToPendingScriptContextCloseList(Js::ScriptContext * scriptContext)
{
    Assert(scriptContext != nullptr);

    if (rootPendingClose == nullptr)
    {
        rootPendingClose = scriptContext;
        return;
    }

    // Prepend to the list.
    scriptContext->SetNextPendingClose(rootPendingClose);
    rootPendingClose = scriptContext;
}

void
ThreadContext::RemoveFromPendingClose(Js::ScriptContext * scriptContext)
{
    Assert(scriptContext != nullptr);

    if (rootPendingClose == nullptr)
    {
        // We already sent a close message, ignore the notification.
        return;
    }

    // Base case: The root is being removed. Move the root along.
    if (scriptContext == rootPendingClose)
    {
        rootPendingClose = rootPendingClose->GetNextPendingClose();
        return;
    }

    Js::ScriptContext * currScriptContext = rootPendingClose;
    Js::ScriptContext * nextScriptContext = nullptr;
    while (currScriptContext)
    {
        nextScriptContext = currScriptContext->GetNextPendingClose();
        if (!nextScriptContext)
        {
            break;
        }

        if (nextScriptContext == scriptContext) {
            // The next pending close ScriptContext is the one to be removed - set prev->next to next->next
            currScriptContext->SetNextPendingClose(nextScriptContext->GetNextPendingClose());
            return;
        }
        currScriptContext = nextScriptContext;
    }

    // We expect to find scriptContext in the pending close list.
    Assert(false);
}

void ThreadContext::ClosePendingScriptContexts()
{
    Js::ScriptContext * scriptContext = rootPendingClose;
    if (scriptContext == nullptr)
    {
        return;
    }
    Js::ScriptContext * nextScriptContext;
    do
    {
        nextScriptContext = scriptContext->GetNextPendingClose();
        scriptContext->Close(false);
        scriptContext = nextScriptContext;
    }
    while (scriptContext);
    rootPendingClose = nullptr;
}

void
ThreadContext::AddToPendingProjectionContextCloseList(IProjectionContext *projectionContext)
{
    pendingProjectionContextCloseList->Add(projectionContext);
}

void
ThreadContext::RemoveFromPendingClose(IProjectionContext* projectionContext)
{
    pendingProjectionContextCloseList->Remove(projectionContext);
}

void ThreadContext::ClosePendingProjectionContexts()
{
    IProjectionContext* projectionContext;
    for (int i = 0 ; i < pendingProjectionContextCloseList->Count(); i++)
    {
        projectionContext = pendingProjectionContextCloseList->Item(i);
        projectionContext->Close();
    }
    pendingProjectionContextCloseList->Clear();

}

void
ThreadContext::RegisterScriptContext(Js::ScriptContext *scriptContext)
{
    // NOTE: ETW rundown thread may be reading the scriptContextList concurrently. We don't need to
    // lock access because we only insert to the front here.

    scriptContext->next = this->scriptContextList;
    if (this->scriptContextList)
    {
        Assert(this->scriptContextList->prev == NULL);
        this->scriptContextList->prev = scriptContext;
    }
    scriptContext->prev = NULL;
    this->scriptContextList = scriptContext;

    if(NoJIT())
    {
        scriptContext->ForceNoNative();
    }
    scriptContextCount++;
    scriptContextEverRegistered = true;
}

void
ThreadContext::UnregisterScriptContext(Js::ScriptContext *scriptContext)
{
    // NOTE: ETW rundown thread may be reading the scriptContextList concurrently. Since this function
    // is only called by ~ScriptContext() which already synchronized to ETW rundown, we are safe here.

    if (scriptContext == this->scriptContextList)
    {
        Assert(scriptContext->prev == NULL);
        this->scriptContextList = scriptContext->next;
    }
    else
    {
        scriptContext->prev->next = scriptContext->next;
    }

    if (scriptContext->next)
    {
        scriptContext->next->prev = scriptContext->prev;
    }
    scriptContextCount--;
}

ThreadContext::CollectCallBack *
ThreadContext::AddRecyclerCollectCallBack(RecyclerCollectCallBackFunction callback, void * context)
{
    AutoCriticalSection autocs(&csCollectionCallBack);
    CollectCallBack * collectCallBack = this->collectCallBackList.PrependNode(&HeapAllocator::Instance);
    collectCallBack->callback = callback;
    collectCallBack->context = context;
    this->hasCollectionCallBack = true;
    return collectCallBack;
}

void
ThreadContext::RemoveRecyclerCollectCallBack(ThreadContext::CollectCallBack * collectCallBack)
{
    AutoCriticalSection autocs(&csCollectionCallBack);
    this->collectCallBackList.RemoveElement(&HeapAllocator::Instance, collectCallBack);
    this->hasCollectionCallBack = !this->collectCallBackList.Empty();
}

void
ThreadContext::PreCollectionCallBack(CollectionFlags flags)
{
#ifdef PERF_COUNTERS
    PHASE_PRINT_TESTTRACE1(Js::DeferParsePhase, _u("TestTrace: deferparse - # of func: %d # deferparsed: %d\n"), PerfCounter::CodeCounterSet::GetTotalFunctionCounter().GetValue(), PerfCounter::CodeCounterSet::GetDeferredFunctionCounter().GetValue());
#endif
    // This needs to be done before ClearInlineCaches since that method can empty the list of
    // script contexts with inline caches
    this->ClearScriptContextCaches();

    // Clear up references to types to avoid keep them alive
    this->ClearPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesCaches();

    // Clean up unused memory before we start collecting
    this->CleanNoCasePropertyMap();
    this->TryEnterExpirableCollectMode();

    const BOOL concurrent = flags & CollectMode_Concurrent;
    const BOOL partial = flags & CollectMode_Partial;

    if (!partial)
    {
        // Integrate allocated pages from background JIT threads
#if ENABLE_NATIVE_CODEGEN
        if (codeGenNumberThreadAllocator)
        {
            codeGenNumberThreadAllocator->Integrate();
        }
#endif
    }

    RecyclerCollectCallBackFlags callBackFlags = (RecyclerCollectCallBackFlags)
        ((concurrent ? Collect_Begin_Concurrent : Collect_Begin) | (partial? Collect_Begin_Partial : Collect_Begin));
    CollectionCallBack(callBackFlags);
}

void
ThreadContext::PreSweepCallback()
{
#ifdef PERSISTENT_INLINE_CACHES
    ClearInlineCachesWithDeadWeakRefs();
#else
    ClearInlineCaches();
#endif

    ClearIsInstInlineCaches();

    ClearEquivalentTypeCaches();

    this->dynamicObjectEnumeratorCacheMap.Clear();
}

void
ThreadContext::CollectionCallBack(RecyclerCollectCallBackFlags flags)
{
    DListBase<CollectCallBack>::Iterator i(&this->collectCallBackList);
    while (i.Next())
    {
        i.Data().callback(i.Data().context, flags);
    }
}

void
ThreadContext::WaitCollectionCallBack()
{
    // Avoid taking the lock if there are no call back
    if (hasCollectionCallBack)
    {
        AutoCriticalSection autocs(&csCollectionCallBack);
        CollectionCallBack(Collect_Wait);
    }
}

void
ThreadContext::PostCollectionCallBack()
{
    CollectionCallBack(Collect_End);

    TryExitExpirableCollectMode();

    // Recycler is null in the case where the ThreadContext is in the process of creating the recycler and
    // we have a GC triggered (say because the -recyclerStress flag is passed in)
    if (this->recycler != NULL && this->recycler->InCacheCleanupCollection())
    {
        this->recycler->ClearCacheCleanupCollection();
        for (Js::ScriptContext *scriptContext = scriptContextList; scriptContext; scriptContext = scriptContext->next)
        {
            scriptContext->CleanupWeakReferenceDictionaries();
        }
    }
}

#ifdef FAULT_INJECTION
void
ThreadContext::DisposeScriptContextByFaultInjectionCallBack()
{
    if (FAULTINJECT_SCRIPT_TERMINATION_ON_DISPOSE) {
        int scriptContextToClose = -1;

        /* inject only if we have more than 1 script context*/
        uint totalScriptCount = GetScriptContextCount();
        if (totalScriptCount > 1) {
            if (Js::Configuration::Global.flags.FaultInjectionScriptContextToTerminateCount > 0)
            {
                scriptContextToClose = Js::Configuration::Global.flags.FaultInjectionScriptContextToTerminateCount % totalScriptCount;
                for (Js::ScriptContext *scriptContext = GetScriptContextList(); scriptContext; scriptContext = scriptContext->next)
                {
                    if (scriptContextToClose-- == 0)
                    {
                        scriptContext->DisposeScriptContextByFaultInjection();
                        break;
                    }
                }
            }
            else
            {
                fwprintf(stderr, _u("***FI: FaultInjectionScriptContextToTerminateCount Failed, Value should be > 0. \n"));
            }
        }
    }
}
#endif

#pragma region "Expirable Object Methods"
void
ThreadContext::TryExitExpirableCollectMode()
{
    // If this feature is turned off or if we're already in profile collection mode, do nothing
    // We also do nothing if expiration is explicitly disabled by someone lower down the stack
    if (PHASE_OFF1(Js::ExpirableCollectPhase) || !InExpirableCollectMode() || this->disableExpiration)
    {
        return;
    }

    if (InExpirableCollectMode())
    {
        OUTPUT_TRACE(Js::ExpirableCollectPhase, _u("Checking to see whether to complete Expirable Object Collection: GC Count is %d\n"), this->expirableCollectModeGcCount);
        if (this->expirableCollectModeGcCount > 0)
        {
            this->expirableCollectModeGcCount--;
        }

        if (this->expirableCollectModeGcCount == 0 &&
            (this->recycler->InCacheCleanupCollection() || CONFIG_FLAG(ForceExpireOnNonCacheCollect)))
        {
            OUTPUT_TRACE(Js::ExpirableCollectPhase, _u("Completing Expirable Object Collection\n"));

            ExpirableObjectList::Iterator expirableObjectIterator(this->expirableObjectList);

            while (expirableObjectIterator.Next())
            {
                ExpirableObject* object = expirableObjectIterator.Data();

                Assert(object);

                if (!object->IsObjectUsed())
                {
                    object->Expire();
                }
            }

            // Leave expirable collection mode
            expirableCollectModeGcCount = -1;
        }
    }
}

bool
ThreadContext::InExpirableCollectMode()
{
    // We're in expirable collect if we have expirable objects registered,
    // and expirableCollectModeGcCount is not negative
    // and when debugger is attaching, it might have set the function to deferredParse.
    return (expirableObjectList != nullptr &&
            numExpirableObjects > 0 &&
            expirableCollectModeGcCount >= 0 &&
            (this->GetDebugManager() != nullptr &&
            !this->GetDebugManager()->IsDebuggerAttaching()));
}

void
ThreadContext::TryEnterExpirableCollectMode()
{
    // If this feature is turned off or if we're already in profile collection mode, do nothing
    if (PHASE_OFF1(Js::ExpirableCollectPhase) || InExpirableCollectMode())
    {
        OUTPUT_TRACE(Js::ExpirableCollectPhase, _u("Not running Expirable Object Collection\n"));
        return;
    }

    double entryPointCollectionThreshold = Js::Configuration::Global.flags.ExpirableCollectionTriggerThreshold / 100.0;

    double currentThreadNativeCodeRatio = ((double) GetCodeSize()) / Js::Constants::MaxThreadJITCodeHeapSize;

    OUTPUT_TRACE(Js::ExpirableCollectPhase, _u("Current native code ratio: %f\n"), currentThreadNativeCodeRatio);
    if (currentThreadNativeCodeRatio > entryPointCollectionThreshold)
    {
        OUTPUT_TRACE(Js::ExpirableCollectPhase, _u("Setting up Expirable Object Collection\n"));

        this->expirableCollectModeGcCount = Js::Configuration::Global.flags.ExpirableCollectionGCCount;

        ExpirableObjectList::Iterator expirableObjectIterator(this->expirableObjectList);

        while (expirableObjectIterator.Next())
        {
            ExpirableObject* object = expirableObjectIterator.Data();

            Assert(object);
            object->EnterExpirableCollectMode();
        }

        if (this->entryExitRecord != nullptr)
        {
            // If we're in script, we will do a stack walk, find the JavascriptFunction's on the stack
            // and mark their entry points as being used so that we don't prematurely expire them

            Js::ScriptContext* topScriptContext = this->entryExitRecord->scriptContext;
            Js::JavascriptStackWalker walker(topScriptContext, TRUE);

            Js::JavascriptFunction* javascriptFunction = nullptr;
            while (walker.GetCallerWithoutInlinedFrames(&javascriptFunction))
            {
                if (javascriptFunction != nullptr && Js::ScriptFunction::Is(javascriptFunction))
                {
                    Js::ScriptFunction* scriptFunction = (Js::ScriptFunction*) javascriptFunction;
                    Js::FunctionEntryPointInfo* entryPointInfo =  scriptFunction->GetFunctionEntryPointInfo();
                    entryPointInfo->SetIsObjectUsed();
                    scriptFunction->GetFunctionBody()->MapEntryPoints([](int index, Js::FunctionEntryPointInfo* entryPoint){
                        entryPoint->SetIsObjectUsed();
                    });
                }
            }

        }
    }
}

void
ThreadContext::RegisterExpirableObject(ExpirableObject* object)
{
    Assert(this->expirableObjectList);
    Assert(object->registrationHandle == nullptr);

    ExpirableObject** registrationData = this->expirableObjectList->PrependNode();
    (*registrationData) = object;
    object->registrationHandle = (void*) registrationData;
    OUTPUT_VERBOSE_TRACE(Js::ExpirableCollectPhase, _u("Registered 0x%p\n"), object);

    numExpirableObjects++;
}

void
ThreadContext::UnregisterExpirableObject(ExpirableObject* object)
{
    Assert(this->expirableObjectList);
    Assert(object->registrationHandle != nullptr);
    Assert(this->expirableObjectList->HasElement((ExpirableObject* const *) object->registrationHandle));

    ExpirableObject** registrationData = (ExpirableObject**) object->registrationHandle;
    Assert(*registrationData == object);

    this->expirableObjectList->MoveElementTo(registrationData, this->expirableObjectDisposeList);
    object->registrationHandle = nullptr;
    OUTPUT_VERBOSE_TRACE(Js::ExpirableCollectPhase, _u("Unregistered 0x%p\n"), object);
    numExpirableObjects--;
}

void
ThreadContext::DisposeExpirableObject(ExpirableObject* object)
{
    Assert(this->expirableObjectDisposeList);
    Assert(object->registrationHandle == nullptr);

    this->expirableObjectDisposeList->Remove(object);

    OUTPUT_VERBOSE_TRACE(Js::ExpirableCollectPhase, _u("Disposed 0x%p\n"), object);
}
#pragma endregion

void
ThreadContext::ClearScriptContextCaches()
{
    for (Js::ScriptContext *scriptContext = scriptContextList; scriptContext != nullptr; scriptContext = scriptContext->next)
    {
        scriptContext->ClearScriptContextCaches();
    }
}

#ifdef PERSISTENT_INLINE_CACHES
void
ThreadContext::ClearInlineCachesWithDeadWeakRefs()
{
    for (Js::ScriptContext *scriptContext = scriptContextList; scriptContext != nullptr; scriptContext = scriptContext->next)
    {
        scriptContext->ClearInlineCachesWithDeadWeakRefs();
    }

    if (PHASE_TRACE1(Js::InlineCachePhase))
    {
        size_t size = 0;
        size_t freeListSize = 0;
        size_t polyInlineCacheSize = 0;
        uint scriptContextCount = 0;
        for (Js::ScriptContext *scriptContext = scriptContextList;
            scriptContext;
            scriptContext = scriptContext->next)
        {
            scriptContextCount++;
            size += scriptContext->GetInlineCacheAllocator()->AllocatedSize();
            freeListSize += scriptContext->GetInlineCacheAllocator()->FreeListSize();
#ifdef POLY_INLINE_CACHE_SIZE_STATS
            polyInlineCacheSize += scriptContext->GetInlineCacheAllocator()->GetPolyInlineCacheSize();
#endif
        };
        printf("Inline cache arena: total = %5I64u KB, free list = %5I64u KB, poly caches = %5I64u KB, script contexts = %u\n",
            static_cast<uint64>(size / 1024), static_cast<uint64>(freeListSize / 1024), static_cast<uint64>(polyInlineCacheSize / 1024), scriptContextCount);
    }
}

void
ThreadContext::ClearInvalidatedUniqueGuards()
{
    // If a propertyGuard was invalidated, make sure to remove it's entry from unique property guard table of other property records.
    PropertyGuardDictionary &guards = this->recyclableData->propertyGuards;

    guards.Map([this](Js::PropertyRecord const * propertyRecord, PropertyGuardEntry* entry, const RecyclerWeakReference<const Js::PropertyRecord>* weakRef)
    {
        entry->uniqueGuards.MapAndRemoveIf([=](RecyclerWeakReference<Js::PropertyGuard>* guardWeakRef)
        {
            Js::PropertyGuard* guard = guardWeakRef->Get();
            bool shouldRemove = guard != nullptr && !guard->IsValid();
            if (shouldRemove)
            {
                if (PHASE_TRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase))
                {
                    Output::Print(_u("FixedFields: invalidating guard: name: %s, address: 0x%p, value: 0x%p, value address: 0x%p\n"),
                                  propertyRecord->GetBuffer(), guard, guard->GetValue(), guard->GetAddressOfValue());
                    Output::Flush();
                }
                if (PHASE_TESTTRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase))
                {
                    Output::Print(_u("FixedFields: invalidating guard: name: %s, value: 0x%p\n"),
                                  propertyRecord->GetBuffer(), guard->GetValue());
                    Output::Flush();
                }
            }

            return shouldRemove;
        });
    });
}

void
ThreadContext::ClearInlineCaches()
{
    if (PHASE_TRACE1(Js::InlineCachePhase))
    {
        size_t size = 0;
        size_t freeListSize = 0;
        size_t polyInlineCacheSize = 0;
        uint scriptContextCount = 0;
        for (Js::ScriptContext *scriptContext = scriptContextList;
        scriptContext;
            scriptContext = scriptContext->next)
        {
            scriptContextCount++;
            size += scriptContext->GetInlineCacheAllocator()->AllocatedSize();
            freeListSize += scriptContext->GetInlineCacheAllocator()->FreeListSize();
#ifdef POLY_INLINE_CACHE_SIZE_STATS
            polyInlineCacheSize += scriptContext->GetInlineCacheAllocator()->GetPolyInlineCacheSize();
#endif
        };
        printf("Inline cache arena: total = %5I64u KB, free list = %5I64u KB, poly caches = %5I64u KB, script contexts = %u\n",
            static_cast<uint64>(size / 1024), static_cast<uint64>(freeListSize / 1024), static_cast<uint64>(polyInlineCacheSize / 1024), scriptContextCount);
    }

    Js::ScriptContext *scriptContext = this->scriptContextList;
    while (scriptContext != nullptr)
    {
        scriptContext->ClearInlineCaches();
        scriptContext = scriptContext->next;
    }

    inlineCacheThreadInfoAllocator.Reset();
    protoInlineCacheByPropId.ResetNoDelete();
    storeFieldInlineCacheByPropId.ResetNoDelete();

    registeredInlineCacheCount = 0;
    unregisteredInlineCacheCount = 0;
}

void
ThreadContext::ClearIsInstInlineCaches()
{
    Js::ScriptContext *scriptContext = this->scriptContextList;
    while (scriptContext != nullptr)
    {
        scriptContext->ClearIsInstInlineCaches();
        scriptContext = scriptContext->next;
    }

    isInstInlineCacheThreadInfoAllocator.Reset();
    isInstInlineCacheByFunction.ResetNoDelete();
}
#endif //PERSISTENT_INLINE_CACHES

void
ThreadContext::ClearEquivalentTypeCaches()
{
#if ENABLE_NATIVE_CODEGEN
    // Called from PreSweepCallback to clear pointers to types that have no live object references left.
    // The EquivalentTypeCache used to keep these types alive, but this caused memory growth in cases where
    // entry points stayed around for a long time.
    // In future we may want to pin the reference/guard type to the entry point, but that choice will depend
    // on a use case where pinning the type helps us optimize. Lacking that, clearing the guard type is a
    // simpler short-term solution.
    // Note that clearing unmarked types from the cache and guard is needed for correctness if the cache doesn't keep
    // the types alive.
    FOREACH_DLISTBASE_ENTRY_EDITING(Js::EntryPointInfo *, entryPoint, &equivalentTypeCacheEntryPoints, iter)
    {
        bool isLive = entryPoint->ClearEquivalentTypeCaches();
        if (!isLive)
        {
            iter.RemoveCurrent(&equivalentTypeCacheInfoAllocator);
        }
    }
    NEXT_DLISTBASE_ENTRY_EDITING;

    // Note: Don't reset the list, because we're only clearing the dead types from these caches.
    // There may still be type references we need to keep an eye on.
#endif
}

Js::EntryPointInfo **
ThreadContext::RegisterEquivalentTypeCacheEntryPoint(Js::EntryPointInfo * entryPoint)
{
    return equivalentTypeCacheEntryPoints.PrependNode(&equivalentTypeCacheInfoAllocator, entryPoint);
}

void
ThreadContext::UnregisterEquivalentTypeCacheEntryPoint(Js::EntryPointInfo ** entryPoint)
{
    equivalentTypeCacheEntryPoints.RemoveElement(&equivalentTypeCacheInfoAllocator, entryPoint);
}

void
ThreadContext::RegisterProtoInlineCache(Js::InlineCache * inlineCache, Js::PropertyId propertyId)
{
    if (PHASE_TRACE1(Js::TraceInlineCacheInvalidationPhase))
    {
        Output::Print(_u("InlineCacheInvalidation: registering proto cache 0x%p for property %s(%u)\n"),
            inlineCache, GetPropertyName(propertyId)->GetBuffer(), propertyId);
        Output::Flush();
    }

    RegisterInlineCache(protoInlineCacheByPropId, inlineCache, propertyId);
}

void
ThreadContext::RegisterStoreFieldInlineCache(Js::InlineCache * inlineCache, Js::PropertyId propertyId)
{
    if (PHASE_TRACE1(Js::TraceInlineCacheInvalidationPhase))
    {
        Output::Print(_u("InlineCacheInvalidation: registering store field cache 0x%p for property %s(%u)\n"),
            inlineCache, GetPropertyName(propertyId)->GetBuffer(), propertyId);
        Output::Flush();
    }

    RegisterInlineCache(storeFieldInlineCacheByPropId, inlineCache, propertyId);
}

void
ThreadContext::RegisterInlineCache(InlineCacheListMapByPropertyId& inlineCacheMap, Js::InlineCache * inlineCache, Js::PropertyId propertyId)
{
    InlineCacheList* inlineCacheList;
    if (!inlineCacheMap.TryGetValue(propertyId, &inlineCacheList))
    {
        inlineCacheList = Anew(&this->inlineCacheThreadInfoAllocator, InlineCacheList, &this->inlineCacheThreadInfoAllocator);
        inlineCacheMap.AddNew(propertyId, inlineCacheList);
    }

    Js::InlineCache** inlineCacheRef = inlineCacheList->PrependNode();
    Assert(inlineCacheRef != nullptr);
    *inlineCacheRef = inlineCache;
    inlineCache->invalidationListSlotPtr = inlineCacheRef;
    this->registeredInlineCacheCount++;
}

void ThreadContext::NotifyInlineCacheBatchUnregistered(uint count)
{
    this->unregisteredInlineCacheCount += count;
    // Negative or 0 InlineCacheInvalidationListCompactionThreshold forces compaction all the time.
    if (CONFIG_FLAG(InlineCacheInvalidationListCompactionThreshold) <= 0 ||
        this->registeredInlineCacheCount / this->unregisteredInlineCacheCount < (uint)CONFIG_FLAG(InlineCacheInvalidationListCompactionThreshold))
    {
        CompactInlineCacheInvalidationLists();
    }
}

void
ThreadContext::InvalidateProtoInlineCaches(Js::PropertyId propertyId)
{
    InlineCacheList* inlineCacheList;
    if (protoInlineCacheByPropId.TryGetValueAndRemove(propertyId, &inlineCacheList))
    {
        if (PHASE_TRACE1(Js::TraceInlineCacheInvalidationPhase))
        {
            Output::Print(_u("InlineCacheInvalidation: invalidating proto caches for property %s(%u)\n"),
                          GetPropertyName(propertyId)->GetBuffer(), propertyId);
            Output::Flush();
        }

        InvalidateAndDeleteInlineCacheList(inlineCacheList);
    }
}

void
ThreadContext::InvalidateStoreFieldInlineCaches(Js::PropertyId propertyId)
{
    InlineCacheList* inlineCacheList;
    if (storeFieldInlineCacheByPropId.TryGetValueAndRemove(propertyId, &inlineCacheList))
    {
        if (PHASE_TRACE1(Js::TraceInlineCacheInvalidationPhase))
        {
            Output::Print(_u("InlineCacheInvalidation: invalidating store field caches for property %s(%u)\n"),
                          GetPropertyName(propertyId)->GetBuffer(), propertyId);
            Output::Flush();
        }

        InvalidateAndDeleteInlineCacheList(inlineCacheList);
    }
}

void
ThreadContext::InvalidateAndDeleteInlineCacheList(InlineCacheList* inlineCacheList)
{
    Assert(inlineCacheList != nullptr);

    uint cacheCount = 0;
    FOREACH_SLISTBASE_ENTRY(Js::InlineCache*, inlineCache, inlineCacheList)
    {
        cacheCount++;
        if (inlineCache != nullptr)
        {
            if (PHASE_VERBOSE_TRACE1(Js::TraceInlineCacheInvalidationPhase))
            {
                Output::Print(_u("InlineCacheInvalidation: invalidating cache 0x%p\n"), inlineCache);
                Output::Flush();
            }

            memset(inlineCache, 0, sizeof(Js::InlineCache));
        }
    }
    NEXT_SLISTBASE_ENTRY;
    Adelete(&this->inlineCacheThreadInfoAllocator, inlineCacheList);
    this->registeredInlineCacheCount = this->registeredInlineCacheCount > cacheCount ? this->registeredInlineCacheCount - cacheCount : 0;
}

void
ThreadContext::CompactInlineCacheInvalidationLists()
{
    Assert(this->unregisteredInlineCacheCount > 0);
    CompactProtoInlineCaches();

    if (this->unregisteredInlineCacheCount > 0)
    {
        CompactStoreFieldInlineCaches();
    }
}

void
ThreadContext::CompactProtoInlineCaches()
{
    protoInlineCacheByPropId.MapUntil([this](Js::PropertyId propertyId, InlineCacheList* inlineCacheList)
    {
        CompactInlineCacheList(inlineCacheList);
        return this->unregisteredInlineCacheCount == 0;
    });
}

void
ThreadContext::CompactStoreFieldInlineCaches()
{
    storeFieldInlineCacheByPropId.MapUntil([this](Js::PropertyId propertyId, InlineCacheList* inlineCacheList)
    {
        CompactInlineCacheList(inlineCacheList);
        return this->unregisteredInlineCacheCount == 0;
    });
}

void
ThreadContext::CompactInlineCacheList(InlineCacheList* inlineCacheList)
{
    Assert(inlineCacheList != nullptr);
    uint cacheCount = 0;
    FOREACH_SLISTBASE_ENTRY_EDITING(Js::InlineCache*, inlineCache, inlineCacheList, iterator)
    {
        if (inlineCache == nullptr)
        {
            iterator.RemoveCurrent(&this->inlineCacheThreadInfoAllocator);
            cacheCount++;
        }
    }
    NEXT_SLISTBASE_ENTRY_EDITING;

    if (cacheCount > 0)
    {
        this->unregisteredInlineCacheCount = this->unregisteredInlineCacheCount > cacheCount ?
            this->unregisteredInlineCacheCount - cacheCount : 0;
    }
}

#if DBG
bool
ThreadContext::IsProtoInlineCacheRegistered(const Js::InlineCache* inlineCache, Js::PropertyId propertyId)
{
    return IsInlineCacheRegistered(protoInlineCacheByPropId, inlineCache, propertyId);
}

bool
ThreadContext::IsStoreFieldInlineCacheRegistered(const Js::InlineCache* inlineCache, Js::PropertyId propertyId)
{
    return IsInlineCacheRegistered(storeFieldInlineCacheByPropId, inlineCache, propertyId);
}

bool
ThreadContext::IsInlineCacheRegistered(InlineCacheListMapByPropertyId& inlineCacheMap, const Js::InlineCache* inlineCache, Js::PropertyId propertyId)
{
    InlineCacheList* inlineCacheList;
    if (inlineCacheMap.TryGetValue(propertyId, &inlineCacheList))
    {
        return IsInlineCacheInList(inlineCache, inlineCacheList);
    }
    else
    {
        return false;
    }
}

bool
ThreadContext::IsInlineCacheInList(const Js::InlineCache* inlineCache, const InlineCacheList* inlineCacheList)
{
    Assert(inlineCache != nullptr);
    Assert(inlineCacheList != nullptr);

    FOREACH_SLISTBASE_ENTRY(Js::InlineCache*, curInlineCache, inlineCacheList)
    {
        if (curInlineCache == inlineCache)
        {
            return true;
        }
    }
    NEXT_SLISTBASE_ENTRY;

    return false;
}
#endif

#if ENABLE_NATIVE_CODEGEN
ThreadContext::PropertyGuardEntry*
ThreadContext::EnsurePropertyGuardEntry(const Js::PropertyRecord* propertyRecord, bool& foundExistingEntry)
{
    PropertyGuardDictionary &guards = this->recyclableData->propertyGuards;
    PropertyGuardEntry* entry;

    foundExistingEntry = guards.TryGetValue(propertyRecord, &entry);
    if (!foundExistingEntry)
    {
        entry = RecyclerNew(GetRecycler(), PropertyGuardEntry, GetRecycler());

        guards.UncheckedAdd(CreatePropertyRecordWeakRef(propertyRecord), entry);
    }

    return entry;
}

Js::PropertyGuard*
ThreadContext::RegisterSharedPropertyGuard(Js::PropertyId propertyId)
{
    Assert(IsActivePropertyId(propertyId));

    const Js::PropertyRecord * propertyRecord = GetPropertyName(propertyId);

    bool foundExistingGuard;
    PropertyGuardEntry* entry = EnsurePropertyGuardEntry(propertyRecord, foundExistingGuard);

    if (entry->sharedGuard == nullptr)
    {
        entry->sharedGuard = Js::PropertyGuard::New(GetRecycler());
    }

    Js::PropertyGuard* guard = entry->sharedGuard;

    PHASE_PRINT_VERBOSE_TRACE1(Js::FixedMethodsPhase, _u("FixedFields: registered shared guard: name: %s, address: 0x%p, value: 0x%p, value address: 0x%p, %s\n"),
        propertyRecord->GetBuffer(), guard, guard->GetValue(), guard->GetAddressOfValue(), foundExistingGuard ? _u("existing") : _u("new"));
    PHASE_PRINT_TESTTRACE1(Js::FixedMethodsPhase, _u("FixedFields: registered shared guard: name: %s, value: 0x%p, %s\n"),
        propertyRecord->GetBuffer(), guard->GetValue(), foundExistingGuard ? _u("existing") : _u("new"));

    return guard;
}

void
ThreadContext::RegisterLazyBailout(Js::PropertyId propertyId, Js::EntryPointInfo* entryPoint)
{
    const Js::PropertyRecord * propertyRecord = GetPropertyName(propertyId);

    bool foundExistingGuard;
    PropertyGuardEntry* entry = EnsurePropertyGuardEntry(propertyRecord, foundExistingGuard);
    if (!entry->entryPoints)
    {
        entry->entryPoints = RecyclerNew(recycler, PropertyGuardEntry::EntryPointDictionary, recycler, /*capacity*/ 3);
    }
    entry->entryPoints->UncheckedAdd(entryPoint, NULL);
}

void
ThreadContext::RegisterUniquePropertyGuard(Js::PropertyId propertyId, Js::PropertyGuard* guard)
{
    Assert(IsActivePropertyId(propertyId));
    Assert(guard != nullptr);

    RecyclerWeakReference<Js::PropertyGuard>* guardWeakRef = this->recycler->CreateWeakReferenceHandle(guard);
    RegisterUniquePropertyGuard(propertyId, guardWeakRef);
}

void
ThreadContext::RegisterUniquePropertyGuard(Js::PropertyId propertyId, RecyclerWeakReference<Js::PropertyGuard>* guardWeakRef)
{
    Assert(IsActivePropertyId(propertyId));
    Assert(guardWeakRef != nullptr);

    Js::PropertyGuard* guard = guardWeakRef->Get();
    Assert(guard != nullptr);

    const Js::PropertyRecord * propertyRecord = GetPropertyName(propertyId);

    bool foundExistingGuard;

    
    PropertyGuardEntry* entry = EnsurePropertyGuardEntry(propertyRecord, foundExistingGuard);

    entry->uniqueGuards.Item(guardWeakRef);

    if (PHASE_TRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase))
    {
        Output::Print(_u("FixedFields: registered unique guard: name: %s, address: 0x%p, value: 0x%p, value address: 0x%p, %s entry\n"),
            propertyRecord->GetBuffer(), guard, guard->GetValue(), guard->GetAddressOfValue(), foundExistingGuard ? _u("existing") : _u("new"));
        Output::Flush();
    }

    if (PHASE_TESTTRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase))
    {
        Output::Print(_u("FixedFields: registered unique guard: name: %s, value: 0x%p, %s entry\n"),
            propertyRecord->GetBuffer(), guard->GetValue(), foundExistingGuard ? _u("existing") : _u("new"));
        Output::Flush();
    }
}

void
ThreadContext::RegisterConstructorCache(Js::PropertyId propertyId, Js::ConstructorCache* cache)
{
    Assert(Js::ConstructorCache::GetOffsetOfGuardValue() == Js::PropertyGuard::GetOffsetOfValue());
    Assert(Js::ConstructorCache::GetSizeOfGuardValue() == Js::PropertyGuard::GetSizeOfValue());
    RegisterUniquePropertyGuard(propertyId, reinterpret_cast<Js::PropertyGuard*>(cache));
}

void
ThreadContext::InvalidatePropertyGuardEntry(const Js::PropertyRecord* propertyRecord, PropertyGuardEntry* entry, bool isAllPropertyGuardsInvalidation)
{
    Assert(entry != nullptr);

    if (entry->sharedGuard != nullptr)
    {
        Js::PropertyGuard* guard = entry->sharedGuard;

        if (PHASE_TRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase))
        {
            Output::Print(_u("FixedFields: invalidating guard: name: %s, address: 0x%p, value: 0x%p, value address: 0x%p\n"),
                propertyRecord->GetBuffer(), guard, guard->GetValue(), guard->GetAddressOfValue());
            Output::Flush();
        }

        if (PHASE_TESTTRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase))
        {
            Output::Print(_u("FixedFields: invalidating guard: name: %s, value: 0x%p\n"), propertyRecord->GetBuffer(), guard->GetValue());
            Output::Flush();
        }

        guard->Invalidate();
    }

    uint count = 0;
    entry->uniqueGuards.Map([&count, propertyRecord](RecyclerWeakReference<Js::PropertyGuard>* guardWeakRef)
    {
        Js::PropertyGuard* guard = guardWeakRef->Get();
        if (guard != nullptr)
        {
            if (PHASE_TRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase))
            {
                Output::Print(_u("FixedFields: invalidating guard: name: %s, address: 0x%p, value: 0x%p, value address: 0x%p\n"),
                    propertyRecord->GetBuffer(), guard, guard->GetValue(), guard->GetAddressOfValue());
                Output::Flush();
            }

            if (PHASE_TESTTRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase))
            {
                Output::Print(_u("FixedFields: invalidating guard: name: %s, value: 0x%p\n"),
                    propertyRecord->GetBuffer(), guard->GetValue());
                Output::Flush();
            }

            guard->Invalidate();
            count++;
        }
    });

    entry->uniqueGuards.Clear();

    
    // Count no. of invalidations done so far. Exclude if this is all property guards invalidation in which case
    // the unique Guards will be cleared anyway.
    if (!isAllPropertyGuardsInvalidation)
    {
        this->recyclableData->constructorCacheInvalidationCount += count;
        if (this->recyclableData->constructorCacheInvalidationCount > (uint)CONFIG_FLAG(ConstructorCacheInvalidationThreshold))
        {
            // TODO: In future, we should compact the uniqueGuards dictionary so this function can be called from PreCollectionCallback
            // instead
            this->ClearInvalidatedUniqueGuards();
            this->recyclableData->constructorCacheInvalidationCount = 0;
        }
    }

    if (entry->entryPoints && entry->entryPoints->Count() > 0)
    {
        Js::JavascriptStackWalker stackWalker(this->GetScriptContextList());
        Js::JavascriptFunction* caller;
        while (stackWalker.GetCaller(&caller, /*includeInlineFrames*/ false))
        {
            // If the current frame is already from a bailout - we do not need to do on stack invalidation
            if (caller != nullptr && Js::ScriptFunction::Is(caller) && !stackWalker.GetCurrentFrameFromBailout())
            {
                BYTE dummy;
                Js::FunctionEntryPointInfo* functionEntryPoint = caller->GetFunctionBody()->GetDefaultFunctionEntryPointInfo();
                if (functionEntryPoint->IsInNativeAddressRange((DWORD_PTR)stackWalker.GetInstructionPointer()))
                {
                    if (entry->entryPoints->TryGetValue(functionEntryPoint, &dummy))
                    {
                        functionEntryPoint->DoLazyBailout(stackWalker.GetCurrentAddressOfInstructionPointer(),
                            caller->GetFunctionBody(), propertyRecord);
                    }
                }
            }
        }
        entry->entryPoints->Map([=](Js::EntryPointInfo* info, BYTE& dummy, const RecyclerWeakReference<Js::EntryPointInfo>* infoWeakRef)
        {
            OUTPUT_TRACE2(Js::LazyBailoutPhase, info->GetFunctionBody(), _u("Lazy bailout - Invalidation due to property: %s \n"), propertyRecord->GetBuffer());
            info->Invalidate(true);
        });
        entry->entryPoints->Clear();
    }
}

void
ThreadContext::InvalidatePropertyGuards(Js::PropertyId propertyId)
{
    const Js::PropertyRecord* propertyRecord = GetPropertyName(propertyId);
    PropertyGuardDictionary &guards = this->recyclableData->propertyGuards;
    PropertyGuardEntry* entry;
    if (guards.TryGetValueAndRemove(propertyRecord, &entry))
    {
        InvalidatePropertyGuardEntry(propertyRecord, entry, false);
    }
}

void
ThreadContext::InvalidateAllPropertyGuards()
{
    PropertyGuardDictionary &guards = this->recyclableData->propertyGuards;
    if (guards.Count() > 0)
    {
        guards.Map([this](Js::PropertyRecord const * propertyRecord, PropertyGuardEntry* entry, const RecyclerWeakReference<const Js::PropertyRecord>* weakRef)
        {
            InvalidatePropertyGuardEntry(propertyRecord, entry, true);
        });

        guards.Clear();
    }
}
#endif

void
ThreadContext::InvalidateAllProtoInlineCaches()
{
    protoInlineCacheByPropId.Map([this](Js::PropertyId propertyId, InlineCacheList* inlineCacheList)
    {
        InvalidateAndDeleteInlineCacheList(inlineCacheList);
    });
    protoInlineCacheByPropId.Reset();
}

#if DBG

// Verifies if object is registered in any proto InlineCache
bool
ThreadContext::IsObjectRegisteredInProtoInlineCaches(Js::DynamicObject * object)
{
    return protoInlineCacheByPropId.MapUntil([object](Js::PropertyId propertyId, InlineCacheList* inlineCacheList)
    {
        FOREACH_SLISTBASE_ENTRY(Js::InlineCache*, inlineCache, inlineCacheList)
        {
            if (inlineCache != nullptr && !inlineCache->IsEmpty())
            {
                // Verify this object is not present in prototype chain of inlineCache's type
                bool isObjectPresentOnPrototypeChain =
                    Js::JavascriptOperators::MapObjectAndPrototypesUntil<true>(inlineCache->GetType()->GetPrototype(), [=](Js::RecyclableObject* prototype)
                {
                    return prototype == object;
                });
                if (isObjectPresentOnPrototypeChain) {
                    return true;
                }
            }
        }
        NEXT_SLISTBASE_ENTRY;
        return false;
    });
}

// Verifies if object is registered in any storeField InlineCache
bool
ThreadContext::IsObjectRegisteredInStoreFieldInlineCaches(Js::DynamicObject * object)
{
    return storeFieldInlineCacheByPropId.MapUntil([object](Js::PropertyId propertyId, InlineCacheList* inlineCacheList)
    {
        FOREACH_SLISTBASE_ENTRY(Js::InlineCache*, inlineCache, inlineCacheList)
        {
            if (inlineCache != nullptr && !inlineCache->IsEmpty())
            {
                // Verify this object is not present in prototype chain of inlineCache's type
                bool isObjectPresentOnPrototypeChain =
                    Js::JavascriptOperators::MapObjectAndPrototypesUntil<true>(inlineCache->GetType()->GetPrototype(), [=](Js::RecyclableObject* prototype)
                {
                    return prototype == object;
                });
                if (isObjectPresentOnPrototypeChain) {
                    return true;
                }
            }
        }
        NEXT_SLISTBASE_ENTRY;
        return false;
    });
}
#endif

bool
ThreadContext::AreAllProtoInlineCachesInvalidated()
{
    return protoInlineCacheByPropId.Count() == 0;
}

void
ThreadContext::InvalidateAllStoreFieldInlineCaches()
{
    storeFieldInlineCacheByPropId.Map([this](Js::PropertyId propertyId, InlineCacheList* inlineCacheList)
    {
        InvalidateAndDeleteInlineCacheList(inlineCacheList);
    });
    storeFieldInlineCacheByPropId.Reset();
}

bool
ThreadContext::AreAllStoreFieldInlineCachesInvalidated()
{
    return storeFieldInlineCacheByPropId.Count() == 0;
}

#if DBG
bool
ThreadContext::IsIsInstInlineCacheRegistered(Js::IsInstInlineCache * inlineCache, Js::Var function)
{
    Assert(inlineCache != nullptr);
    Assert(function != nullptr);
    Js::IsInstInlineCache* firstInlineCache;
    if (this->isInstInlineCacheByFunction.TryGetValue(function, &firstInlineCache))
    {
        return IsIsInstInlineCacheInList(inlineCache, firstInlineCache);
    }
    else
    {
        return false;
    }
}
#endif

void
ThreadContext::RegisterIsInstInlineCache(Js::IsInstInlineCache * inlineCache, Js::Var function)
{
    Assert(function != nullptr);
    Assert(inlineCache != nullptr);
    // We should never cross-register or re-register a cache that is already on some invalidation list (for its function or some other function).
    // Every cache must be first cleared and unregistered before being registered again.
    AssertMsg(inlineCache->function == nullptr, "We should only register instance-of caches that have not yet been populated.");
    Js::IsInstInlineCache** inlineCacheRef = nullptr;

    if (this->isInstInlineCacheByFunction.TryGetReference(function, &inlineCacheRef))
    {
        AssertMsg(!IsIsInstInlineCacheInList(inlineCache, *inlineCacheRef), "Why are we registering a cache that is already registered?");
        inlineCache->next = *inlineCacheRef;
        *inlineCacheRef = inlineCache;
    }
    else
    {
        inlineCache->next = nullptr;
        this->isInstInlineCacheByFunction.Add(function, inlineCache);
    }
}

void
ThreadContext::UnregisterIsInstInlineCache(Js::IsInstInlineCache * inlineCache, Js::Var function)
{
    Assert(inlineCache != nullptr);
    Js::IsInstInlineCache** inlineCacheRef = nullptr;

    if (this->isInstInlineCacheByFunction.TryGetReference(function, &inlineCacheRef))
    {
        Assert(*inlineCacheRef != nullptr);
        if (inlineCache == *inlineCacheRef)
        {
            *inlineCacheRef = (*inlineCacheRef)->next;
            if (*inlineCacheRef == nullptr)
            {
                this->isInstInlineCacheByFunction.Remove(function);
            }
        }
        else
        {
            Js::IsInstInlineCache * prevInlineCache;
            Js::IsInstInlineCache * curInlineCache;
            for (prevInlineCache = *inlineCacheRef, curInlineCache = (*inlineCacheRef)->next; curInlineCache != nullptr;
                prevInlineCache = curInlineCache, curInlineCache = curInlineCache->next)
            {
                if (curInlineCache == inlineCache)
                {
                    prevInlineCache->next = curInlineCache->next;
                    return;
                }
            }
            AssertMsg(false, "Why are we unregistering a cache that is not registered?");
        }
    }
}

void
ThreadContext::InvalidateIsInstInlineCacheList(Js::IsInstInlineCache* inlineCacheList)
{
    Assert(inlineCacheList != nullptr);
    Js::IsInstInlineCache* curInlineCache;
    Js::IsInstInlineCache* nextInlineCache;
    for (curInlineCache = inlineCacheList; curInlineCache != nullptr; curInlineCache = nextInlineCache)
    {
        if (PHASE_VERBOSE_TRACE1(Js::TraceInlineCacheInvalidationPhase))
        {
            Output::Print(_u("InlineCacheInvalidation: invalidating instanceof cache 0x%p\n"), curInlineCache);
            Output::Flush();
        }
        // Stash away the next cache before we zero out the current one (including its next pointer).
        nextInlineCache = curInlineCache->next;
        // Clear the current cache to invalidate it.
        memset(curInlineCache, 0, sizeof(Js::IsInstInlineCache));
    }
}

void
ThreadContext::InvalidateIsInstInlineCachesForFunction(Js::Var function)
{
    Js::IsInstInlineCache* inlineCacheList;
    if (this->isInstInlineCacheByFunction.TryGetValueAndRemove(function, &inlineCacheList))
    {
        InvalidateIsInstInlineCacheList(inlineCacheList);
    }
}

void
ThreadContext::InvalidateAllIsInstInlineCaches()
{
    isInstInlineCacheByFunction.Map([this](const Js::Var function, Js::IsInstInlineCache* inlineCacheList)
    {
        InvalidateIsInstInlineCacheList(inlineCacheList);
    });
    isInstInlineCacheByFunction.Clear();
}

bool
ThreadContext::AreAllIsInstInlineCachesInvalidated() const
{
    return isInstInlineCacheByFunction.Count() == 0;
}

#if DBG
bool
ThreadContext::IsIsInstInlineCacheInList(const Js::IsInstInlineCache* inlineCache, const Js::IsInstInlineCache* inlineCacheList)
{
    Assert(inlineCache != nullptr);
    Assert(inlineCacheList != nullptr);

    for (const Js::IsInstInlineCache* curInlineCache = inlineCacheList; curInlineCache != nullptr; curInlineCache = curInlineCache->next)
    {
        if (curInlineCache == inlineCache)
        {
            return true;
        }
    }

    return false;
}
#endif

void ThreadContext::RegisterTypeWithProtoPropertyCache(const Js::PropertyId propertyId, Js::Type *const type)
{
    Assert(propertyId != Js::Constants::NoProperty);
    Assert(IsActivePropertyId(propertyId));
    Assert(type);

    PropertyIdToTypeHashSetDictionary &typesWithProtoPropertyCache = recyclableData->typesWithProtoPropertyCache;
    TypeHashSet *typeHashSet;
    if(!typesWithProtoPropertyCache.TryGetValue(propertyId, &typeHashSet))
    {
        typeHashSet = RecyclerNew(recycler, TypeHashSet, recycler);
        typesWithProtoPropertyCache.Add(propertyId, typeHashSet);
    }

    typeHashSet->Item(type, false);
}

void ThreadContext::InvalidateProtoTypePropertyCaches(const Js::PropertyId propertyId)
{
    Assert(propertyId != Js::Constants::NoProperty);
    Assert(IsActivePropertyId(propertyId));
    InternalInvalidateProtoTypePropertyCaches(propertyId);
}

void ThreadContext::InternalInvalidateProtoTypePropertyCaches(const Js::PropertyId propertyId)
{
    // Get the hash set of registered types associated with the property ID, invalidate each type in the hash set, and
    // remove the property ID and its hash set from the map
    PropertyIdToTypeHashSetDictionary &typesWithProtoPropertyCache = recyclableData->typesWithProtoPropertyCache;
    TypeHashSet *typeHashSet;
    if(typesWithProtoPropertyCache.Count() != 0 && typesWithProtoPropertyCache.TryGetValueAndRemove(propertyId, &typeHashSet))
    {
        DoInvalidateProtoTypePropertyCaches(propertyId, typeHashSet);
    }
}

void ThreadContext::InvalidateAllProtoTypePropertyCaches()
{
    PropertyIdToTypeHashSetDictionary &typesWithProtoPropertyCache = recyclableData->typesWithProtoPropertyCache;
    if (typesWithProtoPropertyCache.Count() > 0)
    {
        typesWithProtoPropertyCache.Map([this](Js::PropertyId propertyId, TypeHashSet * typeHashSet)
        {
            DoInvalidateProtoTypePropertyCaches(propertyId, typeHashSet);
        });
        typesWithProtoPropertyCache.Clear();
    }
}

void ThreadContext::DoInvalidateProtoTypePropertyCaches(const Js::PropertyId propertyId, TypeHashSet *const typeHashSet)
{
    Assert(propertyId != Js::Constants::NoProperty);
    Assert(typeHashSet);

    typeHashSet->Map(
        [propertyId](Js::Type *const type, const bool unused, const RecyclerWeakReference<Js::Type>*)
        {
            type->GetPropertyCache()->ClearIfPropertyIsOnAPrototype(propertyId);
        });
}

Js::ScriptContext **
ThreadContext::RegisterPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext(Js::ScriptContext * scriptContext)
{
    return prototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext.PrependNode(&prototypeChainEnsuredToHaveOnlyWritableDataPropertiesAllocator, scriptContext);
}

void
ThreadContext::UnregisterPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext(Js::ScriptContext ** scriptContext)
{
    prototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext.RemoveElement(&prototypeChainEnsuredToHaveOnlyWritableDataPropertiesAllocator, scriptContext);
}

void
ThreadContext::ClearPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesCaches()
{
    bool hasItem = false;
    FOREACH_DLISTBASE_ENTRY(Js::ScriptContext *, scriptContext, &prototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext)
    {
        scriptContext->ClearPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesCaches();
        hasItem = true;
    }
    NEXT_DLISTBASE_ENTRY;

    if (!hasItem)
    {
        return;
    }

    prototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext.Reset();
    prototypeChainEnsuredToHaveOnlyWritableDataPropertiesAllocator.Reset();
}

BOOL ThreadContext::HasPreviousHostScriptContext()
{
    return hostScriptContextStack->Count() != 0;
}

HostScriptContext* ThreadContext::GetPreviousHostScriptContext()
{
    return hostScriptContextStack->Peek();
}

void ThreadContext::PushHostScriptContext(HostScriptContext* topProvider)
{
    // script engine can be created coming from GetDispID, so push/pop can be
    // happening after the first round of enterscript as well. we might need to
    // revisit the whole callRootLevel but probably not now.
    // Assert(HasPreviousHostScriptContext() || callRootLevel == 0);
    hostScriptContextStack->Push(topProvider);
}

HostScriptContext* ThreadContext::PopHostScriptContext()
{
    return hostScriptContextStack->Pop();
    // script engine can be created coming from GetDispID, so push/pop can be
    // happening after the first round of enterscript as well. we might need to
    // revisit the whole callRootLevel but probably not now.
    // Assert(HasPreviousHostScriptContext() || callRootLevel == 0);
}

#if DBG || defined(PROFILE_EXEC)
bool
ThreadContext::AsyncHostOperationStart(void * suspendRecord)
{
    bool wasInAsync = false;
    Assert(!this->IsScriptActive());
    Js::ScriptEntryExitRecord * lastRecord = this->entryExitRecord;
    if (lastRecord != NULL)
    {
        if (!lastRecord->leaveForHost)
        {
#if DBG
            wasInAsync = !!lastRecord->leaveForAsyncHostOperation;
            lastRecord->leaveForAsyncHostOperation = true;
#endif
#ifdef PROFILE_EXEC
            lastRecord->scriptContext->ProfileSuspend(Js::RunPhase, (Js::Profiler::SuspendRecord *)suspendRecord);
#endif
        }
        else
        {
            Assert(!lastRecord->leaveForAsyncHostOperation);
        }
    }
    return wasInAsync;
}

void
ThreadContext::AsyncHostOperationEnd(bool wasInAsync, void * suspendRecord)
{
    Assert(!this->IsScriptActive());
    Js::ScriptEntryExitRecord * lastRecord = this->entryExitRecord;
    if (lastRecord != NULL)
    {
        if (!lastRecord->leaveForHost)
        {
            Assert(lastRecord->leaveForAsyncHostOperation);
#if DBG
            lastRecord->leaveForAsyncHostOperation = wasInAsync;
#endif
#ifdef PROFILE_EXEC
            lastRecord->scriptContext->ProfileResume((Js::Profiler::SuspendRecord *)suspendRecord);
#endif
        }
        else
        {
            Assert(!lastRecord->leaveForAsyncHostOperation);
            Assert(!wasInAsync);
        }
    }
}

#endif

#ifdef RECYCLER_DUMP_OBJECT_GRAPH
void DumpRecyclerObjectGraph()
{
    ThreadContext * threadContext = ThreadContext::GetContextForCurrentThread();
    if (threadContext == nullptr)
    {
        Output::Print(_u("No thread context"));
    }
    threadContext->GetRecycler()->DumpObjectGraph();
}
#endif

#if ENABLE_NATIVE_CODEGEN
BOOL ThreadContext::IsNativeAddress(void * pCodeAddr)
{
    PreReservedVirtualAllocWrapper *preReservedVirtualAllocWrapper = this->GetPreReservedVirtualAllocator();
    if (preReservedVirtualAllocWrapper->IsInRange(pCodeAddr))
    {
        return TRUE;
    }

    if (!this->IsAllJITCodeInPreReservedRegion())
    {
        CustomHeap::CodePageAllocators::AutoLock autoLock(&this->codePageAllocators);
        return this->codePageAllocators.IsInNonPreReservedPageAllocator(pCodeAddr);
    }
    return FALSE;
}
#endif

#if ENABLE_PROFILE_INFO
void ThreadContext::EnsureSourceProfileManagersByUrlMap()
{
    if(this->recyclableData->sourceProfileManagersByUrl == nullptr)
    {
        this->EnsureRecycler();
        this->recyclableData->sourceProfileManagersByUrl = RecyclerNew(GetRecycler(), SourceProfileManagersByUrlMap, GetRecycler());
    }
}

//
// Returns the cache profile manager for the URL and hash combination for a particular dynamic script. There is a ref count added for every script context
// that references the shared profile manager info.
//
Js::SourceDynamicProfileManager* ThreadContext::GetSourceDynamicProfileManager(_In_z_ const WCHAR* url, _In_ uint hash, _Inout_ bool* addRef)
{
      EnsureSourceProfileManagersByUrlMap();
      Js::SourceDynamicProfileManager* profileManager = nullptr;
      SourceDynamicProfileManagerCache* managerCache;
      bool newCache = false;
      if(!this->recyclableData->sourceProfileManagersByUrl->TryGetValue(url, &managerCache))
      {
          if(this->recyclableData->sourceProfileManagersByUrl->Count() >= INMEMORY_CACHE_MAX_URL)
          {
              return nullptr;
          }
          managerCache = RecyclerNewZ(this->GetRecycler(), SourceDynamicProfileManagerCache);
          newCache = true;
      }
      bool createProfileManager = false;
      if(!managerCache->sourceProfileManagerMap)
      {
          managerCache->sourceProfileManagerMap = RecyclerNew(this->GetRecycler(), SourceDynamicProfileManagerMap, this->GetRecycler());
          createProfileManager = true;
      }
      else
      {
          createProfileManager = !managerCache->sourceProfileManagerMap->TryGetValue(hash, &profileManager);
      }
      if(createProfileManager)
      {
          if(managerCache->sourceProfileManagerMap->Count() < INMEMORY_CACHE_MAX_PROFILE_MANAGER)
          {
              profileManager = RecyclerNewZ(this->GetRecycler(), Js::SourceDynamicProfileManager, this->GetRecycler());
              managerCache->sourceProfileManagerMap->Add(hash, profileManager);
          }
      }
      else
      {
          profileManager->Reuse();
      }

      if(!*addRef)
      {
          managerCache->AddRef();
          *addRef = true;
          OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, _u("Addref dynamic source profile manger - Url: %s\n"), url);
      }

      if (newCache)
      {
          // Let's make a copy of the URL because there is no guarantee this URL will remain alive in the future.
          size_t lengthInChars = wcslen(url) + 1;
          WCHAR* urlCopy = RecyclerNewArrayLeaf(GetRecycler(), WCHAR, lengthInChars);
          js_memcpy_s(urlCopy, lengthInChars * sizeof(WCHAR), url, lengthInChars * sizeof(WCHAR));
          this->recyclableData->sourceProfileManagersByUrl->Add(urlCopy, managerCache);
      }
      return profileManager;
}

//
// Decrement the ref count for this URL and cleanup the corresponding record if there are no other references to it.
//
uint ThreadContext::ReleaseSourceDynamicProfileManagers(const WCHAR* url)
{
    // If we've already freed the recyclable data, we're shutting down the thread context so skip clean up
    if (this->recyclableData == nullptr) return 0;

    SourceDynamicProfileManagerCache* managerCache = this->recyclableData->sourceProfileManagersByUrl->Lookup(url, nullptr);
    uint refCount = 0;
    if(managerCache)  // manager cache may be null we exceeded -INMEMORY_CACHE_MAX_URL
    {
        refCount = managerCache->Release();
        OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, _u("Release dynamic source profile manger %d Url: %s\n"), refCount, url);
        Output::Flush();
        if(refCount == 0)
        {
            this->recyclableData->sourceProfileManagersByUrl->Remove(url);
        }
    }
    return refCount;
}
#endif

void ThreadContext::EnsureSymbolRegistrationMap()
{
    if (this->recyclableData->symbolRegistrationMap == nullptr)
    {
        this->EnsureRecycler();
        this->recyclableData->symbolRegistrationMap = RecyclerNew(GetRecycler(), SymbolRegistrationMap, GetRecycler());
    }
}

const Js::PropertyRecord* ThreadContext::GetSymbolFromRegistrationMap(const char16* stringKey)
{
    this->EnsureSymbolRegistrationMap();

    return this->recyclableData->symbolRegistrationMap->Lookup(stringKey, nullptr);
}

const Js::PropertyRecord* ThreadContext::AddSymbolToRegistrationMap(const char16* stringKey, charcount_t stringLength)
{
    this->EnsureSymbolRegistrationMap();

    const Js::PropertyRecord* propertyRecord = this->UncheckedAddPropertyId(stringKey, stringLength, /*bind*/false, /*isSymbol*/true);

    Assert(propertyRecord);

    // The key is the PropertyRecord's buffer (the PropertyRecord itself) which is being pinned as long as it's in this map.
    // If that's ever not the case, we'll need to duplicate the key here and put that in the map instead.
    this->recyclableData->symbolRegistrationMap->Add(propertyRecord->GetBuffer(), propertyRecord);

    return propertyRecord;
}

void ThreadContext::ClearImplicitCallFlags()
{
    SetImplicitCallFlags(Js::ImplicitCall_None);
}

void ThreadContext::ClearImplicitCallFlags(Js::ImplicitCallFlags flags)
{
    Assert((flags & Js::ImplicitCall_None) == 0);
    SetImplicitCallFlags((Js::ImplicitCallFlags)(implicitCallFlags & ~flags));
}

void ThreadContext::CheckAndResetImplicitCallAccessorFlag()
{
    Js::ImplicitCallFlags accessorCallFlag = (Js::ImplicitCallFlags)(Js::ImplicitCall_Accessor & ~Js::ImplicitCall_None);
    if ((GetImplicitCallFlags() & accessorCallFlag) != 0)
    {
        ClearImplicitCallFlags(accessorCallFlag);
        AddImplicitCallFlags(Js::ImplicitCall_NonProfiledAccessor);
    }
}

bool ThreadContext::HasNoSideEffect(Js::RecyclableObject * function) const
{
    Js::FunctionInfo::Attributes attributes = Js::FunctionInfo::GetAttributes(function);

    return this->HasNoSideEffect(function, attributes);
}

bool ThreadContext::HasNoSideEffect(Js::RecyclableObject * function, Js::FunctionInfo::Attributes attributes) const
{
    if (((attributes & Js::FunctionInfo::CanBeHoisted) != 0)
        || ((attributes & Js::FunctionInfo::HasNoSideEffect) != 0 && !IsDisableImplicitException()))
    {
        Assert((attributes & Js::FunctionInfo::HasNoSideEffect) != 0);
        return true;
    }

    return false;
}

bool
ThreadContext::RecordImplicitException()
{
    // Record the exception in the implicit call flag
    AddImplicitCallFlags(Js::ImplicitCall_Exception);
    if (IsDisableImplicitException())
    {
        // Indicate that we shouldn't throw if ImplicitExceptions have been disabled
        return false;
    }
    // Disabling implicit exception when disabling implicit calls can result in valid exceptions not being thrown.
    // Instead we tell not to throw only if an implicit call happened and they are disabled. This is to cover the case
    // of an exception being thrown because an implicit call not executed left the execution in a bad state.
    // Since there is an implicit call, we expect to bailout and handle this operation in the interpreter instead.
    bool hasImplicitCallHappened = IsDisableImplicitCall() && (GetImplicitCallFlags() & ~Js::ImplicitCall_Exception);
    return !hasImplicitCallHappened;
}

void ThreadContext::SetThreadServiceWrapper(ThreadServiceWrapper* inThreadServiceWrapper)
{
    AssertMsg(threadServiceWrapper == NULL || inThreadServiceWrapper == NULL, "double set ThreadServiceWrapper");
    threadServiceWrapper = inThreadServiceWrapper;
}

ThreadServiceWrapper* ThreadContext::GetThreadServiceWrapper()
{
    return threadServiceWrapper;
}

uint ThreadContext::GetRandomNumber()
{
#ifdef ENABLE_CUSTOM_ENTROPY
    return (uint)GetEntropy().GetRand();
#else
    uint randomNumber = 0;
    errno_t e = rand_s(&randomNumber);
    Assert(e == 0);
    return randomNumber;
#endif
}

#ifdef ENABLE_JS_ETW
void ThreadContext::EtwLogPropertyIdList()
{
    propertyMap->Map([&](const Js::PropertyRecord* propertyRecord){
        EventWriteJSCRIPT_HOSTING_PROPERTYID_LIST(propertyRecord, propertyRecord->GetBuffer());
    });
}
#endif

#ifdef ENABLE_PROJECTION
void ThreadContext::AddExternalWeakReferenceCache(ExternalWeakReferenceCache *externalWeakReferenceCache)
{
    this->externalWeakReferenceCacheList.Prepend(&HeapAllocator::Instance, externalWeakReferenceCache);
}

void ThreadContext::RemoveExternalWeakReferenceCache(ExternalWeakReferenceCache *externalWeakReferenceCache)
{
    Assert(!externalWeakReferenceCacheList.Empty());
    this->externalWeakReferenceCacheList.Remove(&HeapAllocator::Instance, externalWeakReferenceCache);
}

void ThreadContext::MarkExternalWeakReferencedObjects(bool inPartialCollect)
{
    SListBase<ExternalWeakReferenceCache *>::Iterator iteratorWeakRefCache(&this->externalWeakReferenceCacheList);
    while (iteratorWeakRefCache.Next())
    {
        iteratorWeakRefCache.Data()->MarkNow(recycler, inPartialCollect);
    }
}

void ThreadContext::ResolveExternalWeakReferencedObjects()
{
    SListBase<ExternalWeakReferenceCache *>::Iterator iteratorWeakRefCache(&this->externalWeakReferenceCacheList);
    while (iteratorWeakRefCache.Next())
    {
        iteratorWeakRefCache.Data()->ResolveNow(recycler);
    }
}

#if DBG_DUMP
void ThreadContext::RegisterProjectionMemoryInformation(IProjectionContextMemoryInfo* projectionContextMemoryInfo)
{
    Assert(this->projectionMemoryInformation == nullptr || this->projectionMemoryInformation == projectionContextMemoryInfo);

    this->projectionMemoryInformation = projectionContextMemoryInfo;
}

void ThreadContext::DumpProjectionContextMemoryStats(LPCWSTR headerMsg, bool forceDetailed)
{
    if (this->projectionMemoryInformation)
    {
        this->projectionMemoryInformation->DumpCurrentStats(headerMsg, forceDetailed);
    }
}

IProjectionContextMemoryInfo* ThreadContext::GetProjectionContextMemoryInformation()
{
    return this->projectionMemoryInformation;
}
#endif
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
Js::Var ThreadContext::GetMemoryStat(Js::ScriptContext* scriptContext)
{
    ScriptMemoryDumper dumper(scriptContext);
    return dumper.Dump();
}

void ThreadContext::SetAutoProxyName(LPCWSTR objectName)
{
    recyclableData->autoProxyName = objectName;
}
#endif
//
// Regex helpers
//

UnifiedRegex::StandardChars<uint8>* ThreadContext::GetStandardChars(__inout_opt uint8* dummy)
{
    if (standardUTF8Chars == 0)
    {
        ArenaAllocator* allocator = GetThreadAlloc();
        standardUTF8Chars = Anew(allocator, UnifiedRegex::UTF8StandardChars, allocator);
    }
    return standardUTF8Chars;
}

UnifiedRegex::StandardChars<char16>* ThreadContext::GetStandardChars(__inout_opt char16* dummy)
{
    if (standardUnicodeChars == 0)
    {
        ArenaAllocator* allocator = GetThreadAlloc();
        standardUnicodeChars = Anew(allocator, UnifiedRegex::UnicodeStandardChars, allocator);
    }
    return standardUnicodeChars;
}

void ThreadContext::CheckScriptInterrupt()
{
    if (TestThreadContextFlag(ThreadContextFlagCanDisableExecution))
    {
        if (this->IsExecutionDisabled())
        {
            Assert(this->DoInterruptProbe());
            throw Js::ScriptAbortException();
        }
    }
    else
    {
        this->CheckInterruptPoll();
    }
}

void ThreadContext::CheckInterruptPoll()
{
    // Disable QC when implicit calls are disabled since the host can do anything before returning back, like servicing the
    // message loop, which may in turn cause script code to be executed and implicit calls to be made
    if (!IsDisableImplicitCall())
    {
        InterruptPoller *poller = this->interruptPoller;
        if (poller)
        {
            poller->CheckInterruptPoll();
        }
    }
}

void *
ThreadContext::GetDynamicObjectEnumeratorCache(Js::DynamicType const * dynamicType)
{
    void * data;
    return this->dynamicObjectEnumeratorCacheMap.TryGetValue(dynamicType, &data)? data : nullptr;
}

void
ThreadContext::AddDynamicObjectEnumeratorCache(Js::DynamicType const * dynamicType, void * cache)
{
    this->dynamicObjectEnumeratorCacheMap.Item(dynamicType, cache);
}

InterruptPoller::InterruptPoller(ThreadContext *tc) :
    threadContext(tc),
    lastPollTick(0),
    lastResetTick(0),
    isDisabled(FALSE)
{
    tc->SetInterruptPoller(this);
}

void InterruptPoller::CheckInterruptPoll()
{
    if (!isDisabled)
    {
        Js::ScriptEntryExitRecord *entryExitRecord = this->threadContext->GetScriptEntryExit();
        if (entryExitRecord)
        {
            Js::ScriptContext *scriptContext = entryExitRecord->scriptContext;
            if (scriptContext)
            {
                this->TryInterruptPoll(scriptContext);
            }
        }
    }
}


void InterruptPoller::GetStatementCount(ULONG *pluHi, ULONG *pluLo)
{
    DWORD resetTick = this->lastResetTick;
    DWORD pollTick = this->lastPollTick;
    DWORD elapsed;

    elapsed = pollTick - resetTick;

    ULONGLONG statements = (ULONGLONG)elapsed * InterruptPoller::TicksToStatements;
    *pluLo = (ULONG)statements;
    *pluHi = (ULONG)(statements >> 32);
}

void ThreadContext::DisableExecution()
{
    Assert(TestThreadContextFlag(ThreadContextFlagCanDisableExecution));
    // Hammer the stack limit with a value that will cause script abort on the next stack probe.
    this->SetStackLimitForCurrentThread(Js::Constants::StackLimitForScriptInterrupt);

    return;
}

void ThreadContext::EnableExecution()
{
    Assert(this->GetStackProber());
    // Restore the normal stack limit.
    this->SetStackLimitForCurrentThread(this->GetStackProber()->GetScriptStackLimit());

    // It's possible that the host disabled execution after the script threw an exception
    // of it's own, so we shouldn't clear that. Only exceptions for script termination
    // should be cleared.
    if (GetRecordedException() == GetPendingTerminatedErrorObject())
    {
        SetRecordedException(NULL);
    }
}

bool ThreadContext::TestThreadContextFlag(ThreadContextFlags contextFlag) const
{
    return (this->threadContextFlags & contextFlag) != 0;
}

void ThreadContext::SetThreadContextFlag(ThreadContextFlags contextFlag)
{
    this->threadContextFlags = (ThreadContextFlags)(this->threadContextFlags | contextFlag);
}

void ThreadContext::ClearThreadContextFlag(ThreadContextFlags contextFlag)
{
    this->threadContextFlags = (ThreadContextFlags)(this->threadContextFlags & ~contextFlag);
}

#ifdef ENABLE_GLOBALIZATION
#ifdef _CONTROL_FLOW_GUARD
Js::DelayLoadWinCoreMemory * ThreadContext::GetWinCoreMemoryLibrary()
{
    delayLoadWinCoreMemoryLibrary.EnsureFromSystemDirOnly();
    return &delayLoadWinCoreMemoryLibrary;
}
#endif

Js::DelayLoadWinCoreProcessThreads * ThreadContext::GetWinCoreProcessThreads()
{
    delayLoadWinCoreProcessThreads.EnsureFromSystemDirOnly();
    return &delayLoadWinCoreProcessThreads;
}

Js::DelayLoadWinRtString * ThreadContext::GetWinRTStringLibrary()
{
    delayLoadWinRtString.EnsureFromSystemDirOnly();

    return &delayLoadWinRtString;
}

#ifdef ENABLE_PROJECTION
Js::DelayLoadWinRtError * ThreadContext::GetWinRTErrorLibrary()
{
    delayLoadWinRtError.EnsureFromSystemDirOnly();

    return &delayLoadWinRtError;
}

Js::DelayLoadWinRtTypeResolution* ThreadContext::GetWinRTTypeResolutionLibrary()
{
    delayLoadWinRtTypeResolution.EnsureFromSystemDirOnly();

    return &delayLoadWinRtTypeResolution;
}

Js::DelayLoadWinRtRoParameterizedIID* ThreadContext::GetWinRTRoParameterizedIIDLibrary()
{
    delayLoadWinRtRoParameterizedIID.EnsureFromSystemDirOnly();

    return &delayLoadWinRtRoParameterizedIID;
}
#endif

#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_ES6_CHAR_CLASSIFIER)
Js::WindowsGlobalizationAdapter* ThreadContext::GetWindowsGlobalizationAdapter()
{
    return &windowsGlobalizationAdapter;
}

Js::DelayLoadWindowsGlobalization* ThreadContext::GetWindowsGlobalizationLibrary()
{
    delayLoadWindowsGlobalizationLibrary.Ensure(this->GetWinRTStringLibrary());

    return &delayLoadWindowsGlobalizationLibrary;
}
#endif

#ifdef ENABLE_FOUNDATION_OBJECT
Js::WindowsFoundationAdapter* ThreadContext::GetWindowsFoundationAdapter()
{
    return &windowsFoundationAdapter;
}

Js::DelayLoadWinRtFoundation* ThreadContext::GetWinRtFoundationLibrary()
{
    delayLoadWinRtFoundationLibrary.EnsureFromSystemDirOnly();

    return &delayLoadWinRtFoundationLibrary;
}
#endif
#endif // ENABLE_GLOBALIZATION

bool ThreadContext::IsCFGEnabled()
{
#if defined(_CONTROL_FLOW_GUARD)
    PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY CfgPolicy;
    BOOL isGetMitigationPolicySucceeded = GetWinCoreProcessThreads()->GetMitigationPolicyForProcess(
        GetCurrentProcess(),
        ProcessControlFlowGuardPolicy,
        &CfgPolicy,
        sizeof(CfgPolicy));
    Assert(isGetMitigationPolicySucceeded || !AutoSystemInfo::Data.IsCFGEnabled());
    return CfgPolicy.EnableControlFlowGuard && AutoSystemInfo::Data.IsCFGEnabled();
#else
    return false;
#endif
}


//Masking bits according to AutoSystemInfo::PageSize
#define PAGE_START_ADDR(address) ((size_t)(address) & ~(size_t)(AutoSystemInfo::PageSize - 1))
#define IS_16BYTE_ALIGNED(address) (((size_t)(address) & 0xF) == 0)
#define OFFSET_ADDR_WITHIN_PAGE(address) ((size_t)(address) & (AutoSystemInfo::PageSize - 1))

void ThreadContext::SetValidCallTargetForCFG(PVOID callTargetAddress, bool isSetValid)
{
#ifdef _CONTROL_FLOW_GUARD
    if (IsCFGEnabled())
    {
        AssertMsg(IS_16BYTE_ALIGNED(callTargetAddress), "callTargetAddress is not 16-byte page aligned?");

        PVOID startAddressOfPage = (PVOID) (PAGE_START_ADDR(callTargetAddress));
        size_t codeOffset = OFFSET_ADDR_WITHIN_PAGE(callTargetAddress);

        CFG_CALL_TARGET_INFO callTargetInfo[1];

        callTargetInfo[0].Offset = codeOffset;
        callTargetInfo[0].Flags = (isSetValid? CFG_CALL_TARGET_VALID : 0);

        AssertMsg((size_t)callTargetAddress - (size_t)startAddressOfPage <= AutoSystemInfo::PageSize - 1, "Only last bits corresponding to PageSize should be masked");
        AssertMsg((size_t)startAddressOfPage + (size_t)codeOffset == (size_t)callTargetAddress, "Wrong masking of address?");

        BOOL isCallTargetRegistrationSucceed = GetWinCoreMemoryLibrary()->SetProcessCallTargets(GetCurrentProcess(), startAddressOfPage, AutoSystemInfo::PageSize, 1, callTargetInfo);

        if (!isCallTargetRegistrationSucceed)
        {
            if (GetLastError() == ERROR_COMMITMENT_LIMIT)
            {
                //Throw OOM, if there is not enough virtual memory for paging (required for CFG BitMap)
                Js::Throw::OutOfMemory();
            }
            else
            {
                Js::Throw::InternalError();
            }
        }
#if DBG
        if (isSetValid)
        {
            _guard_check_icall((uintptr_t) callTargetAddress);
        }

        if (PHASE_TRACE1(Js::CFGPhase))
        {
            if (!isSetValid)
            {
                Output::Print(_u("DEREGISTER:"));
            }
            Output::Print(_u("CFGRegistration: StartAddr: 0x%p , Offset: 0x%x, TargetAddr: 0x%x \n"), (char*) startAddressOfPage, callTargetInfo[0].Offset, ((size_t) startAddressOfPage + (size_t) callTargetInfo[0].Offset));
            Output::Flush();
        }
#endif
    }
#endif // _CONTROL_FLOW_GUARD
}

// Despite the name, callers expect this to return the highest propid + 1.

uint ThreadContext::GetHighestPropertyNameIndex() const
{
    return propertyMap->GetLastIndex() + 1 + Js::InternalPropertyIds::Count;
}

#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
void ThreadContext::ReportAndCheckLeaksOnProcessDetach()
{
    bool needReportOrCheck = false;
#ifdef LEAK_REPORT
    needReportOrCheck = needReportOrCheck || Js::Configuration::Global.flags.IsEnabled(Js::LeakReportFlag);
#endif
#ifdef CHECK_MEMORY_LEAK
    needReportOrCheck = needReportOrCheck ||
        (Js::Configuration::Global.flags.CheckMemoryLeak && MemoryLeakCheck::IsEnableOutput());
#endif

    if (!needReportOrCheck)
    {
        return;
    }

    // Report leaks even if this is a force termination and we have not clean up the thread
    // This is call during process detach. No one should be creating new thread context.
    // So don't need to take the lock
    ThreadContext * current = GetThreadContextList();

    while (current)
    {
#if DBG
        current->pageAllocator.ClearConcurrentThreadId();
#endif
        Recycler * recycler = current->GetRecycler();

#ifdef LEAK_REPORT
        if (Js::Configuration::Global.flags.IsEnabled(Js::LeakReportFlag))
        {
            AUTO_LEAK_REPORT_SECTION(Js::Configuration::Global.flags, _u("Thread Context (%p): Process Termination (TID: %d)"), current, current->threadId);
            LeakReport::DumpUrl(current->threadId);

            // Heuristically figure out which one is the root tracker script engine
            // and force close on it
            if (current->rootTrackerScriptContext != nullptr)
            {
                current->rootTrackerScriptContext->Close(false);
            }
            recycler->ReportLeaksOnProcessDetach();
        }
#endif
#ifdef CHECK_MEMORY_LEAK
        recycler->CheckLeaksOnProcessDetach(_u("Process Termination"));
#endif
        current = current->Next();
    }
}
#endif

#ifdef LEAK_REPORT
void
ThreadContext::SetRootTrackerScriptContext(Js::ScriptContext * scriptContext)
{
    Assert(this->rootTrackerScriptContext == nullptr);
    this->rootTrackerScriptContext = scriptContext;
    scriptContext->isRootTrackerScriptContext = true;
}

void
ThreadContext::ClearRootTrackerScriptContext(Js::ScriptContext * scriptContext)
{
    Assert(this->rootTrackerScriptContext == scriptContext);
    this->rootTrackerScriptContext->isRootTrackerScriptContext = false;
    this->rootTrackerScriptContext = nullptr;
}
#endif

#ifdef ENABLE_BASIC_TELEMETRY
#if ENABLE_NATIVE_CODEGEN
JITTimer::JITTimer()
{
    Reset();
}

double JITTimer::Now()
{
    return timer.Now();
}

JITStats JITTimer::GetStats()
{
     return stats;
}

void JITTimer::Reset()
{
     stats = { 0 };
}

void JITTimer::LogTime(double ms)
{
    if (ms <= 5.0)
    {
        stats.lessThan5ms++;
    }
    else if (ms <= 10.0)
    {
        stats.within5And10ms++;
    }
    else if (ms <= 20.0)
    {
        stats.within10And20ms++;
    }
    else if (ms <= 50.0)
    {
        stats.within20And50ms++;
    }
    else if (ms <= 100.0)
    {
        stats.within50And100ms++;
    }
    else if (ms <= 300.0)
    {
        stats.within100And300ms++;
    }
    else
    {
        stats.greaterThan300ms++;
    }
}
#endif // NATIVE_CODE_GEN

ParserTimer::ParserTimer()
{
    Reset();
}

double ParserTimer::Now()
{
    return timer.Now();
}

ParserStats ParserTimer::GetStats()
{
    return stats;
}

void ParserTimer::Reset()
{
    stats = { 0 };
}

void ParserTimer::LogTime(double ms)
{
    if (ms <= 1.0)
    {
        stats.lessThan1ms++;
    }
    else if (ms <= 3.0)
    {
        stats.within1And3ms++;
    }
    else if (ms <= 10.0)
    {
        stats.within3And10ms++;
    }
    else if (ms <= 20.0)
    {
        stats.within10And20ms++;
    }
    else if (ms <= 50.0)
    {
        stats.within20And50ms++;
    }
    else if (ms <= 100.0)
    {
        stats.within50And100ms++;
    }
    else if (ms <= 300.0)
    {
        stats.within100And300ms++;
    }
    else
    {
        stats.greaterThan300ms++;
    }
}
#endif // ENABLE_BASIC_TELEMETRY

AutoTagNativeLibraryEntry::AutoTagNativeLibraryEntry(Js::RecyclableObject* function, Js::CallInfo callInfo, PCWSTR name, void* addr)
{
    // Save function/callInfo values (for StackWalker). Compiler may stackpack/optimize them for built-in native functions.
    entry.function = function;
    entry.callInfo = callInfo;
    entry.name = name;
    entry.addr = addr;

    ThreadContext* threadContext = function->GetScriptContext()->GetThreadContext();
    threadContext->PushNativeLibraryEntry(&entry);
}

AutoTagNativeLibraryEntry::~AutoTagNativeLibraryEntry()
{
    ThreadContext* threadContext = entry.function->GetScriptContext()->GetThreadContext();
    Assert(threadContext->PeekNativeLibraryEntry() == &entry);
    threadContext->PopNativeLibraryEntry();
}
