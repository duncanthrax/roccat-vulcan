#!/bin/bash

cd $(dirname $(realpath $0))/../src

DATE=$(date -R)
VERSION="1.1.5-2"

for RELEASE in bionic groovy hirsute
do

cat >debian/changelog <<END
roccat-vulcan ($VERSION~$RELEASE) $RELEASE; urgency=medium

  * Update to $VERSION

 -- Tom Kistner <tom@kistner.nu>  $DATE
END

debuild -S | tee debuild.log 2>&1
dput ppa:duncanthrax/roccat-vulcan "$(perl -ne 'print $1 if /dpkg-genchanges --build=source >(.*)/' debuild.log)"
rm -f debuild.log

done
