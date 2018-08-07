        ;; Set stack and jump to _main

        CPU P3
        
        EXTERN main, __exit
        GLOBAL _mbheader, _start, jmp_multiboot, _ap_data, _ap, _ap_code
        
        SECTION .text._start EXEC NOWRITE ALIGN=4
_mbheader:
        align 4
        dd 1BADB002h            ; magic
        dd 3h                   ; features (page aligned modules, mem info, video mode table)
        dd -(3h + 1BADB002h)    ; checksum
        dd 0h                   ; align to 8 byte
_mbi2_s:                        ; mbi 2 header
        dd 0xe85250d6                          ; magic
        dd 0x0                                 ; features
        dd (_mbi2_e - _mbi2_s)                 ; size
        dd -(0xe85250d6 + (_mbi2_e - _mbi2_s)) ; checksum
                                ; end tag
        dw 0x0                  ; type
        dw 0x0                  ; flags
        dd 0x8                  ; size
_mbi2_e:


BITS 16

        align 4
_ap:

        mov edx, 0                              ; microcode location - high
        mov eax, [AP_CODE + _ap_data - _ap + 4] ; microcode location - low
        mov ecx, 0x79                           ; MSR update microcode
        wrmsr

        mov eax, (AP_CODE + _ap_data - _ap)
        mov ecx, 1
        lock xadd [eax], ecx

._loop:
        hlt
        jmp ._loop

        align 4
_ap_data:
        dd 0x80000000 ; checked by microcode.c
        dd 0x80000000

BITS 32

_start:
        ;; copy AP start up code on to AP_CODE physical page
        mov ecx, (_start - _ap)
        mov esi, _ap
        mov edi, AP_CODE
        rep movsb

        mov     esp, _stack
        mov     edx, ebx
        push    __exit
        jmp      main

jmp_multiboot:
        xchg    eax, ebx
        mov     eax, 2BADB002h
        jmp     edx

        align 4
_ap_code:
        dd AP_CODE

        SECTION .bss
        resb 4096
_stack: 
        
        ;; EOF
