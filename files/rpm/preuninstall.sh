# $1 refers to the number of instances left after this operation (0=remove, 1=upgrade)

if [ "$1" -eq 0 ]; then
    # Package is being removed
    systemctl stop nsclient
    systemctl disable nsclient
fi