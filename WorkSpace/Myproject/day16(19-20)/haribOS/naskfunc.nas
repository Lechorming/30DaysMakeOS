; naskfunc
; TAB=4

[FORMAT "WCOFF"]				; 制作目标文件的模式
[INSTRSET "i486p"]				; 要使用486的命令的标记
[BITS 32]						; 制作32位模式用的机器语言
[FILE "naskfunc.nas"]			; 源文件名信息

		GLOBAL	_io_hlt, _io_cli, _io_sti, _io_stihlt
		GLOBAL	_io_in8,  _io_in16,  _io_in32
		GLOBAL	_io_out8, _io_out16, _io_out32
		GLOBAL	_io_load_eflags, _io_store_eflags
		GLOBAL	_load_gdtr, _load_idtr
		GLOBAL	_store_cr0, _load_cr0
		GLOBAL	_load_tr, _farjmp, _farcall
		GLOBAL	_asm_inthandler21, _asm_inthandler27, _asm_inthandler2c
		GLOBAL	_asm_inthandler20
		GLOBAL	_memtest_sub
		GLOBAL	_asm_hrb_api
		EXTERN	_inthandler21, _inthandler27, _inthandler2c
		EXTERN	_inthandler20
		EXTERN	_hrb_api
        
; 以下是实际的函数

[SECTION .text]		; 目标文件中写了这些之后再写程序

_io_hlt:	; void io_hlt(void);
		HLT
		RET	

_io_cli:     ; void io_cli(void);
        CLI
        RET

_io_sti:     ; void io_sti(void);
        STI
        RET

_io_stihlt: ; void io_stihlt(void);
        STI
        HLT
        RET

_io_in8:     ; int io_in8(int port);
        MOV      EDX, [ESP+4]      ; port
        MOV      EAX,0
        IN       AL, DX
        RET

_io_in16:   ; int io_in16(int port);
        MOV      EDX, [ESP+4]      ; port
        MOV      EAX,0
        IN       AX, DX
        RET

_io_in32:   ; int io_in32(int port);
        MOV      EDX, [ESP+4]      ; port
        IN       EAX, DX
        RET

_io_out8:   ; void io_out8(int port, int data);
        MOV      EDX, [ESP+4]      ; port
        MOV      AL, [ESP+8]       ; data
        OUT      DX, AL
        RET

_io_out16:  ; void io_out16(int port, int data);
        MOV      EDX, [ESP+4]      ; port
        MOV      EAX, [ESP+8]      ; data
        OUT      DX, AX
        RET

_io_out32:  ; void io_out32(int port, int data);
        MOV      EDX, [ESP+4]      ; port
        MOV      EAX, [ESP+8]      ; data
        OUT      DX, EAX
        RET

_io_load_eflags:		; int io_load_eflags(void);
        PUSHFD			; 指PUSH EFLAGS到栈里面
        POP		EAX		; 再将EFLAGS取到EAX里
        RET

_io_store_eflags:   ; void io_store_eflags(int eflags);
        MOV      EAX, [ESP+4]
        PUSH EAX
        POPFD ; 指POP EFLAGS
        RET
        
_load_gdtr:		; void load_gdtr(int limit, int addr);
		MOV		AX,[ESP+4]
		MOV		[ESP+6],AX		; 将limit写入addr对应的内存
		LGDT	[ESP+6]			; 告诉CPU段描述符表在哪
		RET

_load_idtr:		; void load_idtr(int limit, int addr);
		MOV		AX,[ESP+4]		
		MOV		[ESP+6],AX		; 将limit写入addr对应的内存
		LIDT	[ESP+6]			; 告诉CPU中断描述符表在哪
		RET

_load_tr:		; void load_tr(int tr);
        LTR		[ESP+4]			; tr
        RET

_farjmp:		; void farjmp(int eip, int cs);
        JMP		FAR [ESP+4]		; eip, cs
        RET

_farcall:		; void farcall(int eip, int cs);
		CALL	FAR [ESP+4]		; eip, cs
		RET

_asm_inthandler20:
        PUSH     ES
        PUSH     DS
        PUSHAD
        MOV      EAX, ESP
        PUSH     EAX
        MOV      AX, SS
        MOV      DS, AX
        MOV      ES, AX
        CALL     _inthandler20
        POP      EAX
        POPAD
        POP      DS
        POP      ES
        IRETD

_asm_inthandler21:
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		EAX,ESP
		PUSH	EAX
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler21
		POP		EAX
		POPAD
		POP		DS
		POP		ES
		IRETD

_asm_inthandler27:
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		EAX,ESP
		PUSH	EAX
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler27
		POP		EAX
		POPAD
		POP		DS
		POP		ES
		IRETD

_asm_inthandler2c:
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		EAX,ESP
		PUSH	EAX
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler2c
		POP		EAX
		POPAD
		POP		DS
		POP		ES
		IRETD
		
_load_cr0:		; int load_cr0(void);
		MOV		EAX, CR0
		RET
		
_store_cr0:		; void store_cr0(int cr0);
		MOV		EAX, [ESP+4]
		MOV		CR0, EAX
		RET		
		
_memtest_sub:   ; unsigned int memtest_sub(unsigned int start, unsigned int end)
		PUSH	EDI						; (由于还要使用EBX, ESI, EDI）
		PUSH	ESI						
		PUSH	EBX						
		MOV		ESI,0xaa55aa55			; pat0 = 0xaa55aa55;
		MOV		EDI,0x55aa55aa			; pat1 = 0x55aa55aa;
		MOV		EAX, [ESP+12+4]			; i = start;
mts_loop:
		MOV		EBX, EAX
		ADD		EBX,0xffc				; p = i + 0xffc;
		MOV		EDX, [EBX]				; old = *p;
		MOV		[EBX], ESI				; *p = pat0;
		XOR		DWORD [EBX],0xffffffff	; *p ^= 0xffffffff;
		CMP		EDI, [EBX]				; if (*p ! = pat1) goto fin;
		JNE		mts_fin
		XOR		DWORD [EBX],0xffffffff	; *p ^= 0xffffffff;
		CMP		ESI, [EBX]				; if (*p ! = pat0) goto fin;
		JNE		mts_fin
		MOV		[EBX], EDX				; *p = old;
		ADD		EAX,0x1000				; i += 0x1000;
		CMP		EAX, [ESP+12+8]			; if (i <= end) goto mts_loop;
		JBE		mts_loop
        POP		EBX
        POP		ESI
        POP		EDI
		RET
mts_fin:
		MOV		[EBX], EDX				; *p = old;
		POP		EBX
		POP		ESI
		POP		EDI
		RET

_asm_hrb_api:
        STI
        PUSHAD  ; 用于保存寄存器值的PUSH
        PUSHAD  ; 用于向hrb_api传值的PUSH
        CALL	_hrb_api
        ADD		ESP,32
        POPAD
        IRETD
