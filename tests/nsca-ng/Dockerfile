FROM debian:stable-slim

RUN apt-get update && \
    apt-get install -y nsca-ng-server procps && \
    mkdir -p /var/run/nsca-ng && \
    mkdir -p /nsca-ng && \
    chmod 0777 /nsca-ng

# Copy the entrypoint script.
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Standard NSCA-NG TLS port.
EXPOSE 5668

# Default credentials. Override with -e on `docker run` for negative tests.
ENV NSCA_NG_PASSWORD="change_me"
ENV NSCA_NG_IDENTITY="nscp"

ENTRYPOINT ["/entrypoint.sh"]
