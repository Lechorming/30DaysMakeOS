; haribote-os boot asm
; TAB=4

BOTPAK	EQU		0x00280000		; bootpack要复制到的内存地址
DSKCAC	EQU		0x00100000		; 磁盘中的数据要复制到的内存地址
DSKCAC0	EQU		0x00008000		; 磁盘中的数据被IPL加载到的内存地址（实模式）

; BOOT_INFO的地址信息
CYLS	EQU		0x0ff0			; IPL加载了多少个柱面到内存0x8200中的地址
LEDS	EQU		0x0ff1			; 键盘的LED灯状态的地址
VMODE	EQU		0x0ff2			; 颜色位数的地址
SCRNX	EQU		0x0ff4			; 分辨率的X的地址
SCRNY	EQU		0x0ff6			; 分辨率的Y的地址
VRAM	EQU		0x0ff8			; 图像缓冲区的开始地址

		ORG		0xc200			; 程序的内存装载地址

; 画面模式设置

		MOV		AL,0x13			; VGA显卡, 320*200*8位色彩
		MOV		AH,0x00
		INT		0x10
		MOV		BYTE [VMODE],8	; 记录画面模式
		MOV		WORD [SCRNX],320
		MOV		WORD [SCRNY],200
		MOV		DWORD [VRAM],0x000a0000
		
; 用BIOS取得键盘上各种LED指示灯的状态

		MOV		AH,0x02
		INT		0x16 			; 键盘BIOS
		MOV		[LEDS],AL
		
; PIC关闭一切中断
;   根据AT兼容机的规格，如果要初始化PIC，
;   必须在CLI之前进行，否则有时会挂起。
;   随后进行PIC的初始化。

        MOV		AL,0xff
        OUT		0x21,AL
        NOP				         ; 如果连续执行OUT指令，有些机种会无法正常运行
        OUT		0xa1,AL
        
        CLI				         ; 禁止CPU级别的中断
        
; 为了让CPU能访问1MB以上的内存，设定A20GATE

		CALL	waitkbdout
		MOV		AL,0xd1
		OUT		0x64,AL
		CALL	waitkbdout
		MOV		AL,0xdf			; enable A20
		OUT		0x60,AL
		CALL	waitkbdout

; 切换保护模式

[INSTRSET "i486p"]				; 要使用486的指令的声明

		LGDT	[GDTR0]			; 设定临时GDT
		MOV		EAX,CR0
		AND		EAX,0x7fffffff	; 设置31位为0（为了禁止分页）
		OR		EAX,0x00000001	; 设置0位为1（为了切换到保护模式）
		MOV		CR0,EAX
		JMP		pipelineflush
		
pipelineflush:
		MOV		AX,1*8			; 可读写的段 32位
		MOV		DS,AX
		MOV		ES,AX
		MOV		FS,AX
		MOV		GS,AX
		MOV		SS,AX

; bootpack的转送
; 将bootpack地址(文件末尾)的数据复制到0x00280000地址处，长度为512KB
        MOV		ESI,bootpack	; 转送源
        MOV		EDI,BOTPAK		; 转送目的地 BOTPAK=0x00280000 bootpack的加载位置
        MOV		ECX,512*1024/4
        CALL	memcpy

; 将IPL读进来在内存0x8000位置的数据复制到内存0x00100000位置去，长度180KB

; 首先从启动引导扇区开始
; 将0x7c00地址的数据复制到0x00100000地址处，长度为512B
        MOV		ESI,0x7c00		; 转送源
        MOV		EDI,DSKCAC		; 转送目的地 DSKCAC=0x00100000
        MOV		ECX,512/4
        CALL	memcpy

; 所有剩下的
; 将0x00008200地址的数据复制到0x00100200地址处，长度为180KB-512B
        MOV		ESI,DSKCAC0+512	; 转送源 DSKCAC0=0x00008000 表示磁盘缓存地址（实模式）
        MOV		EDI,DSKCAC+512	; 转送目的地 DSKCAC0=0x00100000 表示磁盘缓存地址
        MOV		ECX,0
        MOV		CL,BYTE [CYLS]
        IMUL	ECX,512*18*2/4	; 从柱面数变换为字节数/4
        SUB		ECX,512/4		; 减去IPL
        CALL	memcpy
        
; 必须由asmhead来完成的工作，至此全部完毕
; 以后就交由bootpack来完成

; bootpack的启动
; 将bootpack.hrb的第0x10c8地址的数据复制到0x00310000地址，长度为4520B
; 这部分数据是头文件里的数据
		MOV		EBX,BOTPAK		; BOTPAK=0x00280000
		MOV		ECX,[EBX+16]
		ADD		ECX,3			; ECX += 3;
		SHR		ECX,2			; ECX /= 4; SHR:右移 EXC=0x11a8
		JZ		skip			; 没有要转送的东西时
		MOV		ESI,[EBX+20]	; 转送源
		ADD		ESI,EBX
		MOV		EDI,[EBX+12]	; 转送目的地
		CALL	memcpy

; 最后将0x310000代入到ESP里。
skip:
		MOV		ESP,[EBX+12]	; 栈初始值
		JMP		DWORD 2*8:0x1b	; 2*8:0x000001b是指第2个段的0x1b号地址，第2个段的基地址是0x280000，
								; 所以实际上是从0x28001b开始执行的，也就是bootpack.hrb的0x1b号地址。

waitkbdout:
		IN		 AL,0x64
		AND		 AL,0x02
		IN		 AL,0x60		; 空读(为了清空数据缓冲区中的垃圾数据，不会影响JNZ)
		JNZ		waitkbdout		; 比较结果不为0则继续等待
		RET

memcpy:
		MOV		EAX,[ESI]
		ADD		ESI,4
		MOV		[EDI],EAX
		ADD		EDI,4
		SUB		ECX,1
		JNZ		memcpy			; 比较结果不为0则继续复制
		RET
; memcpyはアドレスサイズプリフィクスを入れ忘れなければ、ストリング命令でも書ける

		ALIGNB	16								; 一直添加DBO,直到地址能被16整除为止,猜测是为了4字节对齐
GDT0:
		RESB	8								; NULL selector
		DW		0xffff,0x0000,0x9200,0x00cf		; 可以读写的段（segment）32bit
		DW		0xffff,0x0000,0x9a28,0x0047		; 可以执行的段（segment）32bit（bootpack用）

		DW		0
GDTR0:
		DW		8*3-1
		DD		GDT0

		ALIGNB	16
bootpack:
