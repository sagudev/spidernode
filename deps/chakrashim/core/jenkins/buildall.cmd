::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------

@echo off
setlocal

set JENKINS_BUILD=True
call %~dp0..\test\jenkins.buildall.cmd %*

endlocal
