; naskfunc
; TAB=4

; intel家族
; 8086->80186->286->386->486->Pentium->PentiumPro->PentiumII
; 286的CPUcpu地址总线还是16位  386开始CPU地址总线就是32位

[FORMAT "WCOFF"]
[INSTRSET "i486p"]				; 使用486的语言，避免EAX当成标签
[BITS 32]						; 位数是32位


[FILE "naskfunc.nas"]			; 

		GLOBAL	_io_hlt, _io_cli, _io_sti, _io_stihlt
		GLOBAL	_io_in8,  _io_in16,  _io_in32
		GLOBAL	_io_out8, _io_out16, _io_out32
		GLOBAL	_io_load_eflags, _io_store_eflags
		GLOBAL	_load_gdtr, _load_idtr
		GLOBAL	_load_cr0, _store_cr0
		GLOBAL  _load_tr
		GLOBAL	_asm_inthandler0d, _asm_inthandler20, _asm_inthandler21, _asm_inthandler27, _asm_inthandler2c
		GLOBAL	_memtest_sub
		GLOBAL	_farjmp
		GLOBAL  _farcall
		GLOBAL	_asm_hrb_api, _start_app
		EXTERN	_inthandler0d, _inthandler20, _inthandler21, _inthandler27, _inthandler2c
		EXTERN  _hrb_api

[SECTION .text]

_io_hlt:	; void io_hlt(void); 暂停cpu
		HLT
		RET

_io_cli:	; void io_cli(void); 禁止中断
		CLI
		RET

_io_sti:	; void io_sti(void); 允许中断
		STI
		RET

_io_stihlt:	; void io_stihlt(void);
		STI
		HLT
		RET

_io_in8:	; int io_in8(int port);
		MOV		EDX,[ESP+4]		; port
		MOV		EAX,0
		IN		AL,DX
		RET

_io_in16:	; int io_in16(int port);
		MOV		EDX,[ESP+4]		; port
		MOV		EAX,0
		IN		AX,DX
		RET

_io_in32:	; int io_in32(int port);
		MOV		EDX,[ESP+4]		; port
		IN		EAX,DX
		RET

_io_out8:	; void io_out8(int port, int data);
		MOV		EDX,[ESP+4]		; port
		MOV		AL,[ESP+8]		; data
		OUT		DX,AL
		RET

_io_out16:	; void io_out16(int port, int data);
		MOV		EDX,[ESP+4]		; port
		MOV		EAX,[ESP+8]		; data
		OUT		DX,AX
		RET

_io_out32:	; void io_out32(int port, int data);
		MOV		EDX,[ESP+4]		; port
		MOV		EAX,[ESP+8]		; data
		OUT		DX,EAX
		RET

_io_load_eflags:	; int io_load_eflags(void);
		PUSHFD		; wpush EFLAGS到EAX
		POP		EAX
		RET

_io_store_eflags:	; void io_store_eflags(int eflags);
		MOV		EAX,[ESP+4]
		PUSH	EAX
		POPFD		; POP EFLAGS从EAX
		RET

_load_gdtr:		; void load_gdtr(int limit, int addr);
		MOV		AX,[ESP+4]		; limit
		MOV		[ESP+6],AX
		LGDT	[ESP+6]
		RET

_load_idtr:		; void load_idtr(int limit, int addr);
		MOV		AX,[ESP+4]		; limit
		MOV		[ESP+6],AX
		LIDT	[ESP+6]
		RET

_load_cr0:		; int load_cr0(void);
		MOV		EAX,CR0
		RET

_store_cr0:		; void store_cr0(int cr0);
		MOV		EAX,[ESP+4]
		MOV		CR0,EAX
		RET
		
; void load_tr(int tr)
; 改变TR(task register<任务寄存器:用来记录当前运行的任务>)的值，为任务切换做准备
_load_tr:
		LTR    [ESP+4]
		RET

_asm_inthandler0d:
		STI
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		AX,SS
		CMP		AX,1*8    
		JNE		.from_app
