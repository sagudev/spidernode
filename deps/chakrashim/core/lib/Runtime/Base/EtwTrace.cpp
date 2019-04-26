//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#ifdef ENABLE_JS_ETW
#include "Core/EtwTraceCore.h"
#include "Base/EtwTrace.h"

using namespace Js;

//
// This C style callback is invoked by ETW when a trace session is started/stopped
// by an ETW controller for the Jscript and MSHTML providers.
//

void EtwCallbackApi::OnSessionChange(ULONG controlCode, PVOID callbackContext)
{
    PMCGEN_TRACE_CONTEXT context = (PMCGEN_TRACE_CONTEXT)callbackContext;

    // A manifest based provider can be enabled to multiple event tracing sessions
    // As long as there is at least 1 enabled session, isEnabled will be TRUE
    // We only care about Jscript events.
    if(context->RegistrationHandle == Microsoft_JScriptHandle)
    {
        switch(controlCode)
        {
        case EVENT_CONTROL_CODE_ENABLE_PROVIDER:
        case EVENT_CONTROL_CODE_CAPTURE_STATE:
            if(McGenLevelKeywordEnabled(context,
                TRACE_LEVEL_INFORMATION,
                JSCRIPT_RUNDOWNSTART_KEYWORD))
            {
                EtwTrace::PerformRundown(/*start*/ true);
            }

            if(McGenLevelKeywordEnabled(context,
                TRACE_LEVEL_INFORMATION,
                JSCRIPT_RUNDOWNEND_KEYWORD))
            {
                EtwTrace::PerformRundown(/*start*/ false);
            }
            break;
        case EVENT_CONTROL_CODE_DISABLE_PROVIDER:
            break; // Do Nothing
        }
    }
}

//
// Registers the ETW provider - this is usually done on Jscript DLL load
// After registration, we will receive callbacks when ETW tracing is enabled/disabled.
//
void EtwTrace::Register()
{
    EtwTraceCore::Register();

#ifdef TEST_ETW_EVENTS
    TestEtwEventSink::Load();
#endif
}

//
// Unregister to ensure we do not get callbacks.
//
void EtwTrace::UnRegister()
{
    EtwTraceCore::UnRegister();

#ifdef TEST_ETW_EVENTS
    TestEtwEventSink::Unload();
#endif
}

