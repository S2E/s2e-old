/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Andreas Kirchner <akalypse@gmail.com>
 *
 */

/*
 * To understand inline ARM assembler, read this: http://www.ethernut.de/en/documents/arm-inline-asm.html
 */

#include <stdio.h>
#include <stdlib.h>
#include "s2earm.h"

//for system calls
#include <unistd.h>
#include <sys/types.h>


void testsub() {
	printf("Hello Subroutine\n");
    __asm__ __volatile__(
    		"MOV r0, #66\n"
    		"MOV r1, #66\n"
    		"MOV r2, #66\n"
    );
	printf("Exit!\n");
}

void main( int argc, char *argv[ ], char *envp[ ] ) {
	s2e_rawmon_loadmodule("s2eandroid",0x8460,0x1AD4);

	int symb;
	int symb2;
	printf("Hell0 Android!\n");
	s2e_message("Hello S2E, Here is Android.");
	s2e_warning("Hello S2E, Android writes a warning.");
	printf("S2E VERSION\t\t\t: %d\n",s2e_version());
	printf("PATH ID\t\t\t\t: %d\n",s2e_get_path_id());
	printf("S2E RAM OBJECT BITS\t\t: %d\n",s2e_get_ram_object_bits());
	printf("Process ID: %d\n",getpid());
	s2e_disable_forking();
	s2e_enable_forking();
	s2e_make_symbolic(&symb,sizeof(symb), "x");
	s2e_make_symbolic(&symb2,sizeof(symb2), "y");

//	s2e_get_example(&symb2,sizeof(symb2));

	if(symb==symb2) {
		printf("test1");
//		s2e_print_expression("x:",symb);
//		s2e_print_expression("y:",symb2);
		s2e_concretize(&symb,sizeof(symb));


		char* msg = "End of State %i. Concretized x is: %i";
		char* endmsg = malloc(sizeof(msg) + 20);

		sprintf(endmsg,msg,s2e_get_path_id(),symb);
		s2e_message(endmsg);
		s2e_kill_state(333, "kill1");
	} else {
		printf("test2");
		s2e_message("symb and symb2 are not equal!");
	}
//  the following Statement kills Android emulator


}
