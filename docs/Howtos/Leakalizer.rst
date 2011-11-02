=========================================
How to use Leakalizer?
=========================================

This tutorial describes how to "leakalize", i.e., how to use Leakalizer to detect data leaks. This tutorial answers the following questions:

* What is Leakalizer?
* I want a quick start. How?
* How do I compile Leakalizer?
* How do I boot Android in Leakalizer?
* How do I configure the Leakalizer plugins?
* What is the 'Send UID' benchmark?
* What are the 'Auto-Symbex' benchmarks?
* What is the 'Symbolic Maze' benchmark?

The tutorial has been tested with the following configuration:
* Ubuntu 10.10 (Probably it will work with other Ubuntu versions and Linux distributions with little modifications)
* Android SDK (you will need it to connect to the virtual smartphone with ADB, the Android Debugger Bridge that is included into the Android SDK)

What is Leakalizer?
===================

Leakalizer detects data leaks inside Android applications. Initially, it was called S2EAndroid, because one component is a modified version of S2E (the ARM port of S2E). On top of the modified S2E, Leakalizer plugins detect events from the modified Android system to symbolically execute one application of interest and to detect simple data leaks.

Leakalizer was initially built by Andreas Kirchner (akalypse@gmail.com) during his master thesis ("Data Leak Detection in Smartphone applications") - with the great help of the S2E team.

I want a quick start. How?
==========================
If you quickly want to tryout Leakalizer:

1. Get the source code of Leakalizer
2. Compile: ::

        export LEAKALIZER="/path/to/leakalizer"
		cd $LEAKALIZER/build
		make all
		
	Make might complain (permission denied...), then you have to give some files executable rights (chmod +x <file): ::
	
		chmod +x ../android-emulator/android/tools/gen-hw-config.py
		chmod +x ../android-emulator/hxtool
		chmod +x ../android-emulator/feature_to_c.sh
		chmod +x ../android-emulator/android/build/mkdeps.sh
		chmod +x ../android-emulator/android-configure.sh
		chmod +x ../klee/configure
		chmod +x ../stp/src/AST/genkinds.pl
	
3. Start Leakalizer with a pre-built snapshot: ::

	$LEAKALIZER/build/startLeakalizer.sh
4. Click on "Leakalizer Eval App" in the Launcher of the GUI.
5. Open a new terminal window on your host computer to open a socket listening at localhost port 6667: ::

	socket -sl 6667 (sudo apt-get install socket, if 'socket' is not installed on your Ubuntu)
	
6. Click on 'Send UID' and wait until Leakalizer has executed the application code for each state and the application idles. If you see something like this in the terminal, Leakalizer is finished (it switches between the execution states without performing interesting stuff): ::

	275 [State 2] Switching from state 2 to state 0
	275 [State 2] Switched to symbolic execution at pc = 0xc002b424
	275 [State 0] Switched to concrete execution at pc = 0xafd0e1dc
	286 [State 0] Switching from state 0 to state 1
	286 [State 0] Switched to symbolic execution at pc = 0xc002b424
	286 [State 1] Switched to concrete execution at pc = 0xc002b424
	297 [State 1] Switching from state 1 to state 2
	297 [State 1] Switched to symbolic execution at pc = 0xafd0e1dc
	297 [State 2] Switched to concrete execution at pc = 0xc002b424
	308 [State 2] Switching from state 2 to state 1
	308 [State 2] Switched to symbolic execution at pc = 0xc002b424
	308 [State 1] Switched to concrete execution at pc = 0xafd0e1dc
	319 [State 1] Switching from state 1 to state 0
	319 [State 1] Switched to symbolic execution at pc = 0xafd0e1dc
	319 [State 0] Switched to concrete execution at pc = 0xc002b424

7. Look at the Log file to see if Leakalizer has found the data leak: ::

	grep -Ri "data leak" $LEAKALIZER/build/s2e-last/messages.txt

