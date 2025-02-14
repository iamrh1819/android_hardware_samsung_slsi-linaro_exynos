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

#include "SrinfoSipc.h"
#include "Container.h"

#define SRINFO_LAST_PATH	CPDUMP_PATH "/err"
#define SRINFO_LAST			"last_sr_info"

SrinfoSipc::SrinfoSipc() {}

void SrinfoSipc::store_srinfo(int dev_fd)
{
	const char *suffix = Container::getCbdArgs()->cpn.rat;
	int ret, fd;
	unsigned size;
	struct shmem_srinfo *rdbuf;

	ret = check_srinfo_last(suffix);
	if (!ret) {
		cbd_info("srinfo_last file exist, skip!\n");
		return;
	}

	rdbuf = alloc_srinfo(&size, dev_fd);
	if (!rdbuf) {
		cbd_info("alloc srinfo fail\n");
		return;
	}

	ret = Log::create_directory(SRINFO_PATH);
	if (ret)
		goto exit;

	ret = Log::create_directory(SRINFO_LAST_PATH);
	if (ret)
		goto exit;

	fd = create_srinfo_last(suffix);
	if (fd < 0) {
		cbd_info("sr_info_last open fail(%d)\n", fd);
		goto exit;
	}
	ret = write_srinfo_last(fd, rdbuf->buf, size);
	if (ret < 0) {
		cbd_info("srinfo_last write fail(%d)\n", ret);
		close(fd);
		goto exit;
	}
	close(fd);

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

void SrinfoSipc::restore_srinfo(int dev_fd)
{
	const char *suffix = Container::getCbdArgs()->cpn.rat;
	int ret, fd, size;
	char *rdbuf, filepath[64], filebak[64];
	struct shmem_srinfo *sr_args;

	fd = open_srinfo_last(suffix);
	if (fd < 0) {
		cbd_info("sr_info_last open fail(%d)\n", fd);
		return;
	}

	rdbuf = (char *)malloc(SRINFO_READ_SIZE);
	if (!rdbuf) {
		cbd_info("read buf alloc(%d) fail\n", SRINFO_READ_SIZE);
		goto exit;
	}
	memset(rdbuf, 0x00, SRINFO_READ_SIZE);

	sr_args = (struct shmem_srinfo *)rdbuf;
	size = read(fd, sr_args->buf, SRINFO_READ_SIZE - sizeof(unsigned));
	if (size <= 0) {
		cbd_err("last_sr_info file read fail\n");
		goto exit_free;
	}
	sr_args->size = size;

	ret = ioctl(dev_fd, IOCTL_SET_SRINFO, sr_args);
	if (ret < 0) {
		cbd_err("ioctl fail - Set srinfo(%d)\n", ret);
		goto exit_free;
	}

#ifndef DEBUG
	sprintf(filepath, "%s/%s_%s", SRINFO_LAST_PATH, SRINFO_LAST, suffix);
	ret = unlink(filepath);
	if (ret < 0 && ret != -EPERM) {
		cbd_err("delete (%s) file fail\n", filebak);
		goto exit_free;
	}
#else
	sprintf(filepath, "%s/%s_%s", SRINFO_LAST_PATH, SRINFO_LAST, suffix);
	sprintf(filebak, "%s/%s_%s.bak", SRINFO_LAST_PATH, SRINFO_LAST, suffix);

	ret = unlink(filebak);
	if (ret < 0 && ret != -EPERM) {
		cbd_err("delete (%s) file fail\n", filebak);
		goto exit_free;
	}

	ret = rename(filepath, filebak);
	if (ret < 0) {
		cbd_err("rename (%s -> %s) file fail\n", filepath, filebak);
		goto exit_free;
	}
#endif
	cbd_info("done = %s\n", rdbuf);

exit_free:
	free(rdbuf);
exit:
	close(fd);
}

struct shmem_srinfo *SrinfoSipc::alloc_srinfo(unsigned *len, int boot_fd)
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

	/* srinfo has "{JVER:" format from offset 0x10 */
	magic = (unsigned *)(args->buf + 0x12);
	if (*magic != 'REVJ') {
		cbd_info("Crash info string was invalid(%x/%x)\n", *magic, 'REVJ');
		goto exit;
	}

	return args;
exit:
	free(args);
	return NULL;
}

