#!/bin/bash

MRBW_WIFI_REPO="git@github.com:IowaScaledEngineering/mrbw-wifi"
TMPDIR=`mktemp -d`

VERSION_STR=`echo "mrbwwifi-$1_$2_$3"`


print_help()
{
        echo "make-firmware-package.sh help"
        echo "  Stamps version numbers and makes mrbwwifi firmware package"
        echo " "
        echo "make-firmware-package.sh usage:"
        echo " make-firmware-package.sh <major> <minor> <delta>"
        echo " Example: make-firmware-package.sh 5 0 2"
        exit 1
}

if [ $# -lt 3 ] ; then
        print_help
fi

if [[ `echo $1 | sed 's/^[1-9]$//' | wc -c` -eq 0 ]] ; then
        echo "Fail - major version ($1) not valid, must be 5-9"
        exit
fi
if [[ `echo $1 | sed 's/^[0-9]$//' | wc -c` -eq 0 ]] ; then
        echo "Fail - minor version ($1) not valid, must be 0-9"
        exit
fi
if [[ `echo $1 | sed 's/^[0-9]$//' | wc -c` -eq 0 ]] ; then
        echo "Fail - delta version ($1) not valid, must be 0-9"
        exit
fi



echo "**** DANGER! DANGER! DANGER, WILL ROBINSON! ****"
echo " You're about to tag MRBW-WIFI with version $VERSION_STR"
while true; do
   read -p " Are you ABSOLUTELY SURE you want to do this? [y/n] " yn
   case $yn in
      [Yy]* ) break;;
      [Nn]* ) exit 1;;
      * ) echo "Invalid response - y or n";;
   esac
done


pushd $TMPDIR
echo "Changing to $TMPDIR"
git clone $MRBW_WIFI_REPO
cd mrbw-wifi
git tag > ../tag-log
if [[ `grep $VERSION_STR ../tag-log | wc -c` -ne 0 ]] ; then
        echo "ERROR! ERROR! You've already used tag $VERSION_STR."
        exit 1
fi

GIT_VER=`git rev-parse HEAD`

cp src/wibridge-python/version.py ../temp-version.py
sed -e 's/^_version_major_ .*/_version_major_ = '$1'/' < ../temp-version.py > ../temp-version2.py
sed -e 's/^_version_minor_ .*/_version_minor_ = '$2'/' < ../temp-version2.py > ../temp-version3.py
sed -e 's/^_version_delta_ .*/_version_delta_ = '$3'/' < ../temp-version3.py > ../temp-version4.py
sed -e 's/^_version_git_ .*/_version_git_ = "'$GIT_VER'"/' < ../temp-version4.py > src/wibridge-python/version.py

git add src/wibridge-python/version.py

BIN_FILENAME="mrbwwifi-$1_$2_$3.bin"

cd src/firmware-builder
python3 firmware-builder.py ../wibridge-python/ ../releases/$BIN_FILENAME $1 $2 $3 $GITVER
git add ../releases/$BIN_FILENAME
cd ../..

git commit -m "Automatically generating $BIN_FILENAME"
git push