//
// Enumerate through all the script contexts in the process and log events
// for each function loaded. Depending on the argument, start or end events are logged.
// In particular, a rundown is needed for the 'Attach' scenario of profiling.
//
void EtwTrace::PerformRundown(bool start)
{
    // Lock threadContext list during etw rundown
    AutoCriticalSection autoThreadContextCs(ThreadContext::GetCriticalSection());

    ThreadContext * threadContext = ThreadContext::GetThreadContextList();
    if(start)
    {
        JS_ETW(EventWriteDCStartInit());
    }
    else
    {
        JS_ETW(EventWriteDCEndInit());
    }

    while(threadContext != nullptr)
    {
        // Take etw rundown lock on this thread context
        AutoCriticalSection autoEtwRundownCs(threadContext->GetEtwRundownCriticalSection());

        ScriptContext* scriptContext = threadContext->GetScriptContextList();
        while(scriptContext != NULL)
        {
            if(scriptContext->IsClosed())
            {
                scriptContext = scriptContext->next;
                continue;
            }
            if(start)
            {
                JS_ETW(EventWriteScriptContextDCStart(scriptContext));

                if(scriptContext->GetSourceContextInfoMap() != nullptr)
                {
                    scriptContext->GetSourceContextInfoMap()->Map( [=] (DWORD_PTR sourceContext, SourceContextInfo * sourceContextInfo)
                    {
                        if (sourceContext != Constants::NoHostSourceContext)
                        {
                            JS_ETW(LogSourceEvent(EventWriteSourceDCStart,
                                sourceContext,
                                scriptContext,
                                /* sourceFlags*/ 0,
                                sourceContextInfo->url));
                        }
                    });
                }
            }
            else
            {
                JS_ETW(EventWriteScriptContextDCEnd(scriptContext));

                if(scriptContext->GetSourceContextInfoMap() != nullptr)
                {
                    scriptContext->GetSourceContextInfoMap()->Map( [=] (DWORD_PTR sourceContext, SourceContextInfo * sourceContextInfo)
                    {
                        if (sourceContext != Constants::NoHostSourceContext)
                        {
                            JS_ETW(LogSourceEvent(EventWriteSourceDCEnd,
                                sourceContext,
                                scriptContext,
                                /* sourceFlags*/ 0,
                                sourceContextInfo->url));
                        }
                    });
                }
            }

            scriptContext->MapFunction([&start] (FunctionBody* body)
            {
                if(body->HasInterpreterThunkGenerated())
                {
                    if(start)
                    {
                        LogMethodInterpretedThunkEvent(EventWriteMethodDCStart, body);
                    }
                    else
                    {
                        LogMethodInterpretedThunkEvent(EventWriteMethodDCEnd, body);
                    }
                }

                body->MapEntryPoints([&](int index, FunctionEntryPointInfo * entryPoint)
                {
                    if(entryPoint->IsCodeGenDone())
                    {
                        if (start)
                        {
                            LogMethodNativeEvent(EventWriteMethodDCStart, body, entryPoint);
                        }
                        else
                        {
                            LogMethodNativeEvent(EventWriteMethodDCEnd, body, entryPoint);
                        }
                    }
                });

                body->MapLoopHeadersWithLock([&](uint loopNumber, LoopHeader* header)
                {
                    header->MapEntryPoints([&](int index, LoopEntryPointInfo * entryPoint)
                    {
                        if(entryPoint->IsCodeGenDone())
                        {
                            if(start)
                            {
                                LogLoopBodyEventBG(EventWriteMethodDCStart, body, header, entryPoint, ((uint16)body->GetLoopNumberWithLock(header)));
                            }
                            else
                            {
                                LogLoopBodyEventBG(EventWriteMethodDCEnd, body, header, entryPoint, ((uint16)body->GetLoopNumberWithLock(header)));
                            }
                        }
                    });
                });
            });

            scriptContext = scriptContext->next;
        }
        if (EventEnabledJSCRIPT_HOSTING_CEO_START())
        {
            threadContext->EtwLogPropertyIdList();
        }

        threadContext = threadContext->Next();
    }
    if(start)
    {
        JS_ETW(EventWriteDCStartComplete());
    }
    else
    {
        JS_ETW(EventWriteDCEndComplete());
    }
}

//
// Returns an ID for the source file of the function.
//
DWORD_PTR EtwTrace::GetSourceId(FunctionBody* body)
{
    DWORD_PTR sourceId = body->GetHostSourceContext();

    // For dynamic scripts - use fixed source ID of -1.
    // TODO: Find a way to generate unique ID for dynamic scripts.
    if(sourceId == Js::Constants::NoHostSourceContext)
    {
        sourceId = (DWORD_PTR)-1;
    }
    return sourceId;
}

//
// Returns an ID to identify the function.
//
uint EtwTrace::GetFunctionId(FunctionProxy* body)
{
    return body->GetFunctionNumber();
}

void EtwTrace::LogSourceUnloadEvents(ScriptContext* scriptContext)
{
    if(scriptContext->GetSourceContextInfoMap() != nullptr)
    {
        scriptContext->GetSourceContextInfoMap()->Map( [&] (DWORD_PTR sourceContext, SourceContextInfo * sourceContextInfo)
        {
            if(sourceContext != Constants::NoHostSourceContext)
            {
                JS_ETW(LogSourceEvent(EventWriteSourceUnload,
                    sourceContext,
                    scriptContext,
                    /* sourceFlags*/ 0,
                    sourceContextInfo->url));
            }
        });
    }

    JS_ETW(EventWriteScriptContextUnload(scriptContext));
}

