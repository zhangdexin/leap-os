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


