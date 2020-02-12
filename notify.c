#include "notify.h"

char target_dir[PATH_MAX] = {0};
int no_debug = 0;
int pid_info = 0;
int perm = 0;
int provided_path = 0;

static void usage(void)
{
  printf("usage: notify <options>\n");
  printf("\t --help\n");
  printf("\t --path the directory path to monitor (\"/\" if not specified)\n");
  printf("\t --permission <allow | deny> grant or revoke access to <path>\n");
  printf("\t --nodebug prevent this program from being run or attached to by a debugger\n");
  printf("\t --pidinfo attempt to collect data about the process acting on the file (system)\n");

  exit(EXIT_FAILURE);
}


static void get_options(int argc, char **argv)
{
  while (1)
  {
    int c = -1;

    static struct option long_options[] = {
      {"help", no_argument, NULL, 0},     /** 0 help **/
      {"path", required_argument, &provided_path, 1},
      {"permission", required_argument, NULL, 2},
      {"nodebug", no_argument, &no_debug, 3},
      {"pidinfo", no_argument, &pid_info, 4},
      {0, 0, 0, 0}
    };

    int option_index = 0;
    c = getopt_long_only(argc, argv, "", long_options, &option_index);

    if (c == -1)
    {
      return;
    }
    switch(option_index)
    {
    case 0:
    default:
    {
      usage();
    }
    case 1:
    {
      strncpy(target_dir, optarg, PATH_MAX);
      break;
    }
    case 2:
    {
      /**
       * activate permission granting. This has two side-effects:
       * process ids are more likely to be valid and resident in the
       * /proc/ directory; and, performance will be slower when accessing
       * files, though probably not by very much.
       **/
      if (! strncasecmp("allow", optarg, 5))
      {
        perm = FAN_ALLOW;
      }
      else if (! strncasecmp("deny", optarg, 4))
      {
        perm = FAN_DENY;
      }
      break;
    }
    case 3:
    {
      /**
       * This is a trick that prevents gdb or another debugger from running
       * or attaching to this program. It does not prevent more sophisticated
       * attacks nor disassembly; it may be countered through binary patching.
       *
       * Once I make the ptrace call below, I am "debugging myself," and only
       * one debugger can attach to me at once, so this blocks out other debuggers.
       **/
      /** no_debug is set to 3 **/
      if (ptrace(PTRACE_TRACEME, 0, 1, 0 ) < 0)
      {
        printf("ptrace detected, exiting\n");
        exit(EXIT_FAILURE);
      }
      printf("debugging is disabled\n");

      break;
    }
    case 4:
    {
      /** pid_info is set to 4, no other action necessary **/
      break;
    }
    }
  }
}

/**
 * A simple string filter, returns non-zero if needle is in haystack,
 * zero otherwise. needle must be shorter or the same length as
 * haystack.
 **/
static int filter_path(const char *haystack, const char *needle)
{
  int ccode = 0;
  if ((haystack != NULL) && (needle != NULL))
  {
    size_t haystack_len = strnlen(haystack, PATH_MAX);
    if ((haystack_len > 0) && (haystack_len < PATH_MAX))
    {
      size_t needle_len = strnlen(needle, PATH_MAX);
      if ((needle_len > 0) &&
          (needle_len < PATH_MAX) &&
          (needle_len <= haystack_len))
      {
        if (strstr(haystack, needle) != NULL)
        {
          ccode = 1;
        }
      }
    }
  }
  return ccode;
}


/**
 * When get notified for a running process, we can read files
 * from /proc/<pid>.
 *
 * Most notifications get to this point after the originating process
 * is dead. We still have the process id, but usually there is no
 * corresponding for it directory under /proc.
 *
 * However, when using the --permission argument, /proc/<pid> will usually
 * be evaluated before the process is dead.
 *
 * This prints two artifacts from within /proc/<pid>. There is a lot
 * more that we could and store within this function given additional
 * time.
 **/
static void get_pid_info(int32_t pid)
{
  if (pid <= 0)
  {
    return;
  }
  int bytes_read = 0, proc_fd = -1;

  /**
   * static vars are initialized by the compiler to zero, just trying
   * to be literal with these.
   **/
  static char proc_buf[PROC_BUF_SIZE] = {0};
  static char file_buf[FILE_BUF_SIZE] = {0};
  snprintf(proc_buf, PROC_BUF_SIZE, "/proc/%d", pid);

  printf("pid: %d", pid);

  /**
   * print the process' command line
   **/
  snprintf(file_buf, FILE_BUF_SIZE, "%s/cmdline", proc_buf);
  proc_fd = open(file_buf, O_RDONLY);
  if (proc_fd == -1)
  {
    printf(" (expired)\n");
    return;
  }
  bytes_read = read(proc_fd, file_buf, FILE_BUF_SIZE);
  if (bytes_read > 0)
  {
    printf("\ncommand line: %s\n", file_buf);
  }
  close(proc_fd);

  /**
   * print the process' stack
   **/
  snprintf(file_buf, FILE_BUF_SIZE, "%s/stack", proc_buf);
  proc_fd = open(file_buf, O_RDONLY);
  if (proc_fd == -1)
  {
    printf(" (expired)\n");
    return;
  }
  bytes_read = read(proc_fd, file_buf, FILE_BUF_SIZE);
  if (bytes_read > 0)
  {
    printf("\nstack: %s\n", file_buf);
  }
  close(proc_fd);

  /**
   * clearing buffers is excessive but good practice for
   * sensitive programs.
   **/
  memset(proc_buf, 0x00, PROC_BUF_SIZE);
  memset(file_buf, 0x00, FILE_BUF_SIZE);

  return;
}

