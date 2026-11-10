[CmdletBinding()]
param(
    [string]$KeilPath,
    [switch]$KeepProject
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$testDirectory = $PSScriptRoot
$projectDirectory = Split-Path -Parent $testDirectory
$sourceProject = Join-Path $projectDirectory 'BF006.uvproj'
$testProject = Join-Path $projectDirectory 'contact_overload_test.uvproj'
$buildLog = Join-Path $projectDirectory 'contact_overload_build.log'
$debugLog = Join-Path $testDirectory 'contact_overload_debug.log'
$objectDirectory = Join-Path $testDirectory 'Objects'
$listingDirectory = Join-Path $testDirectory 'Listings'

if ([string]::IsNullOrWhiteSpace($KeilPath))
{
    $keilCandidates = @(
        'D:\Professional\keil5 for C51\Installed\UV4\UV4.exe',
        'C:\Keil_v5\UV4\UV4.exe',
        'C:\Keil\UV4\UV4.exe'
    )
    $KeilPath = $keilCandidates |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
}

if ([string]::IsNullOrWhiteSpace($KeilPath))
{
    throw 'Keil UV4.exe was not found.'
}

New-Item -ItemType Directory -Path $objectDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $listingDirectory -Force | Out-Null
Remove-Item -LiteralPath $buildLog,$debugLog -Force -ErrorAction SilentlyContinue

try
{
    $projectText = Get-Content -LiteralPath $sourceProject -Encoding UTF8 -Raw
    $testFilesXml = @'
          <Files>
            <File>
              <FileName>STARTUP.A51</FileName>
              <FileType>2</FileType>
              <FilePath>.\STARTUP.A51</FilePath>
            </File>
            <File>
              <FileName>contact_overload.c</FileName>
              <FileType>1</FileType>
              <FilePath>..\src\contact_overload.c</FilePath>
            </File>
            <File>
              <FileName>contact_overload_test.c</FileName>
              <FileType>1</FileType>
              <FilePath>.\tests\contact_overload_test.c</FilePath>
            </File>
          </Files>
'@

    $projectText = $projectText.Replace('<OutputDirectory>.\Objects\</OutputDirectory>', '<OutputDirectory>.\tests\Objects\</OutputDirectory>')
    $projectText = [regex]::Replace(
        $projectText,
        '<OutputName>.*?</OutputName>',
        '<OutputName>ContactOverloadTest</OutputName>',
        1
    )
    $projectText = $projectText.Replace('<CreateHexFile>1</CreateHexFile>', '<CreateHexFile>0</CreateHexFile>')
    $projectText = $projectText.Replace('<ListingPath>.\Listings\</ListingPath>', '<ListingPath>.\tests\Listings\</ListingPath>')
    $projectText = $projectText.Replace('<UseSimulator>0</UseSimulator>', '<UseSimulator>1</UseSimulator>')
    $projectText = $projectText.Replace('<UseTarget>1</UseTarget>', '<UseTarget>0</UseTarget>')
    $projectText = $projectText.Replace('<RunToMain>1</RunToMain>', '<RunToMain>0</RunToMain>')
    $projectText = [regex]::Replace(
        $projectText,
        '<InitializationFile></InitializationFile>',
        '<InitializationFile>.\tests\contact_overload_test.ini</InitializationFile>',
        1
    )
    $projectText = [regex]::Replace(
        $projectText,
        '<Files>[\s\S]*?</Files>',
        $testFilesXml,
        1
    )
    [IO.File]::WriteAllText(
        $testProject,
        $projectText,
        (New-Object Text.UTF8Encoding($false))
    )

    $build = Start-Process -FilePath $KeilPath `
        -ArgumentList @('-r', 'contact_overload_test.uvproj', '-j0', '-o', 'contact_overload_build.log') `
        -WorkingDirectory $projectDirectory `
        -WindowStyle Hidden `
        -Wait `
        -PassThru

    if (-not (Test-Path -LiteralPath $buildLog -PathType Leaf))
    {
        throw "Keil did not create the test build log. Exit code: $($build.ExitCode)"
    }

    $buildText = Get-Content -LiteralPath $buildLog -Encoding Default -Raw
    if (($buildText -notmatch '0 Error\(s\)') -or
        ($buildText -match 'UNRESOLVED EXTERNAL'))
    {
        Write-Output $buildText
        throw 'Contact overload test build failed.'
    }

    $debug = Start-Process -FilePath $KeilPath `
        -ArgumentList @('-d', 'contact_overload_test.uvproj', '-j0') `
        -WorkingDirectory $projectDirectory `
        -WindowStyle Hidden `
        -PassThru

    if (-not $debug.WaitForExit(30000))
    {
        $debug.Kill()
        throw 'Keil simulator test timed out.'
    }

    if (-not (Test-Path -LiteralPath $debugLog -PathType Leaf))
    {
        throw 'Keil did not create the simulator test log.'
    }

    $debugText = Get-Content -LiteralPath $debugLog -Encoding Default -Raw
    if ($debugText -notmatch 'CONTACT_OVERLOAD_TEST_RESULT=0')
    {
        Write-Output $debugText
        throw 'Contact overload behavior test failed.'
    }

    Write-Output 'CONTACT_OVERLOAD_TEST_RESULT=0'
}
finally
{
    if (-not $KeepProject)
    {
        Remove-Item -LiteralPath $testProject -Force -ErrorAction SilentlyContinue
    }
}
