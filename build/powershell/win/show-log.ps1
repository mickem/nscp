<#
.SYNOPSIS
    Fetches and displays NSCP logs from an Azure VM
.DESCRIPTION
    This is not to be used, it is only for setting up personal test machines
.PARAMETER ResourceGroupName
    The name for the resource group.
.PARAMETER VmName
    The name for the virtual machine.
#>
param(
    [string]$ResourceGroupName = "NSCP-RG",
    [string]$VmName = "NSCP-Test"
)

# Ensure required modules are installed
foreach ($module in @('Az.Accounts', 'Az.Compute')) {
    if (-not (Get-Module -ListAvailable -Name $module)) {
        Write-Host "Installing module $module..."
        Install-Module -Name $module -Scope CurrentUser -Force -AllowClobber
    }
    Import-Module $module
}

function Get-RemoteLog {
    param(
        [string]$FilePath,
        [string]$Label
    )
    Write-Host "● Fetching $Label..."

    # Step 1: Compress the file on the VM and get the number of chunks needed
    $scriptBlock = @"
if (Test-Path '$FilePath') {
    `$bytes = [System.IO.File]::ReadAllBytes('$FilePath')
    `$ms = New-Object System.IO.MemoryStream
    `$gz = New-Object System.IO.Compression.GZipStream(`$ms, [System.IO.Compression.CompressionMode]::Compress)
    `$gz.Write(`$bytes, 0, `$bytes.Length)
    `$gz.Close()
    `$b64 = [Convert]::ToBase64String(`$ms.ToArray())
    [System.IO.File]::WriteAllText('C:\temp\log_transfer.b64', `$b64)
    `$chunkSize = 3000
    `$totalChunks = [Math]::Ceiling(`$b64.Length / `$chunkSize)
    Write-Output "CHUNKS:`$totalChunks"
} else {
    Write-Output "FILE_NOT_FOUND"
}
"@
    $result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName `
        -VMName $VmName `
        -CommandId 'RunPowerShellScript' `
        -ScriptString $scriptBlock
    if ($result.Status -ne "Succeeded") {
        Write-Error "❌ Failed to fetch $Label. Status: $($result.Status)"
        return
    }
    $output = ($result.Value | Where-Object { $_.Code -eq "ComponentStatus/StdOut/succeeded" }).Message
    if (-not $output -or $output.Contains("FILE_NOT_FOUND")) {
        Write-Host "  No $Label found at $FilePath"
        return
    }

    if ($output -match "CHUNKS:(\d+)") {
        $totalChunks = [int]$Matches[1]
    } else {
        Write-Error "❌ Failed to prepare $Label for transfer"
        return
    }

    # Step 2: Fetch each chunk
    Write-Host "  Transferring $totalChunks chunk(s)..."
    $b64 = ""
    for ($i = 0; $i -lt $totalChunks; $i++) {
        $scriptBlock = @"
`$b64 = [System.IO.File]::ReadAllText('C:\temp\log_transfer.b64')
`$chunkSize = 3000
`$start = $i * `$chunkSize
`$len = [Math]::Min(`$chunkSize, `$b64.Length - `$start)
Write-Output `$b64.Substring(`$start, `$len)
"@
        $result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName `
            -VMName $VmName `
            -CommandId 'RunPowerShellScript' `
            -ScriptString $scriptBlock
        if ($result.Status -ne "Succeeded") {
            Write-Error "❌ Failed to fetch chunk $($i+1)/$totalChunks of $Label"
            return
        }
        $chunk = ($result.Value | Where-Object { $_.Code -eq "ComponentStatus/StdOut/succeeded" }).Message
        $b64 += ($chunk -replace '\s', '')
    }

    # Step 3: Decode and decompress
    try {
        $compressed = [Convert]::FromBase64String($b64)
        $ms = New-Object System.IO.MemoryStream(,$compressed)
        $gz = New-Object System.IO.Compression.GZipStream($ms, [System.IO.Compression.CompressionMode]::Decompress)
        $sr = New-Object System.IO.StreamReader($gz)
        $content = $sr.ReadToEnd()
        $sr.Close()
    } catch {
        Write-Error "❌ Failed to decompress $Label : $_"
        Write-Host "  Base64 length: $($b64.Length)"
        return
    }

    Write-Host "================ $Label ================"
    Write-Host $content
    Write-Host ""
}

Get-RemoteLog -FilePath 'C:\temp\install.log' -Label 'MSI Install Log'
Get-RemoteLog -FilePath 'C:\Program Files\NSClient++\nsclient.log' -Label 'NSClient++ Log'