void EtwTrace::LogMethodInterpreterThunkLoadEvent(FunctionBody* body)
{
    LogMethodInterpretedThunkEvent(EventWriteMethodLoad, body);
}

void EtwTrace::LogMethodNativeLoadEvent(FunctionBody* body, FunctionEntryPointInfo* entryPoint)
{
    LogMethodNativeEvent(EventWriteMethodLoad, body, entryPoint);
}

void EtwTrace::LogLoopBodyLoadEvent(FunctionBody* body, LoopHeader* loopHeader, LoopEntryPointInfo* entryPoint, uint16 loopNumber)
{
    Assert(loopNumber == body->GetLoopNumberWithLock(loopHeader));
    LogLoopBodyEventBG(EventWriteMethodLoad, body, loopHeader, entryPoint, loopNumber);
}

void EtwTrace::LogMethodInterpreterThunkUnloadEvent(FunctionBody* body)
{
    LogMethodInterpretedThunkEvent(EventWriteMethodUnload, body);
}


void EtwTrace::LogMethodNativeUnloadEvent(FunctionBody* body, FunctionEntryPointInfo* entryPoint)
{
    LogMethodNativeEvent(EventWriteMethodUnload, body, entryPoint);
}

void EtwTrace::LogLoopBodyUnloadEvent(FunctionBody* body, LoopHeader* loopHeader, LoopEntryPointInfo* entryPoint)
{
    LogLoopBodyEvent(EventWriteMethodUnload, body, loopHeader, entryPoint);
}


//
// Logs the runtime script context load event
//
void EtwTrace::LogScriptContextLoadEvent(ScriptContext* scriptContext)
{
    JS_ETW(EventWriteScriptContextLoad(
        scriptContext));
}

//
// Logs the runtime source module load event.
//
void EtwTrace::LogSourceModuleLoadEvent(ScriptContext* scriptContext, DWORD_PTR sourceContext, _In_z_ const char16* url)
{
    AssertMsg(sourceContext != Constants::NoHostSourceContext, "We should not be logged this if there is no source code available");

    JS_ETW(LogSourceEvent(EventWriteSourceLoad,
        sourceContext,
        scriptContext,
        /* sourceFlags*/ 0,
        url));
}

//
// This emulates the logic used by the F12 profiler to give names to functions
//
const char16* EtwTrace::GetFunctionName(FunctionBody* body)
{
    return body->GetExternalDisplayName();
}

size_t EtwTrace::GetLoopBodyName(_In_ FunctionBody* body, _In_ LoopHeader* loopHeader, _Out_writes_opt_z_(size) char16* nameBuffer, _In_ size_t size)
{
    return body->GetLoopBodyName(body->GetLoopNumber(loopHeader), nameBuffer, size);
}

_Success_(return == 0)
size_t EtwTrace::GetSimpleJitFunctionName(
    Js::FunctionBody *const body,
    _Out_writes_opt_z_(nameCharCapacity) char16 *const name,
    const size_t nameCharCapacity)
{
    Assert(body);
    Assert(name);
    Assert(nameCharCapacity != 0);

    const char16 *const suffix = _u("Simple");
    const size_t suffixCharLength = _countof(_u("Simple")) - 1;

    const char16 *const functionName = GetFunctionName(body);
    const size_t functionNameCharLength = wcslen(functionName);
    const size_t requiredCharCapacity = functionNameCharLength + suffixCharLength + 1;
    if(requiredCharCapacity > nameCharCapacity)
    {
        return requiredCharCapacity;
    }

    wcscpy_s(name, nameCharCapacity, functionName);
    wcscpy_s(&name[functionNameCharLength], nameCharCapacity - functionNameCharLength, suffix);
    return 0;
}

#endif
