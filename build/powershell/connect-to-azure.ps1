foreach ($module in @('Az.Accounts')) {
    if (-not (Get-Module -ListAvailable -Name $module)) {
        Write-Host "Installing module $module..."
        Install-Module -Name $module -Scope CurrentUser -Force -AllowClobber
    }
    Import-Module $module
}

# Azure's "MFA for Azure resource management" enforcement rejects every
# create/update/delete with "Resource '...' was disallowed by Azure: ... without
# authenticating through MFA" while leaving reads working, so no read-only probe
# can see it coming — only the token's own claims can. A token that did MFA says
# so in amr, and one that satisfied the p1 authentication context Azure asks for
# carries it in acrs.
$mfaClaimsChallenge = "eyJhY2Nlc3NfdG9rZW4iOnsiYWNycyI6eyJlc3NlbnRpYWwiOnRydWUsInZhbHVlcyI6WyJwMSJdfX19"
function Test-AzArmTokenHasMfa {
    # $null when the answer cannot be determined; never throws — this is a
    # diagnostic, and must not be the thing that stops a working login.
    try {
        $token = Get-AzAccessToken -ResourceUrl "https://management.azure.com/" -AsSecureString -ErrorAction Stop
        $jwt = [System.Net.NetworkCredential]::new("", $token.Token).Password
        $payload = $jwt.Split('.')[1].Replace('-', '+').Replace('_', '/')
        while ($payload.Length % 4) { $payload += '=' }
        $claims = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($payload)) | ConvertFrom-Json
        $names = $claims.PSObject.Properties.Name
        if ($names -contains 'acrs' -and @($claims.acrs).Count -gt 0) { return $true }
        if ($names -contains 'amr' -and (@($claims.amr) -contains 'mfa')) { return $true }
        return $false
    }
    catch { return $null }
}

Write-Host "● Connecting to Azure account..."
# Az autosaves the context to ~/.Azure, so a context keeps being returned long
# after its refresh token expired (or the tenant started demanding MFA again).
# The presence of a context therefore proves nothing — spend one cheap ARM call
# to prove it still works, or every script downstream sails past this check and
# dies on its first real Azure call instead ("credentials ... have expired,
# please run Connect-AzAccount" out of New-AzPublicIpAddress & friends).
$context = Get-AzContext
if ($context) {
    try {
        Get-AzSubscription -SubscriptionId $context.Subscription.Id -TenantId $context.Tenant.Id -ErrorAction Stop | Out-Null
    }
    catch {
        # Not worth quoting the exception: a dead token surfaces here as the
        # thoroughly misleading "subscription ... was not found in tenant ...",
        # with the real reason relegated to a warning from the token acquisition.
        Write-Warning "The cached Azure credentials for $($context.Account.Id) have expired or need re-authentication; signing in again."
        $context = $null
    }
}
if (-not $context) {
    # Re-authenticate against the tenant we were using, so a conditional-access
    # /MFA re-prompt does not come back as "rerun with -TenantId <id>".
    $login = @{}
    if ($tenantId = (Get-AzContext).Tenant.Id) { $login.TenantId = $tenantId }
    # WSL / headless hosts have no local browser for the interactive account
    # picker (it hangs on "Please select the account..."), so fall back to
    # device-code auth on Linux/macOS — open the printed URL in any browser
    # and enter the code.
    if ($IsLinux -or $IsMacOS) {
        Connect-AzAccount -UseDeviceAuthentication @login
    } else {
        Connect-AzAccount @login
    }
    Set-AzContext -Subscription (Get-AzSubscription)[0]
}

# Warn rather than throw: a tenant that does not enforce MFA for resource
# management writes happily with a password-only token, and service principals
# and managed identities never carry an MFA claim at all, so a hard stop here
# would block setups that work. Requesting the p1 challenge unprompted is the
# other thing not to do — a tenant that has no such authentication context
# defined would fail a login that would otherwise have succeeded.
$account = (Get-AzContext).Account
if ($account.Type -eq 'User' -and (Test-AzArmTokenHasMfa) -eq $false) {
    Write-Warning @"
$($account.Id) is signed in without MFA (the token carries no mfa/acrs claim).
Reads will work, but if this tenant enforces MFA for Azure resource management
every VM/resource-group creation will fail with "was disallowed by Azure".
To re-authenticate with MFA before provisioning anything:
  Connect-AzAccount -Tenant $((Get-AzContext).Tenant.Id) -ClaimsChallenge "$mfaClaimsChallenge"
"@
}
Write-Host "✅ Successfully connected to Azure."
