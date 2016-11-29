big.LITTLE
=======


#1  多核异构调度算法
-------


http://www.linaro.org/?s=big.LITTLE

http://git.linaro.org

https://wiki.linaro.org/Archived%20LSK%20Versions

big.LITTLE CPUs can be configured in 2 modes of operation:

    IKS – In Kernel Switcher (also known as CPU Migration)

    GTS - Global Task Scheduling (also known as big.LITTLE MP)

and another schuler for ISA

    Cluser Migration in kernel for Nvidia Tegra3


| 调度算法  |  项目         |   地址                                                |
|:---------:|:-------------:|:-----------------------------------------------------:|
| GTS       | big.LITTLE-mp | http://git.linaro.org/arm/big.LITTLE/mp.git           |
| IKS       | switcher      | http://git.linaro.org/arm/big.LITTLE/switcher.git


#2  参考
-------


##2.1  big.LITTLE
-------


[如何评价 ARM 的 big.LITTLE 大小核切换技术？](https://www.zhihu.com/question/23299449)

[tegra3 CPU auto hotplug和Big/little switch工作的基本原理](http://blog.csdn.net/21cnbao/article/details/7221994)

[低功耗CPU是怎么做到的？](https://www.zhihu.com/question/26655435)

[大小核心切换实现省电 big.LITTLE详解](http://www.enet.com.cn/article/2012/1211/A20121211206764.shtml), http://www.icpcw.com/Parts/CPU/New/2967/296709_all.htm


http://www.linaro.org/?s=big.LITTLE

[big.LITTLE Software Update](http://www.linaro.org/blog/hardware-update/big-little-software-update/)

[Energy Aware Scheduling (EAS) progress update](http://www.linaro.org/blog/core-dump/energy-aware-scheduling-eas-progress-update/)

[ARM大小核big.LITTLE的HMP调度器](http://blog.csdn.net/figo1802/article/details/53338505)

[三星宣布异核多处理方案，Exynos 5 Octa 将成为真正的 8 核芯片](http://www.ifanr.com/news/344228)

[How is Heterogeneous Multi-Processing (HMP) scheduling implemented in Linux Kernel (Samsung Exynos5422)?](http://stackoverflow.com/questions/25498215/how-is-heterogeneous-multi-processing-hmp-scheduling-implemented-in-linux-kern)

[Ten Things to Know About big.LITTLE](https://community.arm.com/groups/processors/blog/2013/06/18/ten-things-to-know-about-biglittle)

[ODROID-XU4](http://www.hardkernel.com/main/products/prdt_info.php?g_code=G143452239825)


[【转】有关Big.Little MP的一些说明](http://tieba.baidu.com/p/2779966905)


##2.2 	cpufreq
-------


[关闭cpu自动降频](http://blog.csdn.net/yatusiter/article/details/8983859)


[Linux系统下CPU频率的调整](http://blog.csdn.net/myarrow/article/details/7917181/)

[Linux系统CPU频率调整工具使用](http://www.cnblogs.com/276815076/p/5434295.html)

[Ubuntu 下对CPU进行降频](http://www.educity.cn/linux/513183.html)

[linux下设置CPU频率](http://www.cppblog.com/jerryma/archive/2012/04/25/172713.aspx)

[Linux cpufreq 机制了解](http://www.cnblogs.com/armlinux/archive/2011/11/12/2396780.html)

[cpufreq变频子系统](http://blog.chinaunix.net/uid-7295895-id-3076467.html)

[(转)关闭cpu](http://blog.sina.com.cn/s/blog_858820890101cplt.html)

[CPU frequency scaling (简体中文)](https://wiki.archlinux.org/index.php?title=CPU_Frequency_Scaling_(%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87)&oldid=187726)

[patch to add support for scaling_available_frequencies for cm7](https://github.com/zefie/thunderc_kernel_xionia/commit/083a8c8d678aaa21871ea4a36494b85a815202c6)

[[Gb][Thunderbolt]How To Enable Scaling_Available_Frequencies](http://rootzwiki.com/topic/9056-gbthunderbolthow-to-enable-scaling-available-frequencies/)

[大开眼界：Ubuntu下10个厉害的Indicator小程序](http://tieba.baidu.com/p/2637557447)

[Power Management Guide 电源管理指南](http://blog.chinaunix.net/uid-20209814-id-1727427.html), [电源管理指南](http://www.360doc.com/content/13/0131/09/4776158_263357318.shtml)

[centos内核编译选项参考](http://cookingbsd.blog.51cto.com/5404439/929347/)


[为笔记本电脑用户配置cpufreqd](http://biancheng.dnbcw.info/linux/228673.html)

[变更CPU频率管理策略](http://tieba.baidu.com/p/2964008035)

[告诉你Ubuntu笔记本节能的小方法!](http://www.orsoon.com/news/53465.html)

[linux cpufreq framework(1)_概述](http://www.wowotech.net/pm_subsystem/cpufreq_overview.html)

[金步国先生文章（一）--关于编译2.6.X内核的选项](http://blog.sina.com.cn/s/blog_48c49d5d010090lg.html)
[减少 Linux 耗电，第 2 部分: 一般设置和与调控器相关的设置](http://www.ibm.com/developerworks/cn/linux/l-cpufreq-2/)


[DVFS--动态电压频率调整](http://blog.chinaunix.net/uid-24666775-id-3328064.html), http://blog.csdn.net/green1900/article/details/40742663, http://blog.csdn.net/myarrow/article/details/8089049

[ubuntu－CPU频率调节](http://blog.csdn.net/hackerwin7/article/details/17100547)

[Cpufreq应用程序在arm开发板端的交叉编译及实现](http://blog.csdn.net/linweig/article/details/5972317)

[](http://blog.csdn.net/linweig/article/details/5972317)

[Linux系统下CPU频率的调整](http://blog.csdn.net/myarrow/article/details/7917181/)

[Linux 2.6 menuconfig内核编译配置选项详解](http://blog.csdn.net/stoneliul/article/details/8949818)

[CPU频率调节(SpeedStep, PowerNow)](http://blog.chinaunix.net/uid-9882718-id-1996237.html)

[Linux内核的cpufreq(变频)机制](http://blog.chinaunix.net/uid-26127124-id-3549156.html)

[ArchLinux 电源管理：acpid + cpufreq + pm-utils](http://www.blogbus.com/songyueju-logs/184136902.html)

[使用cpufreq-bench评估cpufreq策略对系统性能的影响](http://blog.csdn.net/21cnbao/article/details/7218413)

[Linux CPU core的电源管理(2)_cpu topology](http://www.wowotech.net/pm_subsystem/cpu_topology.html)



| 变频 | big.LITTLE |
|:------:|:----------------:|
| 定频低频 | 定核大核 |
| 定频中频 | 定核小核 |
| 定频高频 | 变核交换 |
| 变频         | 变核热插拔 |


1--频率调节的指标

2--能耗比选择大小核

3--负载决定开关核和任务迁移以及交换




