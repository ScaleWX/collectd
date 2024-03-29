#!/bin/sh
DEFAULT_VERSION="5.12.0.brl2"

if [ -d .git ]; then
	VERSION="`git describe --abbrev=7 2> /dev/null | sed -e '/^collectd-/!d' -e 's///' -e 'y/-/./'`"
fi

if test -z "$VERSION"; then
	VERSION="$DEFAULT_VERSION"
fi

printf "%s" "$VERSION"
