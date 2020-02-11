#include "notify.h"

uint8_t cwdir[PATH_MAX] = {0};
uint8_t target_dir[PATH_MAX] = {0};
uint8_t target_mount[PATH_MAX] = {0};
int no_debug = 0;
int pid_info = 0;


static void usage(void)
{
  printf("usage: notify <options>\n");
  printf("\t --help\n");
  printf("\t --path the directory path to monitor (cwd if not specified)\n");
  printf("\t --mount the mount point to monitor (mutually exclusive to --path)\n");
  printf("\t --nodebug prevent this program from being run or attached to by a debugger\n");
  printf("\t --pidinfo attempt to collect data about the process acting on the file (system)\n");

  exit(EXIT_FAILURE);
}


static void get_options(int argc, char **argv)
{
  while (1)
  {
    int c = -1;
    char *end = NULL;

    static struct option long_options[] = {
      {"help", no_argument, NULL, 0},     /** 0 help **/
      {"path", required_argument, NULL, 1},
      {"mount", required_argument, NULL, 2},
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
      strncpy((char *)target_dir, optarg, PATH_MAX);
      break;
    }
    case 2:
    {
      strncpy((char *)target_mount, optarg, PATH_MAX);
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

    (void)end;
    if (! strnlen((const char *)target_dir, PATH_MAX))
    {
      strncpy((char *)target_dir, (char *)cwdir, PATH_MAX);
    }
  }
}


/**
 * Most notifications get to this point after the originating process
 * is dead. We still have the process id, but usually there is no
 * corresponding for it directory under /proc.
 *
 * When we do get notified for a running process, we can read files
 * from /proc/<pid>.
 **/
static void get_pid_info(int32_t pid)
{
  /**
   * static vars are initialized by the compiler to zero, just trying
   * to be literate.
   **/
  static uint8_t proc_buf[PROC_BUF_SIZE] = {0};
  static uint8_t file_buf[FILE_BUF_SIZE] = {0};
  snprintf((char *)proc_buf, PROC_BUF_SIZE, "/proc/%d", pid);
  snprintf((char *)file_buf, FILE_BUF_SIZE, "%s/cmdline", proc_buf);

  printf("pid: %d", pid);
  int proc_fd = open((const char *)file_buf, O_RDONLY | O_CLOEXEC);
  if (proc_fd == -1)
  {
    printf(" (expired)\n");
    return;
  }

  int bytes_read = read(proc_fd, file_buf, FILE_BUF_SIZE);
  if (bytes_read > 0)
  {
    printf("\ncommand line: %s\n", file_buf);
  }

  memset(proc_buf, 0x00, PROC_BUF_SIZE);
  memset(file_buf, 0x00, FILE_BUF_SIZE);

  return;
}

static void permission(int fd, const struct fanotify_event_metadata *data)
{
  struct fanotify_response response = {.fd = data->fd,
                                         .response = FAN_ALLOW};
  get_pid_info(data->pid);
  write(fd, &response, sizeof(struct fanotify_response));
  return;
}


static void read_poll(int fd)
{
  static struct fanotify_event_metadata poll_buf[POLL_BUF_SIZE] = {0};
  static uint8_t _path[PATH_MAX] = {0};
  static uint8_t file_path[PATH_MAX] = {0};

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

    if (pid_info && (metadata->mask & FA_PERM_MASK) &&
        (metadata->fd >= 0) && (metadata->pid >= 0))
    {
      permission(fd, metadata);
    }

    if (metadata->fd >= 0)
    {
      snprintf((char *)_path, PATH_MAX, "/proc/self/fd/%d", metadata->fd);
      size_t path_len = readlink((char *)_path, (char *)file_path, PATH_MAX - 1);
      if (path_len == -1) {
        perror("readlink");
        exit(EXIT_FAILURE);
      }

      file_path[path_len] = '\0';
      printf("File %s\n\n", file_path);
      close(metadata->fd);
    }
    metadata = FAN_EVENT_NEXT(metadata, len);
    memset(_path, 0x00, PATH_MAX);
    memset(file_path, 0x00, PATH_MAX);
  } /** while event ok **/
  memset(poll_buf, 0x00, sizeof(poll_buf));
  return;
}

int main(int argc, char **argv)
{

  char in_buf[16] = {0};

  if (NULL == getcwd((char *)cwdir, PATH_MAX))
  {
    perror("getcwd()");
    exit(EXIT_FAILURE);
  }

  get_options(argc, argv);
  printf("cwd: %s\n", cwdir);
  printf("target dir: %s\n", target_dir);
  printf("target mount: %s\n", target_mount);

  int notify_fd = fanotify_init(FA_INIT_FLAGS, FA_INIT_EVENT_FLAGS);
  if (notify_fd == -1)
  {
    perror("fanotify_init()");
    exit(EXIT_FAILURE);
  }
  printf("notify fd: %d\n", notify_fd);

  if (fanotify_mark(notify_fd,
                    FA_MARK_FLAGS,
                    FA_MARK_MASK,
                    AT_FDCWD,
                    (const char *)target_dir))
  {
    perror("fa_notify_mark()");
    exit(EXIT_FAILURE);
  }

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
