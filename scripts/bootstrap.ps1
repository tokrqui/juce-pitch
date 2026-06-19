<#
Simple bootstrap script to download JUCE and the VST3 SDK into the expected locations.
Run from project root in PowerShell (Windows):

    .\scripts\bootstrap.ps1

This will clone JUCE into ../JUCE and vst3sdk into ../vst3sdk if they don't already exist.
#>

$root = Split-Path -Path $PSScriptRoot -Parent

function Clone-IfMissing($url, $targetRel) {
    $target = Join-Path $root $targetRel
    if (Test-Path $target) {
        Write-Host "Already present: $target"
        return
    }
    Write-Host "Cloning $url -> $target"
    git clone --depth 1 $url $target
}

Clone-IfMissing 'https://github.com/juce-framework/JUCE.git' '..\JUCE'
Clone-IfMissing 'https://github.com/steinbergmedia/vst3sdk.git' '..\vst3sdk'

Write-Host "Bootstrap complete. You can now run CMake (preset 'windows-vs2022') or open the folder in Visual Studio."
