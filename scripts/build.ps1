param([string]$BuildDirectory = "$PSScriptRoot/../build")
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath("$PSScriptRoot/..")
$taskBuild=[IO.Path]::GetFullPath($BuildDirectory)
& cmake -S $taskRoot -B $taskBuild -A x64
if($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
& cmake --build $taskBuild --config Release
if($LASTEXITCODE -ne 0) { throw 'Build failed' }
& ctest --test-dir $taskBuild -C Release --output-on-failure
if($LASTEXITCODE -ne 0) { throw 'Tests failed' }
New-Item -ItemType Directory -Path "$taskRoot/dist" -Force | Out-Null
Copy-Item -LiteralPath "$taskBuild/Release/MouseSide.exe" -Destination "$taskRoot/dist/MouseSide.exe" -Force
Write-Output "$taskRoot/dist/MouseSide.exe"
