## 环境准备

该操作系统是由C语言和汇编语言实现，由于cpu可以运行的是机器语言，所以我们所编写的代码都需要转化为机器语言<br/>

    汇编语言 --->  机器语言
    高级语言 --->  机器语言

这里几乎所有的工具都是围绕着两个来展开


### 工程结构 ###
```
> dir harib

Mode         Name
----         ----
d----        a
d----        apilib
d----        bball
d----        beepdown
d----        calc
d----        chklang
d----        color
d----        gview
d----        haribote
d----        hello3
d----        hello4
d----        hello5
d----        invader
d----        iroha
d----        line
d----        mmldata
d----        mmlplay
d----        nihongo
d----        noodle
d----        notrec
d----        pictdata
d----        sosu
d----        star
d----        tek
d----        tview
d----        type
d----        typeipl
d----        walk
d----        winhello
-a---        !cons_9x.bat
-a---        !cons_nt.bat
-a---        apilib.h
-a---        app_make.txt
-a---        haribote.rul
-a---        image.iso
-a---        ipl09.nas
-a---        make.bat
-a---        Makefile
```
d表示目录(directory)，表示文件(archive)
其中haribote这个文件夹下是操作系统的全部代码


### 虚拟机 ###
为了调试方便，作者（书目的作者，下同）使用了虚拟机来运行img文件，作者使用的虚拟机是qemu
相关目录在z_tools/qemu下


### 汇编器 ###
汇编器就是将汇编代码编译成机器语言的工具
作者是自己开发汇编器nask，是基于大名鼎鼎的NASM基础上做一些改动，作者使用的汇编语言也是intel的指令集，简单的例子：
```
# 工程中汇编代码都是以.nas扩展名保存
..\z_tools\nask.exe helloos.nas helloos.img
```
这里例子就是将hellos.nas（汇编语言）翻译成helloos.img（机器语言），这时这个helloos.img可以被cpu执行，这里就可以被qemu执行


### C语言编译过程 ###
典型的C语言编译过程为：<br>
1、预编译 --> 2、编译 --> 3、汇编 --> 4、链接
- 预编译是将预处理指令（#include、#define、#ifdef 等#开始的代码行）进行处理，删除注释和多余的空白字符，生成一份新的代码
- 编译是将预编译步骤的产物编译成汇编代码
- 汇编是将编译步骤产生的汇编代码编译成目标文件
- 链接是将目标文件链接起来编译成机器语言

> 这里提一下，目标文件时一种比较特殊的机器语言文件，必须与其他文件链接才能，这也是c语言的一个比较麻烦的点，关于链接这里就不展开说了

理论大家都知道了，那实操呢，本书是这样进行c语言的编译
以bootpack.c为例：
- 第一步，使用cc1.exe从bootpack.c生成bootpack.gas
- 第二步，使用gas2nask.exe从bootpack.gas生成bootpack.nas
- 第三步，使用nask.exe从bootpack.nas生成bootpack.obj
- 第四步，使用obi2bim.exe从bootpack.obj生成bootpack.bim
- 第五步，使用bim2hrb.exe从bootpack.bim生成bootpack.hrb

解释下这些步骤：
- cc1.exe将C语言代码编译成汇编语言，但是cc1.exe是作者从gcc改造而来，所以变成成的汇编语言是gas的，这里gas是AT&T语法，作者使用intel语法（可以使用nask编译）。所以需要转化为nask认识的语法
- gas2nask.exe是将gas转为nask形式的汇编代码，所以这里扩展名就是nas了
- nask.exe将nas文件编译成obj文件，即目标文件
- bootpack.bim做链接工作，这里可能会链接多个obj。这里还没有转化为机器语言文件，这里作者是要做些为了适配该操作系统来做一些文件调整
- 左后bim2hrb.exe转为机器语言文件


### make工具链 ###

首先最开始说的是这个make的相关工具，可以将以上的翻译工作写成脚本（makefile），make命令执行即可。关于makefile的写法这里不展开讲



