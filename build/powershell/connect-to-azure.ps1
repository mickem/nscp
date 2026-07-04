foreach ($module in @('Az.Accounts')) {
    if (-not (Get-Module -ListAvailable -Name $module)) {
        Write-Host "Installing module $module..."
        Install-Module -Name $module -Scope CurrentUser -Force -AllowClobber
    }
    Import-Module $module
}

Write-Host "● Connecting to Azure account..."
if (-not (Get-AzContext)) {
    # WSL / headless hosts have no local browser for the interactive account
    # picker (it hangs on "Please select the account..."), so fall back to
    # device-code auth on Linux/macOS — open the printed URL in any browser
    # and enter the code.
    if ($IsLinux -or $IsMacOS) {
        Connect-AzAccount -UseDeviceAuthentication
    } else {
        Connect-AzAccount
    }
    Set-AzContext -Subscription (Get-AzSubscription)[0]
}
Write-Host "✅ Successfully connected to Azure."
