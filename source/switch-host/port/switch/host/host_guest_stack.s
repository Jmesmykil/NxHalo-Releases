/* AAPCS64 stack transition. x9 is caller-saved and MUST NOT retain the
 * native SP across guest code. x19 and the frame/link registers are preserved
 * by both LP64 and ILP32 callees. Arguments/return stay in full ABI registers. */
.text
.global host_call_on_guest_stack
.type host_call_on_guest_stack, %function
host_call_on_guest_stack:
    stp x29, x30, [sp, #-32]!
    str x19, [sp, #16]
    mov x29, sp
    mov x19, sp
    mov x16, x1
    mov sp, x0
    mov w0, w2
    mov w1, w3
    mov w2, w4
    mov w3, w5
    blr x16
    mov sp, x19
    ldr x19, [sp, #16]
    ldp x29, x30, [sp], #32
    ret
.size host_call_on_guest_stack, .-host_call_on_guest_stack
