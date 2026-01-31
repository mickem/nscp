# $1 refers to the number of installed instances (1=install, 2=upgrade)

if [ "$1" -eq 1 ]; then
    # Initial installation
    systemctl daemon-reload
    systemctl enable nsclient
    systemctl start nsclient
elif [ "$1" -eq 2 ]; then
    # Upgrade
    systemctl daemon-reload
    systemctl try-restart nsclient
fi