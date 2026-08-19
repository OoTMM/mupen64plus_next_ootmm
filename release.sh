#!/bin/sh

rm -rf dist

HAVE_PARALLEL_RDP=1 HAVE_PARALLEL_RSP=1 HAVE_THR_AL=1 LLE=1 make -j || exit 1

mkdir -p dist/mupen64plus_ootmm-linux-amd64
cp mupen64plus_ootmm_libretro.so ./dist/mupen64plus_ootmm-linux-amd64
cp mupen64plus_ootmm_libretro.info ./dist/mupen64plus_ootmm-linux-amd64

cd dist
zip -r mupen64plus_ootmm-linux-amd64.zip mupen64plus_ootmm-linux-amd64
rm -rf mupen64plus_ootmm-linux-amd64
cd ..

