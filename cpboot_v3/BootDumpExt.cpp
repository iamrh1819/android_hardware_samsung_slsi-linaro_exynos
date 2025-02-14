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

#include "BootDumpExt.h"
#include "Container.h"

BootDumpExt::BootDumpExt()
{
	memset(&std_boot, 0, sizeof(struct std_boot_args));
	memset(&std_dump, 0, sizeof(struct std_dump_args));
}

int BootDumpExt::boot()
{
	int ret = 0;
	int spin = 50;
	char prop_buf[PROPERTY_VALUE_MAX] = {0, };

	cbd_info("CP boot device = %s\n", Container::getCbdArgs()->cpn.node_boot);
	cbd_info("CP binary file = %s\n", Container::getCbdArgs()->cpn.path_bin);
#ifdef CONFIG_PROTOCOL_SIT
	cbd_info("CP NV normal file = %s\n", Container::getCbdArgs()->cpn.path_nv_norm);
	cbd_info("CP NV prot file = %s\n", Container::getCbdArgs()->cpn.path_nv_prot);
#else
	cbd_info("CP NV data file = %s\n", Container::getCbdArgs()->cpn.path_nv_data);
#endif

	switch (Container::getCbdArgs()->lnk_boot) {
	case LINKDEV_SPI:
		cbd_info("BOOT link SPI\n");
		break;
	case LINKDEV_PCIE:
		cbd_info("BOOT link PCIE\n");
		break;
	default:
		cbd_info("ERR! BOOT link# %d not supported\n", Container::getCbdArgs()->lnk_boot);
		ret = -ENODEV;
		goto exit;
	}

	switch (Container::getCbdArgs()->lnk_main) {
	case LINKDEV_PCIE:
		cbd_info("MAIN link PCIE\n");
		break;
	default:
		cbd_info("ERR! MAIN link# %d not supported\n", Container::getCbdArgs()->lnk_main);
		ret = -ENODEV;
		goto exit;
	}

	/* Wait for completion of RILD's NV validity check */
	while (spin--) {
		property_get(VPROP_RFS_CHECKDONE, prop_buf, "0");
		if (prop_buf[0] == '1')
			break;
		usleep(100000);
	}
	cbd_info("NV validation %s\n", spin < 0 ? "TIMEOUT" : "done");

	/*
	** Start CP BOOT
	*/

	/* Prepare BOOT arguments which are specific to SHANNON */
	if (!prepare_boot_args(CP_BOOT_MODE_NORMAL)) {
		cbd_info("ERR! prepare_boot_args fail\n");
		ret = -EFAULT;
		goto exit;
	}

	ret = shannon_normal_boot();
	if (ret < 0) {
		cbd_info("ERR! shannon_normal_boot fail\n");
		goto exit;
	}

exit:
	std_boot_close_args();

	return ret;
}

int BootDumpExt::dump()
{
	int ret;
	char reason[MAX_PREFIX_LEN];

	cbd_info("Prepare CP crash dump\n");
	if (!std_dump_prepare_args()) {
		cbd_info("ERR! std_dump_prepare_args fail\n");
		ret = -EFAULT;
		goto exit;
	}

	if (!prepare_boot_args(CP_BOOT_MODE_DUMP)) {
		cbd_info("ERR! prepare_boot_args fail\n");
		ret = -EFAULT;
		goto exit;
	}

	cbd_info("Get log dump\n");
	get_log_dump("shmem", LOG_IDX_SHMEM);
	get_log_dump("databuf_dl", LOG_IDX_DATABUF_DL);
	get_log_dump("databuf_ul", LOG_IDX_DATABUF_UL);

	cbd_info("Prepare CP crash dump\n");
	ret = shannon_dump_boot();
	if (ret < 0) {
		cbd_dump_info("ERR! shannon_dump_boot fail\n");
		goto exit;
	}

	cbd_info("Receive CP crash dump\n");
	ret = std_dump_receive_cp_dump();
	if (ret < 0) {
		cbd_dump_info("ERR! std_dump_receive_cp_dump fail\n");
		goto exit;
	}

	cbd_info("Save kernel log\n");
	snprintf(reason, MAX_PREFIX_LEN, "dmesg");
	Log::save_logs(LOGB_DMESG, reason);

	/* Save srinfo from shmem to file */
	if (Container::getSrinfo())
		Container::getSrinfo()->store_srinfo(std_boot.fds[FD_DEV]);

	sync();

#ifdef CONFIG_PROTOCOL_SIT
	if (Log::debug_level == DBG_AUTO) {
		cbd_info("No trigger dump upload\n");
		ret = 0;
		goto exit;
	}
#endif

	cbd_info("Trigger dump upload\n");
	ret = std_dump_upload();
	if (ret < 0) {
		cbd_dump_info("ERR! std_dump_upload fail\n");
		goto exit;
	}

exit:
	if (ret < 0) {
		snprintf(reason, MAX_PREFIX_LEN, "cpcrash_%s_dump_fail", Container::getCbdArgs()->cpn.rat);
		Log::save_logs(LOGB_DMESG, reason);
	}

	std_dump_close_args();
	std_boot_close_args();

	return ret;
}

