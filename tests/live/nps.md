# NPS and RADIUS acceptance lab

Use a disposable Windows Server with Desktop Experience and the NPS role.
Standalone local test accounts are sufficient for basic PAP; use a domain member
and a test domain when validating AD-dependent policies. Enable CheckWindowsApps,
CheckNet and WEBServer on the target NSClient++ instance.

Register the agent's actual source IP as a RADIUS client. Configure a narrow lab
network policy allowing PAP for one limited test identity and the probe's
NAS-Identifier (`nsclient-monitor`). Keep that policy separate from production
EAP policies. Enable NPS success and failure auditing. Configure file accounting
if testing logging. The RADIUS server must return Message-Authenticator in replies.

On the NSClient++ host, create protected shared-secret/password files readable by
the agent service account. The paths below are on that host, not the test runner.

```powershell
$env:NSCP_TARGET_URL = 'https://nps-lab:8443'
$env:NSCP_TARGET_PASSWORD = '<NSClient++ web password>'
$env:NSCP_NPS_LIVE = '1'
$env:NSCP_NPS_HOST = 'nps-lab'
$env:NSCP_NPS_USERNAME = 'nps-test'
$env:NSCP_NPS_SECRET_FILE = 'C:\nps-test\secret.txt'
$env:NSCP_NPS_PASSWORD_FILE = 'C:\nps-test\password.txt'
# Optional, current file after rotation:
$env:NSCP_NPS_LOG_FILE = 'C:\Windows\System32\LogFiles\IN260928.log'
# Optional, localized performance object name:
# $env:NSCP_NPS_COUNTER_OBJECT = 'NPS Authentication Server'
npm run test:live -- nps.test.ts
```

The suite sends one successful and one intentionally rejected authentication,
then checks that the NPS event reader sees both types. It tests real provider
availability, not exact traffic totals. Run it on a quiet lab; additional traffic
can contribute to the aggregate. Authentication can succeed before its audit
record becomes visible, so the suite polls for the records.

For independent interoperability, send equivalent requests with FreeRADIUS
`radclient`, using the same registered source and shared secret. For accounting,
send Accounting-Start and Accounting-Stop with a unique test session ID; verify
Accounting-Response and the corresponding records in the selected log. Set
`require-traffic=true` on the accounting check when checking those writes.

On the disposable VM, separately verify stopped IAS, auditing disabled, Security
log access denied, an unavailable accounting destination, and log rotation. Restore
each setting before the next case. Missing prerequisite data must be UNKNOWN;
stopped IAS must fail `check_service`, and an unanswered probe must be CRITICAL.
Faults and exact thresholds are covered deterministically by the normal C++/Jest
suites. PEAP/EAP-TLS and Entra MFA need their own authentication client/lab and are
outside the PAP probe's coverage.
