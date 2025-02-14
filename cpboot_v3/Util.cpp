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

#include <sys/prctl.h>
#include <sys/capability.h>
#include <cutils/android_filesystem_config.h>

#include "Util.h"
#include "Container.h"

#define STR_CMDLINE "/proc/cmdline"

/* support below capability since Linux 3.5 */
#define CAP_SYSLOG			34
#define CAP_BLOCK_SUSPEND	36

/*
 * Parsing cmdline strings
 */
char *Util::get_cmdline_str(char *buf, unsigned size, const char *find)
{
	char *ptr;
	int fd;

	fd = open(STR_CMDLINE, O_RDONLY);
	if (fd >= 0) {
		int n = read(fd, buf, size - 1);
		if (n < 0)
			n = 0;

		/* get rid of trailing newline, it happens */
		if (n > 0 && buf[n-1] == '\n')
			n--;

		buf[n] = 0;
		close(fd);
	} else
		buf[0] = 0;

	ptr = buf;
	while (ptr && *ptr) {
		char *x = strchr(ptr, ' ');
		if (x != 0)
			*x++ = 0;
		if (strncmp(ptr, find, strlen(find)) == 0) {
			cbd_info("find str %s form %s\n", ptr, STR_CMDLINE);
			return ptr;
		}
		ptr = x;
	}
	return NULL;
}

/*
 * switchUser - Switches UID to radio, preserving CAP_NET_ADMIN capabilities.
 * Our group, cache, was set by init.
 * get from rild.c
 */
void Util::switch_user(void)
{
	struct __user_cap_header_struct header = {
		.version = _LINUX_CAPABILITY_VERSION_3,
		.pid     = 0  /* 0 = change myself */
	};
	struct __user_cap_data_struct cap[_LINUX_CAPABILITY_U32S_3] = {};
	uint64_t target_cap = (1UL << CAP_NET_ADMIN) | (1UL << CAP_SYS_ADMIN) | (1UL << CAP_NET_RAW) |
		(1UL << CAP_SYS_BOOT) | (1UL << CAP_SYSLOG) | (1UL << CAP_BLOCK_SUSPEND);

	prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
	setuid(AID_RADIO);

	cap[0].permitted = cap[0].effective = (__u32)(target_cap);
	cap[0].inheritable = 0;
	cap[1].permitted = cap[1].effective = (__u32)(target_cap >> 32);
	cap[1].inheritable = 0;

	if (capset(&header, cap) < 0)
		cbd_err("capset failed\n");
}

#if defined(CONFIG_THROUGHPUT_MONITOR)
void *Util::traffic_monitor(void* arg)
{
	FILE *fp, *fp_qos;
	const char *iface = "rmnet0,rmnet1,rmnet2,rmnet3,rmnet4";
	char buffer[1024], name[32];
	unsigned long rx = 0, tx = 0, delta = 0, prev = 0, tmp_sum = 0;
	unsigned Mbps = 0;
	int fd;

	fd = open("/dev/network_throughput", O_RDWR);
	if (!fd) {
		cbd_info("Device doesn't support qos\n");
		goto error;
	}

	while (1) {
		fp = fopen("/proc/net/dev", "r");
		if (!fp) {
			cbd_err("Fail to open /proc/net/dev node\n");
			break;
		}

		/* Ignore unnecessary header data */
		fgets(buffer, sizeof(buffer), fp);
		fgets(buffer, sizeof(buffer), fp);
		tmp_sum = 0;

		while(fgets(buffer, sizeof(buffer), fp)) {
			buffer[strlen(buffer) - 1] = '\0';
			sscanf(buffer, "%30[^:]%*[:] %10lu %*s %*s %*s %*s %*s %*s %*s %10lu", name, &rx, &tx);

			if (!strstr(iface, name))
				continue;

			tmp_sum += tx;
			tmp_sum += rx;
		}

		delta = tmp_sum - prev;
		Mbps = (delta * 8) / (1000 * 1000);

		sprintf(buffer, "0x%x\n", Mbps);
		write(fd, buffer, strlen(buffer) + 1);
		prev = tmp_sum;

		if (fp)
			fclose(fp);

		sleep(1);
	}

error:
	if (fd)
		close(fd);

	pthread_exit(NULL);
}
#endif

int Util::create_empty_nv(char *path, size_t size)
{
	int ret = 0;
	int saved = 0;
	int nv_fd = -1;
	char *nv_data = NULL;

	cbd_info("try to create NV(%s) with size(%ld)\n", path, size);

	nv_fd = open(path, O_RDWR | O_CREAT | O_SYNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (nv_fd < 0) {
		cbd_err("ERR! NV(%s) open fail\n", path);
		ret = -EFAULT;
		goto exit;
	}

	if (!size) {
		cbd_info("ERR! wrong size(%ld)\n", size);
		ret = -EFAULT;
		goto exit;
	}

	nv_data = (char *)malloc(size);
	if (!nv_data) {
		cbd_info("ERR! malloc(%ld) fail\n", size);
		ret = -ENOMEM;
		goto exit;
	}
	memset(nv_data, 0xFF, size);

	saved = write(nv_fd, nv_data, size);
	if (saved < 0) {
		cbd_err("ERR! NV(%s) write fail\n", path);
		ret = -EFAULT;
		goto exit;
	} else if (saved != size) {
		cbd_info("ERR! NV(%s) partial saved(%d) but size(%ld)\n", path, saved, size);
		ret = -EIO;
		goto exit;
	}

	if (fsync(nv_fd)) {
		cbd_err("ERR! fsync(nv_fd) fail\n");
		goto exit;
	}

	cbd_info("NV(%s) saved %d bytes\n", path, saved);

exit:
	if (nv_fd >= 0)
		close(nv_fd);

	if (nv_data)
		free(nv_data);

	return ret;
}

bool Util::getNvFd(enum cp_boot_mode mode, char *nv_path, int nv_size, int *nv_fd)
{
	int fd = -1;
	int ret;

	if (mode == CP_BOOT_MODE_NORMAL && nv_path[0]) {
		fd = open(nv_path, O_RDONLY);
		if(fd < 0) {
			if (errno != ENOENT) {
				cbd_err("ERR! NV(%s) open fail\n", nv_path);
				return false;
			}

			cbd_info("No NV(%s) file\n", nv_path);

			if (Util::create_empty_nv(nv_path, nv_size) < 0) {
				cbd_info("ERR! create_empty_nv(%s, %d) fail\n", nv_path, nv_size);

				ret = remove(nv_path);
				if (ret)
					cbd_err("ERR! NV(%s) remove fail\n", nv_path);

				return false;
			}

			fd = open(nv_path, O_RDONLY);
			if (fd < 0) {
				cbd_err("ERR! NV(%s) open fail\n", nv_path);
				return false;
			}
		}

		cbd_info("NV(%s) opened (fd %d)\n", nv_path, fd);
	}

	*nv_fd = fd;

	return true;
}

void Util::toUpperCase(char* src, char *dst)
{
	char* s = src;
	char* d = dst;

	for (; *s; ++s,++d) {
		*d = toupper((unsigned char) *s);
	}
}

void Util::byteToHex(char *dst, size_t dstSize, char *src, size_t srcSize)
{
	int loop = std::min(dstSize/3, srcSize);

	if (!dst || !src || !loop)
		return;

	for (int i = 0; i < loop; i++)
		snprintf(&dst[i*3], dstSize - (i*3), "%02X ", src[i]);

	dst[loop*3 - 1] = '\0';
}

