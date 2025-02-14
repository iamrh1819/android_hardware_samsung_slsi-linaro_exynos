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

#define LOG_FREESPACE_MIN_MB 	500

#ifdef CONFIG_DUMP_LIMIT
#define MAX_DUMP_INDEX		20
#define DEFAULT_DUMP_LIMIT 	5
#endif

void Log::kprintf_init(void)
{
#ifdef DEBUG_KERNEL_MSG
	const char *name = "/dev/kmsg";
	kmsg_fd = open(name, O_RDWR);
#endif
}

void Log::kprintf_deinit(void)
{
#ifdef DEBUG_KERNEL_MSG
	if (kmsg_fd != STDOUT_FILENO)
		close(kmsg_fd);
#endif
}

char *Log::get_log_dir(void)
{
	return log_path;
}

void Log::update_log_dir(void)
{
	memset(&log_path[0], 0, sizeof(log_path));
#ifdef CONFIG_DUMP_LIMIT
	snprintf(log_path, MAX_PATH_LEN, "%s/%02d", CPDUMP_PATH, property_get_int32(VPROP_CDUMP_INDEX, 0));
#else
	snprintf(log_path, MAX_PATH_LEN, "%s", CPDUMP_PATH);
#endif
}

int Log::create_log_directory(void)
{
	int err;

	err = create_directory(CPDUMP_PATH);
	if (err)
		goto exit;

#ifdef CONFIG_DUMP_LIMIT
	err = create_directory(get_log_dir());
	if (err)
		goto exit;
#endif

	err = check_fs_log_directory();
	if (err)
		goto exit;

	return 0;

exit:
	return err;
}

int Log::create_directory(const char *path)
{
	struct stat ldir_st;
	struct group *g;
	int cur_gid = -1;
	int err;

	err = stat(path, &ldir_st);
	if (!err) { /* path exist */
		if (!S_ISDIR(ldir_st.st_mode)) {
			cbd_info("(%s) is not a directory\n", path);
			return -EFAULT;
		}

		cur_gid = ldir_st.st_gid;
	} else {
		err = mkdir(path, 0775);
		if (err < 0 && errno != EEXIST) {
			cbd_err("log path create fail err=%d\n", err);
			return err;
		}

		cbd_info("log path (%s) created\n", path);

		/* allow an error on security context */
		err = selinux_android_restorecon(path, SELINUX_ANDROID_RESTORECON_RECURSE);
		if (err)
			cbd_err("selinux restorecon failed path(%s) err(%d)\n", path, err);
	}

	/* change group id to "log" from "radio" for access from other process */
	g = getgrnam("log");
	if (g && cur_gid != g->gr_gid) {
		err = chown(path, -1, g->gr_gid);
		if (err)
			cbd_err("log path(%s) gid(%d) err(%d)\n", path, g->gr_gid, err);
	}

	return 0;
}

#ifdef CONFIG_DUMP_LIMIT
void Log::organize_dump_files()
{
	int current_dump_file_index = 0;
	int allowed_num_dump_files = 0;
	char target_dir_path[MAX_PATH_LEN] = {0, };
	int removal_index = 0; /* directory index planned to be removed */
	char cdump_limit_string[MAX_PATH_LEN] = {0, };
	char cdump_index_string[MAX_PATH_LEN] = {0, };

	current_dump_file_index = property_get_int32(VPROP_CDUMP_INDEX, 0);
	allowed_num_dump_files = property_get_int32(VPROP_CDUMP_LIMIT, DEFAULT_DUMP_LIMIT);

	/* error handling: fix VPROP_CDUMP_LIMIT to MAX_DUMP_LIMIT if it exceeds max */
	if (allowed_num_dump_files > MAX_DUMP_INDEX || allowed_num_dump_files < 1) {
		cbd_err("Number of dump files allowed to be stored is invalid\n");
		snprintf(cdump_limit_string, MAX_PATH_LEN, "%d", MAX_DUMP_INDEX);
		property_set(VPROP_CDUMP_LIMIT, cdump_limit_string);
		allowed_num_dump_files = MAX_DUMP_INDEX;
		cbd_err("set %s to %d\n", VPROP_CDUMP_LIMIT, MAX_DUMP_INDEX);
	}

	/**
	 * if current dump file index A is less than allowed number of dump files B,
	 * set (removing) target index to MAX_DUMP_INDEX + A - B + 1.
	 * This is because of the possibility that current dump index is looped back one (MAX_DUMP_INDEX to 0)
	 */
	if (current_dump_file_index < allowed_num_dump_files) {
		removal_index = MAX_DUMP_INDEX + current_dump_file_index - allowed_num_dump_files + 1;
	} else {
		removal_index = current_dump_file_index - allowed_num_dump_files;
	}

	/* now it is time to remove directories */
	while (removal_index != current_dump_file_index) {
		sprintf(target_dir_path, "%s/%02d", CPDUMP_PATH, removal_index);
		remove_directory(target_dir_path);
		if (removal_index != 0)
			removal_index--;
		else /* removed log dir index was 0, should check MAX_DUMP_INDEX log dir next time */
			removal_index = MAX_DUMP_INDEX;
	}
	/* update dump index to the next one */
	if (current_dump_file_index == MAX_DUMP_INDEX)
		snprintf(cdump_index_string, MAX_PATH_LEN, "%d", 0);
	else
		snprintf(cdump_index_string, MAX_PATH_LEN, "%d", current_dump_file_index + 1);
	property_set(VPROP_CDUMP_INDEX, cdump_index_string);
	update_log_dir();
}

