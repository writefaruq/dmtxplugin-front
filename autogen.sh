#!/bin/sh

aclocal && \
autoheader && \
libtoolize --automake --copy --force && \
automake --add-missing --copy && \
autoconf
./configure
