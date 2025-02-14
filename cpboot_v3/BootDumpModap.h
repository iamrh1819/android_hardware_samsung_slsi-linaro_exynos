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

#include "BootDump.h"
#include "Protocol.h"

class BootDumpModap : public BootDump {
public:
	BootDumpModap();

private:
	int boot() override;
	int dump() override;

	void build_std_dload_control() override;
	void boot_wake_lock(int lock);
	int load_cp_image_by_stage(u32 stage, enum cp_boot_mode mode);
	int load_cp_images(enum cp_boot_mode mode);

	bool prepare_boot_args(enum cp_boot_mode mode) override;
	int std_security_req(u32 mode, u32 p2, u32 p3);
	int std_check_cp_secure_fail(u32 value);

	struct std_boot_args *getStdBoot() override;
	struct std_dump_args *getStdDump() override;
	void setCbdArgs(char *name) override;

private:
	struct std_boot_args std_boot;
	struct std_dump_args std_dump;
};

