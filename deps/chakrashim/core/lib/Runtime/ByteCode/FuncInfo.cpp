//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

FuncInfo::FuncInfo(
    const char16 *name,
    ArenaAllocator *alloc,
    Scope *paramScope,
    Scope *bodyScope,
    ParseNode *pnode,
    Js::ParseableFunctionInfo* byteCodeFunction)
    : alloc(alloc),
    varRegsCount(0),
    constRegsCount(2),
    inArgsCount(0),
    innerScopeCount(0),
    currentInnerScopeIndex((uint)-1),
    firstTmpReg(Js::Constants::NoRegister),
    curTmpReg(Js::Constants::NoRegister),
    outArgsMaxDepth(0),
    outArgsCurrentExpr(0),
#if DBG
    outArgsDepth(0),
#endif
    name(name),
    nullConstantRegister(Js::Constants::NoRegister),
    undefinedConstantRegister(Js::Constants::NoRegister),
    trueConstantRegister(Js::Constants::NoRegister),
    falseConstantRegister(Js::Constants::NoRegister),
    thisPointerRegister(Js::Constants::NoRegister),
    superRegister(Js::Constants::NoRegister),
    superCtorRegister(Js::Constants::NoRegister),
    newTargetRegister(Js::Constants::NoRegister),
    envRegister(Js::Constants::NoRegister),
    frameObjRegister(Js::Constants::NoRegister),
    frameSlotsRegister(Js::Constants::NoRegister),
    paramSlotsRegister(Js::Constants::NoRegister),
    frameDisplayRegister(Js::Constants::NoRegister),
    funcObjRegister(Js::Constants::NoRegister),
    localClosureReg(Js::Constants::NoRegister),
    yieldRegister(Js::Constants::NoRegister),
    paramScope(paramScope),
    bodyScope(bodyScope),
    funcExprScope(nullptr),
    root(pnode),
    capturedSyms(nullptr),
    capturedSymMap(nullptr),
    currentChildFunction(nullptr),
    currentChildScope(nullptr),
    callsEval(false),
    childCallsEval(false),
    hasArguments(false),
    hasHeapArguments(false),
    isTopLevelEventHandler(false),
    hasLocalInClosure(false),
    hasClosureReference(false),
    hasGlobalReference(false),
    hasCachedScope(false),
    funcExprNameReference(false),
    applyEnclosesArgs(false),
    escapes(false),
    hasDeferredChild(false),
    childHasWith(false),
    hasLoop(false),
    hasEscapedUseNestedFunc(false),
    needEnvRegister(false),
    hasCapturedThis(false),
    staticFuncId(-1),
    inlineCacheMap(nullptr),
    slotProfileIdMap(alloc),
    argsPlaceHolderSlotCount(0),
    thisScopeSlot(Js::Constants::NoProperty),
    innerThisScopeSlot(Js::Constants::NoProperty),
    superScopeSlot(Js::Constants::NoProperty),
    innerSuperScopeSlot(Js::Constants::NoProperty),
    superCtorScopeSlot(Js::Constants::NoProperty),
    innerSuperCtorScopeSlot(Js::Constants::NoProperty),
    newTargetScopeSlot(Js::Constants::NoProperty),
    innerNewTargetScopeSlot(Js::Constants::NoProperty),
    isThisLexicallyCaptured(false),
    isSuperLexicallyCaptured(false),
    isSuperCtorLexicallyCaptured(false),
    isNewTargetLexicallyCaptured(false),
    inlineCacheCount(0),
    rootObjectLoadInlineCacheCount(0),
    rootObjectLoadMethodInlineCacheCount(0),
    rootObjectStoreInlineCacheCount(0),
    isInstInlineCacheCount(0),
    referencedPropertyIdCount(0),
    argumentsSymbol(nullptr),
    innerArgumentsSymbol(nullptr),
    nonUserNonTempRegistersToInitialize(alloc),
    constantToRegister(alloc, 17),
    stringToRegister(alloc, 17),
    doubleConstantToRegister(alloc, 17),
    stringTemplateCallsiteRegisterMap(alloc, 17),
    targetStatements(alloc)
{
    this->byteCodeFunction = byteCodeFunction;
    bodyScope->SetFunc(this);
    if (paramScope != nullptr)
    {
        paramScope->SetFunc(this);
    }
    if (pnode && pnode->sxFnc.NestedFuncEscapes())
    {
        this->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("Child")));
    }
}

