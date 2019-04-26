//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "JsrtPch.h"
#include "JsrtInternal.h"
#include "RuntimeDebugPch.h"
#include "ThreadContextTlsEntry.h"
#include "JsrtDebugUtils.h"
#include "Codex/Utf8Helper.h"

#define VALIDATE_IS_DEBUGGING(jsrtDebugManager) \
    if (jsrtDebugManager == nullptr || !jsrtDebugManager->IsDebugEventCallbackSet()) \
    { \
        return JsErrorDiagNotInDebugMode; \
    }

#define VALIDATE_RUNTIME_IS_AT_BREAK(runtime) \
    if (runtime->GetThreadContext()->GetDebugManager() == nullptr || !runtime->GetThreadContext()->GetDebugManager()->IsAtDispatchHalt()) \
    { \
        return JsErrorDiagNotAtBreak; \
    }

#define VALIDATE_RUNTIME_STATE_FOR_START_STOP_DEBUGGING(threadContext) \
    if (threadContext->GetRecycler() && threadContext->GetRecycler()->IsHeapEnumInProgress()) \
    { \
        return JsErrorHeapEnumInProgress; \
    } \
    else if (threadContext->IsInThreadServiceCallback()) \
    { \
        return JsErrorInThreadServiceCallback; \
    } \
    else if (threadContext->IsInScript()) \
    { \
        return JsErrorRuntimeInUse; \
    } \
    ThreadContextScope scope(threadContext); \
    if (!scope.IsValid()) \
    { \
        return JsErrorWrongThread; \
    }

CHAKRA_API JsDiagStartDebugging(
    _In_ JsRuntimeHandle runtimeHandle,
    _In_ JsDiagDebugEventCallback debugEventCallback,
    _In_opt_ void* callbackState)
{
    return GlobalAPIWrapper([&]() -> JsErrorCode {

        VALIDATE_INCOMING_RUNTIME_HANDLE(runtimeHandle);

        PARAM_NOT_NULL(debugEventCallback);

        JsrtRuntime * runtime = JsrtRuntime::FromHandle(runtimeHandle);
        ThreadContext * threadContext = runtime->GetThreadContext();

        VALIDATE_RUNTIME_STATE_FOR_START_STOP_DEBUGGING(threadContext);

        if (runtime->GetJsrtDebugManager() != nullptr && runtime->GetJsrtDebugManager()->IsDebugEventCallbackSet())
        {
            return JsErrorDiagAlreadyInDebugMode;
        }

        // Create the debug object to save callback function and data
        runtime->EnsureJsrtDebugManager();

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        jsrtDebugManager->SetDebugEventCallback(debugEventCallback, callbackState);

        if (threadContext->GetDebugManager() != nullptr)
        {
            threadContext->GetDebugManager()->SetLocalsDisplayFlags(Js::DebugManager::LocalsDisplayFlags::LocalsDisplayFlags_NoGroupMethods);
        }

        for (Js::ScriptContext *scriptContext = runtime->GetThreadContext()->GetScriptContextList();
        scriptContext != nullptr && !scriptContext->IsClosed();
            scriptContext = scriptContext->next)
        {
            Assert(!scriptContext->IsScriptContextInDebugMode());

            Js::DebugContext* debugContext = scriptContext->GetDebugContext();

            if (debugContext->GetHostDebugContext() == nullptr)
            {
                debugContext->SetHostDebugContext(jsrtDebugManager);
            }

            if (FAILED(scriptContext->OnDebuggerAttached()))
            {
                Debugger_AttachDetach_fatal_error(); // Inconsistent state, we can't continue from here
                return JsErrorFatal;
            }

            Js::ProbeContainer* probeContainer = debugContext->GetProbeContainer();
            probeContainer->InitializeInlineBreakEngine(jsrtDebugManager);
            probeContainer->InitializeDebuggerScriptOptionCallback(jsrtDebugManager);
        }

        return JsNoError;
    });
}