struct std_boot_args *BootDumpExt::getStdBoot()
{
	return &std_boot;
}

struct std_dump_args *BootDumpExt::getStdDump()
{
	return &std_dump;
}

void BootDumpExt::setCbdArgs(char *name)
{
	struct std_cbd_args *cbd_args = Container::getCbdArgs();

#ifdef CONFIG_PROTOCOL_SIT
	cbd_info("S5100 sit modem\n");
	cbd_args->type = SEC_S5100_SIT;
	sprintf(cbd_args->cpn.node_boot, "/dev/umts_boot0");
	sprintf(cbd_args->cpn.node_dump, "/dev/umts_boot0");
	sprintf(cbd_args->cpn.path_nv_data, "/mnt/vendor/efs/nv_data.bin");
	sprintf(cbd_args->cpn.path_nv_norm, "/mnt/vendor/efs/nv_normal.bin");
	sprintf(cbd_args->cpn.path_nv_prot, "/mnt/vendor/efs/nv_protected.bin");
	cbd_args->cpn.num_stages = 8; /* boot, toc, main, vss, apm, nv, nv_prot, fin */
#else
	cbd_info("S5100 modem\n");
	cbd_args->type = SEC_S5100;
	/* ToDo: "nr" is for 2CP device, can use "umts" on 1CP device */
	sprintf(cbd_args->cpn.node_boot, "/dev/nr_boot0");
	sprintf(cbd_args->cpn.node_dump, "/dev/nr_ramdump0");
	sprintf(cbd_args->cpn.path_nv_data, "/mnt/vendor/efs/nv_nr_data.bin");
	cbd_args->cpn.num_stages = 6; /* boot, toc, main, vss, nv, fin */
#endif

	cbd_args->lnk_boot = LINKDEV_SPI;
	cbd_args->lnk_main = LINKDEV_PCIE;
	sprintf(cbd_args->cpn.name, "%s", name);
	cbd_args->cpn.rat = "nr";
	cbd_args->cpn.node_status = cbd_args->cpn.node_boot;
	sprintf(cbd_args->cpn.path_bin, "/dev/block/by-name/modem");
	cbd_args->cpn.nv_size = (512 << 10);
	cbd_args->cpn.first_stage_toc_type = TOC_BOOT;
}

void BootDumpExt::build_std_dload_control()
{
	unsigned int idx, stage;
	struct std_dload_control *dl_ctrl = std_boot.dl_ctrl;
	struct std_toc_element *toc = std_boot.toc;
	struct modem_comp *cpn = &(Container::getCbdArgs()->cpn);

	stage = 1;
	for (idx = 0; idx < std_boot.num_stages; idx++) {
		unsigned int stage_tmp;

		/* End of TOC */
		if (std_boot.toc_idx[TOC_OFFSET] == idx)
			break;

		if (std_boot.toc_idx[cpn->first_stage_toc_type] == idx) {
			stage_tmp = stage;
			stage = 0;
		}

		dl_ctrl[stage].stage = stage;
		dl_ctrl[stage].start = 1;
		dl_ctrl[stage].download = 1;
		dl_ctrl[stage].validate = 0;
		dl_ctrl[stage].finish = 1;
		dl_ctrl[stage].b_fd = std_boot.fds[FD_BIN];
		dl_ctrl[stage].b_offset = toc[idx].b_offset;
		dl_ctrl[stage].b_size = toc[idx].size;
		dl_ctrl[stage].crc = toc[idx].crc;

		if (std_boot.toc_idx[TOC_BOOT] == idx) {
			dl_ctrl[stage].start = 0;
			dl_ctrl[stage].download = 0;
			dl_ctrl[stage].validate = 1;
		}

		if (std_boot.toc_idx[TOC_MAIN] == idx)
			dl_ctrl[stage].validate = 1;

		build_std_dload_control_nv_fd(stage, idx);

		cbd_info("stage=%u, name:%s b_off=0x%08x, m_offset=0x%08x b_size=0x%08x\n",
			dl_ctrl[stage].stage, toc[idx].name, dl_ctrl[stage].b_offset,
			dl_ctrl[stage].m_offset, dl_ctrl[stage].b_size);

		if (stage == 0)
			stage = stage_tmp;
		else
			stage++;
	}

	dl_ctrl[idx].stage = STD_UDL_FIN_STAGE;
	dl_ctrl[idx].start = 1;
}

