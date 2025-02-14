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

#include "ProtocolSit.h"
#include "Container.h"

ProtocolSit::ProtocolSit() {}

int ProtocolSit::get_factory_prop()
{
	char prop_buf[PROPERTY_VALUE_MAX] = {0, };

	property_get(PROP_RADIO_MULTISIM_CONFIG, prop_buf, "");

	if (strncmp(prop_buf, "dsds", 4) == 0) {
		cbd_info("dsds\n");
		return 2;
	}
	cbd_info("no dsds\n");

	return 1;
}

#ifdef CONFIG_TYPE_MODAP
int ProtocolSit::std_boot_finish_handshake()
{
	int ret = -1;

	u32 req = MSG(MSG_AP2CP, MSG_FINALIZE, MSG_NONE_TOC, MSG_PASS);
	u32 exp = MSG(MSG_CP2AP, MSG_FINALIZE, MSG_NONE_TOC, MSG_PASS);

	ret = std_udl_req_resp(OPER_BOOT, req, exp);
	if (ret < 0) {
		cbd_info("ERR! req:0x%X exp:0x%X\n", req, exp);
	}

	return ret;
}
#endif

int ProtocolSit::std_dump_receive_cp_dump()
{
	int ret = -1;

	struct sit_header sendHeader;
	struct sit_header recvHeader;
	struct sit_dump_info dump_info;

	struct sit_send_header ssendHeader;
	struct sit_recv_header rrecvHeader;
	int recv_length = 0, prev_length = 0;
	int readLen = 0, writeLen = 0, saveLen = 0;
	u8 *pRawdata = NULL;
	u32 req, exp;
	int sequence = 0;

	/* Send DUMP Ready */
	req = MSG(MSG_AP2CP, MSG_READY, MSG_NONE_TOC, MSG_DUMP);
	exp = MSG(MSG_CP2AP, MSG_READY, MSG_NONE_TOC, MSG_DUMP);

	ret = std_udl_req_resp(OPER_DUMP, req, exp);
	if (ret < 0) {
		cbd_info("ERR!  Send READY request and wait response (req:0x%X exp:0x%X)\n",
			req, exp);
		goto exit;
	}

	/* Send DUMP START request and wait for the dump information from CP */
	sendHeader.cmd = MSG(MSG_AP2CP, MSG_UPLOAD, MSG_NONE_TOC, MSG_START);
	sendHeader.length = 0;

	ret = write(Container::getStdDump()->fds[FD_DEV], &sendHeader, sizeof(struct sit_header));
	if (ret < 0) {
		cbd_err("ERR! Send DUMP START request (req:0x%X)\n", req);
		goto exit;
	}

	ret = std_udl_poll(OPER_DUMP, POLLIN, WAIT_POLL_TIME);
	if (ret < 0) {
		cbd_info("ERR! Wait for dump information\n");
		goto exit;
	}

	ret = read(Container::getStdDump()->fds[FD_DEV], &recvHeader, sizeof(struct sit_header));
	if (ret < (int)sizeof(struct sit_header)) {
		cbd_err("ERR! Try reading dump information\n");
		ret = -1;
		goto exit;
	}

	if(recvHeader.cmd != MSG(MSG_CP2AP, MSG_UPLOAD, MSG_NONE_TOC, MSG_START)) {
		cbd_err("ERR! Wrong CMD received, (recv:0x%x)\n", recvHeader.cmd);
		ret = -1;
		goto exit;
	}

	memset(&dump_info, 0x0, sizeof(struct sit_dump_info));
	ret = read(Container::getStdDump()->fds[FD_DEV], &dump_info, recvHeader.length);
	if (ret < recvHeader.length) {
		cbd_err("ERR! Receiving Dump info\n");
		goto exit;
	}

	make_cp_crash_reason((char *)dump_info.reason);

	ret = write(Container::getStdDump()->fds[FD_INFO], Container::getCbdArgs()->reason, strlen(Container::getCbdArgs()->reason));
	if (ret < 0)
		cbd_err("ERR! writing dump info to info fd\n");

	if (fsync(Container::getStdDump()->fds[FD_INFO]) < 0)
		cbd_err("ERR! fsync info file\n");

	if(close(Container::getStdDump()->fds[FD_INFO]) < 0)
		cbd_err("ERR! close info file\n");
	else
		Container::getStdDump()->fds[FD_INFO] = 0;

	memset(&ssendHeader, 0x0, sizeof(struct sit_send_header));
	memset(&rrecvHeader, 0x0, sizeof(struct sit_recv_header));

	ssendHeader.cmd = MSG(MSG_AP2CP, MSG_UPLOAD, MSG_NONE_TOC, MSG_DATA);
	ssendHeader.length = 8;
	ssendHeader.total_size = dump_info.dump_size;
	ssendHeader.data_offset = 0;

	while (1) {
#ifdef USE_SPI_BOOT_LINK
		ret = write(Container::getStdDump()->fds[FD_DEV], &ssendHeader, sizeof(struct sit_send_header));
		if (ret < 0) {
			cbd_err("ERR! write fail\n");
			goto exit;
		}
#endif
		// receiving dump header data
		ret = std_ul_recv_raw_data(&rrecvHeader, sizeof(struct sit_recv_header));
		if (ret < (int)sizeof(struct sit_recv_header)) {
			cbd_err("ERR! receive rrecvHeader ret: %d\n", ret);
			ret = -1;
			goto exit;
		}

		if (rrecvHeader.cmd != MSG(MSG_CP2AP, MSG_UPLOAD, MSG_NONE_TOC, MSG_DATA)) {
			cbd_err("received: 0x%X != expected: 0xC20B\n", rrecvHeader.cmd);
			goto exit;
		}
		recv_length = (rrecvHeader.length - 8);
		if (recv_length <=0) {
			cbd_err("ERR! recv_length is %d\n", recv_length);
			goto exit;
		}

		//manage raw data buffer
		if (pRawdata != NULL) {
			if (prev_length < recv_length) {
				free(pRawdata);
				pRawdata = NULL;
			} else {
				memset(pRawdata, 0x0, prev_length);
			}
		}

		if (pRawdata == NULL) {
			pRawdata = (u8*)calloc(recv_length, sizeof(u8));
			if (pRawdata == NULL) {
				ret = -ENOMEM;
				cbd_err("ERR! malloc fail\n");
				goto exit;
			}
			prev_length = recv_length;
		}

		//receive dump raw data!
		readLen = ret = std_ul_recv_raw_data(pRawdata, recv_length);
		if (ret < 0) {
			cbd_err("ERR recv dump data fail\n");
			goto exit;
		}

		ssendHeader.data_offset += readLen;

		writeLen = ret = write(Container::getStdDump()->fds[FD_DUMP], pRawdata, readLen);
		if (ret < 0) {
			cbd_err("ERR write dump data fail\n");
			goto exit;
		}

		saveLen += writeLen;

		if (ssendHeader.data_offset >= ssendHeader.total_size) {
			if (ssendHeader.data_offset > ssendHeader.total_size)
				cbd_info("Warning:: too much data received\n");
			cbd_info("Complete Crash Dump!!\n");
			break;
		}
		sequence++;
	}
	if (saveLen != (int)ssendHeader.total_size) {
		cbd_err("ERR! wrong size\n");
		ret = -EFAULT;
		goto exit;
	}

	fsync(Container::getStdDump()->fds[FD_DUMP]);

	/* Send DUMP Done */
	req = MSG(MSG_AP2CP, MSG_UPLOAD, MSG_NONE_TOC, MSG_DONE);
	exp = MSG(MSG_CP2AP, MSG_UPLOAD, MSG_NONE_TOC, MSG_DONE);

	ret = std_udl_req_resp(OPER_DUMP, req, exp);
	if (ret < 0) {
		cbd_info("ERR!  Send DONE request and wait response (req:0x%X exp:0x%X)\n",
			req, exp);
		goto exit;
	}

	ret = 0;

exit:
	if (pRawdata)
		free(pRawdata);

	return ret;
}

