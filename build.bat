@echo off
if not defined TM_SDK_DIR (
    echo TM_SDK_DIR environment variable is not set. Please point it to your The Machinery directory.
    set errorlevel=1
) else (
    call "%TM_SDK_DIR%/bin/tmbuild.exe"
)
if NOT ["%errorlevel%"]==["0"] pause