# A real FreeRADIUS server used by checknet-freeradius.test.ts to prove
# check_radius interoperates with an independent RADIUS implementation, not
# just with the Node fixture in checknet-radius.test.ts.
#
# 3.2.5+ is required: it is the BlastRADIUS release, which signs every reply
# to an Access-Request with Message-Authenticator (check_radius refuses a reply
# without one) and lets a client demand one on requests
# (`require_message_authenticator = yes` in freeradius/clients.conf), so a
# passing test also proves our request-side HMAC is correct.
#
# Published with `-p ...:1812/udp` via raw `docker run` (testcontainers only
# maps TCP); the test waits for the "Ready to process requests" log line.
FROM freeradius/freeradius-server:3.2.10

# Replace the stock default site with a minimal PAP-only one that also
# answers Status-Server; inner-tunnel stays because the eap module refers to it.
RUN rm -f /etc/freeradius/sites-enabled/default
COPY Dockerfiles/entrypoints/freeradius/clients.conf /etc/freeradius/clients.conf
COPY Dockerfiles/entrypoints/freeradius/authorize /etc/freeradius/mods-config/files/authorize
COPY Dockerfiles/entrypoints/freeradius/site /etc/freeradius/sites-enabled/nscp
# A Windows checkout may carry CRLF (`* text=auto`); a stray \r would end up
# inside the quoted password and the shared secret.
RUN sed -i 's/\r$//' /etc/freeradius/clients.conf \
        /etc/freeradius/mods-config/files/authorize \
        /etc/freeradius/sites-enabled/nscp

EXPOSE 1812/udp

# -X: foreground, single-threaded, full debug log. The log is what a failing
# test prints, and it is where FreeRADIUS says why it dropped a packet.
CMD ["freeradius", "-X"]
