FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y squid apache2-utils && \
    rm -rf /var/lib/apt/lists/*

# Create htpasswd file: testuser / testpass
RUN htpasswd -bc /etc/squid/htpasswd testuser testpass

COPY squid.conf /etc/squid/squid.conf

# Port 3128 — open proxy (no authentication required)
# Port 3129 — authenticated proxy (basic auth required)
EXPOSE 3128 3129

CMD ["squid", "-N", "-f", "/etc/squid/squid.conf"]
