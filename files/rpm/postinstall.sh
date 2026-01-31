# $1 refers to the number of installed instances (1=install, 2=upgrade)

if [ "$1" -eq 1 ]; then
    # Initial installation
    systemctl daemon-reload
    systemctl enable myservice
    systemctl start myservice
elif [ "$1" -eq 2 ]; then
    # Upgrade
    systemctl daemon-reload
    systemctl try-restart myservice
fi