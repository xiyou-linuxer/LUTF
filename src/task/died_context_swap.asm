[bits 64]
section .text
global context_set
context_set:
    ;切换任务 context_set(&next_context)
    ;rdi中存储sigcontext的地址
    mov r8, [rdi]
    mov r9, [rdi + 8*1]
    mov r10, [rdi + 8*2]
    mov r11, [rdi + 8*3]
    mov r12, [rdi + 8*4]
    mov r13, [rdi + 8*5]
    mov r14, [rdi + 8*6]
    mov r15, [rdi + 8*7]
    ;mov rdi, [rdi + 8*8]
    mov rsi, [rdi + 8*9]
    mov rbp, [rdi + 8*10]
    mov rbx, [rdi + 8*11]
    mov rdx, [rdi + 8*12]
    mov rax, [rdi + 8*13]
    mov rcx, [rdi + 8*14]
    mov rsp, [rdi + 8*15]   ;切换栈

    mov rcx, [rdi + 8*16]   ;rip的内容
    push rcx

    mov rcx, [rdi + 8*14]
    mov rdi, [rdi + 8*8]

    ret

