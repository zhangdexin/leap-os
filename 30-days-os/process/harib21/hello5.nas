[FORMAT "WCOFF"] ; WCOFF模式可以指定SECTION
[INSTRSET "i486p"]
[BITS 32]
[FILE "hello5.nas"]

		GLOBAL	_HariMain

[SECTION .text] ; 数据段

_HariMain:
		MOV		EDX,2
		MOV		EBX,msg
		INT		0x40
		MOV		EDX,4
		INT		0x40

[SECTION .data] ; 代码段

msg:
		DB	"hello, world", 0x0a, 0
