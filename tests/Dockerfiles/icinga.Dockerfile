# Integration-test fixture for the IcingaClient module.
#
# Runs a single-node Icinga 2 daemon with:
#  * the `api` feature enabled (REST listener on 5665, self-signed cert);
#  * an ApiUser whose name/password are supplied via env vars;
#  * a fixed `test-host` and a handful of passive Service objects that the
#    icinga-submit suite targets when validating that `nscp icinga
#    submit_icinga` actually causes Icinga 2 to update each service's
#    last_check_result.
#
# We build on debian:bookworm-slim and install Debian's `icinga2` package
# rather than using `icinga/icinga2`. The official image expects systemd
# and bundles a whole multi-feature config; for a self-contained test we
# just want `icinga2 daemon -F` with our config dropped into conf.d.
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        icinga2 \
        icinga2-bin \
        icinga2-common \
        ca-certificates \
        curl \
    && rm -rf /var/lib/apt/lists/*

# Icinga 2 REST API.
EXPOSE 5665

# Default credentials; tests override these via -e for the negative test
# that submits with a bad password.
ENV ICINGA_API_USER=nscp
ENV ICINGA_API_PASSWORD=change_me

COPY Dockerfiles/entrypoints/icinga.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
