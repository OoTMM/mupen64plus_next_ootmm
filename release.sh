#!/bin/sh

rm -rf dist

make -j
mkdir -p dist/mupen64plus_ootmm-linux-amd64
cp mupen64plus_ootmm_libretro.so ./dist/mupen64plus_ootmm-linux-amd64
cp mupen64plus_ootmm_libretro.info ./dist/mupen64plus_ootmm-linux-amd64

cd dist
zip -r mupen64plus_ootmm-linux-amd64.zip mupen64plus_ootmm-linux-amd64
rm -rf mupen64plus_ootmm-linux-amd64
cd ..

