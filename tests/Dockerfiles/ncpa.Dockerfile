FROM python:3.12-alpine

# The stock check_ncpa plugin from a pinned NCPA release, not a rewrite:
# the point of this image is to prove the agent answers the client Nagios
# and Nagios XI actually ship. The plugin is a single self-contained file
# with no dependencies beyond the standard library.
ARG NCPA_VERSION=v3.1.2
RUN wget -q -O /usr/bin/check_ncpa.py \
    "https://raw.githubusercontent.com/NagiosEnterprises/ncpa/${NCPA_VERSION}/client/check_ncpa.py" && \
    chmod +x /usr/bin/check_ncpa.py

ENTRYPOINT ["python3", "/usr/bin/check_ncpa.py"]
