#!/bin/bash 

renice 20 $$ >/dev/null 2>&1 

test -z "$OCHA_INDEXER_ARGS" && OCHA_INDEXER_ARGS="--quiet"
test -z "$XDG_DATA_DIRS" && XDG_DATA_DIRS=/usr/local/share:/usr/share:/opt/gnome/share:/opt/kde/share:/opt/kde3/share:$HOME/.kde/share:$HOME/.gnome/share
test -z "$XDG_DATA_HOME" && XDG_DATA_HOME=$HOME/.local/share

for i in $HOME/Desktop $HOME/.gnome-desktop; do
	test -d $i && ocha_indexer $OCHA_INDEXER_ARGS directory $i
done

grep -l NETSCAPE-Bookmark-file $(find $HOME/.??* -name 'bookmarks.html') | while read l; do 
	ocha_indexer $OCHA_INDEXER_ARGS bookmarks $l
done
	
echo $XDG_DATA_HOME:$XDG_DATA_DIRS | tr ':' '\n' | while read l; do
	test -d $l && ocha_indexer $OCHA_INDEXER_ARGS applications $l
done

