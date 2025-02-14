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

#include <sys/ioctl.h>
#include <sys/stat.h>

#include "Log.h"

#define SRINFO_PATH		CPDUMP_PATH
#define SRINFO_FILE		"sr_info"
#define SRINFO_MAX_SIZE		0x10000 /* 64KB */
#define SRINFO_READ_SIZE	0x1000 /* 4KB*/

struct shmem_srinfo {
	unsigned size;
	char buf[0];
};

class Srinfo {
protected:
	Srinfo();
	virtual ~Srinfo() {}

public:
	virtual void store_srinfo(int dev_fd) = 0;
	virtual void restore_srinfo(int dev_fd);

protected:
	virtual shmem_srinfo *alloc_srinfo(unsigned *len, int boot_fd) = 0;
	virtual unsigned write_srinfo_file(int fd, char *buf, unsigned size) = 0;
	virtual int open_srinfo_file(const char *suffix);
};