### 如何编译 ###
首先我们来看harb目录下有一个Makefile，每个app文件夹下都有一个Makefile，applib文件夹下也有Makefile，操作系统haribote下也有Makefile，我们先分别来看，从最简单入手。
- apilib的Makefile
  ```
    OBJS_API =	api001.obj api002.obj ...（略）

    TOOLPATH = ../../z_tools/
    INCPATH  = ../../z_tools/haribote/

    MAKE     = $(TOOLPATH)make.exe -r
    NASK     = $(TOOLPATH)nask.exe
    CC1      = $(TOOLPATH)cc1.exe -I$(INCPATH) -Os -Wall -quiet
    GAS2NASK = $(TOOLPATH)gas2nask.exe -a
    OBJ2BIM  = $(TOOLPATH)obj2bim.exe
    MAKEFONT = $(TOOLPATH)makefont.exe
    BIN2OBJ  = $(TOOLPATH)bin2obj.exe
    BIM2HRB  = $(TOOLPATH)bim2hrb.exe
    RULEFILE = ../haribote.rul
    EDIMG    = $(TOOLPATH)edimg.exe
    IMGTOL   = $(TOOLPATH)imgtol.com
    GOLIB    = $(TOOLPATH)golib00.exe 
    COPY     = copy
    DEL      = del

    default :
        $(MAKE) apilib.lib

    apilib.lib : Makefile $(OBJS_API)
        $(GOLIB) $(OBJS_API) out:apilib.lib

    %.obj : %.nas Makefile
        $(NASK) $*.nas $*.obj $*.lst

    clean :
        -$(DEL) *.lst
        -$(DEL) *.obj
        

    src_only :
        $(MAKE) clean
        -$(DEL) *.lib
  ```
    最开始是定义了一些变量，我们的编译工具都是在z_tools里<br/>
    如果我们在这个文件夹执行make命令，就是make default产生apilib.lib, 而apilib.lib通过golib00.exe将所有的obj链接成lib文件，各个obj又都是由nas文件通过nask.exe生成。最下边就是清理操作了
- app的Makefile，我们看下bball这个下边的Makefile
  ```
    // bball/Makefile
    APP      = bball
    STACK    = 72k
    MALLOC   = 0k

    include ../app_make.txt
  ```
  看起来比较简单，定义了三个变量，然include ../app_make.txt，所以重点还是在app_make.txt
  ```
    ...(略)
    APILIBPATH   = ../apilib/
    HARIBOTEPATH = ../haribote/

    MAKE     = $(TOOLPATH)make.exe -r
    ...(略)

    # 默认动作
    default :
        $(MAKE) $(APP).hrb

    # 文件生成规则
    $(APP).bim : $(APP).obj $(APILIBPATH)apilib.lib Makefile ../app_make.txt
        $(OBJ2BIM) @$(RULEFILE) out:$(APP).bim map:$(APP).map stack:$(STACK) \
            $(APP).obj $(APILIBPATH)apilib.lib

    haribote.img : ../haribote/ipl20.bin ../haribote/haribote.sys $(APP).hrb \
            Makefile ../app_make.txt
        $(EDIMG)   imgin:../../z_tools/fdimg0at.tek \
            wbinimg src:../haribote/ipl20.bin len:512 from:0 to:0 \
            copy from:../haribote/haribote.sys to:@: \
            copy from:$(APP).hrb to:@: \
            imgout:haribote.img

    # 一般规则
    %.gas : %.c ../apilib.h Makefile ../app_make.txt
        $(CC1) -o $*.gas $*.c

    %.nas : %.gas Makefile ../app_make.txt
        $(GAS2NASK) $*.gas $*.nas

    %.obj : %.nas Makefile ../app_make.txt
        $(NASK) $*.nas $*.obj $*.lst

    %.org : %.bim Makefile ../app_make.txt
        $(BIM2HRB) $*.bim $*.org $(MALLOC)

    %.hrb : %.org Makefile ../app_make.txt
        $(BIM2BIN) -osacmp in:$*.org out:$*.hrb

    # 命令
    run :
        $(MAKE) haribote.img
        $(COPY) haribote.img ..\..\z_tools\qemu\fdimage0.bin
        $(MAKE) -C ../../z_tools/qemu

    full :
        $(MAKE) -C $(APILIBPATH)
        $(MAKE) $(APP).hrb

    run_full :
        $(MAKE) -C $(APILIBPATH)
        $(MAKE) -C ../haribote
        $(MAKE) run

    ...(略)
  ```
  - default产物是$(APP).hrb，大致步骤和我们讲到c语言步骤类似，这里再重复一遍：c文件编译成gas，gas产生nas，nas生成obj，obj生成bim（这一步会链接apilib.lib库），还会多一步压缩的过程，即bim变成org，大家也可以看到在这两步，作者会去在这个文件中存放该app需要栈和堆的空间大小，这也是我们上文说到的自定制的操作。最后org编译成hrb。
  - run：首先去make haribote.img，haribote.img这里回去haribote获取haribote.sys和ipl09.bin，在加上${app}.hrb合成haribote.img去给qemu执行，这样就跑起来qemu，里边跑的操作系统值包含一个app。
  - run_full和full和run相比就是先去去make apilib和make ../haribote，保证这两个make完成之后再去执行run
