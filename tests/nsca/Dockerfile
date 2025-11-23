FROM debian:stable-slim

RUN groupadd -r nagios && useradd -r -g nagios -d /var/run/nsca -s /sbin/nologin nagios

RUN apt-get update && \
    apt-get install -y nsca libmcrypt4 procps && \
    mkdir -p /var/run/nsca && \
    chown nagios:nagios /var/run/nsca

RUN mkdir -p /nsca && chown nagios:nagios /nsca

# RUN sed -i 's|^#check_result_path=.*$|check_result_path=/nsca/results.txt|' /etc/nsca.cfg
RUN sed -i 's|^command_file=.*$|command_file=/nsca/results.txt|' /etc/nsca.cfg



RUN sed -i 's|^append_to_file=.*$|append_to_file=1|' /etc/nsca.cfg
RUN sed -i 's|^debug=.*$|debug=1|' /etc/nsca.cfg

# Copy the entrypoint script
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

RUN chown nagios:nagios /etc/nsca.cfg

#USER nagios
# Expose the standard NSCA port
EXPOSE 5667


# Default Environment Variables
# 0=None, 1=XOR, 14=AES-128, etc.
ENV ENCRYPTION_METHOD=1
ENV PASSWORD="change_me"

ENTRYPOINT ["/entrypoint.sh"]