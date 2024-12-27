#!/bin/sh

mkdir -p doom
rm -f doom/*
cp README doom/readme
cp COPYING doom/copying
cp ../SDL-1.2/README.MiNT doom/readme.sdl
cp src/doom doom/doom.gtp
m68k-atari-mint-strip doom/doom.gtp

rm -f doom.zip
zip -o doom.zip doom/*


gh release delete latest --cleanup-tag -y
gh release create latest --notes "latest"
gh release upload latest doom.zip --clobber

