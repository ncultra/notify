# Notes

### Short-lived Linux processes

The `fanotify` API faithfully provides a process ID for each file system access, even if that process has terminated 
prior to delivering the notification. When this happens, `notify` captures and prints the process id but can't 
gather any information about the process via the `/proc/<pid>` directory.

*However*, when using the `--permissions <allow | deny>` option, `notify` prevents most processes from terminating
until it is able to gather information from `/proc/<pid>`. The cost if this ability is to slow down file system access,
but not materially so. But the benefit is the ability to 
