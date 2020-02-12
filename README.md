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