bool FuncInfo::IsGlobalFunction() const
{
    return root && root->nop == knopProg;
}

bool FuncInfo::IsDeferred() const
{
    return root && root->sxFnc.pnodeBody == nullptr;
}

BOOL FuncInfo::HasSuperReference() const
{
    return root->sxFnc.HasSuperReference();
}

BOOL FuncInfo::HasDirectSuper() const
{
    return root->sxFnc.HasDirectSuper();
}

BOOL FuncInfo::IsClassMember() const
{
    return root->sxFnc.IsClassMember();
}

BOOL FuncInfo::IsLambda() const
{
    return root->sxFnc.IsLambda();
}

BOOL FuncInfo::IsClassConstructor() const
{
    return root->sxFnc.IsClassConstructor();
}

BOOL FuncInfo::IsBaseClassConstructor() const
{
    return root->sxFnc.IsBaseClassConstructor();
}

void FuncInfo::EnsureThisScopeSlot()
{
    if (this->thisScopeSlot == Js::Constants::NoProperty)
    {
        // In case of split scope param and body has separate closures. So we have to use different scope slots for them.
        bool isSplitScope = this->paramScope && !this->paramScope->GetCanMergeWithBodyScope();
        Scope* scope = isSplitScope ? this->paramScope : this->bodyScope;
        Scope* currentScope = scope->IsGlobalEvalBlockScope() ? this->GetGlobalEvalBlockScope() : scope;

        this->thisScopeSlot = currentScope->AddScopeSlot();
        if (isSplitScope)
        {
            this->innerThisScopeSlot = this->bodyScope->AddScopeSlot();
        }
    }
}

void FuncInfo::EnsureSuperScopeSlot()
{
    if (this->superScopeSlot == Js::Constants::NoProperty)
    {
        // In case of split scope param and body has separate closures. So we have to use different scope slots for them.
        bool isSplitScope = this->paramScope && !this->paramScope->GetCanMergeWithBodyScope();
        Scope* scope = isSplitScope ? this->paramScope : this->bodyScope;

        this->superScopeSlot = scope->AddScopeSlot();
        if (isSplitScope)
        {
            this->innerSuperScopeSlot = this->bodyScope->AddScopeSlot();
        }
    }
}

void FuncInfo::EnsureSuperCtorScopeSlot()
{
    if (this->superCtorScopeSlot == Js::Constants::NoProperty)
    {
        // In case of split scope param and body has separate closures. So we have to use different scope slots for them.
        bool isSplitScope = this->paramScope && !this->paramScope->GetCanMergeWithBodyScope();
        Scope* scope = isSplitScope ? this->paramScope : this->bodyScope;

        this->superCtorScopeSlot = scope->AddScopeSlot();
        if (isSplitScope)
        {
            this->innerSuperCtorScopeSlot = this->bodyScope->AddScopeSlot();
        }
    }
}

void FuncInfo::EnsureNewTargetScopeSlot()
{
    if (this->newTargetScopeSlot == Js::Constants::NoProperty)
    {
        // In case of split scope param and body has separate closures. So we have to use different scope slots for them.
        bool isSplitScope = this->paramScope && !this->paramScope->GetCanMergeWithBodyScope();
        Scope* scope = isSplitScope ? this->paramScope : this->bodyScope;

        this->newTargetScopeSlot = scope->AddScopeSlot();
        if (isSplitScope)
        {
            this->innerNewTargetScopeSlot = this->bodyScope->AddScopeSlot();
        }
    }
}

void FuncInfo::UseInnerSpecialScopeSlots()
{
    Assert(this->paramScope != nullptr && !this->paramScope->GetCanMergeWithBodyScope());
    Js::PropertyId temp = Js::Constants::NoProperty;
    if (this->thisScopeSlot != Js::Constants::NoProperty)
    {
        Assert(this->innerThisScopeSlot != Js::Constants::NoProperty);
        temp = this->thisScopeSlot;
        this->thisScopeSlot = this->innerThisScopeSlot;
        this->innerThisScopeSlot = temp;
    }
    if (this->superScopeSlot != Js::Constants::NoProperty)
    {
        Assert(this->innerSuperScopeSlot != Js::Constants::NoProperty);
        temp = this->superScopeSlot;
        this->superScopeSlot = this->innerSuperScopeSlot;
        this->innerSuperScopeSlot = temp;
    }
    if (this->superCtorScopeSlot != Js::Constants::NoProperty)
    {
        Assert(this->innerSuperCtorScopeSlot != Js::Constants::NoProperty);
        temp = this->superCtorScopeSlot;
        this->superCtorScopeSlot = this->innerSuperCtorScopeSlot;
        this->innerSuperCtorScopeSlot = temp;
    }
    if (this->newTargetScopeSlot != Js::Constants::NoProperty)
    {
        Assert(this->innerNewTargetScopeSlot != Js::Constants::NoProperty);
        temp = this->newTargetScopeSlot;
        this->newTargetScopeSlot = this->innerNewTargetScopeSlot;
        this->innerNewTargetScopeSlot = temp;
    }
}