; 如果是在操作系统发生中断，直接执行就好
		MOV		EAX,ESP
		PUSH	SS				; 保存中断的SS
		PUSH	EAX				; 保存中断的ESP
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler0d
		ADD		ESP,8
		POPAD
		POP		DS
		POP		ES
		ADD     ESP,4 ; 要在INT 0x0d中需要这句
		IRETD
; 如果是在应用程序发生了中断，需要切换DS/SS,起到保护作用
.from_app
		CLI
		MOV		EAX,1*8
		MOV		DS,AX			; 先仅将DS设置为操作系统使用
		MOV		ECX,[0xfe4]		; 操作系统ESP
		ADD		ECX,-8
		MOV		[ECX+4],SS		; 保存中断的SS
		MOV		[ECX  ],ESP		; 保存中断的ESP
		MOV		SS,AX
		MOV		ES,AX
		MOV		ESP,ECX
		STI
		CALL	_inthandler0d
		CLI
		CMP     EAX,0
		JNE     .kill
		POP		ECX
		POP		EAX
		MOV		SS,AX			; 将SS设回应用程序使用
		MOV		ESP,ECX			; 将ESP设回应用程序使用
		POPAD
		POP		DS
		POP		ES
		ADD     ESP,4
		IRETD
.kill:
;	将应用程序强制结束
		MOV		EAX,1*8			; 操作系统用的DS/SS
		MOV		ES,AX
		MOV		SS,AX
		MOV		DS,AX
		MOV		FS,AX
		MOV		GS,AX
		MOV		ESP,[0xfe4]		; 强制返回到start_app的ESP
		STI			; 切换完成恢复中断请求
		POPAD	; 恢复之前保存的寄存器
		RET

; 处理中断时，可能发生在正在执行的函数中，所以需要将寄存器的值保存下来
_asm_inthandler20:
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		AX,SS
		CMP		AX,1*8    
		JNE		.from_app
; 如果是在操作系统发生中断，直接执行就好
		MOV		EAX,ESP
		PUSH	SS				; 保存中断的SS
		PUSH	EAX				; 保存中断的ESP
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler20
		ADD		ESP,8
		POPAD
		POP		DS
		POP		ES
		IRETD
; 如果是在应用程序发生了中断，需要切换DS/SS,起到保护作用
.from_app
		MOV		EAX,1*8
		MOV		DS,AX			; 先仅将DS设置为操作系统使用
		MOV		ECX,[0xfe4]		; 操作系统ESP
		ADD		ECX,-8
		MOV		[ECX+4],SS		; 保存中断的SS
		MOV		[ECX  ],ESP		; 保存中断的ESP
		MOV		SS,AX
		MOV		ES,AX
		MOV		ESP,ECX
		CALL	_inthandler20
		POP		ECX
		POP		EAX
		MOV		SS,AX			; 将SS设回应用程序使用
		MOV		ESP,ECX			; 将ESP设回应用程序使用
		POPAD
		POP		DS
		POP		ES
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
		MOV		AX,SS
		CMP		AX,1*8    
		JNE		.from_app
; 如果是在操作系统发生中断，直接执行就好
		MOV		EAX,ESP
		PUSH	SS				; 保存中断的SS
		PUSH	EAX				; 保存中断的ESP
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler27
		ADD		ESP,8
		POPAD
		POP		DS
		POP		ES
		IRETD
; 如果是在应用程序发生了中断，需要切换DS/SS,起到保护作用
.from_app
		MOV		EAX,1*8
		MOV		DS,AX			; 先仅将DS设置为操作系统使用
		MOV		ECX,[0xfe4]		; 操作系统ESP
		ADD		ECX,-8
		MOV		[ECX+4],SS		; 保存中断的SS
		MOV		[ECX  ],ESP		; 保存中断的ESP
		MOV		SS,AX
		MOV		ES,AX
		MOV		ESP,ECX
		CALL	_inthandler27
		POP		ECX
		POP		EAX
		MOV		SS,AX			; 将SS设回应用程序使用
		MOV		ESP,ECX			; 将ESP设回应用程序使用
		POPAD
		POP		DS
		POP		ES
		IRETD

