---
icon: "🔒"
modules: [WEBServer]
action: none
---
**REST script uploads are staged in a private, randomly named file.**
`PUT /api/v2/scripts/…` no longer writes the upload to `${temp}/<name>`,
where a local user could plant a file of the same name and have it imported
as a command. No configuration change; a staging failure is now reported as
an HTTP 500 instead of importing whatever was on disk. See the
[security notice](../security/notices.md#client-credentials-stay-with-their-target-private-script-upload-staging-and-a-junction-proof-shared-folder).
