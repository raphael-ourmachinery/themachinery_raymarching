#!/bin/sh

if [ -z "$TM_SDK_DIR" ]
then
    echo "TM_SDK_DIR environment variable is not set. Please point it to your The Machinery directory.\n"
else
    ${TM_SDK_DIR}/bin/tmbuild
fi
