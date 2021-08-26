## 实模式切换到保护模式-2

### 前言
上一篇文章我们讲述了关于实模式切换到保护模式一些基础知识，主要是为了方便我们理解这篇文章，我们这篇文章主要是来分析启动区加载并启动之后的代码，这一块代码也是用汇编来写的，文件是harib/haribote/asmhead.nas，这文件的代码是实模式切换到保护模式的功能，之后便可以衔接c语言的代码，话不多说，直接来看。

### 代码分析
操作系统从大的方向来划分可以分成是3个部分，第一个部分使我们之前将的启动区（ipl09），然后就是现在讲的asmhead，再然后就是我们用c语言来写的bootpack。我们从**环境准备**那边文章可以看到，是将这三个部分合并在一起编译成一个操作系统。<br>

接下来我们来看asmhead.nas代码

#### 画面模式设定
```
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
```
首先来看这段代码，INSTRSET指令，是为了能够使用386以后的LGDT, EAX, CR0等关键字，因为我们现在是实模式，即使用8086的一些指令，386以后的的LGDT，EAX指令都是不可以使用，可能会当做标签来处理，加上INSTRSET指令便可以使用。
> 正好通过这里我们看下intel系列发展顺序：<br>
> intel家族:<br>
> 8086->80186->286->386->486->Pentium->PentiumPro->PentiumII<br>
> 286的CPU地址总线还是16位  386开始CPU地址总线就是32位<br>

下边是一些变量的声明，主要是一些信息的地址，我们后边用到的时候来讲。
“ORG  0xc200”最后就是告诉编译器我们程序的开始位置是0xc200，之后用到的地址需要通过这个偏移计算，我们讲**引导启动**中也提过。<br><br>

```
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
```
上边注释相对来说比较清楚了，这一段代码是我们要设定画面的模式。一些显卡的公司协商成立了VESA协会（VideoElectronics Standards Association，视频电子标准协会），从而制作了专用的BIOS，这个追加的BIOS称为VESA BIOS extension”（VESA-BIOS扩展，简略为VBE）。利用它，就可以使用显卡的高分辨率功能了。
这样如果不使用VBE就设定“AH = 0; AL=画面模式号码；”，如果使用VBE，就设定“AX = 0x4f02;BX = 画面模式号码+0x4000”。
>VBE的画面模式号码有：<br>
> 0x101  -> 640*480*8 <br>
> 0x103  -> 800*600*8 <br>
> 0x105  -> 1024*768*8 <br>
> 0x107  -> 1280*1024*8(QEMU中不能指定到0x107) <br>

- 首先第一步要确定是否可以使用vbe，设置[ES:DI]的地址，AX设置为0x4f00（固定数字），执行中断INT 0x10，执行成功就会将相关信息写入到[ES:DI]内存中，不可以使用则会跳转scrn320
- 继续检查vbe的版本，这里版本就是从上一步存放信息的地址取出。版本小于2会跳转到scrn320，版本2以上才满足条件
- 检查是否支持所选的画面模式，我们这里选择"0x105  -> 1024*768*8",不支持的话跳转到scrn32
- 进行一些画面信息的校验，校验没有问题的话切换到vbe模式，切换到使用VBE的画面模式时用“AX = 0x4f02;BX = 画面模式号码+0x4000。除此之外，需要将画面的颜色位数，分辨率x，y，显卡内存地址都写入到我们前边所说的变量声明的内存地址中（VMODE，SCRNX，SCRNY，VRAM）。
- 最后如果无法使用VBE，那我们就只能乖乖使用比较原始的VGA图了，320\*320\*8,同样也需要将画面信息放到内存中，以便于后边程序使用


#### 获取键盘指示灯的状态
```
; 用BIOS取得键盘上各种LED指示灯的状态
keystatus:
    MOV  AH,0x02
	INT  0x16 			; keyboard BIOS
	MOV	 [LEDS],AL
```
这里比较简单通过BIOS将指示灯的状态存放在[LEDS]位置处，LEDS也是我们前边声明的变量

#### 切换到保护模式
```
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
```
模式切换过程中要禁止一切中断。这里由一个OUT指令，用来访问外设，并进行一些操作。这里是阻止PIC中断，CLI禁止CPU级别的中断。这里中断的细节我们到后边再讲。
> CPU访问外部设备有两种方式：<br>
> 内存映射，通过地址总线将外设的内存映射到内存区域（这一块我们在**引导启动**讲过）<br>
> 端口操作：外设都是由自己的控制器，控制器上有寄存器，即为所谓端口，通过IN/OUT指令来访问外设硬件的内存

```
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
```
这段代码不是很好理解，因为涉及到很多中断的知识，我们简要的说明一下，等到后边讲到中断的时候，大家可能就理解了。这里我们要向键盘发送指令，是指令键盘控制电路的附属端口（0xd1）输出0xdf。这个附属端口，连接着主板上的很多地方，通过这个端口发送不同的指令，就可以实现各种各样的控制功能。因为cpu发送到硬件比较慢，waitkbdout表示等待发送的指令到达硬件并完成。<br>
所以这段代码意思就是向外设通知发送0xdf指令，即为设定打开A20GATE。即为让我们的cpu可以访问内存1M以上的空间，我们使用实模式只能是1M(地址总线是19~0)，所以这里命名是A20。<br>

```
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
```