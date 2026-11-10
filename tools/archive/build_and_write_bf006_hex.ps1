<#
.SYNOPSIS
    全量编译 BF006 指定版本，并将校验后的 HEX 写入烧录文件目录。

.DESCRIPTION
    本工具会临时改写 BF006_STC\src\system.h 中的 Model_Device 宏，调用 Keil C51
    全量重编，写入编译日志和最终 HEX 文件，并在任何退出路径恢复 system.h 原始字节。
    这是写入型工具，会更新 Keil Objects/Listings、pro 下的编译日志及烧录文件目录。
#>

[CmdletBinding()]
param(
    [string]$ProjectRoot,

    [ValidateSet('220V_1C1', '220V_1C2', '380V_1C1', '380V_1C2')]
    [string[]]$Variant = @('220V_1C1', '220V_1C2', '380V_1C1', '380V_1C2'),

    [ValidatePattern('^\d{6}$')]
    [string]$DateCode = (Get-Date -Format 'yyMMdd'),

    [string]$KeilPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-BytesSha256
{
    param(
        [byte[]]$Bytes
    )

    $sha256 = [System.Security.Cryptography.SHA256]::Create()

    try
    {
        return ([System.BitConverter]::ToString($sha256.ComputeHash($Bytes))).Replace('-', '')
    }
    finally
    {
        $sha256.Dispose()
    }
}

function Set-ModelDeviceMacro
{
    param(
        [byte[]]$OriginalBytes,
        [string]$Macro,
        [string]$HeaderPath
    )

    $singleByteEncoding = [System.Text.Encoding]::GetEncoding(28591)
    $originalText = $singleByteEncoding.GetString($OriginalBytes)
    $pattern = '(?m)^(#define[ \t]+Model_Device[ \t]+)\S+([^\r\n]*)(\r?)$'
    $matches = [System.Text.RegularExpressions.Regex]::Matches($originalText, $pattern)

    if ($matches.Count -ne 1)
    {
        throw "Model_Device 宏匹配数量异常：$($matches.Count)"
    }

    $replacement = '${1}' + $Macro + '${2}${3}'
    $modifiedText = [System.Text.RegularExpressions.Regex]::Replace($originalText, $pattern, $replacement)
    $modifiedBytes = $singleByteEncoding.GetBytes($modifiedText)

    [System.IO.File]::WriteAllBytes($HeaderPath, $modifiedBytes)
}

function Test-IntelHexBytes
{
    param(
        [byte[]]$Bytes
    )

    $text = [System.Text.Encoding]::ASCII.GetString($Bytes).TrimEnd("`r", "`n")
    $lines = $text -split "`r?`n"

    if ($lines.Count -eq 0)
    {
        throw 'HEX 文件为空。'
    }

    foreach ($line in $lines)
    {
        if ($line -notmatch '^:[0-9A-Fa-f]+$')
        {
            throw "存在非法 Intel HEX 记录：$line"
        }

        $recordText = $line.Substring(1)
        if (($recordText.Length % 2) -ne 0)
        {
            throw "Intel HEX 记录字符数不是偶数：$line"
        }

        $recordBytes = New-Object byte[] ($recordText.Length / 2)
        for ($index = 0; $index -lt $recordBytes.Length; $index++)
        {
            $recordBytes[$index] = [System.Convert]::ToByte($recordText.Substring($index * 2, 2), 16)
        }

        if ($recordBytes.Length -ne ($recordBytes[0] + 5))
        {
            throw "Intel HEX 记录长度字段不匹配：$line"
        }

        $sum = 0
        foreach ($value in $recordBytes)
        {
            $sum = ($sum + $value) -band 0xFF
        }

        if ($sum -ne 0)
        {
            throw "Intel HEX 记录校验和错误：$line"
        }
    }

    if ($lines[-1] -ne ':00000001FF')
    {
        throw 'Intel HEX 缺少结束记录。'
    }

    return $true
}

if ([string]::IsNullOrWhiteSpace($ProjectRoot))
{
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
else
{
    $ProjectRoot = (Resolve-Path $ProjectRoot).Path
}

if ($Variant.Count -ne (@($Variant | Sort-Object -Unique).Count))
{
    throw '版本参数中存在重复项。'
}

$variantMap = @{
    '220V_1C1' = [pscustomobject]@{
        Macro = 'Model_1PH_220_1C1_LCD'
        Output = "BF006_220V_1C1_$DateCode.hex"
    }
    '220V_1C2' = [pscustomobject]@{
        Macro = 'Model_1PH_220_1C2_LCD'
        Output = "BF006_220V_1C2_$DateCode.hex"
    }
    '380V_1C1' = [pscustomobject]@{
        Macro = 'Model_3PH_380_1C1_LCD'
        Output = "BF006_380V_1C1_$DateCode.hex"
    }
    '380V_1C2' = [pscustomobject]@{
        Macro = 'Model_3PH_380_1C2_LCD'
        Output = "BF006_380V_1C2_$DateCode.hex"
    }
}

$headerPath = Join-Path $ProjectRoot 'BF006_STC\src\system.h'
$projectDirectory = Join-Path $ProjectRoot 'BF006_STC\pro'
$projectFile = Join-Path $projectDirectory 'BF006.uvproj'
$generatedHex = Join-Path $projectDirectory 'Objects\BF006三相380一控一.hex'
$deliveryDirectory = Join-Path $ProjectRoot '烧录文件'

foreach ($requiredPath in @($headerPath, $projectFile))
{
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf))
    {
        throw "缺少必需文件：$requiredPath"
    }
}

