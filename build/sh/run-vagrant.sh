#!/bin/sh

destroy_it ()
{
  echo "[::] Destroying virtual machine..."
  vagrant destroy --force 2>&1 1> /dev/null
}

cd $1
echo "[::] Building: $1"
destroy_it

echo "[::] Booting new machine"
vagrant up --no-provision
if [ "$?" -ne "0" ] ; then
  echo "[!!] Create vm failed: $?"
  exit 1
fi

echo "[::] Building NSCP"
vagrant provision
if [ "$?" -ne "0" ] ; then
  echo "[!!] Build failed: $?"
  destroy_it
  exit 1
fi
destroy_it
exit 0
