## 多任务-1

### 序
这一章我们来讲解多任务，作者本书中的多任务也是只是在一个cpu上实现。大家接触操作系统中可以将任务的概念理解成进程，不过linux或者windows的进程要比这里说的任务复杂很多。<br>
那么多任务就是指cpu上可以运行多个任务，多个任务之间的来回切换，达到给人多个程序并行的感觉。


### 知识储备
cpu原生支持多任务，是通过TSS来支持，我们下边要来学习一下关于TSS和LDT

#### TSS（Task State Segment）
TSS即任务状态段，一个CPU执行多个任务的时候，就需要将任务之间来回切换。当CPU执行任务时，会将任务所需要的数据加载到寄存器，栈和内存，而任务执行的最新状态在寄存器中体现，那么当任务切换走的时候需要将寄存器中的数据保存，以便当切换回来时任务能够继续之前的位置执行。那么如何做到呢，就是给每一个任务配备一个TSS，用它来保存任务的寄存器等数据，或者说是任务的上下文。<br>
TSS是cpu已经规定好了，不过需要操作系统来将提给内存位置，之后由cpu来维护。这里维护就是指切换时会自动将数据等保存在TSS内存位置，新的任务的TSS值载入到寄存器中。<br>
先来看下TSS的结构：<br>
<img src="https://user-images.githubusercontent.com/22785392/173829425-02424dfd-8c48-4b4c-a8b4-fecec18e6108.png" width="50%" height="50%" />

我们看到保存的数据大多数都是寄存器的值，要注意有几个地方：
* 有三个ss和ESP，这主要和特权级有关，不同的特权使用不同栈地址。<br>
特权级有4级，操作系统是0特权级，应用程序就是3特权级。由低特权级跳转到高特权级时会用到这三组寄存器，但是由高特权级到低特权级弹出栈就可以，无需要TSS，所以只有3组而不是4组。
* 然后“Previous Task Link”保存上一个任务的TSS的地址，这里主要用于嵌套的任务切换，也就是一个任务嵌套去调用另外一个任务时会用到，作者使用的时非嵌套的任务切换，这里就先不提了。
* 还有一个地方要说明的是，TSS中会保存“LDT Segament Selector”，就是LDT的段选择子，关于LDT我们后边再说。
* I/Omap 这里用于程序执行IO操作针对每个IO端口的权限设置，这里我们也不会用到。
<br><br>

有了这个结构我们知道，这个结构是CPU规定的，也就是说CPU只会这么访问。但是这块数据是需要操作系统来提供。也就是我们需要保存这个数据结构在内存中。
那么CPU怎么去访问呢，哈，还是通过段描述符去访问，TSS的段描述符我们可以放到GDT中，也可以放到IDT中，不过放到IDT中是另外一种实现任务切换的方式（通过中断来实现任务切换）。我们这里并不是使用这个方式来任务切换，我们需要把TSS段描述符放到GDT中。那么cpu如何定位到TSS段描述符，那就是借助TR（Task Register）这个寄存器来定位了。TR寄存器中存放的是TSS段描述符的选择子，看下整体的结构：<br>
<img src="https://user-images.githubusercontent.com/22785392/173829667-ec6af086-b08b-4c6f-9fa8-a3477e730f6b.png" width="80%" height="80%" />
<br>
先看下TSS的段描述符，和一些GDT的描述符几乎没什么区别，不过TSS属于系统段，所以S位为0，type为1001表示是TSS段。type中的第二位为B位(10B1),当这个B位为1时,表示这个任务正在繁忙(执行),为0表示空闲.然后基址存放的就是TSS的内存地址，limit就是TSS的大小，我们这里limit都是设置为103字节。<br>
我们定位到TSS的段描述符还需要TR寄存器指向，那么TR寄存器中存放的就是TSS段描述符的选择子，我们看下TR的大致结构：<br>
<img src="https://user-images.githubusercontent.com/22785392/173829837-6970340d-3421-4292-85ee-06e0f3a14c66.png" width="60%" height="60%" /><br>
由选择器和描述符缓冲器组成，16位的段选择子可以找到TSS的段描述符，描述符缓冲器用来保存描述符的基础地址和界限，这样cpu执行时可以通过TR寄存器直接定位到TSS的地址。我们使用'ltr'汇编命令来将装载TR寄存器（后边代码中会看到）。<br>
也就是说初始化操作系统时需要使用'ltr'汇编命令将当前TSS段载入到寄存器，方便后边的任务切换。

