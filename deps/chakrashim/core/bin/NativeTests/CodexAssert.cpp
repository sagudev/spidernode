//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "stdafx.h"

// The Codex library requires this assertion.
void CodexAssert(bool condition)
{
    condition;
    Assert(condition);
}