Scope *
FuncInfo::GetGlobalBlockScope() const
{
    Assert(this->IsGlobalFunction());
    Scope * scope = this->root->sxFnc.pnodeScopes->sxBlock.scope;
    Assert(scope == nullptr || scope == this->GetBodyScope() || scope->GetEnclosingScope() == this->GetBodyScope());
    return scope;
}

Scope * FuncInfo::GetGlobalEvalBlockScope() const
{
    Scope * globalEvalBlockScope = this->GetGlobalBlockScope();
    Assert(globalEvalBlockScope->GetEnclosingScope() == this->GetBodyScope());
    Assert(globalEvalBlockScope->GetScopeType() == ScopeType_GlobalEvalBlock);
    return globalEvalBlockScope;
}

uint FuncInfo::FindOrAddReferencedPropertyId(Js::PropertyId propertyId)
{
    Assert(propertyId != Js::Constants::NoProperty);
    Assert(referencedPropertyIdToMapIndex != nullptr);
    if (propertyId < TotalNumberOfBuiltInProperties)
    {
        return propertyId;
    }
    uint index;
    if (!referencedPropertyIdToMapIndex->TryGetValue(propertyId, &index))
    {
        index = this->NewReferencedPropertyId();
        referencedPropertyIdToMapIndex->Add(propertyId, index);
    }
    return index + TotalNumberOfBuiltInProperties;
}

uint FuncInfo::FindOrAddRootObjectInlineCacheId(Js::PropertyId propertyId, bool isLoadMethod, bool isStore)
{
    Assert(propertyId != Js::Constants::NoProperty);
    Assert(!isLoadMethod || !isStore);
    uint cacheId;
    RootObjectInlineCacheIdMap * idMap = isStore ? rootObjectStoreInlineCacheMap : isLoadMethod ? rootObjectLoadMethodInlineCacheMap : rootObjectLoadInlineCacheMap;
    if (!idMap->TryGetValue(propertyId, &cacheId))
    {
        cacheId = isStore ? this->NewRootObjectStoreInlineCache() : isLoadMethod ? this->NewRootObjectLoadMethodInlineCache() : this->NewRootObjectLoadInlineCache();
        idMap->Add(propertyId, cacheId);
    }
    return cacheId;
}

#if DBG_DUMP
void FuncInfo::Dump()
{
    Output::Print(_u("FuncInfo: CallsEval:%s ChildCallsEval:%s HasArguments:%s HasHeapArguments:%s\n"),
        IsTrueOrFalse(this->GetCallsEval()),
        IsTrueOrFalse(this->GetChildCallsEval()),
        IsTrueOrFalse(this->GetHasArguments()),
        IsTrueOrFalse(this->GetHasHeapArguments()));
}
#endif

Js::RegSlot FuncInfo::AcquireLoc(ParseNode *pnode)
{
    // Assign a new temp pseudo-register to this expression.
    if (pnode->location == Js::Constants::NoRegister)
    {
        pnode->location = this->AcquireTmpRegister();
    }
    return pnode->location;
}

Js::RegSlot FuncInfo::AcquireTmpRegister()
{
    Assert(this->firstTmpReg != Js::Constants::NoRegister);
    // Allocate a new temp pseudo-register, increasing the locals count if necessary.
    Assert(this->curTmpReg <= this->varRegsCount && this->curTmpReg >= this->firstTmpReg);
    Js::RegSlot tmpReg = this->curTmpReg;
    UInt32Math::Inc(this->curTmpReg);
    if (this->curTmpReg > this->varRegsCount)
    {
        this->varRegsCount = this->curTmpReg;
    }
    return tmpReg;
}

