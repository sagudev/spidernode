#-------------------------------------------------------------------------------------------------------
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#-------------------------------------------------------------------------------------------------------

# Post-Build Script
#
# Run this script after the build step to consume the artifacts produced by the build,
# run tests, and process and deploy build logs.
#
# There may be more non-script tasks in the build definition following this script.
# Any final tasks that need to be done at the end of the build are contained in the
# Finalize Build Script, which should be invoked at the very end of the build.

param (
    [ValidateSet("x86", "x64", "arm", "*")]
    [string]$arch = "*",

    [ValidateSet("debug", "release", "test", "codecoverage", "*")]
    [string]$flavor = "*",

    [ValidateSet("default", "codecoverage", "pogo")]
    [string]$subtype = "default",

    [string]$srcpath = "",
    [string]$binpath = "",
    [string]$objpath = "",

    [Parameter(Mandatory=$True)]
    [string]$srcsrvcmdpath,

    [string]$bvtcmdpath = "",
    [string]$repo = "core",
    [string]$logFile = "",

    # comma separated list of [arch,flavor,arch2,flavor2,...] to build
    [string[]]$pogo = @(),
    [string]$pogoscript = "",

    [switch]$noaction
)

$global:exitcode = 0

if ($arch -eq "*") {

    . "$PSScriptRoot\util.ps1"
    foreach ($arch in ("x86", "x64", "arm")) {
        ExecuteCommand "$PSScriptRoot\post_build.ps1 -arch $arch -flavor $flavor -srcpath ""$srcpath"" -binpath ""$binpath"" -objpath ""$objpath"" -srcsrvcmdpath ""$srcsrvcmdpath"" -bvtcmdpath ""$bvtcmdpath"" -repo ""$repo""" -logFile ""$logFile""
    }

} elseif ($flavor -eq "*") {

    . "$PSScriptRoot\util.ps1"
    foreach ($flavor in ("debug", "test", "release")) {
        ExecuteCommand "$PSScriptRoot\post_build.ps1 -arch $arch -flavor $flavor -srcpath ""$srcpath"" -binpath ""$binpath"" -objpath ""$objpath"" -srcsrvcmdpath ""$srcsrvcmdpath"" -bvtcmdpath ""$bvtcmdpath"" -repo ""$repo""" -logFile ""$logFile""
    }

} else {

    $OuterScriptRoot = $PSScriptRoot
    . "$PSScriptRoot\pre_post_util.ps1"

    $buildName = ConstructBuildName -arch $arch -flavor $flavor -subtype $subtype

    if (($logFile -eq "") -and (Test-Path Env:\TF_BUILD_BINARIESDIRECTORY)) {
        $logFile = "${Env:TF_BUILD_BINARIESDIRECTORY}\logs\post_build.${buildName}.log"
        if (Test-Path -Path $logFile) {
            Remove-Item $logFile -Force
        }
    }

    WriteMessage "======================================================================================"
    WriteMessage "Post build script for $arch $flavor"
    WriteMessage "======================================================================================"

    $bvtcmdpath =  UseValueOrDefault $bvtcmdpath "" (Resolve-Path "$PSScriptRoot\..\..\test\runcitests.cmd")

    WriteCommonArguments
    WriteMessage "BVT Command  : $bvtcmdpath"
    WriteMessage ""

    $srcsrvcmd = ("{0} {1} {2} {3}\bin\{4}\*.pdb" -f $srcsrvcmdpath, $repo, $srcpath, $binpath, $buildName)
    $prefastlog = ("{0}\logs\PrefastCheck.{1}.log" -f $binpath, $buildName)
    $prefastcmd = "$PSScriptRoot\check_prefast_error.ps1 -directory $objpath -logFile $prefastlog"

    # generate srcsrv
    if ((Test-Path $srcsrvcmdpath) -and (Test-Path $srcpath) -and (Test-Path $binpath)) {
        ExecuteCommand($srcsrvcmd)
    }

    # do POGO
    $doPogo=$False
    for ($i=0; $i -lt $pogo.length; $i=$i+2) {
        if (($pogo[$i] -eq $arch) -and ($pogo[$i+1] -eq $flavor)) {
            $doPogo=$True
        }
    }

    if ($subtype -ne "codecoverage") {
        if ($doPogo -and ("$pogoscript" -ne "")) {
            WriteMessage "Building pogo for $arch $flavor"
            $pogocmd = ("{0} {1} {2}" -f $pogoscript, $arch, $flavor)
            ExecuteCommand($pogocmd)
        }

        # run tests
        ExecuteCommand("$bvtcmdpath -$arch$flavor")
    }

    # check prefast
    ExecuteCommand($prefastcmd)

    WriteMessage ""

}

exit $global:exitcode
