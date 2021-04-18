[bits 64]
section .text
global get_reg
get_reg:
    mov [rdi + 8*18], cs
    mov [rdi + 8*18+6], ss
    ret

; struct sigcontext
; {
;   __uint64_t r8;
;   __uint64_t r9;
;   __uint64_t r10;
;   __uint64_t r11;
;   __uint64_t r12;
;   __uint64_t r13;
;   __uint64_t r14;
;   __uint64_t r15;
;   __uint64_t rdi;
;   __uint64_t rsi;
;   __uint64_t rbp;
;   __uint64_t rbx;
;   __uint64_t rdx;
;   __uint64_t rax;
;   __uint64_t rcx;
;   __uint64_t rsp;
;   __uint64_t rip;
;   __uint64_t eflags;
;   unsigned short cs;
;   unsigned short gs;
;   unsigned short fs;
;   unsigned short __pad0;
;   __uint64_t err;
;   __uint64_t trapno;
;   __uint64_t oldmask;
;   __uint64_t cr2;
;   __extension__ union
;     {
;       struct _fpstate * fpstate;
;       __uint64_t __fpstate_word;
;     };
;   __uint64_t __reserved1 [8];
; };
