FROM alpine:3.20

RUN apk add --no-cache \
    build-base \
    tar \
    wget

# Build the real check_nt from the official nagios-plugins release (the
# canonical legacy NSClient client). Only check_nt and the libraries it
# links against are compiled - a full `make` would build every plugin and
# drag in optional dependencies we don't need.
RUN wget https://github.com/nagios-plugins/nagios-plugins/releases/download/release-2.5/nagios-plugins-2.5.tar.gz -O /tmp/np.tar.gz && \
    tar -xzf /tmp/np.tar.gz -C /tmp && \
    cd /tmp/nagios-plugins-2.5 && \
    ./configure --without-openssl --disable-nls && \
    make -C gl && \
    make -C lib && \
    make -C plugins check_nt && \
    cp plugins/check_nt /usr/bin/check_nt && \
    rm -rf /tmp/np.tar.gz /tmp/nagios-plugins-2.5
