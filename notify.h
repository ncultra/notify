#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fanotify.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <time.h>

#define FA_INIT_FLAGS (FAN_CLOEXEC | FAN_CLASS_PRE_CONTENT | FAN_NONBLOCK)
#define FA_INIT_EVENT_FLAGS (O_RDONLY | O_LARGEFILE)

#define FA_MARK_FLAGS (FAN_MARK_ADD | FAN_MARK_FILESYSTEM)

#define FA_MARK_MASK (FAN_ACCESS | FAN_MODIFY | FAN_CLOSE | FAN_OPEN)
#define FA_PERM_MASK (FAN_ACCESS_PERM | FAN_OPEN_PERM | FAN_OPEN_EXEC_PERM | FA_MARK_MASK)

#define POLL_BUF_SIZE 2048
#define PROC_BUF_SIZE 32
/**
 * this is a hack because command lines can be too long to reasonably
 * store in a utility program buffer.
 **/
#define FILE_BUF_SIZE PATH_MAX