CHAKRA_API JsDiagStopDebugging(
    _In_ JsRuntimeHandle runtimeHandle,
    _Out_ void** callbackState)
{
    return GlobalAPIWrapper([&]() -> JsErrorCode {

        VALIDATE_INCOMING_RUNTIME_HANDLE(runtimeHandle);

        PARAM_NOT_NULL(callbackState);

        *callbackState = nullptr;

        JsrtRuntime * runtime = JsrtRuntime::FromHandle(runtimeHandle);
        ThreadContext * threadContext = runtime->GetThreadContext();

        VALIDATE_RUNTIME_STATE_FOR_START_STOP_DEBUGGING(threadContext);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        for (Js::ScriptContext *scriptContext = runtime->GetThreadContext()->GetScriptContextList();
        scriptContext != nullptr && !scriptContext->IsClosed();
            scriptContext = scriptContext->next)
        {
            Assert(scriptContext->IsScriptContextInDebugMode());

            if (FAILED(scriptContext->OnDebuggerDetached()))
            {
                Debugger_AttachDetach_fatal_error(); // Inconsistent state, we can't continue from here
                return JsErrorFatal;
            }

            Js::DebugContext* debugContext = scriptContext->GetDebugContext();

            Js::ProbeContainer* probeContainer = debugContext->GetProbeContainer();
            probeContainer->UninstallInlineBreakpointProbe(nullptr);
            probeContainer->UninstallDebuggerScriptOptionCallback();

            jsrtDebugManager->ClearBreakpointDebugDocumentDictionary();
        }

        *callbackState = jsrtDebugManager->GetAndClearCallbackState();

        return JsNoError;
    });
}

CHAKRA_API JsDiagGetScripts(
    _Out_ JsValueRef *scriptsArray)
{
    return ContextAPIWrapper<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(scriptsArray);

        *scriptsArray = JS_INVALID_REFERENCE;

        JsrtContext *currentContext = JsrtContext::GetCurrent();

        JsrtDebugManager* jsrtDebugManager = currentContext->GetRuntime()->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        Js::JavascriptArray* scripts = jsrtDebugManager->GetScripts(scriptContext);

        if (scripts != nullptr)
        {
            *scriptsArray = scripts;
            return JsNoError;
        }

        return JsErrorDiagUnableToPerformAction;
    });
}

CHAKRA_API JsDiagGetSource(
    _In_ unsigned int scriptId,
    _Out_ JsValueRef *source)
{
    return ContextAPIWrapper<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(source);

        *source = JS_INVALID_REFERENCE;

        JsrtContext *currentContext = JsrtContext::GetCurrent();

        JsrtDebugManager* jsrtDebugManager = currentContext->GetRuntime()->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        Js::DynamicObject* sourceObject = jsrtDebugManager->GetSource(scriptContext, scriptId);

        if (sourceObject != nullptr)
        {
            *source = sourceObject;
            return JsNoError;
        }

        return JsErrorInvalidArgument;
    });
}

CHAKRA_API JsDiagRequestAsyncBreak(
    _In_ JsRuntimeHandle runtimeHandle)
{
    return GlobalAPIWrapper([&]() -> JsErrorCode {

        VALIDATE_INCOMING_RUNTIME_HANDLE(runtimeHandle);

        JsrtRuntime * runtime = JsrtRuntime::FromHandle(runtimeHandle);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        for (Js::ScriptContext *scriptContext = runtime->GetThreadContext()->GetScriptContextList();
        scriptContext != nullptr && !scriptContext->IsClosed();
            scriptContext = scriptContext->next)
        {
            jsrtDebugManager->EnableAsyncBreak(scriptContext);
        }

        return JsNoError;
    });
}

CHAKRA_API JsDiagGetBreakpoints(
    _Out_ JsValueRef *breakpoints)
{
    return GlobalAPIWrapper([&]() -> JsErrorCode {

        PARAM_NOT_NULL(breakpoints);

        *breakpoints = JS_INVALID_REFERENCE;

        JsrtContext *currentContext = JsrtContext::GetCurrent();

        Js::JavascriptArray* bpsArray = currentContext->GetScriptContext()->GetLibrary()->CreateArray();

        JsrtRuntime * runtime = currentContext->GetRuntime();

        ThreadContextScope scope(runtime->GetThreadContext());

        if (!scope.IsValid())
        {
            return JsErrorWrongThread;
        }

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        for (Js::ScriptContext *scriptContext = runtime->GetThreadContext()->GetScriptContextList();
        scriptContext != nullptr && !scriptContext->IsClosed();
            scriptContext = scriptContext->next)
        {
            jsrtDebugManager->GetBreakpoints(&bpsArray, scriptContext);
        }

        *breakpoints = bpsArray;

        return JsNoError;
    });
}

