[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "crack2.nas"]

		GLOBAL  _HariMain
		
[SECTION .text]

_HariMain:
		MOV		EAX,1*8			; 操作系统用数据段号
		MOV		DS,AX			; 将段号赋给DS寄存器
		MOV		BYTE [0x102600],0
		MOV		EDX,4
		INT		0x40
