; leap-os boot asm
; TAB=4

[INSTRSET "i486p"]

BOTPAK	EQU		0x00280000		; bootpack目的地
DSKCAC	EQU		0x00100000		; 磁盘缓存位置
DSKCAC0	EQU		0x00008000		; 磁盘缓存位置（实时模式）

; 有关BOOT_INFO存放地址
CYLS	EQU		0x0ff0			; 设定启动区
LEDS	EQU		0x0ff1
VMODE	EQU		0x0ff2			; 关于颜色数目信息，颜色位数
SCRNX	EQU		0x0ff4			; 分辨率x
SCRNY	EQU		0x0ff6			; 分辨率x
VRAM	EQU		0x0ff8			; 存放图像缓冲区的开始地址


	ORG  0xc200
	
; 画面设定
; 切换到不使用VBE的画面模式时用“AH = 0; AL=画面模式号码；”
; 而切换到使用VBE的画面模式时用“AX = 0x4f02;BX = 画面模式号码+0x4000
; VBE的画面模式号码如下
; 0x101  -> 640*480*8
; 0x103  -> 800*600*8
; 0x105  -> 1024*768*8
; 0x107  -> 1280*1024*8(QEMU中不能指定到0x107)

; 确认vbe是否存在
  ;对ES和DI进行赋值，如果显卡可以使用VBE，
  ;相关的信息要写入到[ES:DI]开始的512字节中,指定内存地址而已
    MOV		AX,0x9000
	MOV		ES,AX
	MOV		DI,0
	MOV		AX,0x4f00
	INT		0x10
	CMP  AX, 0x004f ; 执行INT 10, 如果有VBE,AX就会变成0x004f
	JNE  scrn320

; 检查vbe的版本
  ; VBE版本在2.0以上才可以使用高分辨率
	MOV  AX,[ES:DI+4]
	CMP	 AX,0x0200
	JB   scrn320

VBEMODE EQU     0x105           ; 选择一个画面模式
; 取得画面模式信息, 如果这个画面模式不支持AX是0x004f以外的指
  ; 取得画面模式信息写入到[ES:DI]开始的256字节中
    MOV		CX,VBEMODE
	MOV		AX,0x4f01
	INT		0x10
	CMP		AX,0x004f
	JNE		scrn320

; 取得的画面模式信息确认
  ; 画面模式信息重要信息：
  ; WORD [ES:DI+0x00] 模式属性
  ; WORD [ES:DI+0x12] x的分辨率
  ; WORD [ES:DI+0x14] y的分辨率
  ; BYTE [ES:DI+0x19] 颜色数，必须是8
  ; BYTE [ES:DI+0x1b] 颜色指定方法，必须为4（调色板模式）
  ; DWORD [ES:DI+0x28] VRAM的地址
	CMP  BYTE [ES:DI+0x19],8
	JNE  scrn320
	CMP	 BYTE [ES:DI+0x1b],4
	JNE	 scrn320
	MOV	 AX,[ES:DI+0x00]
	AND	 AX,0x0080
	JZ   scrn320 ; 模式属性的bit7是0就放弃

; 检查完毕，切换模式
	MOV		BX,VBEMODE+0x4000
	MOV		AX,0x4f02
	INT		0x10
	MOV		BYTE [VMODE],8
	MOV		AX,[ES:DI+0x12]
	MOV		[SCRNX],AX
	MOV		AX,[ES:DI+0x14]
	MOV		[SCRNY],AX
	MOV		EAX,[ES:DI+0x28]
	MOV		[VRAM],EAX
	JMP		keystatus ; 跳过scrn320

scrn320:
	MOV		AL,0x13			; VGA图 320x200x8bit彩色
	MOV		AH,0x00
	INT		0x10
	MOV		BYTE [VMODE],8
	MOV		WORD [SCRNX],320
	MOV		WORD [SCRNY],200
	MOV		DWORD [VRAM],0x000a0000
	
; 用BIOS取得键盘上各种LED指示灯的状态
keystatus:
    MOV  AH,0x02
	INT  0x16 			; keyboard BIOS
	MOV	 [LEDS],AL
	
; PIC关闭一切中断，cpu模式转换时禁止中断
;   根据AT兼容机规格，如果要初始化PIC, 必须在CLI之前，否则有时会挂起
;   随后进行PIC初始化
; OUT 0x21,AL 即 io_out(PIC0_IMR, 0xff)
; OUT 0xa1,AL 即 io_out(PIC1_IMR, 0xff)
	MOV	 AL,0xff
	OUT	 0x21,AL
	NOP	 				; OUT命令什么也不做，CPU休息一个时钟周期
	OUT	 0xa1,AL

	CLI					; 禁止CPU级别的中断
	
