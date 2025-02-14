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

class BootDumpExt : public BootDump {
public:
	BootDumpExt();

private:
	int boot() override;
	int dump() override;

	void build_std_dload_control() override;
	int shannon_normal_boot();
	int shannon_dump_boot();

	bool prepare_boot_args(enum cp_boot_mode mode) override;
	int std_boot_load_cp_bootloader();
	int std_boot_load_cp_images();

	struct std_boot_args *getStdBoot() override;
	struct std_dump_args *getStdDump() override;
	void setCbdArgs(char *name) override;

private:
	struct std_boot_args std_boot;
	struct std_dump_args std_dump;
};

