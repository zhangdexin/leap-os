## 引导启动

### 引言
上一节我们讲了工程的结构和编译的流程，我们知道haribote是操作系统的代码文件夹，今天我们来讲述操作系统里的第一节，引导启动程序（ipl09），我们从源码角度来分析讲解下。

### 读取启动区之前
#### CPU的寻址空间
很久之前，intel的cpu 8086有20根地址线，能够访问的地址20位长，即2的20次方=1M，16进制表示的话0x00000到0xFFFFF。
所以cpu刚刚开机的时候能够访问的内存地址是1M，这个被称做实模式。CPU可以访问的1M地址并不是完全分给内存。
<img width="60%" height="60%" src="https://user-images.githubusercontent.com/22785392/127582498-e7e03033-c5de-4fba-8cc9-fcad9e176d36.png" />
如图，CPU的寻址空间0x00000～0x9FFFF(640K)是分给内存的，0xA0000~0xBFFFF(128K)是分给了显卡的，剩下的256K是分给了ROM里的BIOS。
我们经常会说到我的CPU的32位的那么寻址空间就是4G，但是这4G并不是完全分到内存，所以我们看到电脑会显示内存只使用了3.8G左右的原因了

#### 启动BIOS
上一个小节我们说到BIOS是放在ROM(只读存储器)中，ROM可以理解成一块断电后数据也不会丢失的存储空间。CPU的访问存储空间是用段地址+偏移地址来寻址。即CS:IP的形式去访问内存地址(这里内存不是完全指内存)，CS和IP都是寄存器的，关于寄存器这里也就不展开说了。实模式下寄存器的大小是16位，要访问到1M的地址空间，是段基址(CS)*16,也即CS的值左移4位，加上IP的值，那么就可以访问到1M了。<br/>
* 通电的瞬间，CPU的cs:ip被强制初始化为0xF000:0xFFF0，那么访问的地址就是0xFFFF0，这里便是BIOS的入口地址。然后BIOS的真正的代码也不是在这里，这里的代码也还是个跳转指令，跳转到0xFE05B<br/>
* 然后BIOS开始检测内存，显卡等硬件，当检测完成后，开始在0x000～0x3FF处建立数据结构, 中断向量表<br/>
* 最后BIOS去寻找启动盘的0盘0道1扇区的内容，即磁盘上最开始扇区。关于磁盘我也有篇文章讲到[磁盘](https://blog.csdn.net/leapmotion/article/details/118481004).检查这个扇区的最后两个字节是0x55和0xaa，便认为此扇区存在操作系统，便加载该扇区到0x7c00物理地址，然后跳转到该地址（jmp 0: 0x7c00）开始执行这里边的内容。

### ipl程序
IPL(InitialProgram Loader), 在项目的文件是ipl09.nas，09表示都去9个柱面，这9个柱面就能装下一个操作系统了，9\*2\*18*512约为162KB，很小吧，作者也是很注重他的小巧，书中为了大小进行了多次优化（包括压缩）
从代码看起：
```
; leapos-ipl
; TAB=4
; 启动区512字节
; 计算器读写软盘的时候，并不是1字节的读写的，而是以512字节为单位读写，
; 因此软盘的512字节就称为一个扇区，一张软盘共有2880个扇区
; 第一个扇区称为启动区


CYLS   EQU  9      ; define 要读取的柱面个数
ORG  0x7c00     ; 程序要读到的位置
```

先看两行代码，这两行代码都是给编译器看的，并不会翻译成机器语言
* 'CYLS EQU 9'类似一个C语言#define语句，EQU即为equal，告诉编译器，CYLS这个变量是9, 当翻译成机器语言的时候CYLS变量都替换为9。
* 'ORG  0x7c00'，0x7c00是ipl程序加载到内存的地址，但是为什么要有一句ORG呢，其实ORG仅仅是告诉编译器我们的起始地址是0x7c00，方便编译器对一些内存地址的转换。

```
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
```
这一下指令有点多，'JMP entry'是机器语言的第一条指令。本书的启动盘是软盘，和我们平常说的硬盘启动还有一些不一样，一个单磁盘的硬盘，它拥有80个柱面（0-79），2个磁头（正反面，0-1），18个扇区（1-18），总的大小就是80×2×18×512=1474560Byte=1440KB，所以也能看到我们最后img大小就是1440KB。
继续程序这里，首先cpu会读取软盘的前77个字节来判断软盘类型，上边FAT12格式的软盘的前77个字节的描述。而最重要的时第一条，即程序的第一条指令。注释已经说明了每个字节的说明，或者说这些都是规定。

```
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
	MOV  AX,0x0820
	MOV  ES,AX      ; 指定[ES:BX]为读取的位置，加载到ES*16+BX=0x8200位置
	MOV  CH,0       ; 柱面0
	MOV  DH,0       ; 磁头0
	MOV	 CL,2		; 扇区2
	MOV  BX,18*2*CYLS-1 ; 要读取的合计扇区
	CALL readfast   ; 调用读取
```

我们到程序的入口，进行一些初始化的工作，然后指定了要读取的内容到地址0x8200，总共要读取的扇区是18*2*CYLS再减去我们正在使用的第一个扇区，然后我们调用readfast函数进行读取操作。

这里我们还要插一个别的话题，上边有说到BIOS初始化的时候，会在0x000～0x3FF处建立**中断向量表**，中断向量表是在实模式下用来处理中断，通过中断指令"INT 中号"来调用。这种中断也被称为是BIOS中断，BIOS中断主要的功能还是提供了硬件访问的办法，比如说我们接下来讲到的读盘。中断向量表中存放的就是各个中断号和中断处理程序的对应。大致流程就是BIOS把中断向量表建立完成后，当CPU执行到"INT 中断号"指令时，那么cpu就会找到相应的处理程序去调用。
这里说下我们要读盘会怎样操作呢

```
“INT 0x13”这个指令用于磁盘读、写，扇区校验（verify），以及寻道（seek）

同时也还要对以下寄存器进行设置，才能执行相应的功能
AH=0x02;（读盘）
AH=0x03;（写盘）
AH=0x04;（校验）
AH=0x0c;（寻道）

AL=处理对象的扇区数；（只能同时处理连续的扇区）
CH=柱面号 &0xff；
CL=扇区号（0-5位）|（柱面号&0x300）>>2；
DH=磁头号；DL=驱动器号；
ES:BX=缓冲地址；(校验及寻道时不使用)

返回值：
FLACS.CF==0：没有错误，AH==0
FLAGS.CF==1：有错误，错误号码存入AH内（与重置（reset）功能一样）
```

然后我们再继续看我们的代码
```
readfast:      ;使用AL尽量一次性读取数据
; ES: 读取地址   CH:柱面  DH:磁头  CL:扇区  BX:合计要读取的扇区

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
```
到readfast函数这里我们看下，代码比较长，这里的功能其实就是读区BX个扇区到[ES:BX]的内存位置。每次读取是AL个扇区，也可以看到我们程序最多只能读取一个柱面的一个磁头，即最多可以读取18个扇区。CL表示从哪个扇区开始读取。readfast函数，skip1和skip2标签我用伪代码表述下：
```
// readfast
AH = (ES / 32) % 128
AL = 128 - AH

if (BH == 0)
    AH = 18
else
    AH = BL
    
// skip1
if (AL > AH)
    AL = AH

// skip2
if (AL > 19 - CL)
    AL = 19 - CL
```
readfast那里只是给AL和AH赋值一个初值，然后skip判断AL和AH，AL取最小值，然后在和19-CL比较，19-CL正好界定了AL在[CL, 18]区间范围。
skip3做一部分初始化操作，接下来我们用读盘里，因为用到了[ES:BX]内存地址，所以需要把之前BX中的值压栈，SI用来计算失败次数的，初始化为0。
retry这里就是真正在读取数据了，设置参数AH，BX，DL，调用INT 13来读取，没有出错就到next，如果有错误，错误次数加1，超过5次，跳到error结束读取操作，否则初始化参数重试这次的读取。
next就是更新参数进行下一次的读取，更新ES的值（内存地址），CL的值，如果CL小于18直接从readfast再开始读取，如果CL大于等于18，则更新磁头或者柱面，然后再跳到readfast读取。
等到全部读取完成后返回开始调用readfast处。

我们继续看调用完readfast做什么操作
```
	; ....略
	MOV  DH,0       ; 磁头0
	MOV	 CL,2		; 扇区2
	MOV  BX,18*2*CYLS-1 ; 要读取的合计扇区
	CALL readfast   ; 调用读取


; 读取结束，运行haribote.sys
	MOV		BYTE [0x0ff0],CYLS  ; IPL实际读取了多少内容
	JMP		0xc200       ; 规定执行程序写在0x4200以后的地方，所以跳到0x8200+0x4200=0xc200处开始执行程序   
```
这里看我们把总共读取了的柱面个数存放到内存0x0ff0这个位置，使得后边程序的使用需要就可以到这个内存里去取，然后跳到我们刚刚读取的程序开始内存地址处。相当于我们启动程序是这些，但是操作系统程序是0x8200开始，从这里开始有一些声明的部分，文件信息等等之类的，真正的程序是从0x004200开始。

差点忘了，还有一个我们读取失败跳到error，这个error还没有说到，

```
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
```
我们看到又使用了一个BIOS的中断，INT 0x10，我们看下这个调用的详细情况：
```
INT 0x10 显示一个字符

AH=0x0e；
AL=character code；
BH=0；
BL=color code；
返回值：无

注：beep、退格（back space）、CR、LF都会被当做控制字符处理
```
看起来这里应该就是在屏幕上显示load error的信息吧<br>
最开始error标签处，SI赋值了msg的标签的地址, msg地方存放的时要显示的信息<br>
putloop开始调用，SI中的地址取到的值赋给AL，SI加1，判断AL是0的话结束输出跳转到fin，非0的话，输出AL中保存的字符，如此循环，就可以吧load error和换行符打印出来，msg最后是0用来控制循环结束<br>
fin是一个死循环，只执行让CPU睡眠（HLT）指令<br><br>

最后的最后，就是程序的结束处, 填充0及设置该扇区结束标志。
```
	RESB	0x7dfe-$		; 0x7dfe地址到此处填充0
	DB	0x55, 0xaa      ; 启动区的最后两个字节是0x55和0xaa表示是操作系统程序
```


### 总结
本章将启动盘的整体代码讲解了一遍，首先作者使用软盘启动，和硬盘稍有区别，即需要最开始的声明，然后ipl整个程序就是取读取操作系统和应用程序到内存当中，使用BIOS中断的方式读取9个柱面，如果失败就显示load error信息。
