# Real Apache httpd with mod_status enabled, for webservers-live.test.ts.
# The stock httpd.conf ships with the status module commented out and no
# handler mapped; uncomment the module and expose the standard
# /server-status endpoint (ExtendedStatus adds the ReqPerSec/BytesPerSec
# lines check_apache_status parses). Require all granted: requests arrive
# from the docker bridge, not localhost.
FROM httpd:2.4

RUN sed -i 's|^#\(LoadModule status_module\)|\1|' /usr/local/apache2/conf/httpd.conf && \
    printf '\nExtendedStatus On\n<Location "/server-status">\n    SetHandler server-status\n    Require all granted\n</Location>\n' \
      >> /usr/local/apache2/conf/httpd.conf

EXPOSE 80
