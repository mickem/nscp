#!/bin/bash
# MariaDB for the mysql estate role, on Ubuntu. Runs as root before NSClient++
# is installed, with ROLE_PASSWORD in the environment (see role.json and
# README.md, "Adding a role"). The agent connects as nscp over TCP to
# 127.0.0.1; the role bundle carries the same password into [/settings/mysql].
set -euo pipefail
: "${ROLE_PASSWORD:?ROLE_PASSWORD is not set}"
export DEBIAN_FRONTEND=noninteractive

# A fresh VM is often still running cloud-init's or unattended-upgrades' apt;
# wait for the lock instead of failing on it.
apt_get() { apt-get -o DPkg::Lock::Timeout=600 "$@"; }
apt_get update
apt_get install -y mariadb-server
systemctl enable --now mariadb

# root authenticates over the unix socket, so no root password is involved.
# ALTER as well as CREATE: re-running the fragment must reset the password.
mysql <<SQL
CREATE USER IF NOT EXISTS 'nscp'@'127.0.0.1' IDENTIFIED BY '${ROLE_PASSWORD}';
CREATE USER IF NOT EXISTS 'nscp'@'localhost' IDENTIFIED BY '${ROLE_PASSWORD}';
ALTER USER 'nscp'@'127.0.0.1' IDENTIFIED BY '${ROLE_PASSWORD}';
ALTER USER 'nscp'@'localhost' IDENTIFIED BY '${ROLE_PASSWORD}';
FLUSH PRIVILEGES;
SQL

# Prove the login the agent will make, so a broken account fails the install
# stage here rather than surfacing later as a failed check.
mysql --protocol=TCP -h 127.0.0.1 -u nscp -p"${ROLE_PASSWORD}" -e "SELECT VERSION() AS mariadb_version"
