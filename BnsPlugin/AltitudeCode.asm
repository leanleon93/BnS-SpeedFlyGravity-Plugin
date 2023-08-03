EXTERN xyzAddr:QWORD
EXTERN altitude:QWORD
EXTERN isCameraCompareAddr:QWORD
EXTERN lpRemain:QWORD
EXTERN altitudeEnabled:BYTE
EXTERN gravityEnabled:BYTE

.code
hkAltitudeHack PROC
    push rax
    push rbx
    push rcx
    push rdx
    push r8

    mov rax,xyzAddr
    mov rbx,altitude
    mov rcx,isCameraCompareAddr
    mov dl, altitudeEnabled
    mov dh, gravityEnabled

    test rcx, rcx
    jz Exit
    cmp rdi, rcx
    jne Exit

    test dh,dh
    jnz Gravity

    test dl,dl
    jnz AltitudeCode
    
    jmp Exit

AltitudeCode:
    movss xmm7,dword ptr[rax+8h]
    addss xmm7,dword ptr[rbx]
    movss dword ptr[rax+8h],xmm7
    movups xmm7,[rax]
    jmp Exit

Gravity:
    movups [rax+10h],xmm7
    mov r8d,dword ptr[rax+8h]
    mov dword ptr[rax+18h],r8d
    movups xmm7,[rax+10h]
    jmp Exit

Exit:
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop r8

    movups [rdi+1F0h],xmm7
    jmp lpRemain
hkAltitudeHack ENDP
END