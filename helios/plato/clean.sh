#!/bin/bash

set -x

# Clean runtime files.  Useful for debugging and getting ready for packaging

# Clean nginx state
rm -r server/var/*/nginx/*

# Clean django state
rm django/uvicorn.log

# clean the pychache
find . -name '__pycache__' | xargs -n 1 rm -r

# deep clean django state
#rm django/db.sqlite3

set +x