- 来看下操作系统的haribote/makefile吧
  ```
    OBJS_BOOTPACK = bootpack.obj naskfunc.obj ...(略)

    TOOLPATH = ../../z_tools/
    INCPATH  = ../../z_tools/haribote/

    MAKE     = $(TOOLPATH)make.exe -r
    ...(略)

    default :
        $(MAKE) ipl09.bin
        $(MAKE) haribote.sys

    ipl09.bin : ipl09.nas Makefile
        $(NASK) ipl09.nas ipl09.bin ipl09.lst

    asmhead.bin : asmhead.nas Makefile
        $(NASK) asmhead.nas asmhead.bin asmhead.lst

    hankaku.bin : hankaku.txt Makefile
        $(MAKEFONT) hankaku.txt hankaku.bin

    hankaku.obj : hankaku.bin Makefile
        $(BIN2OBJ) hankaku.bin hankaku.obj _hankaku

    bootpack.bim : $(OBJS_BOOTPACK) Makefile
        $(OBJ2BIM) @$(RULEFILE) out:bootpack.bim stack:3136k map:bootpack.map \
            $(OBJS_BOOTPACK)

    bootpack.hrb : bootpack.bim Makefile
        $(BIM2HRB) bootpack.bim bootpack.hrb 0

    haribote.sys : asmhead.bin bootpack.hrb Makefile
        copy /B asmhead.bin+bootpack.hrb haribote.sys

    %.gas :...(略)

    %.nas : ...(略)

    %.obj : ...(略)

    clean :...(略)

    src_only :...(略)

  ```
  先说下这里由三个文件作用，不然看起来可能会懵，ipl09.nas就是启动程序（第一个扇区），asmhead.bin是操作系统最开始的部分，由于不能使用c语言写，所以使用汇编代码完成，然后其他的c文件就是操作系统的实体内容。
  有了上边的基础，这个看起来就很简单，default产物就是ipl09.bin和haribote.sys，ipl09.bin由ipl09.nas生成，haribote.sys由众多obj和asmhead生成，很多obj也是由c文件生成
- 最后是最外边的这个Makefile
  ```
	...(略)

    default :
        $(MAKE) haribote.img

    haribote.img : haribote/ipl09.bin haribote/haribote.sys Makefile \
            ...(略)
        $(EDIMG)   imgin:../z_tools/fdimg0at.tek \
            wbinimg src:haribote/ipl09.bin len:512 from:0 to:0 \
            copy from:haribote/haribote.sys to:@: \
            copy from:ipl09.nas to:@: \
            copy from:make.bat to:@: \
            ...(略)
            imgout:haribote.img

    # make -C change dir and do make
    run :
        $(MAKE) haribote.img
        $(COPY) haribote.img ..\z_tools\qemu\fdimage0.bin
        $(MAKE) -C ../z_tools/qemu

    full :
        $(MAKE) -C haribote
        $(MAKE) -C apilib
        $(MAKE) -C a
        ...(略)
        $(MAKE) haribote.img

    run_full :
        $(MAKE) full
        $(COPY) haribote.img ..\z_tools\qemu\fdimage0.bin
        $(MAKE) -C ../z_tools/qemu

    run_os :
        $(MAKE) -C haribote
        $(MAKE) run

    ...(略)

  ```
  make_run_full是最常用的，这里其实就是把所有的app打进做操系统里边执行<br>
  default产物也是包含所有app的操作系统haribote.img

借此机会大家肯定对工程的结构有了一定的了解，这些makefile多看几遍就很容易理解，没有看懂也不影响我们去编写操作系统，作者的这个操作系统的编译都是使用make/Makefile这样完成