bool BootDumpExt::prepare_boot_args(enum cp_boot_mode mode)
{
	u32 toc_count = 0;

	/* Prepare BOOT arguments */
	if (!std_boot_prepare_args()) {
		cbd_info("ERR! std_boot_prepare_args fail\n");
		goto error;
	}

	if (!std_boot_parse_toc_img(mode, &toc_count))
		goto error;

	std_boot.start_stage = Container::getTocStage();

	if (toc_count != 1)
		std_boot.num_stages = toc_count;

	cbd_info("num_stages: %d\n", std_boot.num_stages);

	/*
	** Set standard DLOAD control parameters with SHANNON BOOT arguments
	*/
	build_std_dload_control();

	return true;

error:
	std_boot_close_args();

	return false;
}

int BootDumpExt::shannon_normal_boot()
{
	int ret;

	cbd_info("Power on CP\n");
	ret = std_boot_power_on();
	if (ret < 0) {
		cbd_info("ERR! std_boot_power_on fail\n");
		goto exit;
	}

	cbd_info("Load CP bootloader\n");
	ret = std_boot_load_cp_bootloader();
	if (ret < 0) {
		cbd_info("ERR! std_boot_load_cp_image fail\n");
		goto exit;
	}

	cbd_info("Start CP bootloader\n");
	ret = std_boot_start_cp_bootloader(CP_BOOT_MODE_NORMAL);
	if (ret < 0) {
		cbd_info("ERR! std_boot_start_cp_bootloader fail\n");
		goto exit;
	}

	cbd_info("Load CP images\n");
	ret = std_boot_load_cp_images();
	if (ret < 0) {
		cbd_info("ERR! std_boot_dload fail\n");
		goto exit;
	}

	if (Container::getSrinfo())
		Container::getSrinfo()->restore_srinfo(std_boot.fds[FD_DEV]);

	cbd_info("Complete normal bootup\n");
	ret = std_boot_complete_normal_bootup();
	if (ret < 0) {
		cbd_info("ERR! std_boot_complete_normal_bootup fail\n");
		goto exit;
	}

exit:
	return ret;
}

int BootDumpExt::shannon_dump_boot()
{
	int ret;

	cbd_info("Power reset CP for CP_BOOT_MODE_DUMP\n");
	ret = std_boot_power_reset(CP_BOOT_MODE_DUMP);
	if (ret < 0) {
		cbd_info("ERR! std_boot_power_reset fail\n");
		goto exit;
	}

	cbd_info("Load CP bootloader\n");
	ret = std_boot_load_cp_bootloader();
	if (ret < 0) {
		cbd_info("ERR! std_boot_load_cp_bootloader fail\n");
		goto exit;
	}

	cbd_info("Start CP bootloader for crash dump\n");
	ret = std_boot_start_cp_bootloader(CP_BOOT_MODE_DUMP);
	if (ret < 0) {
		cbd_info("ERR! std_boot_start_cp_bootloader fail\n");
		goto exit;
	}

exit:
	return ret;
}