_asm_inthandler2c:
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		AX,SS
		CMP		AX,1*8    
		JNE		.from_app
; 如果是在操作系统发生中断，直接执行就好
		MOV		EAX,ESP
		PUSH	SS				; 保存中断的SS
		PUSH	EAX				; 保存中断的ESP
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler2c
		ADD		ESP,8
		POPAD
		POP		DS
		POP		ES
		IRETD
; 如果是在应用程序发生了中断，需要切换DS/SS,起到保护作用
.from_app
		MOV		EAX,1*8
		MOV		DS,AX			; 先仅将DS设置为操作系统使用
		MOV		ECX,[0xfe4]		; 操作系统ESP
		ADD		ECX,-8
		MOV		[ECX+4],SS		; 保存中断的SS
		MOV		[ECX  ],ESP		; 保存中断的ESP
		MOV		SS,AX
		MOV		ES,AX
		MOV		ESP,ECX
		CALL	_inthandler2c
		POP		ECX
		POP		EAX
		MOV		SS,AX			; 将SS设回应用程序使用
		MOV		ESP,ECX			; 将ESP设回应用程序使用
		POPAD
		POP		DS
		POP		ES
		IRETD
		
_memtest_sub:	; unsigned int memtest_sub(unsigned int start, unsigned int end)
		PUSH	EDI						; （由于本程序会使用EBX,ESI,EDI会改变这里值，所以要先保存起来，程序执行完再恢复）
		PUSH	ESI
		PUSH	EBX
		MOV		ESI,0xaa55aa55			; pat0 = 0xaa55aa55;
		MOV		EDI,0x55aa55aa			; pat1 = 0x55aa55aa;
		MOV		EAX,[ESP+12+4]			; i = start;
		
; 每0x1000检查一次，检查0x1000最后四个字节
mts_loop:
		MOV		EBX,EAX
		ADD		EBX,0xffc				; p = i + 0xffc;
		MOV		EDX,[EBX]				; old = *p 先记住修改前的值
		MOV		[EBX],ESI				; *p = pat0; 试写
		XOR		DWORD [EBX],0xffffffff	; *p ^= 0xffffffff; 反转，异或达到取反的效果，pat0的二进制是10交差
		CMP		EDI,[EBX]				; if (*p != pat1) goto fin; 检查反转结果
		JNE		mts_fin
		XOR		DWORD [EBX],0xffffffff	; *p ^= 0xffffffff;  再次反转
		CMP		ESI,[EBX]				; if (*p != pat0) goto fin; 检查是否恢复
		JNE		mts_fin
		MOV		[EBX],EDX				; *p = old;  恢复修改之前的值
		ADD		EAX,0x1000				; i += 0x1000;
		CMP		EAX,[ESP+12+8]			; if (i <= end) goto mts_loop;
		JBE		mts_loop
		POP		EBX
		POP		ESI
		POP		EDI
		RET
mts_fin:
		MOV		[EBX],EDX				; *p = old;
		POP		EBX
		POP		ESI
		POP		EDI
		RET

; 使用far模式jmp来实现任务切换 段号*8:0, 跳转到地址是TSS，执行任务切换的操作
;_taskswitch4:	; void taskswitch4(void);  切换到任务4
;		JMP		4*8:0  
;		RET

; JMP FAR”指令的功能是执行far跳转。在JMP FAR指令中，可以指定一个内存地址，
; CPU会从指定的内存地址中读取4个字节的数据，
; 并将其存入EIP寄存器，再继续读取2个字节的数据，并将其存入CS寄存器
_farjmp:		; void farjmp(int eip, int cs);
		JMP		FAR	[ESP+4]				; eip, cs
		RET

; 不同段的函数调用, 形式同_farjmp
_farcall:       ; void farcall(int eip, int cs);
		CALL    FAR [ESP+4]             ; eip, cs
		RET

