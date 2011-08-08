===================
Contributing to S2E
===================

Anyone is welcome to contribute code to S2E. To do so, please follow the following procedure:

1. Subscribe to the s2e-dev mailing list
2. Request a login/password to the S2E repository
3. Create a private clone of the S2E repository (using the Gitorious interface)
4. Work on your private clone
5. Send a merge request from the gitorious interface
6. We will review the merge request

To be accepted, your code must follow the following rules:

1. All files prefixed with the S2E copyright header. All contributions
   must use the BSD 3-clause license. By default, the plugin maintainer is the one who wrote it, unless
   decided otherwise on the s2e-dev mailing list.

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
     * Currently maintained by:
     *    Your Name <your.name@example.com>
     *
     * All contributors are listed in S2E-AUTHORS file.
     *
     */


2. Your code must be documented. If you write a plugin, please add both the RST and the corresponding HTML file
   to the documentation. The documentation should at least describe all the plugin settings and provide usage examples.
   Please be as thorough as possible in the documentation. The clearer it is, the fewer questions will be asked.
