# Real NGINX with stub_status enabled, for webservers-live.test.ts.
# A dedicated server on 8080 keeps the stock default site untouched; the
# endpoint emits the three-line stub_status format check_nginx_status parses.
FROM nginx:stable-alpine

RUN printf 'server {\n    listen 8080;\n    location /stub_status {\n        stub_status;\n    }\n}\n' \
      > /etc/nginx/conf.d/stub_status.conf

EXPOSE 8080
