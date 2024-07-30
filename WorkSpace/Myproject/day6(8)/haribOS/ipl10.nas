; haribote-ipl
; TAB=4

CYLS	EQU		10				; 磁盘读到哪个柱面

		ORG		0x7c00			; 指明程序的内存装载地址

; 以下的记述用于标准FAT12格式的软盘

		JMP		entry
		DB		0x90
		DB		"HARIBOTE"		; 启动区的名称可以是任意的字符串（8字节）
		DW		512				; 每个扇区（sector）的大小（必须为512字节）
		DB		1				; 簇（cluster）的大小（必须为1个扇区）
		DW		1				; FAT的起始位置（一般从第一个扇区开始）
		DB		2				; FAT的个数（必须为2）
		DW		224				; 根目录的大小（一般设成224项）
		DW		2880			; 该磁盘的大小（必须是2880扇区）
		DB		0xf0			; 磁盘的种类（必须是0xf0）
		DW		9				; FAT的长度（必须是9扇区）
		DW		18				; 1个磁道（track）有几个扇区（必须是18）
		DW		2				; 磁头数（必须是2）
		DD		0				; 不使用分区，必须是0
		DD		2880			; 重写一次磁盘大小
		DB		0,0,0x29		; 意义不明，固定
		DD		0xffffffff		;（可能是）卷标号码
		DB		"HARIBOTEOS "	; 磁盘的名称（11字节）
		DB		"FAT12   "		; 磁盘格式名称（8字节）
		RESB	18				; 先空出18字节

; 程序本体

entry:
		MOV		AX,0			; 初始化寄存器
		MOV		SS,AX
		MOV		SP,0x7c00
		MOV		DS,AX
		MOV		ES,AX

; 读取磁盘(INT 0x13),参数如下
; AH=0x02是读盘,0x03是写盘,0x04是校验,0x0c是寻道
; AL=处理对象的扇区数(只能同时处理连续的扇区)
; CH=柱面号
; CL=扇区号(0-5位)|(柱面号&0x300)>>2
; DH=磁头号
; DL=驱动器号
; ES:BX=缓冲地址,即要加载到的内存地址，计算方法：ES×16+BX，最大1MB
; 返回值:
; FLAGS.CF==0：没有错误，AH==0
; FLAGS.CF==1：有错误，错误号码存入AH内(与重置(reset)功能一样)
; 1张软盘有80个柱面，2个磁头，18个扇区，且一个扇区有512字节，总容量:80×2×18×512=1474560Byte=1440KB
; IPL位于C0-H0-S1（柱面0，磁头0，扇区1的缩写）
; 这里共读了10个柱面,2面磁头,18个扇区-1扇区的数据进内存, 10×2×18×512=184320Byte=180KB
; 写入了内存0x08200～0x34fff
		MOV		AX,0x0820		; 要加载到的内存地址ES:BX
		MOV		ES,AX			
		MOV		CH,0			; 柱面0
		MOV		DH,0			; 磁头0
		MOV		CL,2			; 扇区2
readloop:
		MOV		SI,0			; 记录失败次数
retry:
		MOV		AH,0x02			; AH=0x02 : 读盘
		MOV		AL,1			; 1个扇区
		MOV		BX,0
		MOV		DL,0x00			; A驱动器
		INT		0x13			; 调用磁盘BIOS
		JNC		next			; 读取磁盘成功，读取下一个扇区
		ADD		SI,1			; 失败次数+1
		CMP		SI,5			; 大于5?
		JAE		error			; 大于等于就跳转到error
		MOV		AH,0x00			
		MOV		DL,0x00
		INT		0x13			; 重置驱动器
		JMP		retry
next:
		MOV		AX,ES			; 把内存地址后移0x200
		ADD		AX,0x0020
		MOV		ES,AX			; 因为没有ADD ES,0x020指令，所以要这么操作
		ADD		CL,1			; CL+1
		CMP		CL,18			; CL和18作比较
		JBE		readloop		; CL <= 18 的话跳转至readloop
		MOV		CL,1
		ADD		DH,1
		CMP		DH,2
		JB		readloop		; DH < 2 的话跳转到readloop
		MOV		DH,0
		ADD		CH,1
		CMP		CH,CYLS
		JB		readloop		; CH < CYLS 的话跳转到readloop
		
; 磁盘读取完毕，打开操作系统！
		MOV		[0x0ff0],CH		; 将CYLS的值写到内存地址0x0ff0中
		JMP		0xc200
		
fin:
		HLT						; 让CPU停止，等待指令
		JMP		fin				; 无限循环
		
error:
		MOV		SI,errormsg
		JMP		putloop

succeed:
		MOV		SI,succeedmsg
		JMP		putloop
		
putloop:
		MOV		AL,[SI]
		ADD		SI,1			; 给SI加1
		CMP		AL,0
		JE		fin
		MOV		AH,0x0e			; 显示一个文字
		MOV		BX,15			; 指定字符颜色
		INT		0x10			; 调用显卡BIOS
		JMP		putloop
		
errormsg:
		DB		0x0a, 0x0a		; 换行2次
		DB		"Failed!(from Liao_Zhaoming)"
		DB		0x0a			; 换行
		DB		0
		
succeedmsg:
		DB		0x0a, 0x0a		; 换行2次
		DB		"Succeed!(from Liao_Zhaoming)"
		DB		0x0a			; 换行
		DB		0

msg:
		DB		0x0a, 0x0a		; 换行2次
		DB		"hello, world!(from Liao_Zhaoming)"
		DB		0x0a			; 换行
		DB		0

		RESB	0x7dfe-$		; 填写0x00，直到 0x001fe

		DB		0x55, 0xaa
