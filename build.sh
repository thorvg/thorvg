#!/usr/bin/env bash

# build thorvg
# Usage: ./build.sh [ohos|ios|local|android|all] [static|shared] [debug|release] [extra-build-options]

FILE_NAME="$0"
LOCAL_DIR=$(cd `dirname $0`; pwd)

PLATFORM="${1:-}"
[[ $# -gt 0 ]] && shift
STATIC="${1:-static}"
[[ $# -gt 0 ]] && shift
BUILD_TYPE="${1:-release}"
[[ $# -gt 0 ]] && shift

USAGE="$FILE_NAME [android|ohos|ios|local|all] [static|shared] [debug|release] [extra-build-options]"
myexit() {
  local code="$1"; shift
  echo "$@"
  exit $code
}

[[ -z "$PLATFORM" ]] && myexit 1 "$USAGE"

#
# $1: platform ios/android/ohos/macos/linux
# $2: arch
# $3: cross_file
# $5: extras build options
meson_build() {
  local platform="$1"
  local arch="$2"
  local cross_file="$3"
  local extras="$4"

  BUILD_OPTIONS="--buildtype=$BUILD_TYPE -Ddefault_library=$STATIC -Db_lto=true -Dloaders=all -Dengines=sw,gl"
  if [[ "$platform" == "android" ]] || [[ "$platform" == "ohos" ]]; then
    BUILD_OPTIONS+=" -Dextra=opengl_es,lottie_exp,openmp"
  elif [[ "$platform" == "ios" ]]; then
    BUILD_OPTIONS+=" -Dextra=opengl_es,lottie_exp"
  else
    BUILD_OPTIONS+=" -Dextra=lottie_exp"
  fi
  if [[ "$STATIC" == "static" ]]; then
    BUILD_OPTIONS+=" -Dstatic=true"
  fi
  echo "BUILD_OPTIONS: $BUILD_OPTIONS"

  local build_dir="$LOCAL_DIR/build/binary/$platform/$arch"
  [[ -d "$build_dir" ]] && rm -rf "$build_dir"
  mkdir -p "$build_dir"

  local install_dir="$LOCAL_DIR/build/install/$platform/$arch"
  [[ -d "$install_dir" ]] && rm -rf "$install_dir"
  mkdir -p "$install_dir"

  local cross_params=""
  [[ -z "$cross_file" ]] || cross_params="--cross-file=$cross_file"
  meson setup $BUILD_OPTIONS $extras --prefix="$install_dir" $cross_params $build_dir
  ninja -C $build_dir install

  [[ $? -eq 0 ]] && echo ">>>> build [$platform-$arch] finish！install dir: $install_dir"
}

## Build for Android
## default use env: ANDROID_HOME / ANDROID_SDK_ROOT
## default ndk version: 27.3.13750724
## default api level: 24
build_for_android() {
  local ndk_version="27.3.13750724"
  local sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT}}
  local ndk=${sdk}/ndk/$ndk_version
  local api=24
  if [[ -z "$sdk" ]] || [[ ! -d "$sdk" ]]; then
    myexit 1 "env variable ANDROID_HOME or ANDROID_SDK_ROOT is not set or not a directory"
  fi
  if [[ -z "$ndk" ]] || [[ ! -d "$ndk" ]]; then
    myexit 1 "NDK version $ndk_version is not set or not a directory"
  fi
  # host_tag
  local host_tag=$(ls $ndk/toolchains/llvm/prebuilt/ | head -n 1)

  local cross_file="/tmp/.thorvg_android_cross_aarch64.txt"
  sed -e "s|NDK|$ndk|g" -e "s|HOST_TAG|$host_tag|g" -e "s|API|$api|g" $LOCAL_DIR/cross/android_aarch64.txt > $cross_file
  meson_build android arm64-v8a "$cross_file" $@

  cross_file="/tmp/.thorvg_android_cross_armv7a.txt"
  sed -e "s|NDK|$ndk|g" -e "s|HOST_TAG|$host_tag|g" -e "s|API|$api|g" $LOCAL_DIR/cross/android_armv7a.txt > $cross_file
  meson_build android armeabi-v7a "$cross_file" $@
}

## Build for OpenHarmony
## default use env OHOS_NDK
build_for_ohos() {
  local ndk=${OHOS_NDK:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native}
  if [[ -z "$ndk" ]] || [[ ! -d "$ndk" ]]; then
    myexit 1 "env variable OHOS_NDK is not set and $ndk not exists!"
  fi

  local cross_file="/tmp/.thorvg_ohos_cross.txt"
  sed -e "s|OHOS_NDK|$ndk|g" $LOCAL_DIR/cross/ohos_aarch64.txt > $cross_file

  export PATH=$ndk/build-tools/cmake/bin/:$PATH
  meson_build harmony arm64-v8a "$cross_file" $@ "-Dthreads=false"
}

build_for_ios() {
  meson_build ios arm64-v8a "$LOCAL_DIR/cross/ios_arm64.txt" $@
}

build_for_local() {
  # check system name: macOS, linux, windows
  local sysname=$(uname)
  echo "sysname: $sysname"
  case "$sysname" in
  Darwin*)
  meson_build macos arm64 "" '-Dcpp_args="-mmacosx-version-min=11.0" -Dc_args="-mmacosx-version-min=11.0" -Dcpp_link_args="-mmacosx-version-min=11.0"'
    ;;
  Linux*)
  meson_build linux x64 "" $@
    ;;
  MINGW*)
  meson_build windows x64 "" $@
    ;;
  *)
    myexit 1 "local build only support macOS, linux, windows"
    ;;
  esac
}

build() {
  case $PLATFORM in
    android)
      build_for_android $@ ;;
    ohos)
      build_for_ohos $@;;
    ios)
      build_for_ios $@;;
    local)
      build_for_local $@;;
    all)
      build_for_android $@
      build_for_ohos $@
      build_for_ios $@
      build_for_local $@
      ;;
    *)
      myexit 1 "$USAGE" ;;
  esac
}

build $@