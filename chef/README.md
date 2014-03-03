Chef Usage Instructions
=======================

**NOTE**: This document is work-in-progress and some edges in Chef may be still rough.  In case you encounter any
difficulties following the steps below, please contact us on the S2E-Dev mailing list.

This folder contains all the scripts and libraries needed to operate Chef.  This file documents their usage along
standard Chef workflows.


Preparing a Chef VM
-------------------

(TODO: Prepare a ready-to-run VM.)

1. Create a standard S2E image called `chef_disk.raw` and place it in the `$CHEF_ROOT/vm/` directory.
2. Inside the image, checkout the Chef-adapted interpreter repository and install the interpreter according to its instructions.
3. Copy the `cmd_server.py` file on the image.
4. Run the `cmd_server.py` file inside the image, before saving the snapshot and running it in S2E mode.


Running a Chef VM
-----------------

(Work in progress)

Use the `run_qemu.py` script to run the VM.


Running the experiments in the Chef paper
-----------------------------------------

(Work in progress)
