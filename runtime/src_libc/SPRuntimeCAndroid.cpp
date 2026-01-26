/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_stdlib.h>

#if SPRT_ANDROID
#include <android/api-level.h>

__SPRT_C_FUNC int android_get_application_target_sdk_version();
__SPRT_C_FUNC int __system_property_get(const char *__name, char *__value);
#endif

namespace sprt {

__SPRT_C_FUNC int __sprt_android_get_application_target_sdk_version() {
#if APRT_ANDROID
	return android_get_application_target_sdk_version();
#else
	return 0;
#endif
}

__SPRT_C_FUNC int __sprt_android_get_device_api_level() {
#if APRT_ANDROID
	char value[92] = {0};
	if (__system_property_get("ro.build.version.sdk", value) < 1) {
		return -1;
	}
	int api_level = __sprt_atoi(value);
	return (api_level > 0) ? api_level : -1;
#else
	return 0;
#endif
}

} // namespace sprt