if ([string]::IsNullOrWhiteSpace($KeilPath))
{
    $keilCandidates = @(
        'D:\Professional\keil5 for C51\Installed\UV4\UV4.exe',
        'C:\Keil_v5\UV4\UV4.exe',
        'C:\Keil\UV4\UV4.exe'
    )
    $KeilPath = $keilCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}

if ([string]::IsNullOrWhiteSpace($KeilPath) -or -not (Test-Path -LiteralPath $KeilPath -PathType Leaf))
{
    throw '未找到可用的 Keil UV4.exe，请通过 -KeilPath 指定。'
}

if (-not (Test-Path -LiteralPath $deliveryDirectory -PathType Container))
{
    New-Item -ItemType Directory -Path $deliveryDirectory | Out-Null
}

$originalHeaderBytes = [System.IO.File]::ReadAllBytes($headerPath)
$originalHeaderHash = Get-BytesSha256 -Bytes $originalHeaderBytes
$artifacts = New-Object System.Collections.Generic.List[object]
$summaries = New-Object System.Collections.Generic.List[object]

try
{
    foreach ($variantName in $Variant)
    {
        $configuration = $variantMap[$variantName]
        $logName = "build_$($variantName)_$DateCode.log"
        $logPath = Join-Path $projectDirectory $logName
        $beforeTime = [datetime]::MinValue
        $beforeHash = ''

        Set-ModelDeviceMacro -OriginalBytes $originalHeaderBytes -Macro $configuration.Macro -HeaderPath $headerPath

        if (Test-Path -LiteralPath $generatedHex -PathType Leaf)
        {
            $beforeItem = Get-Item -LiteralPath $generatedHex
            $beforeTime = $beforeItem.LastWriteTimeUtc
            $beforeHash = (Get-FileHash -LiteralPath $generatedHex -Algorithm SHA256).Hash
        }

        Write-Host "正在全量编译 $variantName，宏为 $($configuration.Macro)..."
        $process = Start-Process -FilePath $KeilPath `
            -ArgumentList @('-r', 'BF006.uvproj', '-j0', '-o', $logName) `
            -WorkingDirectory $projectDirectory `
            -WindowStyle Hidden `
            -Wait `
            -PassThru

        if (-not (Test-Path -LiteralPath $logPath -PathType Leaf))
        {
            throw "$variantName 未生成编译日志。"
        }

        $logContent = Get-Content -LiteralPath $logPath -Raw -Encoding Default
        $resultMatches = [System.Text.RegularExpressions.Regex]::Matches(
            $logContent,
            ' - (\d+) Error\(s\), (\d+) Warning\(s\)\.'
        )

        if ($resultMatches.Count -eq 0)
        {
            throw "$variantName 的编译日志中缺少错误数和警告数汇总。"
        }

        $resultMatch = $resultMatches[$resultMatches.Count - 1]
        $errorCount = [int]$resultMatch.Groups[1].Value
        $warningCount = [int]$resultMatch.Groups[2].Value

        if ($errorCount -ne 0)
        {
            throw "$variantName 编译失败，错误数为 $errorCount，日志为 $logPath"
        }

        if (-not (Test-Path -LiteralPath $generatedHex -PathType Leaf))
        {
            throw "$variantName 未生成 HEX 文件。"
        }

        $generatedItem = Get-Item -LiteralPath $generatedHex
        $hexBytes = [System.IO.File]::ReadAllBytes($generatedHex)
        $hexHash = Get-BytesSha256 -Bytes $hexBytes

        if (($generatedItem.LastWriteTimeUtc -le $beforeTime) -and ($hexHash -eq $beforeHash))
        {
            throw "$variantName 的 HEX 未在本轮全量编译中更新。"
        }

        [void](Test-IntelHexBytes -Bytes $hexBytes)

        [void]$artifacts.Add([pscustomobject]@{
            Name = $variantName
            Output = $configuration.Output
            Bytes = $hexBytes
            Hash = $hexHash
        })
        [void]$summaries.Add([pscustomobject]@{
            Version = $variantName
            Errors = $errorCount
            Warnings = $warningCount
            Bytes = $hexBytes.Length
            SHA256 = $hexHash
            KeilExitCode = $process.ExitCode
            Log = $logPath
        })
    }

    $uniqueHashCount = @($artifacts | Select-Object -ExpandProperty Hash | Sort-Object -Unique).Count
    if (($artifacts.Count -gt 1) -and ($uniqueHashCount -ne $artifacts.Count))
    {
        throw '不同版本出现相同 SHA-256，请检查宏是否真正参与条件编译。'
    }

    foreach ($artifact in $artifacts)
    {
        $destination = Join-Path $deliveryDirectory $artifact.Output
        [System.IO.File]::WriteAllBytes($destination, $artifact.Bytes)
        [void](Test-IntelHexBytes -Bytes ([System.IO.File]::ReadAllBytes($destination)))
    }
}
finally
{
    [System.IO.File]::WriteAllBytes($headerPath, $originalHeaderBytes)
    $restoredHeaderHash = (Get-FileHash -LiteralPath $headerPath -Algorithm SHA256).Hash

    if ($restoredHeaderHash -ne $originalHeaderHash)
    {
        throw 'system.h 恢复失败，当前字节与执行前不一致。'
    }
}

$summaries
