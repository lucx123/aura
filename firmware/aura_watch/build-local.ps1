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
$taskStage = Join-Path $env:TEMP 'aura-watch-src'
$taskBuild = Join-Path $env:TEMP 'aura-watch-build'
New-Item -ItemType Directory -Force -Path (Join-Path $taskStage 'main') | Out-Null
foreach ($taskFile in @('CMakeLists.txt', 'partitions.csv', 'sdkconfig.defaults')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $taskFile) -Destination $taskStage
}
Copy-Item -Path (Join-Path $PSScriptRoot 'main\*') -Destination (Join-Path $taskStage 'main')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'sdkconfig.defaults') -Destination (Join-Path $taskStage 'sdkconfig')
if (Test-Path (Join-Path $PSScriptRoot 'dependencies.lock')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'dependencies.lock') -Destination $taskStage
}

& $taskPython (Join-Path $IdfPath 'tools\idf.py') -C $taskStage -B $taskBuild build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Copy-Item -LiteralPath (Join-Path $taskStage 'dependencies.lock') -Destination $PSScriptRoot
$taskOutput = Join-Path $PSScriptRoot 'build'
foreach ($taskSubdir in @('', 'bootloader', 'partition_table')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $taskOutput $taskSubdir) | Out-Null
}
foreach ($taskArtifact in @('aura_watch.bin', 'aura_watch.elf', 'flash_args', 'flasher_args.json',
        'bootloader\bootloader.bin', 'partition_table\partition-table.bin')) {
    Copy-Item -LiteralPath (Join-Path $taskBuild $taskArtifact) -Destination (Join-Path $taskOutput $taskArtifact)
}
Write-Output "Firmware compilado en $taskOutput"