You should see something like this: ::

	Data leak of type v_deviceid1 detected.
	DATA LEAK to destination 10.0.2.2 and port 6667.
	Data leak of type v_deviceid1 detected.
	Data leak of type v_deviceid1 detected.
	DATA LEAK to destination 10.0.2.2 and port 6667.

10.0.2.2 inside the Android emulator point to localhost of your host computer.

8. Close Leakalizer and try out other benchmarks: Repeat the steps 3 and 4, then jump to descriptions of other benchmarks in this tutorial.

If you want to know more about Leakalizer, see below.

How do I compile Leakalizer?
============================

Let's assume that you have the source code of Leakalizer. Create the environment variable $LEAKALIZER which contains the path to the source code and run make. ::

        export LEAKALIZER="/path/to/leakalizer"
	cd $LEAKALIZER/build
	make all	(or make all-debug)
	
This will take a while. Normally, the vanilla Android emulator compiles two times, for X86 and for ARM target. The build infrastructure of Leakalizer is ready to compile each target for S2E-enabled and S2E-disabled, so it would compile four times. We have disabled X86 because we currently do not use it (It compiles but we have not tested anything with it). 

If you want to test Leakalizer with X86, uncomment the following two lines in $LEAKALIZER/android-emulator/Makefile.android: ::

	# EMULATOR_TARGET_ARCH := x86
	# include $(LOCAL_PATH)/Makefile.target

