===================
Contributing to S2E
===================

Submitting patches
==================

S2E welcomes contributions of code (either fixing bugs or adding new functionality).
However, we get a lot of patches, and so we have some guidelines about submitting patches.
If you follow these, you'll help make our task of code review easier and your patch is likely to be committed faster.

Since S2E is based on QEMU, patch submissions mostly follow the QEMU `guidelines <http://wiki.qemu.org/Contribute/SubmitAPatch>`_
(most of the text is taken from the QEMU wiki). Here are the relevant parts applicable to S2E.

All contributions to S2E must be **sent as pull requests** on GitHub. Please create an account and create a private fork of S2E.
Patch contributions should not be posted on the mailing list, bug tracker, posted on forums, or externally hosted and linked to.


**Patches must include a Signed-off-by: line**.  For more information see
`SubmittingPatches 1.12 <http://git.kernel.org/?p=linux/kernel/git/torvalds/linux-2.6.git;a=blob;f=Documentation/SubmittingPatches;h=689e2371095cc5dfea9927120009341f369159aa;hb=f6f94e2ab1b33f0082ac22d71f66385a60d8157f#l297>`_. This is vital or we will not be able to apply your patch! Please use your real name to sign a patch (not an alias name).


**Correct English** is appreciated. If you are not sure, `codespell <http://wiki.qemu.org/Contribute/SpellCheck>`_ or other programs
help finding the most common spelling mistakes in code and documentation.

**Patches should be against current git master**. There's no point submitting a patch which is based on a
released version of S2E because development will have moved on from then and it probably won't even apply to master.

**Split up longer patches** into a patch series of logical code changes.  Each change should compile and execute successfully.
For instance, don't add a file to the makefile in patch one and then add the file itself in patch two.
(This rule is here so that people can later use tools like `git bisect <http://git-scm.com/docs/git-bisect>`_
without hitting points in the commit history where S2E doesn't work for reasons unrelated to the bug they're chasing.)

**Patches that touch QEMU only** should also be sent upstream. If your patch modifies QEMU and S2E, check whether the part
that changes QEMU could also be applied upstream. Split up the patch if necessary.

**Don't include irrelevant changes**. In particular, don't include formatting, coding style or whitespace
changes to bits of code that would otherwise not be touched by the patch.
(It's OK to fix coding style issues in the immediate area (few lines) of the lines you're changing.)
If you think a section of code really does need a reindent or other large-scale style fix,
submit this as a separate patch which makes no semantic changes; don't put it in the same patch as your bug fix.


**Write a good commit message**. S2E follows the usual standard for git commit messages:
the first line (which becomes the email subject line) is "single line summary of change".
Then there is a blank line and a more detailed description of the patch, another blank and your
Signed-off-by: line. Don't include comments like "This is a suggestion for fixing this bug"
(they can go below the "---" line in the email so they don't go into the final commit message).



Submitting larger contributions
===============================

The process for submitting larger contributions is the same as submitting bug fixes.
Everything goes through GitHub.

To be accepted, your code must follow the following rules:

1. All files must be prefixed with the S2E copyright header.
   All contributions must use the BSD 3-clause license.

   You can use the following template:

.. code-block:: c

    /*
     * S2E Selective Symbolic Execution Platform
     *
     * Copyright (c) 2013, ***** YOUR COPYRIGHT ******
     * All rights reserved.
     *
     * Redistribution and use in source and binary forms, with or without
     * modification, are permitted provided that the following conditions are met:
     *     * Redistributions of source code must retain the above copyright
     *       notice, this list of conditions and the following disclaimer.
     *     * Redistributions in binary form must reproduce the above copyright
     *       notice, this list of conditions and the following disclaimer in the
     *       documentation and/or other materials provided with the distribution.
     *     * Neither the name of the copyright holder nor the
     *       names of its contributors may be used to endorse or promote products
     *       derived from this software without specific prior written permission.
     *
     * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
     * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
     * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
     * DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
     * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
     * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
     * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
     * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
     * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
     * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
     *
     * All contributors are listed in the S2E-AUTHORS file.
     */


2. Your code must be documented. If you write a plugin, please add both the RST and the corresponding HTML file
   to the documentation. The documentation should at least describe all the plugin settings and provide usage examples.
   Please be as thorough as possible in the documentation. The clearer it is, the fewer questions will be asked.

3. Notes about coding style/commits/commit messages, etc. described in the previous section also apply here.


Submitting bug reports
======================

We need the following to debug S2E crashes:

0. Ideally, we prefer an SSH login to the machine that contains your environment so that we can
   debug live. This method has been pretty effective in the past and might be the easiest for
   you to set up. Otherwise, see the next points:

1. A stack trace for the crash + ``s2e-last`` folder + git revision of the S2E source you used.
   Make sure to use the latest revision of the master branch if possible.
   If you have changed the S2E source or need custom plugins, first check that
   you can reproduce the bug with vanilla S2E. Otherwise, please read point 3 and 4 below.

2. A guest image that is ready to run and a script to launch it. You must build your image in such a way that it
   can be launched by a script. We should not have to type any commands or do any sort
   of manipulation in the guest. We will reject your image if we spend more than 10 minutes trying
   to run it.

   Try to make the smallest image possible. If you can reproduce the bug with a 128 MB RAM guest,
   do not submit an image that uses 512 MB. We prefer minimal Debian images
   (no desktop, etc.), unless of course your project requires a specific setup.

   Guest images can be quite large, feel free to use Dropbox, Gdrive, or any other
   online service that suits you. Make sure that the interface for your download service is in english.
   We should be able to download your image in a few clicks.

If we cannot reproduce the bugs with the information you gave us above, we will ask you:

3. A Virtual Box VM containing the host environment and the S2E guest.
   It must come with a Vagrant file to minimize setup time on our side.
   Check that you can reproduce the bug in such an environment first.
   If not, see the next point.

4. An SSH login so that we can debug live.
