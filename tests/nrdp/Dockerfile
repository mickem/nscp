FROM php:8.2-apache

RUN apt-get update && \
    apt-get install -y --no-install-recommends wget tar ca-certificates openssl && \
    rm -rf /var/lib/apt/lists/*

# Install Nagios NRDP server
RUN wget -q https://github.com/NagiosEnterprises/nrdp/archive/refs/tags/2.0.5.tar.gz -O /tmp/nrdp.tar.gz && \
    tar -xzf /tmp/nrdp.tar.gz -C /tmp && \
    mkdir -p /var/www/html/nrdp && \
    cp -r /tmp/nrdp-2.0.5/server /var/www/html/nrdp/server && \
    rm -rf /tmp/nrdp.tar.gz /tmp/nrdp-2.0.5

# Create check results spool directory writable by apache
RUN mkdir -p /nrdp/checkresults && \
    chown -R www-data:www-data /nrdp

# Suppress PHP errors/warnings/notices in the HTTP response so the NRDP XML
# reply remains parseable by the nscp client (NRDP 2.0.5 emits a deprecation
# notice on modern PHP versions which would otherwise corrupt the response).
RUN { \
    echo "display_errors = Off"; \
    echo "display_startup_errors = Off"; \
    echo "error_reporting = 0"; \
    } > /usr/local/etc/php/conf.d/nrdp-quiet.ini

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Enable Apache SSL module and default-ssl site so HTTPS is available too.
RUN a2enmod ssl && a2ensite default-ssl

EXPOSE 80
EXPOSE 443

# Default Environment Variables
ENV TOKEN="change_me"
ENV CHECK_RESULTS_DIR="/nrdp/checkresults"

ENTRYPOINT ["/entrypoint.sh"]
