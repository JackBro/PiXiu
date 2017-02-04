PiXiu是什么？
---
PiXiu（貔貅）是我脑洞大开下的产物，灵感来源于[TerarkDB][1]，其作者表示使用了Trie和自动机，从而能在**压缩**的数据上直接进行索引，优于数据库常用的B-Tree，LSM。我非常感兴趣，但实现细节TerarkDB的作者并没有透露。于是，我自己脑补了PiXiu方法。

数据压缩的一种途径是将冗余的数据用更短的记号表示。我的设计是使用General Suffix Tree（通用后缀树）来发现字符串组之间的公共子串，然后用特殊标记来让这些字符串互相引用。

比如：
> 字符串A：BCAAAAAA
> 字符串B：EDAAAAAA

很显然，AB字符串的最后5个字符是相同的。我可以完美地这样表示：
> 字符串A：BCAAAAAA
> 字符串B：ED$A,3,6$

字符串B被改写了。$是特殊符号表示一个压缩组的开始与结束。$A,3,6$的含义很明显了，说明接下来的数据已经存在于叫A的字符串，从位置3到位置6。

后缀树不仅能发现不同字符串之间的重复，自身的重复也可以。字符串A可以这么写。
> 字符串A：BCA$A,3,7$

一下子不好理解吧！我也是在真正实现后缀树的时候发现的。$A,3,7$最终会**递归展开**成AAAAA。这个大家可以拿张纸模拟一下后缀树以及递归解压调用。

后缀树是目前所学中个人觉得最接近理论完美的算法。它能在N的时间，N的空间，表示N个排列，还是一个在线算法。

####还有一个问题！
数据嵌套压缩之后，对比操作将会疯狂递归来解析字符串，这对性能是不是会有影响？必然的。所以红黑树以及别的树就不是那么适用了。还好，我发现了[CritBit Tree][2]。这个算法的特点是所有原始数据永远只会对比一次，其余时间都是用CritBit（我译为特征比特）进行跳转。每添加一条数据，永远只会付出一个指针，一个16bit int，一个8bit int的空间代价。与此同时，时间上界还特别好，跟普通的Trie一样。缺点是指针跳转太多，非常不利于CPU Cache。


这里有什么？
---
以上的算法与设计说起来很简单，但用C++实现起来爆炸复杂！我在2016年10月左右开始系统学习使用C\C++，每天还要上班写App，业余时间全给PiXiu计划了。写到现在，感觉身体也变差了，需要休息一段。如果不赶快发布出来，要是有人开源类似的，那我的Credit不是没了？

原计划是接下来学习Linux C API，自己写个调度器，撸一个协程网络库。再然后，加上分布式。真这么玩下去，要猝死在电脑前了。尽管，这是一个未真正完成的项目。但最独特的内核，我已经保质保量地完成了。所以，我现在发布的是一个没有**任何耦合**的索引引擎。

我有三个理由说明测试是充分的。

1. 所有的组件都和stl的vector或者map进行了随机对照测试。随机增删改数据，只有一致，才会认为可靠。

2. gcov的**coverage 100%**。几行没覆盖到的，是我认为理论上一定会发生的情况，但好像几百万次随机测试也没发生一次，为了保险就不删了。

3. 使用Xcode内置profiler修干净了内存泄漏和越界。（Valgrind在最新的OSX下跑不起，Linux各种还在学习中）


性能测试
---
如果性能上不去，那不是白说。事先声明，这个测试一定是有利于我的。这很正常，要是我测试都不给力，这个项目还有什么价值？

方法：
Python抓取数万HTML页面，URL作为Key，页面代码作为Value，存入PiXiu，记录内存占用与速度。

为什么说有偏向呢？因为，URL和网页都是高冗余的数据，自然会跑出令人惊讶的压缩比。为了让数据不那么失真（不让PiXiu太厉害），我过滤掉了所有的不可见字符（空格等）。篇幅所限，更具体测试流程可以知乎私信我。


Playground
---
正如我前面提到的，这是一个完整的引擎库。但为了大多数开源项目让人快速玩起来的要求，我给PiXiu封装了一个简单的命令行。但实际API不止这两个，再下一章我会说明。

> GET KEY
> 返回KV键值对
> SET KEY::VALUE
> 存储KV，返回当前（current）和总共（total）压缩的bytes数

例子：


API
---


注意事项，缺陷，以及可能的解决方案
---


发布协议
---
BSD协议

[1]:https://www.zhihu.com/question/46787984
[2]:https://github.com/agl/critbit/