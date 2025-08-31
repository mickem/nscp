#Requires -Module Az.Accounts
#Requires -Module Az.Resources

<#
.SYNOPSIS
    Removes all Azure resources created by setup-machine.ps1 by deleting the resource group.
.PARAMETER ResourceGroupName
    The name of the resource group to remove.
#>
param(
[string]$ResourceGroupName = "NSCP-RG"
)

Write-Host "Removing resource group: $ResourceGroupName (this deletes all resources in it)..."
Remove-AzResourceGroup -Name $ResourceGroupName -Force

Write-Host "✅ Resource group '$ResourceGroupName' removed."
