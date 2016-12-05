big.LITTLE
=======




|       文件  |                           描述                                    |                                                |
|:-----------:|:-----------------------------------------------------------------:|
| fair_orig.c | kernel 中原始的 `kernel/sched/fair.c` 源文件                      |
| fair_mp.c   | 带 `hmp` 改进的负载均衡调度算法的fair.c 源文件                                  |
| fair_mpcb.c | 带 `hmpcb` 改进的负载均衡调度算法的 fair.c 源文件                 |
| big_little_hmp.h, big_little_hmp.c 和 fair.c | 将 `hmp` 负载均衡调度器 与 fair.c 分离后的两个文件 |
