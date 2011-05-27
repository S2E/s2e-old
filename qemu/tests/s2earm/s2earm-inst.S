@filename s2earm-inst.S
.text
.arm


@returns version in r0
s2e_version:
.word 0xFF000000
mov 	pc,r14

s2e_enable_symbolic:
.word 0xFF010000
mov 	pc,r14

s2e_disable_symbolic:
.word 0xFF020000
mov 	pc,r14

@requires r0 to point to the address with the string to be printed
s2e_message:
.word 0xFF100000
mov 	pc,r14

s2e_enable_forking:
.word 0xFF090000
mov 	pc,r14

s2e_disable_forking:
.word 0xFF0A0000
mov 	pc,r14

@requires r0: address
@requires r1: size
@requires r2: address to symbolic name
s2e_make_symbolic:
.word 0xFF030000
mov		pc,r14

@returns path id to r0
s2e_get_path_id:
.word 0xFF050000
mov		pc,r14

@requires r0: status code number
@requires r1: address to kill message
s2e_kill_state:
.word 0xFF060000
mov		pc,r14

@requires r0: expression number
@requires r1: address to symbolic name
s2e_print_expression:
.word 0xFF070000
mov		pc,r14

@requires r0: address
@requires r1: size
@requires r2: address to (your abitrary) name of this memory region
s2e_print_memory:
.word 0xFF080000
mov		pc,r14


@requires r0: address
@requires r1: size
s2e_concretize:
.word 0xFF200000
mov		pc,r14

@requires r0: address
@requires r1: size
s2e_get_example:
.word 0xFF210000
mov		pc,r14

s2e_merge_point:
.word 0xFF700000
mov		pc,r14
