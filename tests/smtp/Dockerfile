FROM python:3.11-slim

RUN pip install --no-cache-dir aiosmtpd && \
    mkdir -p /inbox && chmod 0777 /inbox

COPY server.py /app/server.py
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Plain submission (with STARTTLS available) and implicit-TLS submission.
EXPOSE 1025 1465

# Hardcoded test credentials. Override with -e on docker run for negative tests.
ENV SMTP_USERNAME="alerts@example.com"
ENV SMTP_PASSWORD="change_me"

ENTRYPOINT ["/entrypoint.sh"]
