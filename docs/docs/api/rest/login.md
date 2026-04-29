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