/**
 * grant or deny permission to access the file structure
 * being monitored.
 **/
static void permission(int fd,
                       const struct fanotify_event_metadata *data,
                       uint32_t action)
{
  struct fanotify_response response = {.fd = data->fd,
                                       .response = action};
  write(fd, &response, sizeof(struct fanotify_response));
  return;
}

/**
 * Read the file descriptor marked for content by the poll system call.
 * This function runs in a loop until it has read all the available
 * file monitoring events.
 *
 * POLL_BUF_SIZE is the number of notification metadata structures
 * that can be contained by the buffer. The API recommends making
 * the buffer large (on the order of a 4K page or more) so this
 * function can read more events for each signal.
 **/
static void read_poll(int fd)
{
  static struct fanotify_event_metadata poll_buf[POLL_BUF_SIZE] = {{0}};
  static char _path[PATH_MAX] = {0};
  static char file_path[PATH_MAX] = {0};

  size_t len = read(fd, (void *)poll_buf, POLL_BUF_SIZE);
  if (len == -1 && errno != EAGAIN)
  {
    perror("read");
    exit(EXIT_FAILURE);
  }
  if (len <= 0)
  {
    return;
  }

  const struct fanotify_event_metadata *metadata = poll_buf;
  while (FAN_EVENT_OK(metadata, len))
  {
    if (metadata->vers != FANOTIFY_METADATA_VERSION) {
      fprintf(stderr, "Mismatch of fanotify metadata version.\n");
      exit(EXIT_FAILURE);
    }

    /**
     * don't monitor ourselves
     **/
    if ((metadata->fd >= 0) && (metadata->pid != getpid()))
    {
      /**
       * marshall the file path and then see if it passes the filter
       **/
      snprintf(_path, PATH_MAX, "/proc/self/fd/%d", metadata->fd);
      size_t path_len = readlink(_path, file_path, PATH_MAX - 1);
      if (path_len == -1) {
        perror("readlink");
        exit(EXIT_FAILURE);
      }
      file_path[path_len] = '\0';

      /**
       * see if it is caught by the filter
       **/
      if (filter_path(file_path, target_dir))
      {
        printf("\n%s\n", file_path);
        printf("access mask: %016llx\n", metadata->mask);
        if (pid_info)
        {
          get_pid_info(metadata->pid);
        }

        if ((metadata->mask & FA_PERM_MASK) && (metadata->fd >= 0))
        {
          permission(fd, metadata, perm);
        }
      }

      /**
       * if permission mask, allow access for all that pass
       * the filter.
       **/
      if ((metadata->mask & FA_PERM_MASK) && (metadata->fd >= 0))
      {
        permission(fd, metadata, FAN_ALLOW);
      }
      close(metadata->fd);
      memset(_path, 0x00, PATH_MAX);
      memset(file_path, 0x00, PATH_MAX);
    }
    metadata = FAN_EVENT_NEXT(metadata, len);
  } /** while event ok **/
  memset(poll_buf, 0x00, sizeof(poll_buf));
  return;
}

int main(int argc, char **argv)
{

  /**
   * this small buffer is to process one byte at a time
   * from the terminal. 16 bytes is 64 bits, which is an
   * efficient size for a static variable.
   **/
  static char in_buf[16] = {0};

  get_options(argc, argv);
  if (! provided_path)
  {
    printf("--path is a required option\n");
    usage();
  }
  printf("target dir: %s\n", target_dir);

  int notify_fd = fanotify_init(FA_INIT_FLAGS, FA_INIT_EVENT_FLAGS);
  if (notify_fd == -1)
  {
    perror("fanotify_init()");
    exit(EXIT_FAILURE);
  }

  uint64_t mask = 0;
  if (perm != 0)
  {
    mask = FA_PERM_MASK;
  }
  else
  {
    mask = FA_MARK_MASK;
  }

  if (fanotify_mark(notify_fd,
                    FA_MARK_FLAGS,
                    mask,
                    AT_FDCWD,
                    (const char *)target_dir))
  {
    perror("fa_notify_mark()");
    exit(EXIT_FAILURE);
  }

  /**
   * The fanotify interface allows a lot of flexibility through the use of
   * multiple file discriptors "marked" for different notifications. Here
   * we are only using a single marked file descriptor for notifications.
   * The other descriptor is stdandard in and causes the program to exit
   * when it reads a \r.
   **/
  int poll_event = 0;
  nfds_t poll_fds = 2;
  struct pollfd fds[2]= {{0},{0}};
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;
  fds[1].fd = notify_fd;
  fds[1].events = POLLIN;
  while(1)
  {
    poll_event = poll(fds, poll_fds, -1);
    if (poll_event == -1)
    {
      if (errno == EINTR)
      {
        continue;
      }
      perror("poll()");
      exit(EXIT_FAILURE);
    }

    if (poll_event > 0)
    {
      if (fds[0].revents & POLLIN)
      {
        while (read(STDIN_FILENO, in_buf, 1) > 0 && in_buf[0] != '\n')
        {
          continue;
        }
        break;
      }
      if (fds[1].revents & POLLIN)
      {
        read_poll(notify_fd);
      }
    }
  }
  close(notify_fd);

  printf("exiting ...\n");
  return 0;
}
