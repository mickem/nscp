# $1 refers to the number of instances left after this operation (0=remove, 1=upgrade)

if [ "$1" -eq 0 ]; then
    # Package is being removed
    if command -v systemctl >/dev/null 2>&1; then
        systemctl stop nsclient
        systemctl disable nsclient
    fi

    # Remove the user and group created during installation
    if grep -q nsclient /etc/passwd; then
        userdel nsclient || true
    fi

    if grep -q nsclient /etc/group; then
        groupdel nsclient || true
    fi

    # Remove log directory
    rm -rf /var/log/nsclient
fi