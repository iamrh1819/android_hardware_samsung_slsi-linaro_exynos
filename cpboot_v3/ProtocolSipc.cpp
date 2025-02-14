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

#include "ProtocolSipc.h"
#include "Container.h"

#define STD_UDL_AP2CP			0x9000
#define STD_UDL_CP2AP			0xA000

#define STD_UDL_STAGE_SHIFT		8
#define STD_UDL_STAGE_START		0x0
#define STD_UDL_SEND			0x1
#define STD_UDL_CRC				0xC
#define STD_UDL_STAGE_DONE		0xD
#define STD_UDL_STAGE_FAIL		0xF

ProtocolSipc::ProtocolSipc() {}

int ProtocolSipc::get_factory_prop()
{
#define SIM_CONF_PATH "/efs/factory.prop"

	int fd, ret;
	char full_string[MAX_PROP_STRING_LEN] = {0};
	char *sim_value;
	const char *token = "=";

	fd = open(SIM_CONF_PATH, O_RDONLY);
	if (fd < 0) {
		cbd_info("%s open fail(not support)\n", SIM_CONF_PATH);
		return -EINVAL;
	}

	ret = read(fd, full_string, MAX_PROP_STRING_LEN - 1);
	if (ret < 0) {
		cbd_err("%s read fail\n", SIM_CONF_PATH);
		goto exit;
	}

	sim_value = strstr(full_string, token);
	if (sim_value == NULL) {
		cbd_info("can't find token!\n");
		ret = -EINVAL;
		goto exit;
	} else {
		ret = (++sim_value)[0] - '0';
		cbd_info("sim_count: %d\n", ret);
	}

exit:
	close(fd);
	return ret;
}

#ifdef CONFIG_TYPE_MODAP
int ProtocolSipc::std_boot_finish_handshake()
{
	int ret;

	/* BOOT_DONE command */
	ret = std_udl_stage_done(OPER_BOOT, STD_UDL_STAGE_START);
	if (ret < 0) {
		cbd_info("ERR! std_udl_stage_done fail\n");
		goto exit;
	}

	/* FIN command */
	ret = std_udl_stage_start(OPER_BOOT, STD_UDL_FIN_STAGE);
	if (ret < 0) {
		cbd_info("ERR! std_udl_stage_done fail\n");
		goto exit;
	}

	return 0;
exit:
	return ret;
}
#endif