#### LDT（Local Descriptor Table）
我们看到TSS结构中有一个字段表示的是'LDT Segment Selector'，那么这个字段指向哪里呢，他其实是保存是当前任务的LDT段描述符的选择子。那么LDT又是什么呢？<br>
我们看到LDT其实想到GDT，不过LDT是相当于每个任务私有段。举个例子，有一个用户进程（任务），他的数据段和代码段肯定不是放在GDT中，他就是放到LDT里。那么执行这个应用程序的时候就会去LDT中取指令执行。LDT的段描述符也需要放到GDT中。<br>
这个整体的结构我们用一个图来表示下，大家就知道了：<br>
<img src="https://user-images.githubusercontent.com/22785392/174802688-388c0eee-965a-4e1e-8a7c-fa438a64d79a.png" width="80%" height="%80" />
<br>
如图所示，GDT中存放LDT的段描述符，LDT段描述符指向local描述符表，这个描述符表也是存放段描述符，不过存放是当前任务的数据，一般也只是数据段和代码段。<br>
关于LDT的段描述符,我们要注意的点是S位为0(系统段),type为0010就能表示这个段是LDT段.<br>
图中也可以看到cpu通过LDTR（Local Descriptor Table Register）来找到当前任务的LDT段描述符。我们然后再来看下LDTR的结构:<br>
<img src="https://user-images.githubusercontent.com/22785392/174802922-a9894ff1-0c5c-48e2-9b98-35d4e9efb6d2.png" width="60%" height="%60" />
<br>
和TSS的寄存器TR类似，都是由选择子和缓冲器组成。功能也和TR类似，去找LDT的段描述符，再进一步去找到当前任务下的数据.
这里要说一下的是,cpu怎么知道去GDT去找数据还是去LDT中去找数据呢,实际也还是通过选择子去寻找的.<br>
我们再来详细了解下选择子的结构(GDT的选择子和LDT的选择子都是一样),选择子的长度是16位,其中高13位是索引值,第0~1位是RPL,这个是在特权检查时使用,我们后边讲到.然后第2位是TI(Table Indicator),为0时表示CPU去GDT中定位索引,为1时去LDT中定位索引.<br>
那这里我们就知道了, 假如通过CS:IP获取LDT中的指令执行,首先需要使用'lldt [选择子]'将GDT中的LDT段加载到LDTR中, CS中的选择子TI位须为1, 这样CPU去LDTR中找到LDT在GDT中选择子然后定位当前任务的LDT位置,然后使用这个CS中的选择子去LDT中定位相应的代码段,获取到指令执行.

### 任务切换方式
以上我们就讲解了关于TSS和LDT的相关知识,那么我们如果做到任务切换呢,有几种方式来帮我们做到.<br>
首先第一个中断+任务门,任务门中存放的是TSS段选择子,通过触发中断,相应的中断号找到的如果是TSS段描述符,那么接切换到这个任务来执行,执行完成后返回到触发那里的下一条指令.
我们程序中并不是使用这个,不展开讲.<br>
第二个使用call指令,'call cs:ip',cs则指向TSS的选择子,然后就可以做到任务切换了,不过这里CPU默默做了很多事情:
* 检查段描述符的S和type位判断描述符类型,如果是TSS描述符,表示知道要进行任务切换,然后再检查B位是否1,如果处于繁忙状态,就会抛出异常.
* 特权检查
* 将目前的任务的状态保存保存到TR寄存器指向的TSS中
* 加载TSS的选择子到TR寄存器中,然后把新的TSS中的数据加载到寄存器中
* 将eflags寄存器的NT置1,表示是个嵌套的任务切换
* 将旧任务TSS选择子写入到新任务的TSS的Previous Task Link中
* 将新任务的TSS段描述符的B位置1,旧任务的TSS段描述符B位不变
* 开始执行新任务
* 任务执行完成后通过'iretd'指令返回,然后就会进行当前任务B位清0,eflags的NT位清0,任务切换回'Previous Task Link'字段中指向的上一个任务<br>


第三种是使用jmp指令, 'jmp cs:ip',和call指令类似,不同的是jmp不是有来有回的指令,所以也不存在任务嵌套这个说法,那么切换前就会将当前任务的B位清0.相信大家可以理解.<br>


### 总结
本文我们主要讲解的是cpu切换任务的知识贮备,首先先讲TSS,TSS是保存任务的上下文的结构,包括TSS段描述符,TR寄存器等概念.然后讲解LDT,LDT主要用于**用户任务**的数据(代码段和)保存,包括LDT段描述符,LDTR等等.
最后我们讲解了cpu支持的原生的任务切换模式,中断+任务门,call和jmp指令,我们程序中用到jmp.<br>
另外linux用的任务切换这三个都不是,因为TSS在GDT中注册最多只能是8192个任务,而且cpu默默做了很多事情,相对来说效率并不高,实际怎样的实现,这里我们也不展开了.
