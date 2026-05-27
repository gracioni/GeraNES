$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

python -m mkdocs build --clean --config-file "$scriptDir\mkdocs.yml"
