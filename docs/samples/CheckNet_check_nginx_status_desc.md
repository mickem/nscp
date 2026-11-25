#### About `check_nginx_status`

`check_nginx_status` fetches NGINX's
[stub_status](https://nginx.org/en/docs/http/ngx_http_stub_status_module.html)
page and exposes the reported values as filter keywords. The endpoint must be
enabled in the NGINX configuration, conventionally as `/nginx_status` (or
`/stub_status`):

```nginx
location /nginx_status {
    stub_status;
    allow 127.0.0.1;
    deny all;
}
```

The check emits a single record; connection thresholds are opt-in.
If the endpoint cannot be fetched (`invalid_url`, `http_<code>`, `error: ...`)
or does not look like a stub_status page, there is no data to judge: the check
returns **UNKNOWN** with `<url>: <reason>` as the message and no performance
data, so a graph shows a gap rather than a drop to zero.

`accepts`, `handled`, `requests` and `dropped` are cumulative since NGINX
started, so `dropped > 0` stays raised until the next restart once a drop has
ever happened; treat it as a "worker_connections is too low" indicator rather
than a live gauge.
