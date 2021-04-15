#!/bin/sh

set -e
echo -n 'running aclocal.......'
aclocal
echo 'done'

echo -n 'running autoheader....'
autoheader
echo 'done'

echo -n 'running autoconf......'
autoconf
echo 'done'

echo -n 'running automake......'
automake --copy --foreign --add-missing
echo 'done'

echo 'running configure.....'
#./mips.configure
./configure --prefix=/usr/dot1x --with-epscgi=/usr/dot1x --with-openssl=/usr/dot1x --with-openldap=/usr/dot1x --enable-debug=yes "$@"
echo 'done'

