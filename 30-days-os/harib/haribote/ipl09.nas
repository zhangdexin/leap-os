; leapos-ipl
; TAB=4
; 启动区512字节
; 计算器读写软盘的时候，并不是1字节的读写的，而是以512字节为单位读写，
; 因此软盘的512字节就称为一个扇区，一张软盘共有2880个扇区
; 第一个扇区称为启动区


CYLS   EQU  9      ; define 要读取的柱面个数

	ORG  0x7c00     ; 程序要读到的位置
	
; 以下是标准FAT12格式软盘的描述
    JMP  entry      ; JMP无条件跳转
	DB   0x90       ; define byte, 填充一个字节
	DB   "LEAPIPL " ; 启动区的名称(必须是8字节), ipl(initial program loader)
	DW   512        ; define word=2*DB,每个扇区的大小必须是512
	DB   1          ; 簇的大小(必须是1个扇区)
	DW   1          ; FAT的起始位置(一般是第1个扇区)
	DB   2          ; FAT的个数（必须是2）
	DW	 224		; 根目录的大小(一般设成224项)
	DW	 2880		; 该磁盘的大小（必须是2880扇区）
	DB	 0xf0		; 磁盘的种类（必须是0xf0）
	DW	 9			; FAT的长度（必须是9扇区）
	DW	 18			; 1个磁道（track）有几个扇区（必须是18）
	DW	 2			; 磁头数（必须是2）
	DD	 0			; 不使用分区，必须是0，define double word=2*DW
	DD	 2880		; 重写一次磁盘大小
	DB	 0,0,0x29		; 意义不明，固定
	DD	 0xffffffff		; （可能是）卷标号码
	DB	 "LEAP-OS    "	; 磁盘的名称（必须是11字节）
	DB	 "FAT12   "		; 磁盘格式名称（必须是8字节）
	RESB 18				; 空出18字节
	
; 程序主体
entry:
	MOV  AX,0
	MOV  SS,AX
	MOV  SP,0x7c00
	MOV  DS,AX

; 软盘共有80个柱面，2个磁头，每个柱面18个扇区，每个扇区512字节
; 读磁盘CYLS(9)个柱面的数据到0x08200～0x30a00
; IPL的启动区，位于C0-H0-S1（柱面0，磁头0，扇区1的缩写）
; 指定内存的地址，必须同时指定段寄存器，如果省略DS作为默认的段寄存器
; 调用BIOS命令时，ES:BX=缓冲地址；(校验及寻道时不使用)
;     返回值：FLACS.CF==0：没有错误，AH==0 
;             FLAGS.CF==1：有错误，错误号码存入AH内（与重置（reset）功能一样）
	MOV  AX,0x0820
	MOV  ES,AX      ; 指定[ES:BX]为读取的位置，加载到ES*16+BX=0x8200位置
	MOV  CH,0       ; 柱面0
	MOV  DH,0       ; 磁头0
	MOV	 CL,2		; 扇区2
	MOV  BX,18*2*CYLS-1 ; 要读取的合计扇区
	CALL readfast   ; 调用读取
	
; 读取结束，运行haribote.sys
	MOV		BYTE [0x0ff0],CYLS  ; IPL实际读取了多少内容
	JMP		0xc200       ; 规定执行程序写在0x4200以后的地方，所以跳到0x8200+0x4200=0xc200处开始执行程序   

error:
	MOV		AX,0
	MOV		ES,AX
	MOV		SI,msg

putloop:
	MOV	 AL,[SI]
	ADD	 SI,1			; SI加1
	CMP	 AL,0
	JE	 fin
	MOV	 AH,0x0e		; 显示文字
	MOV	 BX,15			; 指定颜色
	INT	 0x10			; 调用显卡BIOS
	JMP	 putloop
fin:
	HLT			
	JMP		fin
msg:
	DB		0x0a, 0x0a		; 换行
	DB		"load error"
	DB		0x0a			; 换行
	DB		0

readfast:      ;使用AL尽量一次性读取数据
; ES: 读取地址   CH:柱面  DH:磁头  CL:扇区  BX:读取的扇区

	MOV		AX,ES			; < 通过ES计算AL最大值 >
	SHL		AX,3			; AX除以32，结果存入AH
	AND		AH,0x7f			; AH是AH除以128所得的余数（512*128=64K）
	MOV		AL,128			; AL = 128 - AH;
	SUB		AL,AH

	MOV		AH,BL			; < 通过BX计算AL的最大值存入AH >
	CMP		BH,0			; if (BH != 0) { AH = 18; }
	JE		.skip1
	MOV		AH,18

.skip1:
	CMP		AL,AH			; if (AL > AH) { AL = AH; }
	JBE		.skip2
	MOV		AL,AH

.skip2:
	MOV		AH,19			; < 通过CL计算AL的最大值并存入AH >
	SUB		AH,CL			; AH = 19 - CL;
	CMP		AL,AH			; if (AL > AH) { AL = AH; }
	JBE		.skip3
	MOV		AL,AH

.skip3:
	PUSH	BX
	MOV		SI,0			; 计算失败次数

retry:
	MOV		AH,0x02			; AH=0x02 : 读取磁盘
	MOV		BX,0
	MOV		DL,0x00			; A驱动器
	PUSH	ES
	PUSH	DX
	PUSH	CX
	PUSH	AX
	INT		0x13			; 调用BIOS命令时，ES
	JNC		next			; 没有出错则跳转到next
	ADD		SI,1			; SI加1
	CMP		SI,5			; SI和5比较
	JAE		error			; SI >= 5 跳转到error
	MOV		AH,0x00
	MOV		DL,0x00			; A驱动器
	INT		0x13			; 驱动器重置
	POP		AX
	POP		CX
	POP		DX
	POP		ES
	JMP		retry

next:
	POP		AX
	POP		CX
	POP		DX
	POP		BX				; ES内容存入BX
	SHR		BX,5			; BX由16字节为单位转换为512字节为单位
	MOV		AH,0
	ADD		BX,AX			; BX += AL;
	SHL		BX,5			; BX由512字节为单位转为16字节为单位
	MOV		ES,BX			; 相当于 ES += AL * 0x20;
	POP		BX
	SUB		BX,AX
	JZ		.ret
	ADD		CL,AL			; CL加上AL
	CMP		CL,18			; CL与18比较
	JBE		readfast		; CL <= 18 跳转到readfast
	MOV		CL,1
	ADD		DH,1
	CMP		DH,2
	JB		readfast		; DH < 2 跳转到readfast
	MOV		DH,0
	ADD		CH,1
	JMP		readfast

.ret:
	RET

	RESB	0x7dfe-$		; 0x7dfe地址到此处填充0
	
	DB		0x55, 0xaa      ; 启动区的最后两个字节是0x55和0xaa表示是操作系统程序
