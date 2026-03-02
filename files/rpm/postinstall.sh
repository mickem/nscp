# $1 refers to the number of installed instances (1=install, 2=upgrade)

if [ "$1" -eq 1 ]; then
    # Initial installation

    # Create nsclient user and group if they don't exist
    if ! grep -q nsclient /etc/passwd; then
        useradd --system --no-create-home --shell /sbin/nologin nsclient
    fi

    # Create log folder if it doesn't exist
    mkdir -p /var/log/nsclient
    chown nsclient:nsclient /var/log/nsclient

    if command -v systemctl >/dev/null 2>&1; then
        systemctl daemon-reload
        systemctl enable nsclient
        systemctl start nsclient
    fi
elif [ "$1" -eq 2 ]; then
    # Upgrade
    if command -v systemctl >/dev/null 2>&1; then
        systemctl daemon-reload
        systemctl try-restart nsclient
    fi
fi