CHAKRA_API JsDiagSetBreakpoint(
    _In_ unsigned int scriptId,
    _In_ unsigned int lineNumber,
    _In_ unsigned int columnNumber,
    _Out_ JsValueRef *breakpoint)
{
    return GlobalAPIWrapper([&]() -> JsErrorCode {

        PARAM_NOT_NULL(breakpoint);

        *breakpoint = JS_INVALID_REFERENCE;

        JsrtContext *currentContext = JsrtContext::GetCurrent();

        JsrtRuntime * runtime = currentContext->GetRuntime();

        ThreadContextScope scope(runtime->GetThreadContext());

        if (!scope.IsValid())
        {
            return JsErrorWrongThread;
        }

        VALIDATE_IS_DEBUGGING(runtime->GetJsrtDebugManager());

        Js::Utf8SourceInfo* utf8SourceInfo = nullptr;

        for (Js::ScriptContext *scriptContext = runtime->GetThreadContext()->GetScriptContextList();
        scriptContext != nullptr && utf8SourceInfo == nullptr && !scriptContext->IsClosed();
            scriptContext = scriptContext->next)
        {
            scriptContext->MapScript([&](Js::Utf8SourceInfo* sourceInfo) -> bool
            {
                if (sourceInfo->GetSourceInfoId() == scriptId)
                {
                    utf8SourceInfo = sourceInfo;
                    return true;
                }
                return false;
            });
        }

        if (utf8SourceInfo != nullptr && utf8SourceInfo->HasDebugDocument())
        {
            JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

            Js::DynamicObject* bpObject = jsrtDebugManager->SetBreakPoint(currentContext->GetScriptContext(), utf8SourceInfo, lineNumber, columnNumber);

            if(bpObject != nullptr)
            {
                *breakpoint = bpObject;
                return JsNoError;
            }

            return JsErrorDiagUnableToPerformAction;
        }

        return JsErrorDiagObjectNotFound;
    });
}

CHAKRA_API JsDiagRemoveBreakpoint(
    _In_ unsigned int breakpointId)
{
    return GlobalAPIWrapper([&]() -> JsErrorCode {

        JsrtContext *currentContext = JsrtContext::GetCurrent();

        JsrtRuntime* runtime = currentContext->GetRuntime();

        ThreadContextScope scope(runtime->GetThreadContext());

        if (!scope.IsValid())
        {
            return JsErrorWrongThread;
        }

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        if (!jsrtDebugManager->RemoveBreakpoint(breakpointId))
        {
            return JsErrorInvalidArgument;
        }

        return JsNoError;
    });
}

CHAKRA_API JsDiagSetBreakOnException(
    _In_ JsRuntimeHandle runtimeHandle,
    _In_ JsDiagBreakOnExceptionAttributes exceptionAttributes)
{
    return GlobalAPIWrapper([&]() -> JsErrorCode {

        VALIDATE_INCOMING_RUNTIME_HANDLE(runtimeHandle);

        JsrtRuntime * runtime = JsrtRuntime::FromHandle(runtimeHandle);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        jsrtDebugManager->SetBreakOnException(exceptionAttributes);

        return JsNoError;
    });
}

CHAKRA_API JsDiagGetBreakOnException(
    _In_ JsRuntimeHandle runtimeHandle,
    _Out_ JsDiagBreakOnExceptionAttributes* exceptionAttributes)
{
    return GlobalAPIWrapper([&]() -> JsErrorCode {

        VALIDATE_INCOMING_RUNTIME_HANDLE(runtimeHandle);

        PARAM_NOT_NULL(exceptionAttributes);

        *exceptionAttributes = JsDiagBreakOnExceptionAttributeNone;

        JsrtRuntime * runtime = JsrtRuntime::FromHandle(runtimeHandle);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        *exceptionAttributes = jsrtDebugManager->GetBreakOnException();

        return JsNoError;
    });
}

CHAKRA_API JsDiagSetStepType(
    _In_ JsDiagStepType stepType)
{
    return ContextAPIWrapper<true>([&](Js::ScriptContext * scriptContext) -> JsErrorCode {

        JsrtContext *currentContext = JsrtContext::GetCurrent();
        JsrtRuntime* runtime = currentContext->GetRuntime();

        VALIDATE_RUNTIME_IS_AT_BREAK(runtime);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        if (stepType == JsDiagStepTypeStepIn)
        {
            jsrtDebugManager->SetResumeType(BREAKRESUMEACTION_STEP_INTO);
        }
        else if (stepType == JsDiagStepTypeStepOut)
        {
            jsrtDebugManager->SetResumeType(BREAKRESUMEACTION_STEP_OUT);
        }
        else if (stepType == JsDiagStepTypeStepOver)
        {
            jsrtDebugManager->SetResumeType(BREAKRESUMEACTION_STEP_OVER);
        }
        else if (stepType == JsDiagStepTypeStepBack)
        {
#if ENABLE_TTD_DEBUGGING
            ThreadContext* threadContext = runtime->GetThreadContext();
            if(threadContext->TTDLog == nullptr || !threadContext->TTDLog->ShouldPerformDebugAction_BreakPointAction())
            {
                return JsErrorInvalidArgument;
            }

            TTD::TTDebuggerSourceLocation bpLocation;
            threadContext->TTDLog->GetPreviousTimeAndPositionForDebugger(bpLocation);
            threadContext->TTDLog->SetPendingTTDBPInfo(bpLocation);

            threadContext->TTDLog->LoadBPListForContextRecreate();

            //don't worry about BP suppression because we are just going to throw after we return

            jsrtDebugManager->SetResumeType(BREAKRESUMEACTION_CONTINUE);
#else
            return JsErrorInvalidArgument;
#endif
        }

        return JsNoError;
    });
}

