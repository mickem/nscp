FROM debian:stable-slim

RUN apt-get update && \
    apt-get install -y nsca-ng-server procps && \
    mkdir -p /var/run/nsca-ng && \
    mkdir -p /nsca-ng

# Copy the entrypoint script
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Expose the standard NSCA-NG port
EXPOSE 5668

# Default Environment Variables
ENV NSCA_NG_PASSWORD="change_me"
ENV NSCA_NG_IDENTITY="nscp"

ENTRYPOINT ["/entrypoint.sh"]
