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

#include <algorithm>
#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cutils/properties.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "Type.h"
#include "Log.h"

class BootDump {
protected:
	BootDump();
	virtual ~BootDump() {}

public:
	virtual int boot() = 0;
	virtual int dump() = 0;

	virtual void set_sim_configuration(void);
	virtual int get_modem_state(int fd);
	virtual int forceUpload(const char *reason);
	virtual struct std_boot_args *getStdBoot() = 0;
	virtual struct std_dump_args *getStdDump() = 0;
	virtual void setCbdArgs(char *name) = 0;

protected:
	virtual int get_log_dump(const char *name, enum cp_log_dump_index idx);

	virtual void build_std_dload_control() = 0;
	virtual void build_std_dload_control_nv_fd(unsigned int stage, unsigned int idx);
	virtual bool prepare_boot_args(enum cp_boot_mode mode) = 0;
	virtual bool std_boot_prepare_args();
	virtual bool std_boot_parse_toc_img(enum cp_boot_mode mode, u32 *toc_count);
	virtual void std_boot_close_args();
	virtual int std_boot_power_on();
	virtual int std_boot_power_reset(enum cp_boot_mode mode);
	virtual int std_boot_start_cp_bootloader(enum cp_boot_mode mode_idx);
	virtual int std_boot_complete_normal_bootup();

	virtual bool std_dump_prepare_args();
	virtual void std_dump_close_args();
	virtual int std_dump_receive_cp_dump();
	virtual int std_dump_upload();

private:
	virtual int upload(int fd2);

	virtual int std_dump_write_versions();
	virtual void std_boot_init_args();
	virtual void std_dump_init_args();
};

