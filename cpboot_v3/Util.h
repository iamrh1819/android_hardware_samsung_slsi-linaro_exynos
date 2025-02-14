/*
 * Copyright (C) 2020 Samsung Electronics
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "Type.h"
#include "Log.h"

class Util {
private:
	Util() {}

public:
	static char *get_cmdline_str(char *buf, unsigned size, const char *find);
	static void switch_user(void);
#if defined(CONFIG_THROUGHPUT_MONITOR)
	static void *traffic_monitor(void* arg);
#endif
	static int create_empty_nv(char *path, size_t size);
	static bool getNvFd(enum cp_boot_mode mode, char *nv_path, int nv_size, int *nv_fd);
	static void toUpperCase(char* src, char *dst);
	static void byteToHex(char *dst, size_t dstSize, char *src, size_t srcSize);
	static void cbd_do_test(char *cmd);
};

