param([string]$BuildDirectory = "$PSScriptRoot/../build")
$ErrorActionPreference='Stop'
$taskBuild = [IO.Path]::GetFullPath($BuildDirectory)
foreach($taskTest in @('core_tests.exe','input_tests.exe')) {
    & (Join-Path $taskBuild $taskTest)
    if($LASTEXITCODE -ne 0) { throw "$taskTest failed ($LASTEXITCODE)" }
}
& (Join-Path $taskBuild 'gui_tests.exe') (Join-Path $taskBuild 'MouseSide.exe')
if($LASTEXITCODE -ne 0) { throw "gui_tests.exe failed ($LASTEXITCODE)" }
Write-Output 'All 3 test suites passed.'