#ifdef CONFIG_TYPE_EXT
int ProtocolSit::std_udl_stage_start(enum operation oper, u32 stage)
{
	int ret;
	u32 req = 0xFFFF;
	u32 exp = 0xFFFF;

	if (stage == STD_UDL_FIN_STAGE) {
		req = MSG(MSG_AP2CP, MSG_FINALIZE, MSG_NONE_TOC, MSG_PASS);
		exp = MSG(MSG_CP2AP, MSG_FINALIZE, MSG_NONE_TOC, MSG_PASS);
	} else {
		req = MSG(MSG_AP2CP, MSG_DOWNLOAD, stage, MSG_START);
		exp = MSG(MSG_CP2AP, MSG_DOWNLOAD, stage, MSG_START);
	}

	/* Send a request to CP, then receive and check a response from CP */
	ret = std_udl_req_resp(oper, req, exp);
	if (ret < 0) {
		cbd_info("ERR! [stage %d] START fail (req:0x%X exp:0x%X)\n",
			stage, req, exp);
	}

	return ret;
}

int ProtocolSit::std_udl_stage_done(enum operation oper, u32 stage)
{
	int ret;
	u32 req = 0xFFFF;
	u32 exp = 0xFFFF;

	if (stage == Container::getBootStage()) {
		req = MSG(MSG_AP2CP, MSG_READY, stage, MSG_BOOT);
		exp = MSG(MSG_CP2AP, MSG_READY, stage, MSG_BOOT);
	} else {
		req = MSG(MSG_AP2CP, MSG_DOWNLOAD, stage, MSG_DONE);
		exp = MSG(MSG_CP2AP, MSG_DOWNLOAD, stage, MSG_DONE);
	}

	/* Send a request to CP, then receive and check a response from CP */
	ret = std_udl_req_resp(oper, req, exp);
	if (ret < 0) {
		cbd_info("ERR! [stage %d] DONE fail (req:0x%X exp:0x%X)\n",
			stage, req, exp);
	}

	return ret;
}

