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

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <ctype.h>
#include <inttypes.h>
#include <dirent.h>
#include <ftw.h>
#include <grp.h>

#include <cutils/properties.h>
#include <selinux/android.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <sys/klog.h>

#include "Type.h"
#include "Util.h"
#include "Version.h"

#define DEBUG_RADIO_MSG
#define DEBUG_KERNEL_MSG

#define __STR(x)	#x
#define STR(x)		__STR(x)

#define ERR2STR		strerror(errno)
#define FMT_ERR		"cbd: (ERR! %s) %s: "
#define FMT_INFO	"cbd: %s: "
#define FMT_KERN_ERR	"<3>cpif: cbd: (ERR! %s) %s: "
#define FMT_KERN_INFO	"<6>cpif: cbd: %s: "

#ifdef DEBUG_RADIO_MSG
#include <log/log.h>
#define CBD_ID		LOG_ID_RADIO
#define CBD_TAG		"boot"
#define PRI_ERR		ANDROID_LOG_ERROR
#define PRI_DBG		ANDROID_LOG_DEBUG

#define __cbd_err(s, args...) \
		__android_log_buf_print(CBD_ID, PRI_ERR, CBD_TAG, FMT_ERR s, ERR2STR, __func__, ##args)
#define __cbd_info(s, args...) \
		__android_log_buf_print(CBD_ID, PRI_DBG, CBD_TAG, FMT_INFO s, __func__, ##args)
#define __cbd_kernel_err(s, args...) \
		dprintf(Log::kmsg_fd, FMT_KERN_ERR s, ERR2STR, __func__, ##args)
#define __cbd_kernel_info(s, args...) \
		dprintf(Log::kmsg_fd, FMT_KERN_INFO s, __func__, ##args)
#else
#define __cbd_err(s, args...)	printf(FMT_ERR s, ERR2STR, __func__, ##args)
#define __cbd_info(s, args...)	printf(FMT_INFO s, __func__, ##args)
#define __cbd_kernel_err(s, args...)	printf(FMT_KERN_ERR s, ERR2STR, __func__, ##args)
#define __cbd_kernel_info(s, args...)	printf(FMT_KERN_INFO s, __func__, ##args)
#endif

#define __cbd_dump_info(s, args...) \
	do { \
		if (Container::getStdDump()->fds[FD_LOG] < 0) \
			break; \
		dprintf(Container::getStdDump()->fds[FD_LOG], "%s: " s, __func__, ##args); \
	} while (0)

#define __cbd_dump_err(s, args...) \
	do { \
		if (Container::getStdDump()->fds[FD_LOG] < 0) \
			break; \
		dprintf(Container::getStdDump()->fds[FD_LOG], "(ERR! %s) %s: " s, ERR2STR, __func__, ##args); \
	} while (0)

#define cbd_info(s, args...) \
	do { \
		__cbd_kernel_info(s, ##args); \
		__cbd_info(s, ##args); \
	} while (0)

#define cbd_err(s, args...) \
	do { \
		__cbd_kernel_err(s, ##args); \
		__cbd_err(s, ##args); \
	} while (0)

#define cbd_dump_info(s, args...) \
	do { \
		cbd_info(s, ##args); \
		__cbd_dump_info(s, ##args); \
	} while (0)

#define cbd_dump_err(s, args...) \
	do { \
		cbd_err(s, ##args); \
		__cbd_dump_err(s, ##args); \
	} while (0)

enum TYPE_LOG {
	LOG_DMESG,
	LOG_DUMPSTATE,
	LOG_BOOT_FAIL,
};

#define LOGB_DMESG		(0x1 << LOG_DMESG)
#define LOGB_DUMPSTATE		(0x1 << LOG_DUMPSTATE)
#define LOGB_BOOTFAIL		(0x1 << LOG_BOOT_FAIL)

/*
 * kprintf - kernel printf
 *
 * Printout message to kmsg for syncing with radio log
 * if not defined BOOT_KERNEL_MSG, dprintf(kmsg_fd, fmt ...) will be printout to
 * STDOUT
 */
#define kprintf(fmt) dprintf(Log::kmsg_fd, fmt)
#define KLOG_BUF_SHIFT	19	/* CONFIG_LOG_BUF_SHIFT from our kernel */
#define KLOG_BUF_LEN	(1 << KLOG_BUF_SHIFT)

struct save_logs_arg {
	int type;
	char *prefix;
};

#ifdef CONFIG_PROTOCOL_SIT
enum crash_handling_mode {
	CRASH_MODE_DUMP_PANIC = 0,		/* kernel panic after dump */
	CRASH_MODE_DUMP_SILENT_RESET,		/* silent reset after dump */
	CRASH_MODE_SILENT_RESET,		/* only silent reset */
	CRASH_MODE_MAX,
};
#endif

class Log {
private:
	Log() {}

public:
	static void kprintf_init(void);
	static void kprintf_deinit(void);

	static char *get_log_dir(void);
	static void update_log_dir(void);
	static int create_log_directory(void);
	static int create_directory(const char *path);

	static void save_logs_thread(int type, char *prefix);
	static void save_logs(int type, char *prefix);

	static const char *get_cbd_version(void);
	static void check_debug_cp_opt(void);
	static void check_debug_level();
#ifdef CONFIG_DUMP_LIMIT
	static void organize_dump_files();
	static int remove_directory(char *path);
#endif

private:
	static int check_fs_log_directory(void);
	static int cpboot_log_needed(void);
	static void *_save_logs(void *arg);
	static int dmesg_to_file(char *of);
#ifdef CONFIG_DUMP_LIMIT
	static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
#endif

public:
	static inline int kmsg_fd = STDOUT_FILENO;
	static inline enum sec_debug_level debug_level = DBG_LOW;
	static inline enum sec_cp_debug debug_cp_opt = DBG_CP_NORMAL;

private:
	/* full "pathname" of log directory */
	static inline char log_path[MAX_PATH_LEN] = CPDUMP_PATH;
	static inline const char *cmd_name[] = {
		[LOG_DMESG]	= "dmesg",
		[LOG_DUMPSTATE]	= "dumpstate",
	};
};

