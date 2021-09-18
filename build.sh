#!/bin/sh
if [ -z "$TM_SDK_DIR" ]
then
    echo "TM_SDK_DIR environment variable is not set. Please point it to your The Machinery directory.\n"
else
    if [ -d "$TM_SDK_DIR/headers" ]
    then
        echo " akih"
        ${TM_SDK_DIR}/bin/tmbuild
    elif [ -f "$TM_SDK_DIR/bin/Debug/tmbuild" ]
    then
        echo " aki"
        ${TM_SDK_DIR}/bin/Debug/tmbuild
    elif [ -f "$TM_SDK_DIR/bin/Release/tmbuild" ]
    then
        echo " aki"
        ${TM_SDK_DIR}/bin/Release/tmbuild
    fi

    if [ -f "./build/.is_general_project" ]
    then
        if [ -f "$TM_SDK_DIR/bin/Debug/the-machinery" ]
        then
            cp bin/Debug/*.so ${TM_SDK_DIR}/bin/Debug/plugins
        elif [ -f "$TM_SDK_DIR/bin/Release/the-machinery" ]
        then
            cp bin/Release/*.so ${TM_SDK_DIR}/bin/Release/plugins
        elif [ -f "$TM_SDK_DIR/bin/the-machinery" ]
        then
            cp bin/Debug/*.so ${TM_SDK_DIR}/bin/plugins
            cp bin/Release/*.so ${TM_SDK_DIR}/bin/plugins
        fi
        exit
    else
        echo "Is this plugin a general plugin? A general plugin is a plugin that should be available in all projects. If you press yes the plugin.dll & .pdb will be copied into the engines plugins folder. Otherweise you need to drag the plugin into your project."
        select yn in "Yes" "No"; do
            case $yn in
                Yes ) 
                    echo "is_general_project" > "./build/.is_general_project"
                    if [ -f "$TM_SDK_DIR/bin/Debug/the-machinery" ]
                    then
                        cp bin/Debug/*.so ${TM_SDK_DIR}/bin/Debug/plugins
                    elif [ -f "$TM_SDK_DIR/bin/Release/the-machinery" ]
                    then
                        cp bin/Release/*.so ${TM_SDK_DIR}/bin/Release/plugins
                    elif [ -f "$TM_SDK_DIR/bin/the-machinery" ]
                    then
                        cp bin/Debug/*.so ${TM_SDK_DIR}/bin/plugins
                        cp bin/Release/*.so ${TM_SDK_DIR}/bin/plugins
                    fi
                    break;;
                No ) exit ; exit;;
            esac
        done
    fi

fi