int ProtocolSit::std_dl_send_bin(u32 stage, int b_fd, u8 *b_buffer, u32 size)
{
	int ret;
	u32 rest = size;
	u32 len, num_frames;
	u32 req = MSG(MSG_AP2CP, MSG_DOWNLOAD, stage, MSG_DATA);
	u32 exp = MSG(MSG_CP2AP, MSG_DOWNLOAD, stage, MSG_DATA);
	struct exynos_payload payload;

	memset(&payload, 0, sizeof(struct exynos_payload));

	/* Set DLOAD command */
	payload.cmd = req;

	/* Calculate the number of frames to be trsnamitted to CP */
	num_frames = (size / STD_UDL_MSS);
	if (size > (STD_UDL_MSS * num_frames))
		num_frames++;

	/* Print DLOAD information at each stage */
	cbd_info("stage = %d\n", stage);
	cbd_info("size = %d (0x%X)\n", size, size);
	cbd_info("mtu = %d\n", STD_UDL_MTU);
	cbd_info("mss = %d\n", STD_UDL_MSS);
	cbd_info("frames = %d\n", num_frames);

	/* Read and send the CP binary */
	while (rest > 0) {
		len = (rest < STD_UDL_MSS) ? rest : STD_UDL_MSS;

		payload.length = len;
		payload.dataInfo.total_size = size;
		payload.dataInfo.data_offset = size - rest;

		ret = std_dl_tx_frame(b_fd, b_buffer, (void *)&payload);
		if (ret < 0) {
			cbd_info("ERR! std_dl_tx_frame fail\n");
			goto exit;
		}

		/* std_dl_tx_frame will modify payload.length. Do not use the length again. */
		rest -= len;
		b_buffer += len;

		/* Receive and check a response from CP */
		ret = std_udl_req_resp(OPER_BOOT, 0, exp);
		if (ret < 0) {
			cbd_info("ERR! std_udl_req_resp fail\n");
			goto exit;
		}
	}

	return 0;

exit:
	return ret;
}

