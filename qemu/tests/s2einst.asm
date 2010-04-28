zerobuf: times 0x20000-16 db 0
db 0xf1
dq 0xbadf00ddeadbeef
cli
hlt

times 5 db 0
