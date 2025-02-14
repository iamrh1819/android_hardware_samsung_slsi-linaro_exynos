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

#include "SrinfoSit2.h"
#include "Container.h"

SrinfoSit2::SrinfoSit2() {}

void SrinfoSit2::store_srinfo(int dev_fd)
{
	const char *suffix = Container::getCbdArgs()->cpn.rat;
	int ret, fd;
	unsigned size;
	struct shmem_srinfo *rdbuf;

	rdbuf = alloc_srinfo(&size, dev_fd);
	if (!rdbuf) {
		cbd_info("alloc srinfo fail\n");
		return;
	}

	ret = Log::create_directory(SRINFO_PATH);
	if (ret)
		goto exit;

	fd = open_srinfo_file(suffix);
	if (fd < 0) {
		cbd_info("sr_info_last create fail(%d)\n", fd);
		goto exit;
	}
	ret = write_srinfo_file(fd, rdbuf->buf, size);
	if (ret < 0) {
		cbd_info("srinfo write fail(%d)\n", ret);
		close(fd);
		goto exit;
	}
	close(fd);

exit:
	free(rdbuf);
}

struct shmem_srinfo *SrinfoSit2::alloc_srinfo(unsigned *len, int boot_fd)
{
	struct shmem_srinfo *args;
	unsigned *magic;
	int ret;

	args = (struct shmem_srinfo *)malloc(SRINFO_READ_SIZE);
	if (!args) {
		cbd_info("read buf alloc(%d) fail\n", SRINFO_READ_SIZE);
		return NULL;
	}
	memset(args, 0x00, SRINFO_READ_SIZE);
	args->size = SRINFO_READ_SIZE - sizeof(unsigned);

	ret = ioctl(boot_fd, IOCTL_GET_SRINFO, args);
	if (ret < 0) {
		cbd_err("ioctl fail - Get srinfo(%d)\n", ret);
		goto exit;
	}
	*len = args->size;

	/* srinfo has "SRINFO" format from offset 0x0 */
	magic = (unsigned *)(args->buf);
	if (*magic != 'NIRS') {
		cbd_info("Crash info string was invalid(%x/%x)\n", *magic, 'NIRS');
		goto exit;
	}

	return args;
exit:
	free(args);
	return NULL;
}

unsigned SrinfoSit2::write_srinfo_file(int fd, char *buf, unsigned size)
{
	int i;
	int ret = -1;
	char *rdbuf = buf + 0x10;

	for (i = 0; i < size; i++) {
		char *rp = rdbuf + i;

		if (*rp == ']')
			break;
	}
	ret = write(fd, rdbuf, i+1);
	if (ret <= 0) {
		cbd_err("raw data write fail\n");
		return ret;
	}
	dprintf(fd, "\n");
	if (fsync(fd))
		cbd_err("ERR! fsync(srinfo) fail\n");

	cbd_info("store done\n");

	return ret;
}