int BootDumpExt::std_boot_load_cp_bootloader()
{
	int ret = 0;
	struct std_dload_control *dlc;
	struct cp_image img;

	dlc = &std_boot.dl_ctrl[Container::getBootStage()];
	cbd_info("size = %d\n", dlc->b_size);

	memset(&img, 0, sizeof(img));

	/* Prepare an image buffer */
	img.binary = (u8*)malloc(dlc->b_size);
	if (!img.binary) {
		cbd_info("ERR! malloc(%d) fail\n", dlc->b_size);
		ret = -ENOMEM;
		goto exit;
	}
	img.size = dlc->b_size;
	img.len = img.size;

	/* Read BOOT loader */
	ret = lseek(dlc->b_fd, dlc->b_offset, SEEK_SET);
	if (ret < 0) {
		cbd_err("ERR! lseek fail\n");
		goto exit;
	}

	ret = read(dlc->b_fd, img.binary, img.size);
	if (ret < 0) {
		cbd_err("ERR! read fail\n");
		goto exit;
	}
	if ((u32)ret != img.size) {
		cbd_info("ERR! read %d != img.size %d\n", ret, img.size);
		ret = -EFAULT;
		goto exit;
	}

	/* Send BOOT loader */
	ret = ioctl(std_boot.fds[FD_DEV], IOCTL_LOAD_CP_IMAGE, &img);
	if (ret) {
		cbd_err("ERR! IOCTL_LOAD_CP_IMAGE fail (%d)\n", ret);
		goto exit;
	}

	cbd_info("xmit bootloader complete!\n");
exit:
	if (img.binary)
		free(img.binary);

	return ret;
}

int BootDumpExt::std_boot_load_cp_images()
{
	int max_stages = std_boot.num_stages;
	int ret = 0;

#ifdef CONFIG_PROTOCOL_SIT
	max_stages += 1;
#endif

	/* {BOOT(?), TOC, MAIN, NV, ... , FIN} stages */
	for (int i = std_boot.start_stage; i < max_stages; i++) {
		struct std_dload_control *dlc = &std_boot.dl_ctrl[i];
		u32 stage = dlc->stage;

		if (dlc->start) {
			ret = Container::getProtocol()->std_udl_stage_start(OPER_BOOT, stage);

			if (ret < 0) {
				cbd_info("ERR! [%d] std_udl_stage_start fail\n", stage);
				goto exit;
			}
		}

		if (dlc->download) {
			u8 *b_buffer;

			if (dlc->b_fd < 0) {
				cbd_info("fd is not valid. skip stage[%u]\n", stage);
				continue;
			}

			if (!dlc->b_size) {
				cbd_info("b_size is not valid. skip stage[%u]\n", stage);
				continue;
			}

			/* Set a file pointer for a CP binary (MAIN, NV, etc.) */
			ret = lseek(dlc->b_fd, dlc->b_offset, SEEK_SET);
			if (ret < 0) {
				cbd_err("ERR! [%d] lseek fail\n", stage);
				goto exit;
			}

			/* Try to cache a CP binary */
			b_buffer = (u8*)malloc(dlc->b_size);
			if (!b_buffer) {
				cbd_err("ERR! [%d] cache failed, use the legacy file access\n", stage);
			} else {
				ret = read(dlc->b_fd, b_buffer, dlc->b_size);
				if (ret != dlc->b_size) {
					cbd_err("ERR! [%d] read fail. size ret:%d exp:%u\n",
						stage, ret, dlc->b_size);
					ret = -EIO;
					goto free_buff;
				}
			}

			ret = Container::getProtocol()->std_dl_send_bin(stage, dlc->b_fd,
				b_buffer, dlc->b_size);

free_buff:
			if (b_buffer)
				free(b_buffer);

			if (ret < 0) {
				cbd_info("ERR! [%d] std_dl_send_bin fail\n", stage);
				goto exit;
			}
		}

		if (dlc->download && dlc->validate) {
			ret = Container::getProtocol()->std_dl_send_crc(stage, dlc->crc);
			if (ret < 0) {
				if (Container::getProtocol()->check_csc_sales_code()) {
					cbd_info("secure err: Invalid Main image\n");
					Container::getProtocol()->std_reboot_system("secure");
				} else {
					cbd_info("ERR! [%d] std_dl_send_crc fail\n", stage);
				}

				goto exit;
			}
		}

		if (dlc->finish) {
			ret = Container::getProtocol()->std_udl_stage_done(OPER_BOOT, stage);
			if (ret < 0) {
				cbd_info("ERR! [%d] std_udl_stage_done fail\n", stage);
				goto exit;
			}
		}
	}

	return 0;

exit:
	return ret;
}