void FuncInfo::ReleaseLoc(ParseNode *pnode)
{
    // Release the temp assigned to this expression so it can be re-used.
    if (pnode && pnode->location != Js::Constants::NoRegister)
    {
        this->ReleaseTmpRegister(pnode->location);
    }
}

void FuncInfo::ReleaseLoad(ParseNode *pnode)
{
    // Release any temp register(s) acquired by an EmitLoad.
    switch (pnode->nop)
    {
    case knopDot:
    case knopIndex:
    case knopCall:
        this->ReleaseReference(pnode);
        break;
    }
    this->ReleaseLoc(pnode);
}

void FuncInfo::ReleaseReference(ParseNode *pnode)
{
    // Release any temp(s) assigned to this reference expression so they can be re-used.
    switch (pnode->nop)
    {
    case knopDot:
        this->ReleaseLoc(pnode->sxBin.pnode1);
        break;

    case knopIndex:
        this->ReleaseLoc(pnode->sxBin.pnode2);
        this->ReleaseLoc(pnode->sxBin.pnode1);
        break;

    case knopName:
        // Do nothing (see EmitReference)
        break;

    case knopCall:
    case knopNew:
        // For call/new, we have to release the ArgOut register(s) in reverse order,
        // but we have the args in a singly linked list.
        // Fortunately, we know that the set we have to release is sequential.
        // So find the endpoints of the list and release them in descending order.
        if (pnode->sxCall.pnodeArgs)
        {
            ParseNode *pnodeArg = pnode->sxCall.pnodeArgs;
            Js::RegSlot firstArg = Js::Constants::NoRegister;
            Js::RegSlot lastArg = Js::Constants::NoRegister;
            if (pnodeArg->nop == knopList)
            {
                do
                {
                    if (this->IsTmpReg(pnodeArg->sxBin.pnode1->location))
                    {
                        lastArg = pnodeArg->sxBin.pnode1->location;
                        if (firstArg == Js::Constants::NoRegister)
                        {
                            firstArg = lastArg;
                        }
                    }
                    pnodeArg = pnodeArg->sxBin.pnode2;
                }
                while (pnodeArg->nop == knopList);
            }
            if (this->IsTmpReg(pnodeArg->location))
            {
                lastArg = pnodeArg->location;
                if (firstArg == Js::Constants::NoRegister)
                {
                    // Just one: first and last point to the same node.
                    firstArg = lastArg;
                }
            }
            if (lastArg != Js::Constants::NoRegister)
            {
                Assert(firstArg != Js::Constants::NoRegister);
                Assert(lastArg >= firstArg);
                do
                {
                    // Walk down from last to first.
                    this->ReleaseTmpRegister(lastArg);
                } while (lastArg-- > firstArg); // these are unsigned, so (--lastArg >= firstArg) will cause an infinite loop if firstArg is 0 (although that shouldn't happen)
            }
        }
        // Now release the call target.
        switch (pnode->sxCall.pnodeTarget->nop)
        {
        case knopDot:
        case knopIndex:
            this->ReleaseReference(pnode->sxCall.pnodeTarget);
            this->ReleaseLoc(pnode->sxCall.pnodeTarget);
            break;
        default:
            this->ReleaseLoad(pnode->sxCall.pnodeTarget);
            break;
        }
        break;
    default:
        this->ReleaseLoc(pnode);
        break;
    }
}

void FuncInfo::ReleaseTmpRegister(Js::RegSlot tmpReg)
{
    // Put this reg back on top of the temp stack (if it's a temp).
    Assert(tmpReg != Js::Constants::NoRegister);
    if (this->IsTmpReg(tmpReg))
    {
        Assert(tmpReg == this->curTmpReg - 1);
        this->curTmpReg--;
    }
}

Js::RegSlot FuncInfo::InnerScopeToRegSlot(Scope *scope) const
{
    Js::RegSlot reg = FirstInnerScopeReg();
    Assert(reg != Js::Constants::NoRegister);

    uint32 index = scope->GetInnerScopeIndex();

    return reg + index;
}

Js::RegSlot FuncInfo::FirstInnerScopeReg() const
{
    // FunctionBody stores this as a mapped reg. Callers of this function want the pre-mapped value.

    Js::RegSlot reg = this->GetParsedFunctionBody()->GetFirstInnerScopeRegister();
    Assert(reg != Js::Constants::NoRegister);

    return reg - this->constRegsCount;
}

