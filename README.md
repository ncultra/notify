# notify

Demonstration of the Linux `fanotify` interface.

Tested with GCC 4.8.5 on Linux v3.10 and GCC 8.3.1 on Linux v5.2.11. It should be OK with GCC and Linux versions in between these two.

### Building and Running

From within the notify directory:

```
$ make
repo is clean
gcc -c -g  -Wall -Werror -fPIC -std=gnu11 -fms-extensions -I. -I/usr/include -o notify.o notify.c
gcc -static -static-libgcc -o notify notify.o
```

```
$ ./notify
--path is a required option
usage: notify <options>
         --help
         --path the directory path to monitor
         --permission <allow | deny> grant or revoke access to <path>
         --nodebug prevent this program from being run or attached to by a debugger
         --pidinfo attempt to collect data about the process acting on the file (system)
```
The Linux `fanotify` API requires supervisor privileges. If you run it without supervisor privileges, you will see an error message similar to this:

```
$ ./notify --path ~/src/
target dir: /home/user/src/
fanotify_init(): Operation not permitted
```
When run with sufficient privileges, `notify` will pause on the screen and dump information about file system accesses. When you press `Enter`, it will quit.

```
$ sudo ./notify --path ~/src --pidinfo --permission allow
target dir: /home/user/src

/home/user/src/pie-crust/pie-crust.c
access mask: 0000000000010000
pid: 7391
command line: cat

stack: [<0>] fsnotify+0x29f/0x3c0
[<0>] do_dentry_open+0xd6/0x380
[<0>] path_openat+0x2c5/0x13b0
[<0>] do_filp_open+0x93/0x100
[<0>] do_sys_open+0x186/0x220
[<0>] do_syscall_64+0x5f/0x1a0
[<0>] entry_SYSCALL_64_after_hwframe+0x44/0xa9
exiting ...
```

