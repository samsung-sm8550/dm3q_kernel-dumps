/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017-2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/* usb hw param */
/* usb notify layer v3.7 */

#define MAX_HWPARAM_STR_LEN 1024
#define MAX_HWPARAM_STRING 10
#define HWPARAM_DATA_LIMIT 100000

enum usb_hw_param {
	USB_CCIC_WATER_INT_COUNT,
	USB_CCIC_DRY_INT_COUNT,
	USB_CCIC_I2C_ERROR_COUNT,
	USB_CCIC_OVC_COUNT,
	USB_CCIC_OTG_USE_COUNT,
	USB_CCIC_DP_USE_COUNT,
	USB_CCIC_VR_USE_COUNT,
	USB_HOST_SUPER_SPEED_COUNT,
	USB_HOST_HIGH_SPEED_COUNT,
	USB_HOST_FULL_SPEED_COUNT,
	USB_HOST_LOW_SPEED_COUNT,
	USB_CLIENT_SUPER_SPEED_COUNT,
	USB_CLIENT_HIGH_SPEED_COUNT,
	USB_HOST_CLASS_AUDIO_COUNT,
	USB_HOST_CLASS_AUDIO_SAMSUNG_COUNT,
	USB_HOST_REVERSE_BYPASS_COUNT,
	USB_HOST_CLASS_COMM_COUNT,
	USB_HOST_CLASS_HID_COUNT,
	USB_HOST_CLASS_PHYSICAL_COUNT,
	USB_HOST_CLASS_IMAGE_COUNT,
	USB_HOST_CLASS_PRINTER_COUNT,
	USB_HOST_CLASS_STORAGE_COUNT,
	USB_HOST_STORAGE_SUPER_COUNT,
	USB_HOST_STORAGE_HIGH_COUNT,
	USB_HOST_STORAGE_FULL_COUNT,
	USB_HOST_CLASS_HUB_COUNT,
	USB_HOST_CLASS_CDC_COUNT,
	USB_HOST_CLASS_CSCID_COUNT,
	USB_HOST_CLASS_CONTENT_COUNT,
	USB_HOST_CLASS_VIDEO_COUNT,
	USB_HOST_CLASS_WIRELESS_COUNT,
	USB_HOST_CLASS_MISC_COUNT,
	USB_HOST_CLASS_APP_COUNT,
	USB_HOST_CLASS_VENDOR_COUNT,
	USB_CCIC_DEX_USE_COUNT,
	USB_CCIC_WATER_TIME_DURATION,
	USB_CCIC_WATER_VBUS_COUNT,
	USB_CCIC_WATER_VBUS_TIME_DURATION,
	USB_CCIC_WATER_LPM_VBUS_COUNT,
	USB_CCIC_WATER_LPM_VBUS_TIME_DURATION,
	USB_CCIC_VBUS_CC_SHORT_COUNT,
	USB_CCIC_VBUS_SBU_SHORT_COUNT,
	USB_CCIC_GND_SBU_SHORT_COUNT,
	USB_MUIC_AFC_NACK_COUNT,
	USB_MUIC_AFC_ERROR_COUNT,
	USB_MUIC_DCD_TIMEOUT_COUNT,
	USB_HALL_FOLDING_COUNT,
	USB_CCIC_USB_KILLER_COUNT,
	USB_CCIC_FWUP_ERROR_COUNT,
	USB_MUIC_BC12_RETRY_SUCCESS_COUNT,
	USB_CCIC_PR_SWAP_COUNT,
	USB_CCIC_DR_SWAP_COUNT,
	USB_CLIENT_ANDROID_AUTO_RESET_POPUP_COUNT,
	USB_CCIC_UNMASK_VBUS_COUNT,
	USB_HOST_SB_COUNT,
	USB_HOST_OVER_AUDIO_DESCRIPTOR_COUNT,
	USB_CCIC_VERSION,
	USB_CCIC_HW_PARAM_MAX,
};
