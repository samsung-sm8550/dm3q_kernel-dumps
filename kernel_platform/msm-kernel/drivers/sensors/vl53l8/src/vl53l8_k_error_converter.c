/*******************************************************************************
* Copyright (c) 2022, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L8 Kernel Driver and is dual licensed,
* either 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
* terms at www.st.com/sla0081
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, VL53L8 Kernel Driver may be distributed under the terms of
* 'BSD 3-clause "New" or "Revised" License', in which case the following
* provisions apply instead of the ones mentioned above :
*
********************************************************************************
*
* License terms: BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*
*******************************************************************************/

#include <linux/errno.h>
#include "vl53l8_k_error_converter.h"
#include "vl53l8_k_error_codes.h"
#include "vl53l5_platform_log.h"

#ifdef STM_VL53L5_SUPPORT_SEC_CODE
#ifdef CONFIG_SENSORS_LAF_FAILURE_DEBUG
#define MAX_ERR_CNT 255
int vl53l8_hash_func(struct vl53l8_k_module_t *p_module, int err)
{
	unsigned int hash = 5381;
	int cnt = 0;

	vl53l8_k_log_info("debug err %d\n", err);
	hash = ((((hash << 4) + hash) + err) % (MAX_TABLE-1)) + 1;

	while (cnt++ < MAX_TABLE-1) {
		if (hash >= MAX_TABLE)
			return -1;
		if (p_module->errdata[hash].last_error_code == 0
			|| err == p_module->errdata[hash].last_error_code)
			return hash;
		if (++hash == MAX_TABLE)
			hash = 1;
	}
	return -1;
}

void vl53l8_error_counter_by_hash(struct vl53l8_k_module_t *p_module, int err)
{
	int key = vl53l8_hash_func(p_module, err);

	if (key > 0 && key < MAX_TABLE) {
		p_module->errdata[key].last_error_code = err;
		if (p_module->errdata[key].last_error_cnt < MAX_ERR_CNT)
			p_module->errdata[key].last_error_cnt++;
		return;
	}
	vl53l8_k_log_error("key err %d", key);
}

void vl53l8_last_error_counter(struct vl53l8_k_module_t *p_module, int err)
{
	if (err == VL53L8_DELAYED_LOAD_FIRMWARE)
		return;

	if (p_module->ldo_status != 0)
		err = VL53L8_LDO_ERROR - p_module->ldo_status;

	if (err >= 0)
		return;

	vl53l8_error_counter_by_hash(p_module, err);
}
#endif
#endif

void vl53l8_k_store_error(struct vl53l8_k_module_t *p_module,
			  int32_t vl53l8_k_error)
{

	if (p_module->last_driver_error != vl53l8_k_error)
		p_module->last_driver_error = vl53l8_k_error;
#ifdef STM_VL53L5_SUPPORT_SEC_CODE
	if (p_module->last_driver_error != VL53L8_PROBE_FAILED)
		p_module->last_driver_error = vl53l8_k_error;
#endif
}

int32_t vl53l8_k_convert_error_to_linux_error(int32_t vl53l8_k_error)
{
	int32_t linux_error = VL53L5_ERROR_NONE;

	switch (vl53l8_k_error) {
	case VL53L5_ERROR_NONE:
		break;
	case VL53L8_K_ERROR_DRIVER_NOT_INITIALISED:
	case VL53L8_K_ERROR_DEVICE_NOT_PRESENT:
	case VL53L8_K_ERROR_DEVICE_NOT_POWERED:
	case VL53L8_K_ERROR_DEVICE_NOT_INITIALISED:
	case VL53L8_K_ERROR_DEVICE_NOT_RANGING:
		linux_error = -EPERM;
		break;
	case VL53L8_K_ERROR_I2C_CHECK_FAILED:
	case VL53L8_K_ERROR_GPIO_IS_DISABLED:
	case VL53L8_K_ERROR_GPIO_REQUEST_FAILED:
	case VL53L8_K_ERROR_GPIO_DIRECTION_SET_FAILED:
	case VL53L8_K_ERROR_ATTEMPT_TO_SET_DISABLED_GPIO:
		linux_error = -EIO;
		break;
	case VL53L8_K_ERROR_RANGE_DATA_NOT_READY:
		linux_error = -EAGAIN;
		break;
	case VL53L8_K_ERROR_FAILED_TO_ALLOCATE_WORKER_WAIT_LIST:
	case VL53L8_K_ERROR_FAILED_TO_ALLOCATE_PROFILE:
	case VL53L8_K_ERROR_FAILED_TO_ALLOCATE_FIRMWARE:
	case VL53L8_K_ERROR_FAILED_TO_ALLOCATE_VERSION:
	case VL53L8_K_ERROR_FAILED_TO_ALLOCATE_MODULE_INFO:
	case VL53L8_K_ERROR_FAILED_TO_ALLOCATE_RAW_DATA:
	case VL53L8_K_ERROR_FAILED_TO_ALLOCATE_PARAMETER_DATA:
		linux_error = -ENOMEM;
		break;
	case VL53L8_K_ERROR_FAILED_TO_COPY_FIRMWARE:
	case VL53L8_K_ERROR_FAILED_TO_COPY_VERSION:
	case VL53L8_K_ERROR_FAILED_TO_COPY_MODULE_INFO:
	case VL53L8_K_ERROR_FAILED_TO_COPY_RAW_DATA:
	case VL53L8_K_ERROR_FAILED_TO_COPY_RANGE_DATA:
	case VL53L8_K_ERROR_FAILED_TO_COPY_ERROR_INFO:
	case VL53L8_K_ERROR_FAILED_TO_COPY_PARAMETER_DATA:
	case VL53L8_K_ERROR_FAILED_TO_COPY_POWER_MODE:
		linux_error = -EFAULT;
		break;
	case VL53L8_K_ERROR_DEVICE_IS_RANGING:
	case VL53L8_K_ERROR_DEVICE_IS_BUSY:
		linux_error = -EBUSY;
		break;
	case VL53L8_K_ERROR_DEVICE_STATE_INVALID:
	case VL53L8_K_ERROR_SPI_BUSNUM_INVALID:
	case VL53L8_K_ERROR_SPI_NEW_DEVICE_FAILED:
	case VL53L8_K_ERROR_SPI_SETUP_FAILED:
	case VL53L8_K_ERROR_SPI_DRIVER_REGISTER_FAILED:
	case VL53L8_K_ERROR_I2C_ADAPTER_INVALID:
	case VL53L8_K_ERROR_I2C_NEW_DEVICE_FAILED:
	case VL53L8_K_ERROR_I2C_ADD_DRIVER_FAILED:
	case VL53L8_K_ERROR_DEVICE_INTERRUPT_NOT_OWNED:
		linux_error = -EINVAL;
		break;
	case VL53L8_K_ERROR_SLEEP_FOR_DATA_INTERRUPTED:
		linux_error = -ERESTARTSYS;
		break;
	case VL53L8_K_ERROR_WORKER_THREAD_TIMEOUT:
	case VL53L8_K_ERROR_RANGE_POLLING_TIMEOUT:
	case VL53L8_K_ERROR_STATE_POLLING_TIMEOUT:
		linux_error = -ETIMEDOUT;
		break;
	default:
		linux_error = -EPERM;
		break;
	}

	return linux_error;
}
