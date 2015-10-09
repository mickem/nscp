# Restart Service Script
# Please enable external scripts and external scrips variable before use.

param (
   [string[]]$serviceName
)
Foreach ($Service in $ServiceName)
{
  Restart-Service $ServiceName -ErrorAction SilentlyContinue -ErrorVariable ServiceError
  If (!$ServiceError) {
    $Time=Get-Date
    Write-Host "Restarted service $Service at $Time"     
  }
  If ($ServiceError) {
    write-host $error[0] 
    exit 3
  }
}  
 


