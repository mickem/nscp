# Login

The login API can be used to verify that the supplied credentials are valid
and to retrieve the current user's session token.

The login controller is mounted on both `/api/v1/login` and `/api/v2/login`.

## Get current user

Returns the username and session token of the currently authenticated user.
This is mainly used by the bundled web UI to know whether a user is logged
in and to obtain the session token used for subsequent calls.

| Key       | Value          |
|-----------|----------------|
| Verb      | GET            |
| Address   | /api/v2/login  |
| Privilege | login.get      |

### Request

```
GET /api/v2/login
```

### Response

```json
{
    "user": "admin",
    "key": "eyJhbGciOi…"
}
```

If authentication fails the endpoint returns `403 Forbidden` like every other
protected endpoint.

### Example

```
curl -k -s -u admin https://localhost:8443/api/v2/login | python -m json.tool
{
    "user": "admin",
    "key": "eyJhbGciOi…"
}
```

The returned `key` can be used as a bearer token on subsequent calls:

```
curl -k -s -H 'Authorization: Bearer eyJhbGciOi…' https://localhost:8443/api/v2/info
```

A key is valid for eight hours from the login that issued it, and survives a
restart of the agent: the keys this endpoint has handed out are written to
`${data-path}/nsclient.db` at a clean shutdown and read back at the next start
(only the SHA-256 of a key is ever stored). `DELETE
/api/v2/login` revokes a key immediately, and so does a change to the user's
password or role. `persist sessions = false` under `[/settings/WEB/server]`
switches this off, so that a restart ends every session as it used to. A user
whose password is configured in cleartext is the one exception to the restart
part - see the [Upgrading](../../setup/upgrading.md) page.

