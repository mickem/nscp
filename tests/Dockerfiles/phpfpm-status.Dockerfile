# Real PHP-FPM with pm.status_path enabled, for webservers-live.test.ts.
# PHP-FPM speaks FastCGI, not HTTP, so an nginx in the same container
# proxies /status to the pool - the same front-door shape every real
# deployment uses and exactly what check_phpfpm_status fetches.
FROM php:8.3-fpm-alpine

RUN apk add --no-cache nginx && \
    sed -i 's|^;pm.status_path.*|pm.status_path = /status|' /usr/local/etc/php-fpm.d/www.conf && \
    printf 'server {\n    listen 8080;\n    location /status {\n        include fastcgi_params;\n        fastcgi_pass 127.0.0.1:9000;\n        fastcgi_param SCRIPT_NAME /status;\n        fastcgi_param SCRIPT_FILENAME /status;\n    }\n}\n' \
      > /etc/nginx/http.d/status.conf

EXPOSE 8080

# php-fpm daemonizes, nginx holds the foreground so the container lives as
# long as the front door answers.
CMD ["sh", "-c", "php-fpm -D && exec nginx -g 'daemon off;'"]