; 为了CPU能够访问1M以上内存空间，设定A20GATE
; 向键盘控制电路发送指令
; 这里的的指令是指令键盘控制电路附属端口输出0xdf
; 这个附属端口连接主板多个地方，发送不同指令，实现不同的控制功能
; 0xdf完成A20GATE信号变成ON, 可以使内存1MB以上变成可使用状态
	CALL  waitkbdout     ; wait_KBC_sendready
	MOV	  AL,0xd1
	OUT	  0x64,AL
	CALL  waitkbdout
	MOV	  AL,0xdf			; enable A20
	OUT	  0x60,AL
	CALL  waitkbdout
		
		
; 切换到保护模式
; CR0(control register 0),只有操作系统才能操作
; 保护模式与先前16位模式不同，段寄存器解释不是乘以16，而是使用GDT
; 保护，应用程序不能随便改变断的设点，也不能使用操作系统专用断
; 切换到保护模式，立即执行JMP指令，机器语言解释发生改变，CPU解释指令发生改变，使用pipeline机制
[INSTRSET "i486p"]				; 使用486指令
	LGDT  [GDTR0]			; 设置临时GDT
	MOV	  EAX,CR0
	AND	  EAX,0x7fffffff	; 设bit31为0（禁止分页）
	OR	  EAX,0x00000001	; 设bit0为1（切换到保护模式）
	MOV	  CR0,EAX
	JMP	  pipelineflush

; 进入保护模式，段寄存器意思发生改变，除了CS以外所有段寄存器值从0x0000变成0x0008
; 0x0008相当于gdt+1的段
pipelineflush:
	MOV	 AX,1*8			;  読み書き可能セグメント32bit
	MOV	 DS,AX
	MOV	 ES,AX
	MOV	 FS,AX
	MOV	 GS,AX
	MOV	 SS,AX
	
; bootpack传送
	MOV	 ESI,bootpack	; 传送源
	MOV	 EDI,BOTPAK		; 传送目的
	MOV	 ECX,512*1024/4
	CALL memcpy

; 磁盘数据最终传送到本来位置

; 启动区
	MOV		ESI,0x7c00		; 传送源
	MOV		EDI,DSKCAC		; 传送目的
	MOV		ECX,512/4
	CALL	memcpy

; 剩下的
	MOV		ESI,DSKCAC0+512	; 传送源
	MOV		EDI,DSKCAC+512	; 传送目的
	MOV		ECX,0
	MOV		CL,BYTE [CYLS]
	IMUL	ECX,512*18*2/4	; 柱面数变换为字节数除以4
	SUB		ECX,512/4		; 减去IPL
	CALL	memcpy

; 必须由asmhead来完成的工作完成
; 以后交由bootpack来完成

; bootpack启动
; 解析bootpack的头部，值可能不同
; [EBX+16] bootpack.hrb的第16号地址，值是0x11a8
; [EBX+20] bootpack.hrb的第20号地址，值是0x10c8
; [EBX+12] bootpack.hrb的第12号地址，值是0x00310000
; 复制[EBX+20]开始[EBX+16]的字节复制到[EBX+12]地址去
	MOV		EBX,BOTPAK
	MOV		ECX,[EBX+16]
	ADD		ECX,3			; ECX += 3;
	SHR		ECX,2			; ECX /= 4;
	JZ		skip			; JZ jump zero
	MOV		ESI,[EBX+20]	; 転送元
	ADD		ESI,EBX
	MOV		EDI,[EBX+12]	; 転送先
	CALL	memcpy
skip:
	MOV		ESP,[EBX+12]	; 栈初始值
	JMP		DWORD 2*8:0x0000001b  ; 2*8放入CS里，移动0x1b地址，即到0x280000+0x1b,开始执行bootpack

waitkbdout:
		IN		 AL,0x64
		AND		 AL,0x02
		JNZ		waitkbdout
		RET

memcpy:
	MOV		EAX,[ESI]
	ADD		ESI,4
	MOV		[EDI],EAX
	ADD		EDI,4
	SUB		ECX,1
	JNZ		memcpy			; 减法运算结果如果不是0，跳转到memcpy
	RET

; 一直添加DB 0，直到地址能被16整除
	ALIGNB	16

; GDT0是特定的GDT, 0号是空区域，不定义段
GDT0:
		RESB	8
		DW		0xffff,0x0000,0x9200,0x00cf	; 可读写段32比特
		DW		0xffff,0x0000,0x9a28,0x0047	; 可执行段32比特（用于bootpack）

		DW		0
GDTR0:
		DW		8*3-1
		DD		GDT0

		ALIGNB	16
bootpack:
