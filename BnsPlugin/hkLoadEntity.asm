EXTERN ReturnAddress2:QWORD
EXTERN ObjectId:QWORD
EXTERN R12Val:QWORD

.code
hkLoadEntity proc
        mov [R12Val],r12
        mov [ObjectId],r9
        jmp ReturnAddress2
hkLoadEntity endp
end