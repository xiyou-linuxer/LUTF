[bits 64]
section .text
global context_swap
context_swap:
    ;交换任务上下文 context_swap(c_context, n_context);   保存当前上下文，切换next上下文
    ;rdi中存储的是c_context，rsi中存储的是n_context
    ;保存当前上下文
    mov [rdi], r8
    mov [rdi + 8*1], r9
    mov [rdi + 8*2], r10
    mov [rdi + 8*3], r11
    mov [rdi + 8*4], r12
    mov [rdi + 8*5], r13
    mov [rdi + 8*6], r14
    mov [rdi + 8*7], r15
    mov [rdi + 8*8], rdi
    mov [rdi + 8*9], rsi
    mov [rdi + 8*10], rbp
    mov [rdi + 8*11], rbx
    mov [rdi + 8*12], rdx
    mov [rdi + 8*13], rax
    mov [rdi + 8*14], rcx
    ; mov [rdi + 8*15], rsp
    mov rcx, [rsp]; [rsp] == rip
    mov [rdi + 8*16], rcx   ;[rdi + 8*16]存储rip的值，返回地址为下一次切换到该任务时的运行指令的地址
    lea rcx, [rsp + 8];???
    mov [rdi + 8*15], rcx   ;下次切换到该任务时，栈的栈顶

    ;切换到next的上下文
    ;Load the next stack pointer and the preserved registers.
    ; mov rdi, rsi
    mov r8, [rsi]
    mov r9, [rsi + 8*1]
    mov r10, [rsi + 8*2]
    mov r11, [rsi + 8*3]
    mov r12, [rsi + 8*4]
    mov r13, [rsi + 8*5]
    mov r14, [rsi + 8*6]
    mov r15, [rsi + 8*7]
    mov rdi, [rsi + 8*8]
    ;mov rsi, [rsi + 8*9]
    mov rbp, [rsi + 8*10]
    mov rbx, [rsi + 8*11]
    mov rdx, [rsi + 8*12]
    mov rax, [rsi + 8*13]
    mov rsp, [rsi + 8*15]   ;切换栈

    mov rcx, [rsi + 8*16]   ;rip的内容
    push rcx

    mov rcx, [rsi + 8*14]
    mov rsi, [rsi + 8*9]

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