int Log::unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
	int rv = remove(fpath);

	if (rv)
		perror(fpath);
	return rv;
}

int Log::remove_directory(char *path)
{
	int err = strncmp(path, CPDUMP_PATH, sizeof(CPDUMP_PATH) - 1);
	if (err) {
		cbd_err("Tried to remove false path: %s, %lu\n", path, sizeof(CPDUMP_PATH) - 1);
		return err;
	}

	return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}
#endif

int Log::cpboot_log_needed(void)
{
	char prop_value[256] = {0, };

	if (property_get("ro.hardware.chipname", prop_value, "NONE") > 0) {
		/* P200311-05349
		 * This issue was reported at Hubble that
		 * Camera can be stucked during saving log.
		 */
		if (!strncmp("exynos990", prop_value, 9) ||
			!strncmp("exynos9830", prop_value, 10))
			return 0;
	}

	return 1;
}

void Log::save_logs_thread(int type, char *prefix)
{
	static struct save_logs_arg log_args = {.type = type, .prefix = prefix};
	pthread_t thr;
	int ret;

	if (debug_level == DBG_LOW)
		return;

	if (!cpboot_log_needed())
		return;

	ret = pthread_create(&thr, NULL, _save_logs, (void *)&log_args);
	if (ret < 0)
		cbd_info("pthread_create fail\n");

	pthread_detach(thr);

	return;
}

void Log::save_logs(int type, char *prefix)
{
	struct save_logs_arg log_args = {.type = type, .prefix = prefix};

	_save_logs((void *)&log_args);
}

void *Log::_save_logs(void *arg)
{
	time_t now;
	struct tm result;
	char log_file_str[256], log_surfix[25];
	struct save_logs_arg *args = (struct save_logs_arg *)arg;
	int type = args->type;
	char *prefix = args->prefix;

	if (create_log_directory())
		return NULL;

	cbd_info("save dmesg begin...\n");

	time(&now);
	localtime_r(&now, &result);
	strftime(log_surfix, 20, "%Y%m%d%H%M_%S", &result);

	if (type & LOGB_BOOTFAIL) {
		sprintf(log_file_str, "%s/%s_last.log", get_log_dir(), prefix);
		dmesg_to_file(log_file_str);
		cbd_info("%s\n", log_file_str);
		return NULL;
	}
	if (type & LOGB_DMESG) {
		sprintf(log_file_str, "%s/%s_%s.log", get_log_dir(), prefix,
			log_surfix);
		dmesg_to_file(log_file_str);
		cbd_info("%s\n", log_file_str);
	}

	return NULL;
}

int Log::dmesg_to_file(char *of)
{
	char buffer[KLOG_BUF_LEN + 1];
	char *p;
	ssize_t ret;
	int n;
	int fd;

	fd = open(of, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0664);
	if (fd < 0) {
		cbd_err("file open fail %s\n", of);
		return fd;
	}
	fchmod(fd, 0664);

	ret = lseek(fd, 0, SEEK_END);
	if (ret < 0) {
		cbd_err("lseek fail %s\n", of);
		goto exit;
	}

	n = klogctl(KLOG_READ_ALL, buffer, KLOG_BUF_LEN);
	if (n < 0) {
		cbd_info("klogctl fail\n");
		close(fd);
		return -1;
	}
	buffer[n] = '\0';

	p = buffer;
	while ((ret = write(fd, p, n + 1))) {
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			cbd_err("write fail\n");
			goto exit;
		}
		p += ret;
		n -= ret;
	}

	if (fsync(fd))
		cbd_err("ERR! fsync(fd) fail\n");

exit:
	close(fd);
	return 0;
}

