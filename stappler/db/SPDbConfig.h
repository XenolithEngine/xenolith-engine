/**
Copyright (c) 2019-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef STAPPLER_DB_SPDBCONFIG_H_
#define STAPPLER_DB_SPDBCONFIG_H_

#include "SPTime.h"

// Server frameworks usually provides their own config for db module

#ifdef MODULE_STAPPLER_DB_CONFIG

#include MODULE_STAPPLER_DB_CONFIG

#else

// use common config

#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::db::config {

constexpr auto AUTH_MAX_TIME = 720_sec;
constexpr size_t AUTH_MAX_LOGIN_ATTEMPT = 4;

constexpr size_t INPUT_MAX_VAR_SIZE = 255;
constexpr size_t INPUT_MAX_REQUEST_SIZE = 0;
constexpr size_t INPUT_MAX_FILE_SIZE = 0;

constexpr auto INPUT_UPDATE_TIME = 1_sec;
constexpr auto INPUT_UPDATE_FREQUENCY = 0.1f;

constexpr size_t FIELD_FILE_DEFAULT_MAX_SIZE = 250_MiB;

constexpr size_t FIELD_EXTRA_DEFAULT_HINT_SIZE = 8_KiB;

constexpr auto FIELD_TEXT_DEFAULT_MIN_SIZE = 3;
constexpr auto FIELD_TEXT_DEFAULT_MAX_SIZE = 256;

constexpr StringView FIELD_PASSWORD_DEFAULT_SALT = StringView("StapplerUserPasswordKey");

constexpr auto STORAGE_DEFAULT_KEY_VALUE_INTERVAL = TimeInterval::seconds(60 * 60 * 24 * 365); // one year
constexpr auto STORAGE_DEFAULT_INTERNAL_INTERVAL = TimeInterval::seconds(60 * 60 * 24 * 30);

constexpr auto STORAGE_INTERFACE_KEY = StringView("SP.StorageInterface");
constexpr auto STORAGE_TRANSACTION_PREFIX = StringView("ST.Tr."); // limit for 6 chars to use with SOO opts (6 + 16 < 23)
constexpr auto STORAGE_TRANSACTION_STACK_KEY = StringView("ST.Transaction.Stack");

constexpr auto UPLOAD_TMP_FILE_PREFIX = StringView("sa.upload");
constexpr auto UPLOAD_TMP_IMAGE_PREFIX = StringView("sa.image");

constexpr auto BROADCAST_CHANNEL_NAME = StringView("serenity_broadcast");

constexpr uint16_t RESOURCE_RESOLVE_MAX_DEPTH = 4;

constexpr auto DEFAULT_PASSWORD_SALT = "SP_USER_PASSWORD_KEY";

}

#endif

#endif /* STAPPLER_DB_SPDBCONFIG_H_ */
