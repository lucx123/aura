param(
    [string]$IdfPath = "$env:TEMP\aura-idf-5.5.5",
    [string]$ToolsPath = "$env:TEMP\aura-idf-tools"
)

$ErrorActionPreference = 'Stop'
$env:PROCESSOR_ARCHITECTURE = 'AMD64'
$env:IDF_PATH = $IdfPath
$env:IDF_TOOLS_PATH = $ToolsPath
$env:IDF_TARGET = 'esp32s3'
$env:IDF_PYTHON_ENV_PATH = Join-Path $ToolsPath 'python_env\idf5.5_py3.11_env'
$taskPython = Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
if (!(Test-Path $taskPython)) {
    throw "Falta el entorno Python de ESP-IDF en $taskPython"
}

$taskBins = @((Split-Path $taskPython))
foreach ($taskTool in @('xtensa-esp-elf-gcc.exe', 'esp32ulp-elf-as.exe', 'cmake.exe', 'ninja.exe')) {
    $taskExecutable = Get-ChildItem (Join-Path $ToolsPath 'tools') -Recurse -File -Filter $taskTool |
        Select-Object -First 1
    if (!$taskExecutable) { throw "Falta la herramienta $taskTool en $ToolsPath" }
    $taskBins += $taskExecutable.DirectoryName
}
$env:PATH = ($taskBins -join ';') + ';' + $env:PATH

# ESP-IDF se compila fuera de la ruta del repositorio, que contiene espacios.
$taskHash = [System.Security.Cryptography.SHA256]::Create()
$taskKey = ([BitConverter]::ToString($taskHash.ComputeHash([Text.Encoding]::UTF8.GetBytes($PSScriptRoot)))).Replace('-', '').Substring(0, 12).ToLowerInvariant()
$taskHash.Dispose()
$taskStage = Join-Path $env:TEMP "aura-$taskKey-src"
$taskBuild = Join-Path $env:TEMP "aura-$taskKey-build"
# Remove only staged source folders so removed files cannot leak into a later build.
$taskStageFull = [IO.Path]::GetFullPath($taskStage)
foreach ($taskFolder in @('main', 'components')) {
    $taskTarget = [IO.Path]::GetFullPath((Join-Path $taskStage $taskFolder))
    if (!$taskTarget.StartsWith($taskStageFull + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Ruta de staging invalida: $taskTarget"
    }
    if (Test-Path -LiteralPath $taskTarget) { Remove-Item -LiteralPath $taskTarget -Recurse -Force }
}
New-Item -ItemType Directory -Force -Path (Join-Path $taskStage 'main') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $taskStage 'components') | Out-Null
foreach ($taskFile in @('CMakeLists.txt', 'partitions.csv', 'sdkconfig.defaults')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $taskFile) -Destination $taskStage
}
Copy-Item -Path (Join-Path $PSScriptRoot 'main\*') -Destination (Join-Path $taskStage 'main')
Copy-Item -Path (Join-Path $PSScriptRoot 'components\*') -Destination (Join-Path $taskStage 'components') -Recurse -Force
# Copy-Item preserves timestamps. A source edited during the previous build can
# be older than that build's object; touch staged inputs to force correct rebuilds.
foreach ($taskInputDir in @('main', 'components')) {
    Get-ChildItem -LiteralPath (Join-Path $taskStage $taskInputDir) -Recurse -File |
        ForEach-Object { $_.LastWriteTimeUtc = [DateTime]::UtcNow }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'sdkconfig.defaults') -Destination (Join-Path $taskStage 'sdkconfig')
if (Test-Path (Join-Path $PSScriptRoot 'dependencies.lock')) {
    # The Windows component manager needs absolute local paths when loading a
    # lock outside the repository. Keep the portable relative form in the repo.
    $taskStagedLock = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'dependencies.lock') -Raw
    $taskStagedLock = $taskStagedLock -replace '(?m)^      path: components[/\\](waveshare__[^\r\n]+)\r?$',
        ('      path: ' + ($taskStage -replace '\\', '/') + '/components/$1')
    Set-Content -LiteralPath (Join-Path $taskStage 'dependencies.lock') -Value $taskStagedLock -NoNewline -Encoding utf8
}

$taskNinja = (Get-ChildItem (Join-Path $ToolsPath 'tools') -Recurse -File -Filter 'ninja.exe' | Select-Object -First 1).FullName
$taskCc = (Get-ChildItem (Join-Path $ToolsPath 'tools') -Recurse -File -Filter 'xtensa-esp32s3-elf-gcc.exe' | Select-Object -First 1).FullName
$taskCxx = (Get-ChildItem (Join-Path $ToolsPath 'tools') -Recurse -File -Filter 'xtensa-esp32s3-elf-g++.exe' | Select-Object -First 1).FullName
& $taskPython (Join-Path $IdfPath 'tools\idf.py') -D "CMAKE_MAKE_PROGRAM=$taskNinja" -D "CMAKE_C_COMPILER=$taskCc" -D "CMAKE_CXX_COMPILER=$taskCxx" -D "CMAKE_ASM_COMPILER=$taskCc" -C $taskStage -B $taskBuild build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Local overrides must remain portable across machines and staging directories.
$taskLock = Get-Content -LiteralPath (Join-Path $taskStage 'dependencies.lock') -Raw
$taskLock = $taskLock -replace '(?m)^      path: .*[/\\]components[/\\](waveshare__[^\r\n]+)\r?$', '      path: components/$1'
Set-Content -LiteralPath (Join-Path $PSScriptRoot 'dependencies.lock') -Value $taskLock -NoNewline -Encoding utf8
$taskOutput = Join-Path $PSScriptRoot 'build'
foreach ($taskSubdir in @('', 'bootloader', 'partition_table')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $taskOutput $taskSubdir) | Out-Null
}
foreach ($taskArtifact in @('aura_watch.bin', 'aura_watch.elf', 'flash_args', 'flasher_args.json',
        'bootloader\bootloader.bin', 'partition_table\partition-table.bin')) {
    Copy-Item -LiteralPath (Join-Path $taskBuild $taskArtifact) -Destination (Join-Path $taskOutput $taskArtifact)
}
Write-Output "Firmware compilado en $taskOutput"

$taskManifest = @{}
foreach ($taskBinary in @('aura_watch.bin', 'bootloader\bootloader.bin', 'partition_table\partition-table.bin')) {
    $taskManifest[$taskBinary] = (Get-FileHash -LiteralPath (Join-Path $taskOutput $taskBinary) -Algorithm SHA256).Hash
}
$taskManifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskOutput 'sha256.json') -Encoding utf8
Copy-Item -LiteralPath (Join-Path $taskStage 'sdkconfig') -Destination (Join-Path $taskOutput 'sdkconfig') -ErrorAction SilentlyContinue
