EXTERN ReturnAddress:QWORD
EXTERN GEngine:QWORD

.code
hkGEngine proc
        mov [GEngine],rax
        mov rax,[rax+00000788h]
        jmp ReturnAddress
hkGEngine endp
end