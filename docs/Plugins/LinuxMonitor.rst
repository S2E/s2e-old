============
LinuxMonitor
============

The LinuxMonitor plugin automatically detects process creation and fires corresponding module load events.
Optionally it will also treat and track virtual memory areas belonging to the process as modules (e.g. loaded
libraries).

Options
-------
In order to function properly, the LinuxMonitor requires a number of Linux kernel symbol addresses and offsets
within kernel data structure objects.
LinuxMonitor has two important sections, 'symbols' and 'globals' that need to be filled with the necessary values.

Symbols
~~~~~~~
Here go the kernel symbols. The addresses can be determined by grepping the matching System.map file:

- do_execve
- start_thread
- init_task


Offsets
~~~~~~~
Here go the offsets within some kernel data structures. They can be determined by using gdb on the kernel (vmlinux)
file, e.g.
::
    
    printf "\tpid: 0x%x\n", ((size_t)&((struct task_struct *)0)->pid)

to print the offset of the 'pid' field within the 'task_struct' struct. Required offsets are:

- task_comm - offset of task_struct.comm
- task_pid - offset of task_struct.pid
- task_mm - offset of task_struct.mm, points to the corresponding mm_struct
- task_next - offset of task_struct.tasks.next, point to the next task_struct
- mm_code_start - offset of mm_struct.start_code
- mm_code_end - offset of mm_struct.end_code
- mm_data_start - offset of mm_struct.start_data
- mm_data_end - offset of mm_struct.end_data
- mm_heap_start - offset of mm_struct.start_brk
- mm_heap_end - offset of mm_struct.brk
- mm_stack_start - offset of mm_struct.start_stack
- vmarea_start - offset of vm_area_struct.vm_start
- vmarea_end - offset of vm_area_struct.vm_end
- vmarea_next - offset of vm_area_struct.vm_next
- vmarea_file - offset of vm_area_struct.vm_file
- file_dentry - offset of file.f_path.dentry
- dentry_name - offset of dentry.d_name.name

track_vm_areas=[true|false]
~~~~~~~~~~~~~~~~~~~~~~~~~~~
Specifies whether virtual memory areas belonging to a process (e.g. loaded libraries) shall also be treated and tracked
like modules.


Configuration Sample
--------------------

::

    pluginsConfig.LinuxMonitor = {
        track_vm_areas = true,
        symbols = {
            do_execve = 0xc10aff49,
            do_exit = 0xc1030bd0,
            start_thread = 0xc1001960,
            init_task = 0xc161ee60
        },
        offsets = {
            task_comm = 0x300,
            task_pid = 0x20c,
            task_mm = 0x1ec,
            task_next = 0x1d0,
            mm_code_start = 0x80,
            mm_code_end = 0x84,
            mm_data_start = 0x88,
            mm_data_end = 0x8c,
            mm_heap_start = 0x90,
            mm_heap_end = 0x94,
            mm_stack_start = 0x98,
            vmarea_start = 0x04,
            vmarea_end = 0x08,
            vmarea_next = 0x0c,
            vmarea_file = 0x48,
            file_dentry = 0xc,
            dentry_name = 0x28
        }
    }