int ProtocolSit::std_dl_tx_frame(int b_fd, u8 *b_buffer, void *frame)
{
	struct exynos_payload *payload = (struct exynos_payload *)frame;
	int ret = 0;
	u32 payload_len = offsetof(struct exynos_payload, pdata) + payload->length;
	u32 rcvd;
	u32 sent;

	memset(payload->pdata, 0, STD_UDL_MSS);

	/* Read a segment of a CP binary */
	if (b_buffer) {
		memcpy(payload->pdata, b_buffer, payload->length);
	} else {
		rcvd = ret = read(b_fd, payload->pdata, payload->length);
		if (ret < 0) {
			cbd_err("ERR! read fail\n");
			goto exit;
		}

		if (rcvd != payload->length) {
			cbd_info("ERR! rcvd %d != frm->len %d\n", rcvd, payload->length);
			ret = -EFAULT;
			goto exit;
		}
	}

	payload->length += sizeof(struct exynos_data_info);

	/* Send the CP binary segment */
	sent = ret = write(Container::getStdBoot()->fds[FD_DEV], payload, payload_len);
	if (ret < 0) {
		cbd_err("ERR! write fail\n");
		goto exit;
	}

	if (sent != payload_len) {
		cbd_info("ERR! sent %d != frm_len %d\n", sent, payload_len);
		ret = -EFAULT;
		goto exit;
	}

	return 0;

exit:
	return ret;
}

int ProtocolSit::std_dl_send_crc(u32 stage, u32 crc)
{
	int ret;
	u32 exp = MSG(MSG_CP2AP, MSG_SECURITY, stage, MSG_PASS);
	struct std_udl_crc_frame crc_frm;

	crc_frm.cmd = MSG(MSG_AP2CP, MSG_SECURITY, stage, MSG_CRC);
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

int ProtocolSit::std_udl_poll(enum operation oper, short events, long timeout)
{
	struct pollfd pfd;
	int count = 0;

	pfd.fd = (oper == OPER_BOOT ? Container::getStdBoot()->fds[FD_DEV] : Container::getStdDump()->fds[FD_DEV]);
	pfd.events = events;
	while (1) {
		int ret;
		pfd.revents = 0;

		/* Wait "events" up to "timeout" msec */
		ret = poll(&pfd, 1, timeout);

		if (pfd.revents & events) {
			break;
		} else if (pfd.revents == (POLLERR | POLLHUP)) {
			goto exit;
		} else if (pfd.revents & POLLHUP) {
			/* TODO */
			if (count++ > 100)
				goto exit;

			usleep(200000);
			continue;
		}

		if (!ret) {
			cbd_err("ERR! poll fail (events 0x%X, TIMEOUT)\n", events);
			goto exit;
		} else if (ret < 0) {
			cbd_err("ERR! poll fail (events 0x%X)\n", events);
			goto exit;
		}
	}

	return 0;

exit:
	return -EIO;
}

int ProtocolSit::std_ul_recv_raw_data(void *buffer, u32 size)
{
	int ret = -1;
	int readLen = 0;

	ret = std_udl_poll(OPER_DUMP, POLLIN, WAIT_POLL_TIME);
	if (ret < 0) {
		cbd_err("ERR! poll fail\n");
		goto exit;
	}

	readLen = ret = read(Container::getStdDump()->fds[FD_DEV], buffer, size);
	if (ret < 0) {
		cbd_err("ERR! read fail\n");
		goto exit;
	}
	ret = readLen;

exit:
	return ret;
}