CHAKRA_API JsDiagGetFunctionPosition(
    _In_ JsValueRef function,
    _Out_ JsValueRef *functionPosition)
{
    return ContextAPIWrapper<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        VALIDATE_INCOMING_REFERENCE(function, scriptContext);
        PARAM_NOT_NULL(functionPosition);

        *functionPosition = JS_INVALID_REFERENCE;

        if (!Js::RecyclableObject::Is(function) || !Js::ScriptFunction::Is(function))
        {
            return JsErrorInvalidArgument;
        }

        Js::ScriptFunction* jsFunction = Js::ScriptFunction::FromVar(function);

        Js::FunctionBody* functionBody = jsFunction->GetFunctionBody();
        if (functionBody != nullptr)
        {
            Js::Utf8SourceInfo* utf8SourceInfo = functionBody->GetUtf8SourceInfo();
            if (utf8SourceInfo != nullptr && !utf8SourceInfo->GetIsLibraryCode())
            {
                ULONG lineNumber = functionBody->GetLineNumber();
                ULONG columnNumber = functionBody->GetColumnNumber();
                uint startOffset = functionBody->GetStatementStartOffset(0);
                ULONG firstStatementLine;
                LONG firstStatementColumn;

                if (functionBody->GetLineCharOffsetFromStartChar(startOffset, &firstStatementLine, &firstStatementColumn))
                {
                    Js::DynamicObject* funcPositionObject = (Js::DynamicObject*)Js::CrossSite::MarshalVar(utf8SourceInfo->GetScriptContext(), scriptContext->GetLibrary()->CreateObject());

                    if (funcPositionObject != nullptr)
                    {
                        JsrtDebugUtils::AddScriptIdToObject(funcPositionObject, utf8SourceInfo);
                        JsrtDebugUtils::AddFileNameOrScriptTypeToObject(funcPositionObject, utf8SourceInfo);
                        JsrtDebugUtils::AddPropertyToObject(funcPositionObject, JsrtDebugPropertyId::line, (uint32) lineNumber, scriptContext);
                        JsrtDebugUtils::AddPropertyToObject(funcPositionObject, JsrtDebugPropertyId::column, (uint32) columnNumber, scriptContext);
                        JsrtDebugUtils::AddPropertyToObject(funcPositionObject, JsrtDebugPropertyId::firstStatementLine, (uint32) firstStatementLine, scriptContext);
                        JsrtDebugUtils::AddPropertyToObject(funcPositionObject, JsrtDebugPropertyId::firstStatementColumn, (int32) firstStatementColumn, scriptContext);

                        *functionPosition = funcPositionObject;

                        return JsNoError;
                    }
                }
            }
        }

        return JsErrorDiagObjectNotFound;
    });
}

CHAKRA_API JsDiagGetStackTrace(
    _Out_ JsValueRef *stackTrace)
{
    return ContextAPIWrapper<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(stackTrace);

        *stackTrace = JS_INVALID_REFERENCE;

        JsrtContext* context = JsrtContext::GetCurrent();
        JsrtRuntime* runtime = context->GetRuntime();

        VALIDATE_RUNTIME_IS_AT_BREAK(runtime);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        *stackTrace = jsrtDebugManager->GetStackFrames(scriptContext);

        return JsNoError;
    });
}