void FuncInfo::SetFirstInnerScopeReg(Js::RegSlot reg)
{
    // Just forward to the FunctionBody.
    this->GetParsedFunctionBody()->MapAndSetFirstInnerScopeRegister(reg);
}

void FuncInfo::AddCapturedSym(Symbol *sym)
{
    if (this->capturedSyms == nullptr)
    {
        this->capturedSyms = Anew(alloc, SymbolTable, alloc);
    }
    this->capturedSyms->AddNew(sym);
}

void FuncInfo::OnStartVisitFunction(ParseNode *pnodeFnc)
{
    Assert(pnodeFnc->nop == knopFncDecl);
    Assert(this->GetCurrentChildFunction() == nullptr);

    this->SetCurrentChildFunction(pnodeFnc->sxFnc.funcInfo);
}

void FuncInfo::OnEndVisitFunction(ParseNode *pnodeFnc)
{
    Assert(pnodeFnc->nop == knopFncDecl);
    Assert(this->GetCurrentChildFunction() == pnodeFnc->sxFnc.funcInfo);

    pnodeFnc->sxFnc.funcInfo->SetCurrentChildScope(nullptr);
    this->SetCurrentChildFunction(nullptr);
}

void FuncInfo::OnStartVisitScope(Scope *scope, bool *pisMergedScope)
{
    *pisMergedScope = false;

    if (scope == nullptr)
    {
        return;
    }

    Scope* childScope = this->GetCurrentChildScope();
    if (childScope)
    {
        if (scope->GetScopeType() == ScopeType_Parameter)
        {
            Assert(childScope->GetEnclosingScope() == scope);
        }
        else if (childScope->GetScopeType() == ScopeType_Parameter
                 && childScope->GetCanMergeWithBodyScope()
                 && scope->GetScopeType() == ScopeType_Block)
        {
            // If param and body are merged then the class declaration in param scope will have body as the parent
            *pisMergedScope = true;
            Assert(childScope == scope->GetEnclosingScope()->GetEnclosingScope());
        }
        else
        {
            Assert(childScope == scope->GetEnclosingScope());
        }
    }

    this->SetCurrentChildScope(scope);
    return;
}

void FuncInfo::OnEndVisitScope(Scope *scope, bool isMergedScope)
{
    if (scope == nullptr)
    {
        return;
    }
    Assert(this->GetCurrentChildScope() == scope || (scope->GetScopeType() == ScopeType_Parameter && this->GetParamScope() == scope));

    this->SetCurrentChildScope(isMergedScope ? scope->GetEnclosingScope()->GetEnclosingScope() : scope->GetEnclosingScope());
}

CapturedSymMap *FuncInfo::EnsureCapturedSymMap()
{
    if (this->capturedSymMap == nullptr)
    {
        this->capturedSymMap = Anew(alloc, CapturedSymMap, alloc);
    }
    return this->capturedSymMap;
}

void FuncInfo::SetHasMaybeEscapedNestedFunc(DebugOnly(char16 const * reason))
{
    if (PHASE_TESTTRACE(Js::StackFuncPhase, this->byteCodeFunction) && !hasEscapedUseNestedFunc)
    {
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        char16 const * r = _u("");

        DebugOnly(r = reason);
        Output::Print(_u("HasMaybeEscapedNestedFunc (%s): %s (function %s)\n"),
            r,
            this->byteCodeFunction->GetDisplayName(),
            this->byteCodeFunction->GetDebugNumberSet(debugStringBuffer));
        Output::Flush();
    }
    hasEscapedUseNestedFunc = true;
}

uint FuncInfo::AcquireInnerScopeIndex()
{
    uint index = this->currentInnerScopeIndex;
    if (index == (uint)-1)
    {
        index = 0;
    }
    else
    {
        index++;
        if (index == (uint)-1)
        {
            Js::Throw::OutOfMemory();
        }
    }
    if (index == this->innerScopeCount)
    {
        this->innerScopeCount = index + 1;
    }
    this->currentInnerScopeIndex = index;
    return index;
}

void FuncInfo::ReleaseInnerScopeIndex()
{
    uint index = this->currentInnerScopeIndex;
    Assert(index != (uint)-1);

    if (index == 0)
    {
        index = (uint)-1;
    }
    else
    {
        index--;
    }
    this->currentInnerScopeIndex = index;
}
