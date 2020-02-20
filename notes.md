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

### Attacking the famonitor interface

Each monitoring program can have multiple file handles via `fanotify_init()` and each of those file handles can have multiple monitoring hooks via `fanotify_mark()`. Each of these hooks gets passed to userspace via a call to `read()`. This may represent an opportunity for a denial-of-service attack by a file-intensive application that repeatedly forks itself.  
 
Due to time constraints, I used `glibc`'s standard library (static) for string parsing, reading, and comparison. There is an opportunity to attack the interface using very large (4k or so) filenames or pathnames with illegal characters, escape and control characters, etc., to break the parsing and reading code.

Chosen file or path names may present a buffer overfow opportunity along with everything that goes with a buffer overflow. For this reason, if this were production code I would read and parse pathnames much more carefully, a few characters at a time if need be.

I would check each library and kernel version for known vulnerabilities and try to find a way to exploit them using filenames and pathnames.

If I was able to gain write access to memory storing the `notify` program (by mapping anonymous pages) I would not break it, but I would make small changes to the code that would allow my own file access to go unreported. I could do this by live-patching the `--path` filter, for one example.

The 5.x series kernels have provide the `fanotify` API with more op tions, including the ability to delve into opaque file handle structures. I would see if I could create a file handle that would break the monitoring code.

The build process becomes frozen when I build `notify` while at the same time monitoring the build directory. As soon as I exit the monitoring program, the build process continues without error. I didn't have time to explore this but there may be an attack vector related to it. 

### Countering Attacks

In the short amount of time I spent on this project, I only did a couple of things to deny obvious attacks. First, I statically link the program, which prevents an attacker from using a compromised dynamic loader/linker (to link to a chosen DLL.)

I provided the `--nodebug` option which will cause `notify` to `ptrace` itself. This prevents a "debugger" from attaching to the program while it is running and breakpointing or editing the program.

All the memory buffers I use are statically allocated. This increases the size of the program but makes it much harder to use memory heap-based attacks that arise from bugs in using library memory heaps or bugs in library memory heaps themselves.

All the ways of preventing binary editing of the program file at rest or while running would be desireable to prevent attacks.

### Production Use

For production use, I would first convert the program into a background service and I would use discrete filter masks rather than one aggregate filter mask. I would make the default output format JSON and send all output to log files *and* one or more sockets. This is just an interface demonstration as it is now.