CHAKRA_API JsDiagGetStackProperties(
    _In_ unsigned int stackFrameIndex,
    _Out_ JsValueRef *properties)
{
    return ContextAPIWrapper<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(properties);

        *properties = JS_INVALID_REFERENCE;

        JsrtContext* context = JsrtContext::GetCurrent();
        JsrtRuntime* runtime = context->GetRuntime();

        VALIDATE_RUNTIME_IS_AT_BREAK(runtime);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        JsrtDebuggerStackFrame* debuggerStackFrame = nullptr;
        if (!jsrtDebugManager->TryGetFrameObjectFromFrameIndex(scriptContext, stackFrameIndex, &debuggerStackFrame))
        {
            return JsErrorDiagObjectNotFound;
        }

        Js::DynamicObject* localsObject = debuggerStackFrame->GetLocalsObject(scriptContext);

        if (localsObject != nullptr)
        {
            *properties = localsObject;
            return JsNoError;
        }

        return JsErrorDiagUnableToPerformAction;
    });
}

CHAKRA_API JsDiagGetProperties(
    _In_ unsigned int objectHandle,
    _In_ unsigned int fromCount,
    _In_ unsigned int totalCount,
    _Out_ JsValueRef *propertiesObject)
{

    return ContextAPIWrapper<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(propertiesObject);

        *propertiesObject = JS_INVALID_REFERENCE;

        JsrtContext* context = JsrtContext::GetCurrent();
        JsrtRuntime* runtime = context->GetRuntime();

        VALIDATE_RUNTIME_IS_AT_BREAK(runtime);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        JsrtDebuggerObjectBase* debuggerObject = nullptr;
        if (!jsrtDebugManager->GetDebuggerObjectsManager()->TryGetDebuggerObjectFromHandle(objectHandle, &debuggerObject) || debuggerObject == nullptr)
        {
            return JsErrorDiagInvalidHandle;
        }

        Js::DynamicObject* properties = debuggerObject->GetChildren(scriptContext, fromCount, totalCount);

        if (properties != nullptr)
        {
            *propertiesObject = properties;
            return JsNoError;
        }

        return JsErrorDiagUnableToPerformAction;
    });
}

CHAKRA_API JsDiagGetObjectFromHandle(
    _In_ unsigned int objectHandle,
    _Out_ JsValueRef *handleObject)
{
    return ContextAPIWrapper<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(handleObject);

        *handleObject = JS_INVALID_REFERENCE;

        JsrtContext* context = JsrtContext::GetCurrent();
        JsrtRuntime* runtime = context->GetRuntime();

        VALIDATE_RUNTIME_IS_AT_BREAK(runtime);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        JsrtDebuggerObjectBase* debuggerObject = nullptr;
        if (!jsrtDebugManager->GetDebuggerObjectsManager()->TryGetDebuggerObjectFromHandle(objectHandle, &debuggerObject) || debuggerObject == nullptr)
        {
            return JsErrorDiagInvalidHandle;
        }

        Js::DynamicObject* object = debuggerObject->GetJSONObject(scriptContext);

        if (object != nullptr)
        {
            *handleObject = object;
            return JsNoError;
        }

        return JsErrorDiagUnableToPerformAction;
    });
}

CHAKRA_API JsDiagEvaluate(
    _In_ const wchar_t *expression,
    _In_ unsigned int stackFrameIndex,
    _Out_ JsValueRef *evalResult)
{
    return ContextAPINoScriptWrapper([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(expression);
        PARAM_NOT_NULL(evalResult);

        *evalResult = JS_INVALID_REFERENCE;

        JsrtContext* context = JsrtContext::GetCurrent();
        JsrtRuntime* runtime = context->GetRuntime();

        VALIDATE_RUNTIME_IS_AT_BREAK(runtime);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        JsrtDebuggerStackFrame* debuggerStackFrame = nullptr;
        if (!jsrtDebugManager->TryGetFrameObjectFromFrameIndex(scriptContext, stackFrameIndex, &debuggerStackFrame))
        {
            return JsErrorDiagObjectNotFound;
        }

        size_t len = wcslen(expression);
        if (len != static_cast<int>(len))
        {
            return JsErrorInvalidArgument;
        }

        Js::DynamicObject* result = nullptr;
        bool success = debuggerStackFrame->Evaluate(scriptContext, expression, static_cast<int>(len), false, &result);

        if (result != nullptr)
        {
            *evalResult = result;
        }

        return success ? JsNoError : JsErrorScriptException;

    }, false /*allowInObjectBeforeCollectCallback*/, true /*scriptExceptionAllowed*/);
}

CHAKRA_API JsDiagEvaluateUtf8(
    _In_ const char *expression,
    _In_ unsigned int stackFrameIndex,
    _Out_ JsValueRef *evalResult)
{
    PARAM_NOT_NULL(expression);
    utf8::NarrowToWide wstr(expression, strlen(expression));
    if (!wstr)
    {
        return JsErrorOutOfMemory;
    }

    return JsDiagEvaluate(wstr, stackFrameIndex, evalResult);
}