;_asm_cons_putchar:
;		STI                 ; INT调用时，对于CPU来说相当于执行了中断处理程序，
;		                    ; 因此在调用的同时CPU会自动执行CLI指令来禁止中断请求, 我们使用STI允许中断
;		PUSHAD
;		PUSH	1
;		AND		EAX,0xff	; 将AH和EAX的高位置0
;		PUSH	EAX         ; 将EAX置为已存入字符编码的状态
;		PUSH	DWORD [0x0fec]	; 读取内存并push该值
;		CALL	_cons_putchar
;		ADD		ESP,12		; 栈中数据丢弃
;		POPAD
;		IRETD               ; 使用INT调用时返回


_asm_hrb_api:
		; 为方便起见从开头禁止中断请求
		PUSH	DS
		PUSH	ES
		PUSHAD		; 用于保存的push
		MOV		EAX,1*8
		MOV		DS,AX			; 先仅将DS设定为操作系统用
		MOV		ECX,[0xfe4]		; 操作系统的ESP
		ADD		ECX,-40
		MOV		[ECX+32],ESP	; 保存应用程序的ESP
		MOV		[ECX+36],SS		; 保存应用程序的SS

; 将PUSHAD后的值复制到系统栈
		MOV		EDX,[ESP   ]
		MOV		EBX,[ESP+ 4]
		MOV		[ECX   ],EDX	; 复制传递给hrb_api
		MOV		[ECX+ 4],EBX	; 复制传递给hrb_api
		MOV		EDX,[ESP+ 8]
		MOV		EBX,[ESP+12]
		MOV		[ECX+ 8],EDX	; 复制传递给hrb_api
		MOV		[ECX+12],EBX	; 复制传递给hrb_api
		MOV		EDX,[ESP+16]
		MOV		EBX,[ESP+20]
		MOV		[ECX+16],EDX	; 复制传递给hrb_api
		MOV		[ECX+20],EBX	; 复制传递给hrb_api
		MOV		EDX,[ESP+24]
		MOV		EBX,[ESP+28]
		MOV		[ECX+24],EDX	; 复制传递给hrb_api
		MOV		[ECX+28],EBX	; 复制传递给hrb_api

		MOV		ES,AX			; 将剩余的段寄存器设置为操作系统使用
		MOV		SS,AX
		MOV		ESP,ECX
		STI			; 恢复中断

		CALL	_hrb_api

		MOV		ECX,[ESP+32]	; 取出应用程序的ESP
		MOV		EAX,[ESP+36]	; 取出应用程序的ESP
		CLI
		MOV		SS,AX
		MOV		ESP,ECX
		POPAD
		POP		ES
		POP		DS
		IRETD		; 这个命令会自动执行STI

_start_app:		; void start_app(int eip, int cs, int esp, int ds);
		PUSHAD		; 将32位寄存器的值全部保存起来
		MOV		EAX,[ESP+36]	; 应用程序用EIP
		MOV		ECX,[ESP+40]	; 应用程序用CS
		MOV		EDX,[ESP+44]	; 应用程序用ESP
		MOV		EBX,[ESP+48]	; 应用程序用DS/SS
		MOV		[0xfe4],ESP		; 操作系统用ESP
		CLI			; 切换过程中禁止中断
		MOV		ES,BX
		MOV		SS,BX
		MOV		DS,BX
		MOV		FS,BX
		MOV		GS,BX
		MOV		ESP,EDX
		STI			; 切换完成后恢复中断请求
		PUSH	ECX				; far-CALL的PUSH(cs)
		PUSH	EAX				; far-CALL的PUSH(eip)
		CALL	FAR [ESP]		; 调用应用程序

;	程序结束后返回此处

		MOV		EAX,1*8			; 操作系统用的DS/SS
		CLI			; 再次进行切换，禁止中断
		MOV		ES,AX
		MOV		SS,AX
		MOV		DS,AX
		MOV		FS,AX
		MOV		GS,AX
		MOV		ESP,[0xfe4]
		STI			; 切换完成后恢复中断
		POPAD	; 恢复之前保存的寄存器值
		RET