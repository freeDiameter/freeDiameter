#!/bin/bash 

# This script will simply retrieve the latest "runtest" script and run it.
pushd ~/fDtests
mv -f runtest.sh runtest.sh.prev
wget "http://www.freediameter.net/hg/freeDiameter/raw-file/tip/contrib/nightly_tests/runtests.sh"
chmod +x runtest.sh
popd
~/fDtests/runtest.sh
