.global _start

.text
_start:
	mov x0, #1 // fd = 1 (stdout)
	adr x1, msg // pointer to message string
	mov x2, #15 // message length in bytes
	mov x8, #64 // syscall 64 is sys_write on AArch64
	svc #0 // invoke kernel
	
	//exit
	mov x0, #0
	mov x8, #93
	svc #0
.data
msg:
	.ascii "Hello Aarch64!\n"
