#!/bin/sh
cd $1
echo "Building: $1"
vagrant destroy --force 2>&1 1> /dev/null
echo "Old machine destroyed"
vagrant up --no-provision
echo "Virtual machine booted"
vagrant provision
EXIT=$?
if $EXIT == 0 ; then
  echo "Build success: $EXIT"
else
  echo "Build failed: $EXIT"
fi
vagrant destroy --force
exit $EXIT