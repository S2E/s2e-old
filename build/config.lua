s2e = {
	kleeArgs = {
	-- Run each state for at least 10 seconds before
    	-- switching to the other:
    	"--use-batching-search=true", "--batch-time=10.0"
	}
}


plugins = {
	-- Enable a plugin that handles S2E custom opcode
	"BaseInstructions",
	"ModuleExecutionDetector",	
	"ModuleTracer",	
	"TranslationBlockTracer",	
	"ExecutionTracer",	
	"AndroidMonitor",
	"AndroidAnnotation",
	"TransmissionObserver",
	"LinuxMonitor",
	"InstructionCounter",
	"FunctionMonitor"
}

pluginsConfig = {

}

pluginsConfig.ModuleExecutionDetector = {
	trackAllModules = true,	
	-- configureAllModules = true
}

pluginsConfig.AndroidMonitor = {
	app_process_name = "ch.epfl.s2e.android"
}

pluginsConfig.AndroidAnnotation = {
	unit = {
		method1 = "Lch/epfl/s2e/android/LeaActivity;.testAutoSymbexInts",
		method2 = "Lch/epfl/s2e/android/LeaActivity;.testAutoSymbexDoubles",
		method3 = "Lch/epfl/s2e/android/LeaActivity;.testAutoSymbexChars",	
		method4 = "Lch/epfl/s2e/android/LeaActivity;.testAutoSymbexFloats"
	}
}

pluginsConfig.LinuxMonitor = {
    
    prelink_file = "../android-misc/prelink-linux-arm.map",
    system_map_file = "../android-misc/System.map",
    track_vm_areas = true,
    threadsize = 8192,
    symbols = {
        start_thread = 0xc00980a8
    },
    offsets = {
        task_comm = 0x2d4,
        task_pid = 0x1ec,
	task_tgid = 0x1f0,   
     	task_mm = 0x1cc,
	task_next = 0x1c0,
	thread_info_task = 0xc,
        mm_code_start = 0x78,
        mm_code_end = 0x7c,
        mm_data_start = 0x80,
        mm_data_end = 0x84,
        mm_heap_start = 0x88,
        mm_heap_end = 0x8c,
        mm_stack_start = 0x90,
        vmarea_start = 0x4,
        vmarea_end = 0x8,
        vmarea_next = 0xc,
        vmarea_file = 0x48,
	file_dentry = 0xc,	
	dentry_name = 0x24
     } 
}
