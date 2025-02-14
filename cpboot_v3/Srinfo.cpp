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

#include "Srinfo.h"

Srinfo::Srinfo() {}

void Srinfo::restore_srinfo(int dev_fd)
{
	/* do nothing */
}

/* open the srinfo file to save the crash info and check the file size,
   if it was bigger than MAX_SIZE, rename to bak file and open new file */
int Srinfo::open_srinfo_file(const char *suffix)
{
	int ret, fd;
	char infofile[64], infobak[64];
	struct stat sb;

	sprintf(infofile, "%s/%s_%s", SRINFO_PATH, SRINFO_FILE, suffix);
	sprintf(infobak, "%s/.%s_%s.bak", SRINFO_PATH, SRINFO_FILE, suffix);
	cbd_info("open file - %s(%s)\n", infofile, infobak);
	ret = fd = open(infofile,
			O_WRONLY | O_APPEND | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (ret < 0) {
		cbd_err("open fail - %s\n", infofile);
		goto exit;
	}

	/* check file size */
	memset(&sb, 0x00, sizeof(struct stat));
	ret = fstat(fd, &sb);
	if (ret) {
		cbd_err("stat fail - srinfo log\n");
		close(fd);
		goto exit;
	}
	ret = fd;

	cbd_info("srinfo log size : %lu(0x%lx/0x%x)\n",
			sb.st_size, sb.st_size, SRINFO_MAX_SIZE);

	/* file size limit */
	if (sb.st_size > SRINFO_MAX_SIZE) {
		close(fd);
		ret = unlink(infobak);
		if (ret < 0 && ret != -EPERM) {
			cbd_err("deleate (%s) file fail\n", infobak);
			goto exit;
		}
		ret = rename(infofile, infobak);
		if (ret < 0) {
			cbd_err("rename (%s -> %s) file fail\n", infofile, infobak);
			goto exit;
		}
		ret = fd = open(infofile,
				O_WRONLY | O_APPEND | O_CREAT,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (fd < 0) {
			cbd_err("srinfo open fail\n");
			goto exit;
		}
	}
exit:
	return ret;
}

