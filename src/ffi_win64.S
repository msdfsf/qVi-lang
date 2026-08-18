.intel_syntax noprefix

# CallContext
.equ offset_ctx_args           , 0
.equ offset_ctx_arg_count      , 8
.equ offset_ctx_arg_stack_size , 12
.equ offset_ctx_ret_arg        , 16
.equ offset_ctx_target         , 40
.equ sizeof_call_context       , 48

# Arg struct
.equ offset_arg_data , 0
.equ offset_arg_size , 8
.equ offset_arg_kind , 16
.equ offset_arg_pass , 17
.equ sizeof_arg      , 24

.equ AK_INT       , 0
.equ AK_FLOAT     , 2
.equ AK_AGGREGATE , 4

# Regs aliases
# using preprocessor, as I just dont know
# how to make this thing work other way :(
#define R_CTX  rbx
#define R_ARG  rsi
#define R_IDX  rdi
#define R_CNT  r10
#define R_CNTd r10d
#define R_ARG_KIND r11
#define R_RET_ARG  r12

# We start 'after' return addr which itself is
# aligned to 16 bytes...
.globl asm_win64Invoke
asm_win64Invoke:
    # Save non-volatile registers to work with
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12

    mov R_CTX, rcx

    # Prepare Stack:
    # - 32 bytes for reg args + stack args
    # - be 16 byte aligned, which, I guess, we are
    # have to use eax as dword ptr is too much of a concept
    mov eax, [R_CTX + offset_ctx_arg_stack_size]
    add rax, 32
    sub rsp, rax

    mov R_CNTd, [R_CTX + offset_ctx_arg_count]
    mov R_ARG, [R_CTX + offset_ctx_args]
    mov R_RET_ARG, R_CTX
    add R_RET_ARG, offset_ctx_ret_arg
    xor R_IDX, R_IDX

    # If return value is not fitting into reg,
    # pointer to store it is the first arg and
    # inputs are shifted. Therefore we just skip
    # one loop iteration
    movzx rax, byte ptr [R_RET_ARG + offset_arg_kind]
    cmp   rax, AK_AGGREGATE
    jne   loop
    mov   rcx, R_RET_ARG
    sub   R_ARG, sizeof_arg
    jmp   next_arg

    loop:
        cmp R_IDX, R_CNT
        je  invoke_call

        movzx R_ARG_KIND, byte ptr [R_ARG + offset_arg_kind]

        # First 4 args to reg
        cmp R_IDX, 4
        jl load_register

        load_stack:
            # offset: RSP + 32 + (Idx - 4) * 8
            # - RSP already subtracted -> arg at RSP + 32!
            mov r11, R_IDX
            shl r11, 3
            mov r9, [R_ARG]
            mov [rsp + r11], r9
            jmp next_arg

        load_register:
            cmp R_ARG_KIND, AK_FLOAT
            je load_float

            load_int:
                cmp R_IDX, 0
                jne arg1_i
                mov rcx, [R_ARG]
                jmp next_arg
                arg1_i:
                cmp R_IDX, 1
                jne arg2_i
                mov rdx, [R_ARG]
                jmp next_arg
                arg2_i:
                cmp R_IDX, 2
                jne arg3_i
                mov r8, [R_ARG]
                jmp next_arg
                arg3_i:
                mov r9, [R_ARG]
                jmp next_arg

            load_float:
                cmp R_IDX, 0
                jne arg1_f
                movq xmm0, qword ptr [R_ARG]
                jmp next_arg
                arg1_f:
                cmp R_IDX, 1
                jne arg2_f
                movq xmm1, qword ptr [R_ARG]
                jmp next_arg
                arg2_f:
                cmp R_IDX, 2
                jne arg3_f
                movq xmm2, qword ptr [R_ARG]
                jmp next_arg
                arg3_f:
                movq xmm3, qword ptr [R_ARG]

        next_arg:
            inc R_IDX
            add R_ARG, sizeof_arg
            jmp loop

    invoke_call:
        mov  rax, [R_CTX + offset_ctx_target]
        call rax

        movzx rdx, byte ptr [R_RET_ARG + offset_arg_kind]
        cmp   rdx, AK_AGGREGATE
        je    cleanup
        cmp   rdx, AK_FLOAT
        je    set_return_float
        mov   [R_RET_ARG], rax
        jmp   cleanup
        set_return_float:
        movq  [R_RET_ARG], xmm0

    cleanup:
    lea rsp, [rbp - 32]
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp

    ret
