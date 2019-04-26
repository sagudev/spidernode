//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "UnicodeText.h"

#include "PlatformAgnostic/DateTime.h"

#if !defined(_WIN32) && defined(DEBUG)
#include <signal.h> // raise(SIGINT)
#endif
