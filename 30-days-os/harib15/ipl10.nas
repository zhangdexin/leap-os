; leapos-ipl
; TAB=4
; 启动区512字节
; 计算器读写软盘的时候，并不是1字节的读写的，而是以512字节为单位读写，
; 因此软盘的512字节就称为一个扇区，一张软盘共有2880个扇区
; 第一个扇区称为启动区


CYLS   EQU  10      ; define

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
; 读磁盘CYLS(10)个柱面的数据到0x08200～0x34fff
; IPL的启动区，位于C0-H0-S1（柱面0，磁头0，扇区1的缩写）
; 指定内存的地址，必须同时指定段寄存器，如果省略DS作为默认的段寄存器
; 调用BIOS命令时，ES:BX=缓冲地址；(校验及寻道时不使用)
;     返回值：FLACS.CF==0：没有错误，AH==0 
;             FLAGS.CF==1：有错误，错误号码存入AH内（与重置（reset）功能一样）
	MOV  AX,0x0820
	MOV  ES,AX      ; 指定[ES:BX]为读取的位置，加载到ES*16+AX=0x8200位置
	MOV  CH,0       ; 柱面0
	MOV  DH,0       ; 磁头0
	MOV	 CL,2		; 扇区2
	
readloop:
	MOV	 SI,0		; 失败次数
	
retry:
	MOV	 AH,0x02	; AH=0x02,表示调用BIOS读取磁盘
	MOV	 AL,1		; 1个扇区
	MOV	 BX,0
	MOV	 DL,0x00	; A驱动器
	INT	 0x13		; 陷入中断调用BIOS
	JNC	 next		; JNC,jump not carry(进位), 表示没出错就跳转
	ADD	 SI,1		; SI加1
	CMP	 SI,5		; SI和5比较
	JAE	 error		; SI >= 5时跳到error, JAE,jump above or equal
	MOV	 AH,0x00
	MOV	 DL,0x00
	INT	 0x13		; AH=0,DL=0,重置驱动器,便于重新读取
	JMP	 retry
	
next:
	MOV	 AX,ES		; 内存地址后移0x200
	ADD	 AX,0x0020
	MOV	 ES,AX		; ADD ES,0x020 因为没有ADD ES,所以这里绕弯
	ADD	 CL,1		; CL加1，下一个扇区
	CMP	 CL,18		; CL和18比较
	JBE	 readloop	; CL <= 18跳到readloop，JBE,jump below equal
	MOV	 CL,1
	ADD	 DH,1       ; 下一个磁头
	CMP	 DH,2
	JB	 readloop	; DH < 2跳到readloop, JE,jump below
	MOV	 DH,0
	ADD	 CH,1       ; 下一个柱面
	CMP	 CH,CYLS
	JB	 readloop	; CH < CYLS跳到readloop
	
	MOV		[0x0ff0],CH  ; IPL记录CH的读取位置
	JMP		0xc200       ; 规定执行程序写在0x4200以后的地方，所以跳到0x8200+0x4200=0xc200处开始执行程序   

error:
	MOV  SI,msg
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

	RESB	0x7dfe-$		; 0x7dfe地址到此处填充0

	DB		0x55, 0xaa      ; 启动区的最后两个字节是0x55和0xaa表示是操作系统程序
