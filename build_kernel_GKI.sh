#!/bin/bash

#1. target config
BUILD_TARGET=dm3q_eur_openx
export MODEL=$(echo $BUILD_TARGET | cut -d'_' -f1)
export PROJECT_NAME=${MODEL}
export REGION=$(echo $BUILD_TARGET | cut -d'_' -f2)
export CARRIER=$(echo $BUILD_TARGET | cut -d'_' -f3)
export TARGET_BUILD_VARIANT=user

#2. sm8550 common config
CHIPSET_NAME=kalama

export ANDROID_BUILD_TOP=$(pwd)
export TARGET_PRODUCT=gki
export TARGET_BOARD_PLATFORM=gki

export ANDROID_PRODUCT_OUT=${ANDROID_BUILD_TOP}/out/target/product/${MODEL}
export OUT_DIR=${ANDROID_BUILD_TOP}/out/msm-${CHIPSET_NAME}-${CHIPSET_NAME}-${TARGET_PRODUCT}

# for Lcd(techpack) driver build
export KBUILD_EXTRA_SYMBOLS="${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/mmrm-driver/Module.symvers \
		${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/mm-drivers/hw_fence/Module.symvers \
		${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/mm-drivers/sync_fence/Module.symvers \
		${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/mm-drivers/msm_ext_display/Module.symvers \
		${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/securemsm-kernel/Module.symvers \
"

# for Audio(techpack) driver build
export MODNAME=audio_dlkm

export KBUILD_EXT_MODULES="${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/mm-drivers/msm_ext_display \
  ${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/mm-drivers/sync_fence \
  ${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/mm-drivers/hw_fence \
  ${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/mmrm-driver \
  ${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/securemsm-kernel \
  ${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/display-drivers/msm \
  ${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/audio-kernel \
  ${ANDROID_BUILD_TOP}/out/vendor/qcom/opensource/camera-kernel \
  "
  
#3. build kernel
cd kernel_platform
RECOMPILE_KERNEL=1 ./build/android/prepare_vendor.sh ${CHIPSET_NAME} ${TARGET_PRODUCT}
