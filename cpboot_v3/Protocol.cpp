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

#include "Protocol.h"
#include "Container.h"

Protocol::Protocol() {}

int Protocol::std_ul_rx_frame(void *buff, u32 size)
{
	int ret;

	/* Wait for a DUMP frame from CP up to WAIT_POLL_TIME ms */
	ret = std_udl_poll(OPER_DUMP, POLLIN, WAIT_POLL_TIME);
	if (ret < 0) {
		cbd_dump_err("ERR! DUMP std_udl_poll fail\n");
		goto exit;
	}

	/* Receive a DUMP frame from CP */
	ret = read(Container::getStdDump()->fds[FD_DEV], buff, size);
	if (ret < 0) {
		cbd_dump_err("ERR! DUMP read fail\n");
		goto exit;
	}

exit:
	return ret;
}

int Protocol::std_udl_req_resp(enum operation oper, u32 req, u32 exp)
{
	int ret = 0;
	u32 resp;
	int fd = (oper == OPER_BOOT ? Container::getStdBoot()->fds[FD_DEV] : Container::getStdDump()->fds[FD_DEV]);

	/* Send a request to CP if exists */
	if (req) {
		cbd_info("request:0x%08X\n", req);
		ret = write(fd, &req, sizeof(u32));
		if (ret < 0) {
			cbd_err("ERR! write fail\n");
			goto exit;
		}
	}

	/* Receive and verify a response from CP if expected */
	if (exp) {
		/* Wait for a response from CP up to WAIT_POLL_TIME ms */
		ret = std_udl_poll(oper, POLLIN, WAIT_POLL_TIME);
		if (ret < 0) {
			cbd_info("ERR! std_udl_poll fail\n");
			goto exit;
		}

		/* Receive a response from CP */
		ret = read(fd, &resp, sizeof(u32));
		if (ret < 0) {
			cbd_err("ERR! read fail\n");
			goto exit;
		}

		/* Verify the response */
		if (resp != exp) {
			cbd_info("ERR! resp 0x%X != exp 0x%X\n", resp, exp);
			ret = -EFAULT;
			goto exit;
		}
	}

	return 0;

exit:
	cbd_info("ERR! request:0x%08X expected:0x%08X\n", req, exp);
	return ret;
}

void Protocol::make_cp_crash_reason(char *cp_reason)
{
	struct crash_reason info;

	memset(Container::getCbdArgs()->reason, 0, CRASH_REASON_SIZE);

	/* Receive the CP CRASH reason */
	if (ioctl(Container::getStdDump()->fds[FD_DEV], IOCTL_GET_CP_CRASH_REASON, &info) < 0) {
		cbd_err("ERR! IOCTL_GET_CP_CRASH_REASON fail\n");
		return;
	}

	cbd_info("reason owner:%d\n", info.owner);
	switch (info.owner) {
	case CRASH_REASON_RIL_MNR:
	case CRASH_REASON_RIL_REQ_FULL:
	case CRASH_REASON_RIL_PHONE_DIE:
	case CRASH_REASON_RIL_RSV_MAX:
	case CRASH_REASON_RIL_TRIGGER_CP_CRASH:
		snprintf(Container::getCbdArgs()->reason, CRASH_REASON_SIZE, CP_CRASH_BY_STR " RIL - %s", info.string);
		break;

	case CRASH_REASON_USER:
		snprintf(Container::getCbdArgs()->reason, CRASH_REASON_SIZE, CP_CRASH_BY_STR " USER - %s", info.string);
		break;

	case CRASH_REASON_MIF_TX_ERR:
	case CRASH_REASON_MIF_RIL_BAD_CH:
	case CRASH_REASON_MIF_RX_BAD_DATA:
	case CRASH_REASON_MIF_RSV_MAX:
	case CRASH_REASON_MIF_FORCED:
		snprintf(Container::getCbdArgs()->reason, CRASH_REASON_SIZE, CP_CRASH_BY_STR " CPIF - %s", info.string);
		break;

	case CRASH_REASON_CLD:
		snprintf(Container::getCbdArgs()->reason, CRASH_REASON_SIZE, CP_CRASH_BY_STR " CLD - %s", info.string);
		break;

	case CRASH_REASON_CP_SRST:
	case CRASH_REASON_CP_RSV_0:
	case CRASH_REASON_CP_RSV_MAX:
	case CRASH_REASON_CP_ACT_CRASH:
		snprintf(Container::getCbdArgs()->reason, CRASH_REASON_SIZE, CP_CRASH_BY_STR " CP - %s", cp_reason);
		break;

	case CRASH_REASON_CP_WDOG_CRASH:
		/* In watchdog case, reason string should be set by AP */
		snprintf(Container::getCbdArgs()->reason, CRASH_REASON_SIZE, CP_CRASH_BY_STR " CP - " CP_WDT_RESET_STR);
		break;

	default:
		snprintf(Container::getCbdArgs()->reason, CRASH_REASON_SIZE, CP_CRASH_BY_STR " (%d)", info.owner);
		break;
	}

	cbd_info("CP CRASH%s\n", Container::getCbdArgs()->reason);
}

