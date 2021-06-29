[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "crack7.nas"]

		GLOBAL	_HariMain

[SECTION .text]

_HariMain:
		MOV		AX,4
		MOV		DS,AX
		CMP		DWORD [DS:0x0004],'Hari'
		JNE		fin					; 不是应用程序，不做任何做操

		MOV		ECX,[DS:0x0000]		; 读取该应用程序数据段大小
		MOV		AX,12
		MOV		DS,AX

crackloop:							; 用123填充
		ADD		ECX,-1
		MOV		BYTE [DS:ECX],123
		CMP		ECX,0
		JNE		crackloop

fin:								; 结束
		MOV		EDX,4
		INT		0x40