int Log::check_fs_log_directory(void)
{
	struct statfs buf;
	int err;
	long fsize_mb;

	err = statfs(get_log_dir(), &buf);
	if (err < 0) {
		cbd_err("statfs failed\n");
		return err;
	}

	fsize_mb = (buf.f_bfree / 1024) * (buf.f_bsize / 1024);
	if (fsize_mb < LOG_FREESPACE_MIN_MB) {
		cbd_info("Block size: %lu, Free Block: %" PRIu64 ", Free Size: %ld MB\n",
			buf.f_bsize, buf.f_bfree, fsize_mb);
		cbd_info("%s: freespace is under %u MB\n", get_log_dir(), LOG_FREESPACE_MIN_MB);
		return -ENOSPC;
	}

	return 0;
}

const char *Log::get_cbd_version(void)
{
	return &(cbd_version[0]);
}

void Log::check_debug_cp_opt(void)
{
	char *str;
	char cmdline[MAX_CMD_LINE_LEN];
	const char *cmd = "sec_debug.cp=";

	int prop_value = property_get_int32(VPROP_DEBUG_CP_OPT, -1);
	if (prop_value >= 0) {
		debug_cp_opt = (enum sec_cp_debug)prop_value;
		cbd_info("cp_debug=%d overwritten by prop\n", debug_cp_opt);
	} else {
		memset(cmdline, 0x00, MAX_CMD_LINE_LEN);
		str = Util::get_cmdline_str(cmdline, MAX_CMD_LINE_LEN, cmd);
		if (str) {
			debug_cp_opt = (enum sec_cp_debug)(*(str + strlen(cmd)) - '0');
			cbd_info("%s, %d\n", str, debug_cp_opt);
		}
	}
}

void Log::check_debug_level()
{
	const char *CMD_SDL = "sec_debug.level=";
	const char *CMD_SDE = "sec_debug.enable=";

	char *str;
	char cmdline[MAX_CMD_LINE_LEN];
	int prop_value;

#ifdef CONFIG_PROTOCOL_SIT
	prop_value = property_get_int32(VPROP_CRASH_MODE, -1);
	if (prop_value >= 0) {
		switch (prop_value) {
		case CRASH_MODE_DUMP_PANIC:
			debug_level = DBG_HIGH;
			break;
		case CRASH_MODE_DUMP_SILENT_RESET:
			debug_level = DBG_AUTO;
			break;
		case CRASH_MODE_SILENT_RESET:
			debug_level = DBG_LOW;
			break;
		default:
			break;
		}

		cbd_info("debug_level=%d overwritten by prop=%d\n", debug_level, prop_value);
		goto exit;
	}
#endif

	prop_value = property_get_int32(PROP_DEBUG_LEVEL, -1);
	if (prop_value >= 0) {
		cbd_info("%s, 0x%X\n", STR(PROP_DEBUG_LEVEL), prop_value);

		switch (prop_value) {
		case 0x4F4C: /*LOW - 0x4f4c*/
			debug_level = DBG_LOW;
			goto exit;
		case 0x4945: /*MID - 0x4945*/
			debug_level = DBG_MID;
			goto exit;
		case 0x4948: /*HIGH - 0x4948*/
			debug_level = DBG_HIGH;
			goto exit;
		default:
			cbd_info("%s, undefined value=0x%X\n", STR(PROP_DEBUG_LEVEL), prop_value);
			break;
		}
	}

	memset(cmdline, 0x00, MAX_CMD_LINE_LEN);
	str = Util::get_cmdline_str(cmdline, MAX_CMD_LINE_LEN, CMD_SDL);
	if (str) {
		if ( *(str + strlen(CMD_SDL)) - '0' != 0) {
			debug_level = DBG_MID;
			cbd_info("%s, %d\n", str, debug_level);
		}
		goto exit;
	}

	memset(cmdline, 0x00, MAX_CMD_LINE_LEN);
	str = Util::get_cmdline_str(cmdline, MAX_CMD_LINE_LEN, CMD_SDE);
	if (str) {
		if ( *(str + strlen(CMD_SDE)) - '0' != 0) {
			debug_level = DBG_MID;
			cbd_info("%s, %d\n", str, debug_level);
		}
		goto exit;
	}

exit:
	/* for "force_upload" mode */
	prop_value = property_get_int32(PROP_FORCE_UPLOAD, -1);
	if (prop_value >= 0 && debug_level == DBG_LOW) {
		cbd_info("force_upload=%d\n", prop_value);

		if (prop_value == 5) {
			debug_level = DBG_MID;
			cbd_info("force_upload on!\n");
		}
	}

	cbd_info("debug_level=%d, cp_debug=%d\n", debug_level, debug_cp_opt);
}

