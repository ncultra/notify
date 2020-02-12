# Notes

### Short-lived Linux processes

The `fanotify` API faithfully provides a process ID for each file system access, even if that process has terminated 
prior to delivering the notification. When this happens, `notify` captures and prints the process id but can't 
gather any information about the process via the `/proc/<pid>` directory.

*However*, when using the `--permissions <allow | deny>` option, `notify` prevents *most* processes from terminating
until it is able to gather information about the process from `/proc/<pid>`. The cost of this ability is to slow down file system access. The benefit is the ability to harvest and save data from the `/proc` filesystem. To try this,
add `--permissions allow` to the `notify` command line.

The most reliable way to guarantee 100% access to short-lived processes is to pair `notify` with a kernel module that uses `kprobes` to hook file system accesses and then harvests process data directly from the kernel.

Another method is to write `ebpf` bytecode that does the same thing. You can inject this bytecode into the Linux kernel and it
will hook kprobes. The [bcc project](https://github.com/iovisor/bcc) does this and is a good place to start. The ability to extract data from the kernel is limited and any monitoring actions need to be bytecode-safe as determined by the `ebpf` virtual machine.



