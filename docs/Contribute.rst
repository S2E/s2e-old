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


All contributions to S2E must be **sent as patches** to the s2e-dev.
Patch contributions should not be posted on the bug tracker, posted on forums, or externally hosted and linked to.

Send patches to the mailing list and **CC the relevant maintainer** -- look in the MAINTAINERS file to find out who that is.

**Send patches inline** so they are easy to reply to with review comments.  Do not put patches in attachments.

**Use the right patch format**. `git format-patch <http://git-scm.com/docs/git-format-patch>`_
will produce patch emails in the right format (check the documentation to find out how to drive it).
You can then edit the cover letter before using ``git send-email`` to mail the files to the mailing list.
(We recommend `git send-email <http://git-scm.com/docs/git-send-email>`_ because mail clients
often mangle patches by wrapping long lines or messing up whitespace.)

**Patch emails must include a Signed-off-by: line**.  For more information see
`SubmittingPatches 1.12 <http://git.kernel.org/?p=linux/kernel/git/torvalds/linux-2.6.git;a=blob;f=Documentation/SubmittingPatches;h=689e2371095cc5dfea9927120009341f369159aa;hb=f6f94e2ab1b33f0082ac22d71f66385a60d8157f#l297>`_. This is vital or we will not be able to apply your patch! Please use your real name to sign a patch (not an alias name).


**Correct English** is appreciated. If you are not sure, `codespell <http://wiki.qemu.org/Contribute/SpellCheck>`_ or other programs
help finding the most common spelling mistakes in code and documentation.

**Patches should be against current git master**. There's no point submitting a patch which is based on a
released version of S2E because development will have moved on from then and it probably won't even apply to master.
We only apply selected bugfixes to release branches and then only as backports once the code has gone into master.

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

When replying to comments on your patches **reply to all and not just the sender** --
keeping discussion on the mailing list means everybody can follow it.

If you fix issues that are raised during review **resend the entire patch series**
not just the one patch that was changed. This allows maintainers to easily apply
the fixed series without having to manually identify which patches are relevant.

**When resending patches add a v2/v3 suffix** (eg [PATCH v2]). This means people can easily
identify whether they're looking at the most recent version. (The first version of a patch need not say "v1",
just [PATCH] is sufficient.) For patch series, the version applies to the whole series --
even if you only change one patch, you resend the entire series and mark it as "v2".
Don't try to track versions of different patches in the series separately.

For later versions of patches **include a summary of changes from previous versions, but not
in the commit message itself**. In an email formatted as a git patch, the commit message is
the part above the "---" line, and this will go into the git changelog when the patch is committed.
This part should be a self-contained description of what this version of the patch does, written
to make sense to anybody who comes back to look at this commit in git in six months' time.
The part below the "---" line and above the patch proper (git format-patch puts the diffstat here)
is a good place to put remarks for people reading the patch email, and this is where the
"changes since previous version" summary belongs.


Submitting larger contributions
===============================

In case you would like to contribute a new feature, it is easier to use gitorious for the review process.

1. Subscribe to the s2e-dev mailing list
2. Request a login/password to the S2E repository
3. Create a private clone of the S2E repository (using the Gitorious interface)
4. Work on your private clone
5. Send a merge request from the gitorious interface
6. We will review the merge request

To be accepted, your code must follow the following rules:

1. All files prefixed with the S2E copyright header. All contributions
   must use the BSD 3-clause license..

   You can use the following template:

.. code-block:: c

    /*
     * S2E Selective Symbolic Execution Platform
     *
     * Copyright (c) 2011, ***** YOUR COPYRIGHT ******
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
     * All contributors are listed in S2E-AUTHORS file.
     *
     */


2. Your code must be documented. If you write a plugin, please add both the RST and the corresponding HTML file
   to the documentation. The documentation should at least describe all the plugin settings and provide usage examples.
   Please be as thorough as possible in the documentation. The clearer it is, the fewer questions will be asked.

3. Notes about coding style/commits/commit messages, etc. described in the previous section also apply here.
