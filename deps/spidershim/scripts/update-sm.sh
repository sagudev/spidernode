#!/bin/bash

set -e

if ! test -f "$1"; then
  >&2 echo 'Please pass in the path to the tar with Spidermonkey.'
  exit 1
fi

if [ -z "$2" ]; then
  >&2 echo 'Please pass in the commit of Spidermonkey.'
  exit 1
fi


if test -d spidermonkey; then
  rm -rf spidermonkey
else
  >&2 echo 'Please run this script from the deps/spidershim subdirectory.'
  exit 1
fi

mkdir mozjs
tar -xjf "$1" -C mozjs
(
  cd mozjs
  mv mozjs* mozjs
)

rsync -av --delete --exclude-from='scripts/rsync-exclusions.txt' mozjs/mozjs/ spidermonkey/

rm -Rf mozjs

for patch in `ls spidermonkey-patches/* | sort`; do
  (cd spidermonkey && patch -p1 --no-backup < "../$patch")
done

git add spidermonkey
# The following will fail if there are no deleted files, so || with true.
git rm -r `git ls-files --deleted spidermonkey` || true
# The following will fail if there are no added files, so || with true.
git add -f `git ls-files --others spidermonkey` || true

scripts/build-spidermonkey-files.py
sed -i '/spaces.bat/d' ./spidermonkey-files.gypi
sed -i -e "\$aNO_EXPAND_LIBS = True" ./spidermonkey/mozglue/build/moz.build # to fix libmozglue.a not found
git add spidermonkey-files.gypi
git commit -m "Sync SpiderMonkey from $2"
