#!/bin/sh

rm -rf dist

BUILD_ENV="HAVE_PARALLEL_RDP=1 HAVE_PARALLEL_RSP=1 HAVE_THR_AL=1 LLE=1"

if [ "$OS" = "windows-x64" ]; then
    arch_name="windows-x64"
    ext="dll"
    cores_dir="cores"
    info_dir="info"
    BUILD_ENV="$BUILD_ENV platform=win64 ARCH=x86_64 MSYSTEM=MINGW64 CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++"
elif [ "$OS" = "linux-x64" ]; then
    arch_name="linux-amd64"
    ext="so"
    cores_dir=
    info_dir=
else
    echo "Unknown OS: $OS"
    exit 1
fi

make $BUILD_ENV -j || exit 1

mkdir -p dist/mupen64plus_ootmm-$arch_name
cp mupen64plus_ootmm_libretro.$ext ./dist/mupen64plus_ootmm-$arch_name/$cores_dir
cp mupen64plus_ootmm_libretro.info ./dist/mupen64plus_ootmm-$arch_name/$info_dir

cd dist
zip -r mupen64plus_ootmm-$arch_name.zip mupen64plus_ootmm-$arch_name
rm -rf mupen64plus_ootmm-$arch_name
cd ..

