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

#include "Log.h"
#include "BootDumpModap.h"
#include "BootDumpExt.h"
#include "Container.h"
#ifdef CONFIG_PROTOCOL_SIT
#include "SrinfoSit2.h"
#else
#include "SrinfoSipc.h"
#endif

struct std_cbd_args *Container::getCbdArgs()
{
	return &cbd_args;
}

BootDump *Container::getBootDump()
{
	return bootdump;
}

std_boot_args *Container::getStdBoot()
{
	if (bootdump)
		return bootdump->getStdBoot();

	return NULL;
}

std_dump_args *Container::getStdDump()
{
	if (bootdump)
		return bootdump->getStdDump();

	return NULL;
}

void Container::setProtocol(Protocol *protocol)
{
	Container::protocol = protocol;
}

Protocol *Container::getProtocol()
{
	return protocol;
}

Srinfo *Container::getSrinfo()
{
	return srinfo;
}

unsigned int Container::getBootStage()
{
	if (cbd_args.cpn.first_stage_toc_type == TOC_BOOT)
		return 0;

	return 1;
}

unsigned int Container::getTocStage()
{
	if (cbd_args.cpn.first_stage_toc_type == TOC_TOC)
		return 0;

	return 1;
}

Status Container::parseArgs(int argc, char **argv)
{
	int opt;
	char *cmd;

	while (1) {
		opt = getopt(argc, argv, "dt:s:b:m:n:o:P:B:D:T:");
		if (opt == -1)
			break;

		switch (opt) {
		case 'd':
			cbd_info("Daemon Mode\n");
			cbd_args.daemon = 1;
			break;

		case 't':
			cmd = optarg;

			if (bootdump)
				break;

#ifdef CONFIG_TYPE_MODAP
			if (!strncmp(cmd, "modap_sit", 9) || !strncmp(cmd, "ss310", 5))
				bootdump = new BootDumpModap();
#endif
#ifdef CONFIG_TYPE_EXT
			if (!strncmp(cmd, "s5100sit", 8) || !strncmp(cmd, "s5100", 5))
				bootdump = new BootDumpExt();
#endif

			if (!bootdump) {
				cbd_info("Unknown modem\n");
				return ERROR;
			}

			bootdump->setCbdArgs(cmd);
#ifndef CONFIG_PROTOCOL_SIT
			srinfo = new SrinfoSipc();
#endif
			break;

		case 's':
			cmd = optarg;
			cbd_info("SRINFO type: %c\n", cmd[0]);
			switch(cmd[0]) {
#ifdef CONFIG_PROTOCOL_SIT
			case '0':
				cbd_info("No srinfo");
				break;
			case '2':
				if (!srinfo)
					srinfo = new SrinfoSit2();
				break;
#else
			case '1':
				break;
#endif
			default:
				cbd_err("invalid srinfo type: %c\n", cmd[0]);
				return ERROR;
			}
			break;

		case 'b':
			cmd = optarg;
			switch (cmd[0]) {
#ifdef CONFIG_TYPE_MODAP
			case 'm':
				cbd_info("boot SHMEM link\n");
				cbd_args.lnk_boot = LINKDEV_SHMEM;
				break;
#endif
#ifdef CONFIG_TYPE_EXT
			case 's':
				cbd_info("boot spi link\n");
				cbd_args.lnk_boot = LINKDEV_SPI;
				break;
			case 'e':
				cbd_info("boot PCIE link\n");
				cbd_args.lnk_boot = LINKDEV_PCIE;
				break;
#endif
			default:
				cbd_info("ERR! invalid boot link %c\n", cmd[0]);
				return ERROR;
			}
			break;

		case 'm':
			cmd = optarg;
			switch (cmd[0]) {
#ifdef CONFIG_TYPE_MODAP
			case 'm':
				cbd_info("main SHMEM link\n");
				cbd_args.lnk_main = LINKDEV_SHMEM;
				break;
#endif
#ifdef CONFIG_TYPE_EXT
			case 'e':
				cbd_info("main PCIE link\n");
				cbd_args.lnk_main = LINKDEV_PCIE;
				break;
#endif
			default:
				cbd_info("ERR! invalid main link %c\n", cmd[0]);
				return ERROR;
			}
			break;

		case 'n': /* TODO */
			cmd = optarg;
			cbd_info("set nv partition : %s\n", cmd);

			sprintf(cbd_args.cpn.path_nv_data, "%s/nv_data.bin", cmd);
			cbd_info("nv data file path : %s\n", cbd_args.cpn.path_nv_data);
			cbd_info("nv norm file path : %s\n", cbd_args.cpn.path_nv_norm);
			cbd_info("nv prot file path : %s\n", cbd_args.cpn.path_nv_prot);
			break;

		case 'o':
			cmd = optarg;
			switch (cmd[0]) {
			case 'u':
				cbd_info("Upload Test\n");
				cbd_args.options |= BOPT_CPUPLOAD;
				break;
			case 'r':
				cbd_info("run with root\n");
				cbd_args.options |= BOPT_ROOT;
				break;
			case 'v':
				cbd_info("verify vss\n");
				cbd_args.options |= BOPT_VERIFY_VSS;
				break;
			case 's':
				cbd_info("reinit vss\n");
				cbd_args.options |= BOPT_REINIT_VSS;
				break;
			default:
				cbd_info("ERR! invalid option %c\n", cmd[0]);
				break;
			}
			break;

		case 'P':
			cmd = optarg;
			cbd_info("set partition number : %s\n", cmd);

			sprintf(cbd_args.cpn.path_bin, "/dev/block/%s", cmd);
			cbd_info("partition path : %s\n", cbd_args.cpn.path_bin);
			break;

		case 'B':
			cmd = optarg;
			cbd_info("set boot_node : %s\n", cmd);

			sprintf(cbd_args.cpn.node_boot, "/dev/%s", cmd);
			cbd_args.cpn.node_status = cbd_args.cpn.node_boot;
			cbd_info("boot_node : %s\n", cbd_args.cpn.node_boot);
			break;

		case 'D':
			cmd = optarg;
			cbd_info("set dump_node : %s\n", cmd);

			sprintf(cbd_args.cpn.node_dump, "/dev/%s", cmd);
			cbd_info("dump_node : %s\n", cbd_args.cpn.node_dump);
			break;

		default:
			cbd_info("WARNING! invalid option %c\n", opt);
			break;
		}
	}

	return OK;
}