unsigned SrinfoSipc::write_srinfo_file(int fd, char *buf, unsigned size)
{
	char timestr[MAX_SUFFIX_LEN], ascii[17];
	time_t now;
	struct tm result;
	u32 linelen = 16;

	time(&now);
	localtime_r(&now, &result);
	strftime(timestr, MAX_SUFFIX_LEN, "%Y-%m-%d %H:%M:%S", &result);
	dprintf(fd, "[%s]\n", timestr);

	cbd_info("store to hex ascii text size=(0x%x)\n", (unsigned)size);
	/*
	   ret = write(fd, buf, size);
	   if (ret <= 0) {
	   cbd_info("raw data write fail\n");
	   goto exit_free;
	   }
	 */
	for (u32 i = 0; i < size; i += linelen) {
		char *rp = buf + i;
		int *rpr = (int *)rp;


		for (u32 j = 0; j < linelen; j++)
			ascii[j] = tochar(*(rp + j));
		ascii[16] = '\0';

		/*
		   cbd_info("line = 0x%04x: %s\n", i, ascii);
		 */

		dprintf(fd, "%04x:%08x %08x %08x %08x %s\n", i, *rpr,
				*(rpr + 1), *(rpr + 2), *(rpr + 3), ascii);
	}
	dprintf(fd, "\n");
	if (fsync(fd))
		cbd_err("ERR! fsync(srinfo_hex) fail\n");

	cbd_info("store done\n");

	return size;
}

/* store raw srinfo to file */
int SrinfoSipc::create_srinfo_last(const char *suffix)
{
	int fd, ret = -1;
	char file[64];

	sprintf(file, "%s/%s_%s", SRINFO_LAST_PATH, SRINFO_LAST, suffix);
	cbd_info("create last_sr_info file - %s\n", file);
	fd = open(file,
		O_RDWR | O_CREAT,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd < 0) {
		cbd_err("last_sr_info create fail\n");
		goto exit;
	}

	ret = lseek(fd, 0, SEEK_SET);
	if (ret < 0) {
		cbd_err("fd_last lseek fail\n");
		close(fd);
		goto exit;
	}
	return fd;
exit:
	return ret;
}

/* store raw srinfo to file */
int SrinfoSipc::open_srinfo_last(const char *suffix)
{
	int fd, ret = -1;
	char file[64];

	sprintf(file, "%s/%s_%s", SRINFO_LAST_PATH, SRINFO_LAST, suffix);
	cbd_info("open last_sr_info file - %s\n", file);
	fd = open(file, O_RDWR);
	if (fd < 0) {
		cbd_err("last_sr_info open fail\n");
		goto exit;
	}

	ret = lseek(fd, 0, SEEK_SET);
	if (ret < 0) {
		cbd_err("fd_last lseek fail\n");
		close(fd);
		goto exit;
	}
	return fd;
exit:
	return ret;
}

int SrinfoSipc::write_srinfo_last(int fd, char *buf, unsigned size)
{
	u32 i;
	char *rdbuf = buf + 0x10;
	int ret;

	for (i = 0; i < size; i++) {
		char *rp = rdbuf + i;

		if (*rp == '}')
			break;
	}
	cbd_info("store to ascii text (%d bytes)\n", i);
	ret = write(fd, rdbuf, i+1);

	if (fsync(fd))
		cbd_err("ERR! fsync(srinfo) fail\n");

	return ret;
}

/* check srinfo file */
int SrinfoSipc::check_srinfo_last(const char *suffix)
{
	struct stat ldir_st;
	char file[64];

	sprintf(file, "%s/%s_%s", SRINFO_LAST_PATH, SRINFO_LAST, suffix);
	cbd_info("%s: full_path: %s\n", __func__, file);

	return stat(file, &ldir_st);
}

char SrinfoSipc::tochar(char x)
{
	return (x > 0x21 && x < 0x7E) ? x : '.';
}