When compilation is finished, you should have at least the following subdirectories in the build-directory: ::
	
	build
	|	
	|-- android-release (or: android-debug)
	|   |
	|   |--	objs		(contains the S2E-enabled version of Leakalizer, in short: Android emulator)
	|   `-- objs-nos2e	(containts the S2E-disabled version of Leakalizer, in short: Leakalizer)
	|	
	|-- klee
	|-- llvm
	|-- tools
	|-- stp

The S2E-disabled version (Android emulator) is faster. Use it to boot Android, install Android applications and prepare everything for the analysis.
The S2E-enabled version (Leakalizer) adds S2E-instrumentation to the Android emulator. Use it to perform the analysis.

In the following, we describe how to use Leakalizer.

How do I boot Android in Leakalizer?
====================================

If you do not have a snapshot perform the following steps:

1. Boot Android in Android-emulator
2. Setup the system, install applications,...
3. Create a snapshot.
4. Load the snapshot in Leakalizer.

If you have a snapshot (Leakalizer has already one snapshot named 'start1'), then you only need step 4.


1. Boot Android in Android emulator
-----------------------------------

You can use the bash script $LEAKALIZER/build/startEmulator.sh or copy/paste the commands below: ::

	cd $LEAKALIZER/build/android-release/objs-nos2e
	./emulator 						 		\
	-no-snapshot-save 					 		\
	-no-snapshot-load 					 		\
	-noskin							 		\
	-sysdir $LEAKALIZER/android-images/ledroid.avd/system 			\
	-datadir $LEAKALIZER/android-images/ledroid.avd 			\
	-kernel $LEAKALIZER/android-images/ledroid.avd/system/zImage 		\
	-data $LEAKALIZER/android-images/ledroid.avd/userdata.img 		\
	-snapstorage $LEAKALIZER/android-images/ledroid.avd/snapshots.img 	\
	-sdcard $LEAKALIZER/android-images/ledroid.avd/sdcard.img 		\
	-memory 256 								\
	-show-kernel  								\
	-verbose  								\
	-qemu -monitor stdio 

Find a description of the parameters here: https://developer.android.com/guide/developing/tools/emulator.html

After a while, the Android emulator should appear as a Window on the screen.

2. Setup the system, install applications,...
---------------------------------------------
You can use the Android emulator like you use it in the Android SDK or your real Android phone. You can install/deinstall applicaitons, connect to the system via "adb shell", etc. 

Let us test Leakalizer with LEA - the Leakalizer Evaluation Application. It displays available microbenchmarks and performs them with a button click. ::

	cd $LEAKALIZER/android-apps/lea
	adb install -r lea.apk

The "-r" switch forces reinstallation of the application, if it is already installed.

You should see an Icon named "Leakalizer Eval App". Move the icon to the desktop for faster access.

If you like the console, try: ::

	(host shell ) adb shell
	(guest shell) cd data/app
	(guest shell) ls

There should be a file named "ch.epfl.s2e.android-1.apk", which is the Android application you have just created.  

That's it. This state will be used later by Leakalizer. Now we have to create a snapshot.

(For later: If you want to uninstall an application, you need the package name of the application, in the case of LEA: adb uninstall ch.epfl.s2e.android )

3. Create a snapshot
--------------------

To create a snapshot of the system, type: ::

	telnet 127.0.0.1 5554
	avd snapshot save start1

"start1" is the name of the snapshot. To view a list of all stored snapshots, type: ::

	avd snapshot list

4. Load the Snapshot with Leakalizer
------------------------------------

You can use the bash script $LEAKALIZER/build/startLeakalizer.sh or copy/paste the commands below: ::

	cd $LEAKALIZER/build/android-release/objs

	./emulator 						 		\
	-snapshot start1                                                        \
        -no-snapshot-save                                                       \
        -noskin                                                                 \
        -sysdir $LEAKALIZER/android-images/ledroid.avd/system                   \
        -datadir $LEAKALIZER/android-images/ledroid.avd                         \
        -kernel $LEAKALIZER/android-images/ledroid.avd/system/zImage            \
        -data $LEAKALIZER/android-images/ledroid.avd/userdata.img               \
        -snapstorage $LEAKALIZER/android-images/ledroid.avd/snapshots.img       \
        -sdcard $LEAKALIZER/android-images/ledroid.avd/sdcard.img               \
        -memory 256                                                             \
        -show-kernel                                                            \
        -verbose                                                                \
        -qemu -monitor stdio                                                    \
        -L $LEAKALIZER/android-emulator/pc-bios                                 \
        -s2e-config-file $LEAKALIZER/build/config.lua                           \
        -s2e-verbose

How do I configure the Leakalizer plugins?
==========================================
Basically, everything is pre-configurized if you want to perform the Leakalizer benchmarks that are included into LEA (Leakalizer Evaluation App).

To make your own experiments, have a look at the file $LEAKALIZER/build/config.lua.

Here is one example: In order to make Leakalizer focus on a particular application, you need to know the process_name of the application, which is the package-name in most cases: ::

	pluginsConfig.AndroidMonitor = {
        	app_process_name = "ch.epfl.s2e.android"
	}


There are some other configuration options that you can change. You can discover these on your own.


What is the 'Send UID' benchmark?
==========================================

Continue to the quick-start chapter at the beginning of this tutorial.

What are the 'Auto-Symbex' benchmarks?
==============================================
The Auto-Symbex benchmark shows how to systematically explore execution paths of a Java method inside an Android application. We call this "auto-symbex".
Up to now, if you want to auto-symbex a method, you have to specify the unique method descriptor in the config file (config.lua). For LEA, this looks as follows: ::

	pluginsConfig.AndroidAnnotation = {
		unit = {
		        method1 = "Lch/epfl/s2e/android/LeaActivity;.testAutoSymbexInts",
		        method2 = "Lch/epfl/s2e/android/LeaActivity;.testAutoSymbexDoubles",
		        method3 = "Lch/epfl/s2e/android/LeaActivity;.testAutoSymbexChars",
		        method4 = "Lch/epfl/s2e/android/LeaActivity;.testAutoSymbexFloats"
		}
	}

The syntax of the method descriptor is the one used by the Dalvik VM inside Android: ::

	L<package>/<ActivityName>;.<methodName>

Whenever a method defined in the configuration file is executed, the concrete values of the parameters of these methods are automatically replaced by symbolic values. Note, that Leakalizer only supports primitive datatypes for now (int, float, boolean, char, ...).

LEA provides three benchmarks to test auto-symbex for different data types. If you compare them, you will notice that floating point operations (float and double) need more execution states due to the way how they are represented and handled by Leakalizer and the Dalvik interpreter.

If you use the benchmark "auto-symbex for integers", Leakalizer executes the following method: ::

	private static void testAutoSymbexInts(boolean ok, int x, int y) {
	    if (ok) {
	        if (x == y) {
	            S2EAndroidWrapper.killState(0, "(int) z: x == y");
	        } else {
	            S2EAndroidWrapper.killState(1, "(int) z: x != y");
	        }
	    } else {
            if (x == y) {
                S2EAndroidWrapper.killState(2, "(int) !z: x == y");
            } else {
                S2EAndroidWrapper.killState(3, "(int) !z: x != y");
            }	        
	    }
	}

This method does not do anything except killingn the state whenever Leakalizer executes the body of an inner branch. This allows us to find out whether Leakalizer has achieved 100% code coverage, i.e., has explored all possible execution paths of this method.

After a moment, Leakalizer will close and you can inspect the result in the log files and see if there are four different messages: ::

	grep -RiA2 "Terminating state" $LEAKALIZER/build/s2e-last/messages.txt

It should result in the following: ::

	[State 1] Terminating state 1 with message 'State was terminated by opcode
		   message: "(int) z: x == y"
		   status: 0'
	--
	[State 0] Terminating state 0 with message 'State was terminated by opcode
		    message: "(int) !z: x == y"
		    status: 2'
	--
	[State 2] Terminating state 2 with message 'State was terminated by opcode
		    message: "(int) z: x != y"
		    status: 1'
	--
	[State 3] Terminating state 3 with message 'State was terminated by opcode
		    message: "(int) !z: x != y"
		    status: 3'

The other auto-symbex-benchmarks are similar. 



What is the 'Symbolic Maze' benchmark?
===============================================

The Symbolic Maze Benchmark makes Leakalizer to explore a textual labyrinth and find all four solutions to the treasure. (Details: https://feliam.wordpress.com/2010/10/07/the-symbolic-maze/ ) ::

	X ... path of the player
	# ... treasure
	| ... wall or hidden way :)
	+ ... wall
	- ... wall



Start: ::

	+-+---+---+
	|X|     |#|
	| | --+ | |
	| |   | | |
	| +-- | | |
	|     |   |
	+-----+---+



Solutions: ::

                +-+---+---+                +-+---+---+
                |X|XXXXX|#|                |X|XXXXX|#|
                |X|X--+X|X|                |XXX--+X|X|
                |X|XXX|X|X|                | |   |X|X|
                |X+--X|X|X|                | +-- |X|X|
                |XXXXX|XXX|                |     |XXX|
                +-----+---+		   +-----+---+

                +-+---+---+                +-+---+---+
                |X|XXXXX|#|                |X|XXXXX|#|
                |X|X--+XXX|                |XXX--+XXX|
                |X|XXX| | |                | |   | | |
                |X+--X| | |                | +-- | | |
                |XXXXX|   |                |     |   |
                +-----+---+                +-----+---+


Actions (represented as integers from 0-3): ::

	0:	UP
	1:	DOWN
	2:	LEFT
	3:	RIGHT


The benchmarks takes approx. 10-15 minutes. Look at the log file (message.txt) to see how Leakalizer found his way through the labyrinth: ::

	grep -Ri "Maze: Solution" $LEAKALIZER/build/s2e-last/messages.txt

You should find four encoded solutions, i.e., something like this: ::

	1456 [State 171] Message from guest (0x12ff860): Maze: Solution 1,3,3,0,3,3,3,3,1,1,1,1,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1457 [State 167] Message from guest (0x124aa18): Maze: Solution 1,3,3,0,3,3,3,3,1,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1613 [State 250] Message from guest (0x1378c60): Maze: Solution 1,1,1,1,3,3,3,3,0,0,2,2,0,0,3,3,3,3,1,3,3,0,0,0,0,0,0,0,0,0,0,0,
	1660 [State 269] Message from guest (0x142ee88): Maze: Solution 1,1,1,1,3,3,3,3,0,0,2,2,0,0,3,3,3,3,1,1,1,1,3,3,0,0,0,0,0,0,0,0,



