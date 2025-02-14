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
#include "Srinfo.h"
#include "Type.h"

/* boot option*/
#define BOPT_ROOT		0x100
#define BOPT_VERIFY_VSS		0x200
#define BOPT_REINIT_VSS		0x400
#define BOPT_CPUPLOAD		0x10000

class Container {
private:
	Container() {}

public:
	static struct std_cbd_args *getCbdArgs();
	static BootDump *getBootDump();
	static std_boot_args *getStdBoot();
	static std_dump_args *getStdDump();
	static void setProtocol(Protocol *protocol);
	static Protocol *getProtocol();
	static Srinfo *getSrinfo();
	static unsigned int getBootStage();
	static unsigned int getTocStage();

	static Status parseArgs(int argc, char **argv);

private:
	static inline struct std_cbd_args cbd_args;
	static inline BootDump *bootdump = NULL;
	static inline Protocol *protocol = NULL;
	static inline Srinfo *srinfo = NULL;
};

