<#
.SYNOPSIS
    Fetches and displays NSCP logs from an Azure Linux VM
.DESCRIPTION
    This is not to be used, it is only for setting up personal test machines
.PARAMETER ResourceGroupName
    The name for the resource group.
.PARAMETER VmName
    The name for the virtual machine.
#>
param(
    [string]$ResourceGroupName = "NSCP-RG",
    [string]$VmName = "NSCP-Rocky-Test"
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
#!/bin/bash
if [ -f '$FilePath' ]; then
    gzip -c '$FilePath' | base64 > /tmp/log_transfer.b64
    TOTAL=`$(wc -c < /tmp/log_transfer.b64)
    CHUNK_SIZE=3000
    CHUNKS=`$(( (TOTAL + CHUNK_SIZE - 1) / CHUNK_SIZE ))
    echo "CHUNKS:`$CHUNKS"
else
    echo "FILE_NOT_FOUND"
fi
"@
    $result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName `
        -VMName $VmName `
        -CommandId 'RunShellScript' `
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
        $skip = $i * 3000
        $scriptBlock = @"
#!/bin/bash
dd if=/tmp/log_transfer.b64 bs=1 skip=$skip count=3000 2>/dev/null
"@
        $result = Invoke-AzVMRunCommand -ResourceGroupName $ResourceGroupName `
            -VMName $VmName `
            -CommandId 'RunShellScript' `
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

Get-RemoteLog -FilePath '/var/log/nsclient/nsclient.log' -Label 'NSClient++ Log'
Get-RemoteLog -FilePath '/var/log/dnf.log' -Label 'DNF Install Log'

