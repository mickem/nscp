foreach ($module in @('Az.Accounts')) {
    if (-not (Get-Module -ListAvailable -Name $module)) {
        Write-Host "Installing module $module..."
        Install-Module -Name $module -Scope CurrentUser -Force -AllowClobber
    }
    Import-Module $module
}

Write-Host "● Connecting to Azure account..."
Connect-AzAccount
Set-AzContext -Subscription (Get-AzSubscription)[0]
Write-Host "✅ Successfully connected to Azure."