int ProtocolSipc::std_dump_receive_cp_dump()
{
	int ret;
	struct std_uload_info *ul_info;
	u32 saved = 0;

	/* Send DUMP START request and wait for the response from CP */
	ret = std_udl_stage_start(OPER_DUMP, STD_UDL_DUMP_STAGE);
	if (ret < 0) {
		cbd_dump_info("ERR! std_udl_stage_start fail\n");
		goto exit;
	}

	/* Receive DUMP information and the CP CRASH reason */
	ret = std_ul_recv_info();
	if (ret < 0) {
		cbd_dump_info("ERR! std_ul_recv_info fail\n");
		goto exit;
	}

	/* Receive DUMP data at every DUMP step */
	ul_info = &Container::getStdDump()->info;
	for (u32 step = 1; step <= ul_info->num_steps; step++) {
		ret = std_ul_recv_data(step);
		if (ret < 0) {
			cbd_dump_info("ERR! std_ul_recv_data fail\n");
			goto exit;
		}
		saved += ret;
	}

	/* Verify the size of total DUMP data */
	if (saved != ul_info->dump_size) {
		cbd_dump_info("ERR! saved %d != dump_size %d\n", saved, ul_info->dump_size);
		ret = -EFAULT;
		goto exit;
	}

	if (fsync(Container::getStdDump()->fds[FD_DUMP])) {
		cbd_dump_err("ERR! fsync(dump_fd) fail\n");
		ret = errno;
		goto exit;
	}
	cbd_dump_info("DUMP DATA saved\n");

	/* Send DUMP DONE request and wait for the response from CP */
	ret = std_udl_stage_done(OPER_DUMP, STD_UDL_DUMP_STAGE);
	if (ret < 0) {
		cbd_dump_info("ERR! DUMP std_udl_stage_done fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}

int ProtocolSipc::std_reboot_system(const char *str)
{
	int ret;
	char reboot[PROPERTY_VALUE_MAX] = "reboot,";

	strcat(reboot, str);
	ret = property_set(PROP_SYS_POWERCTL, reboot);
	if (ret < 0)
		cbd_info("ERR! failed to setprop [sys.powerctl](%s)\n",
				reboot);
	return ret;
}

int ProtocolSipc::std_udl_stage_start(enum operation oper, u32 stage)
{
	int ret;
	u32 req = 0xFFFF;
	u32 exp = 0xFFFF;

	req = STD_UDL_AP2CP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_STAGE_START;
	exp = STD_UDL_CP2AP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_STAGE_START;

	/* Send a request to CP, then receive and check a response from CP */
	ret = std_udl_req_resp(oper, req, exp);
	if (ret < 0) {
		cbd_info("ERR! [stage %d] START fail (req:0x%X exp:0x%X)\n",
			stage, req, exp);
	}

	return ret;
}

int ProtocolSipc::std_udl_stage_done(enum operation oper, u32 stage)
{
	int ret;
	u32 req = 0xFFFF;
	u32 exp = 0xFFFF;

	req = STD_UDL_AP2CP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_STAGE_DONE;
	exp = STD_UDL_CP2AP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_STAGE_DONE;

	/* Send a request to CP, then receive and check a response from CP */
	ret = std_udl_req_resp(oper, req, exp);
	if (ret < 0) {
		cbd_info("ERR! [stage %d] DONE fail (req:0x%X exp:0x%X)\n",
			stage, req, exp);
	}

	return ret;
}

int ProtocolSipc::std_ul_recv_info()
{
	int ret;
	char *buff;
	struct std_uload_info *ul_info = &Container::getStdDump()->info;

	/* Receive DUMP information */
	ret = std_ul_rx_frame(ul_info, sizeof(struct std_uload_info));
	if (ret < 0) {
		cbd_dump_info("ERR! INFO std_ul_rx_frame fail\n");
		goto exit;
	}

	/* Print DUMP information */
	cbd_info("dump_size = %d\n", ul_info->dump_size);
	cbd_info("num_steps = %d\n", ul_info->num_steps);
	cbd_info("reason_len = %d\n", ul_info->reason_len);

	/* Receive CP CRASH reason  */
	buff = Container::getStdDump()->reason + strlen(Container::getStdDump()->reason);
	ret = std_ul_rx_frame(buff, ul_info->reason_len);
	if (ret < 0) {
		cbd_dump_info("ERR! REASON std_ul_rx_frame fail\n");
		goto exit;
	}

	make_cp_crash_reason(Container::getStdDump()->reason);

	/* Store the CP CRASH reason */
	ret = write(Container::getStdDump()->fds[FD_INFO], Container::getCbdArgs()->reason, strlen(Container::getCbdArgs()->reason));
	if (ret < 0) {
		cbd_dump_err("ERR! REASON write fail\n");
		goto exit;
	}

	ret = fsync(Container::getStdDump()->fds[FD_INFO]);
	if (ret) {
		cbd_dump_err("ERR! fsync(info_fd) fail (%d)\n", ret);
		goto exit;
	}

	cbd_dump_info("DUMP INFO saved\n");
	return 0;

exit:
	return ret;
}

int ProtocolSipc::std_ul_recv_data(u32 step)
{
	int ret;
	int saved;
	struct std_udl_frame frm;
	u32 req = STD_UDL_AP2CP | (STD_UDL_DUMP_STAGE << STD_UDL_STAGE_SHIFT) | step;
	u32 exp = STD_UDL_CP2AP | (STD_UDL_DUMP_STAGE << STD_UDL_STAGE_SHIFT) | step;
	u32 seqn;

	memset(&frm, 0, sizeof(struct std_udl_frame));

	/*
	** Send "START of each step" command to CP
	*/
	ret = std_udl_req_resp(OPER_DUMP, req, 0);
	if (ret < 0) {
		cbd_dump_info("ERR! [step %d] start fail (ret %d)\n", step, ret);
		goto exit;
	}

	/*
	** Receive DUMP frames of each step from CP and store them
	*/
	seqn = 1;
	saved = 0;
	do {
		/* Receive a DUMP frame from CP */
		ret = std_ul_rx_frame(&frm, sizeof(struct std_udl_frame));
		if (ret < 0) {
			cbd_dump_info("ERR! [step %d] std_ul_rx_frame fail (ret %d)\n", step, ret);
			goto exit;
		}

		/* Verify the command in the frame */
		if (frm.cmd != exp) {
			cbd_dump_info("ERR! [step %d] cmd 0x%X != exp 0x%X\n", step, frm.cmd, exp);
			ret = -EFAULT;
			goto exit;
		}

		/* Verify the sequence number in the frame */
		if (frm.curr_frame != seqn) {
			cbd_dump_info("ERR! [step %d] curr_frame %d != seqn %d\n",
				step, frm.curr_frame, seqn);
			goto exit;
		}
		seqn++;

		/* Record the information of each step at the start of the step */
		if (frm.curr_frame == 1) {
			cbd_dump_info("[step %d] num_frames = %d\n", step, frm.num_frames);
			cbd_dump_info("[step %d] command = 0x%X\n", step, frm.cmd);
		}

		/* Store the DUMP data in the frame */
		ret = write(Container::getStdDump()->fds[FD_DUMP], frm.data, frm.len);
		if (ret < 0) {
			cbd_dump_err("ERR! [step %d] seq# %d write fail\n", step, frm.curr_frame);
			goto exit;
		}

		/* Update "saved" variable */
		saved += frm.len;
	} while (frm.curr_frame < frm.num_frames);

	cbd_dump_info("[step %d] saved = %d\n", step, saved);
	return saved;

exit:
	return ret;
}

#ifdef CONFIG_TYPE_EXT
int ProtocolSipc::std_dl_send_bin(u32 stage, int b_fd, u8 *b_buffer, u32 size)
{
	int ret;
	u32 rest = size;
	u32 req = STD_UDL_AP2CP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_SEND;
	u32 exp = STD_UDL_CP2AP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_SEND;
	struct std_udl_frame frm;

	memset(&frm, 0, sizeof(struct std_udl_frame));

	/* Set DLOAD command */
	frm.cmd = req;

	/* Calculate the number of frames to be trsnamitted to CP */
	frm.num_frames = (size / STD_UDL_MSS);
	if (size > (STD_UDL_MSS * frm.num_frames))
		frm.num_frames++;

	/* Print DLOAD information at each stage */
	cbd_info("stage = %d\n", stage);
	cbd_info("size = %d (0x%X)\n", size, size);
	cbd_info("mtu = %d\n", STD_UDL_MTU);
	cbd_info("mss = %d\n", STD_UDL_MSS);
	cbd_info("frames = %d\n", frm.num_frames);

	/* Read and send the CP binary */
	while (rest > 0) {
		frm.curr_frame++;
		frm.len = (rest < STD_UDL_MSS) ? rest : STD_UDL_MSS;

		ret = std_dl_tx_frame(b_fd, b_buffer, (void *)&frm);
		if (ret < 0) {
			cbd_info("ERR! std_dl_tx_frame fail\n");
			goto exit;
		}

		rest -= frm.len;
		b_buffer += frm.len;
	}

	/* Receive and check a response from CP */
	ret = std_udl_req_resp(OPER_BOOT, 0, exp);
	if (ret < 0) {
		cbd_info("ERR! std_udl_req_resp fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}

int ProtocolSipc::std_dl_tx_frame(int b_fd, u8 *b_buffer, void *frame)
{
	struct std_udl_frame *frm = (struct std_udl_frame *)frame;
	int ret = 0;
	u32 frm_len = STD_UDL_HDR_LEN + frm->len;
	u32 rcvd;
	u32 sent;

	memset(frm->data, 0, STD_UDL_MSS);

	/* Read a segment of a CP binary */
	if (b_buffer) {
		memcpy(frm->data, b_buffer, frm->len);
	} else {
		rcvd = ret = read(b_fd, frm->data, frm->len);
		if (ret < 0) {
			cbd_err("ERR! read fail\n");
			goto exit;
		}

		if (rcvd != frm->len) {
			cbd_info("ERR! rcvd %d != frm->len %d\n", rcvd, frm->len);
			ret = -EFAULT;
			goto exit;
		}
	}

	/* Send the CP binary segment */
	sent = ret = write(Container::getStdBoot()->fds[FD_DEV], frm, frm_len);
	if (ret < 0) {
		cbd_err("ERR! write fail\n");
		goto exit;
	}

	if (sent != frm_len) {
		cbd_info("ERR! sent %d != frm_len %d\n", sent, frm_len);
		ret = -EFAULT;
		goto exit;
	}

	return 0;

exit:
	return ret;
}

int ProtocolSipc::std_dl_send_crc(u32 stage, u32 crc)
{
	int ret;
	u32 exp = STD_UDL_CP2AP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_CRC;
	struct std_udl_crc_frame crc_frm;

	crc_frm.cmd = STD_UDL_AP2CP | (stage << STD_UDL_STAGE_SHIFT) | STD_UDL_CRC;
	crc_frm.crc = crc;
	cbd_info("cmd = 0x%x, crc = 0x%x\n", crc_frm.cmd, crc_frm.crc);

	/* Send a CRC data */
	ret = write(Container::getStdBoot()->fds[FD_DEV], &crc_frm, sizeof(struct std_udl_crc_frame));
	if (ret < 0) {
		cbd_err("ERR! write fail\n");
		goto exit;
	}

	/* Receive and check a response from CP */
	ret = std_udl_req_resp(OPER_BOOT, 0, exp);
	if (ret < 0) {
		cbd_info("ERR! std_udl_req_resp fail\n");
		goto exit;
	}

	return 0;

exit:
	return ret;
}
#endif

int ProtocolSipc::std_udl_poll(enum operation oper, short events, long timeout)
{
	int ret;
	struct pollfd pfd;

	pfd.fd = (oper == OPER_BOOT ? Container::getStdBoot()->fds[FD_DEV] : Container::getStdDump()->fds[FD_DEV]);
	pfd.events = events;
	while (1) {
		pfd.revents = 0;

		/* Wait "events" up to "timeout" msec */
		ret = poll(&pfd, 1, timeout);
		if (pfd.revents & events)
			break;

		if (ret > 0)
			cbd_info("ERR! poll fail (events 0x%X != revents 0x%X)\n", events, pfd.revents);
		else if (ret == 0)
			cbd_info("ERR! poll fail (events 0x%X, TIMEOUT)\n", events);
		else
			cbd_err("ERR! poll fail (events 0x%X)\n", events);

		ret = -EIO;
		goto exit;
	}

	return 0;

exit:
	return ret;
}

unsigned char ProtocolSipc::check_csc_sales_code()
{
	char secure_reboot[PROPERTY_VALUE_MAX];

	property_get(PROP_SALES_CODE, secure_reboot, "");

	cbd_info("sales_code:%s\n", secure_reboot);

	if (strcmp(secure_reboot, "TMB") == 0)
		return 1;
	else if (strcmp(secure_reboot, "VZW") == 0)
		return 1;

	return 0;
}

