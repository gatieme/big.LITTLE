/*
 * Completely Fair Scheduling (CFS) Class (SCHED_NORMAL/SCHED_BATCH)
 *
 *  Copyright (C) 2007 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 *
 *  Interactivity improvements by Mike Galbraith
 *  (C) 2007 Mike Galbraith <efault@gmx.de>
 *
 *  Various enhancements by Dmitry Adamushko.
 *  (C) 2007 Dmitry Adamushko <dmitry.adamushko@gmail.com>
 *
 *  Group scheduling enhancements by Srivatsa Vaddagiri
 *  Copyright IBM Corporation, 2007
 *  Author: Srivatsa Vaddagiri <vatsa@linux.vnet.ibm.com>
 *
 *  Scaled math optimizations by Thomas Gleixner
 *  Copyright (C) 2007, Thomas Gleixner <tglx@linutronix.de>
 *
 *  Adaptive scheduling granularity, math enhancements by Peter Zijlstra
 *  Copyright (C) 2007 Red Hat, Inc., Peter Zijlstra <pzijlstr@redhat.com>
 */
#ifndef __BIT_LITTLE_HMP_H_INCLUDE__
#define __BIT_LITTLE_HMP_H_INCLUDE__

////////////////////////////////////////////////////////////////////////////////
//  the next information is defined in fair.c but big_little_hmp need
////////////////////////////////////////////////////////////////////////////////
#ifdef CONFIG_FAIR_GROUP_SCHED

#ifndef entity_is_task
#define entity_is_task(se)      (!se->my_q)
#endif

#ifndef for_each_sched_entity
#define for_each_sched_entity(se) \
                for (; se; se = se->parent)
#endif

#else
#endif

/* Only depends on SMP, FAIR_GROUP_SCHED may be removed when useful in lb */
#if defined(CONFIG_SMP) && defined(CONFIG_FAIR_GROUP_SCHED)
/* see the next MACRO in fair.c */
/*
 * We choose a half-life close to 1 scheduling period.
 * Note: The tables below are dependent on this value.
 */
#define LOAD_AVG_PERIOD 32
#define LOAD_AVG_MAX 47742 /* maximum possible load avg */
#define LOAD_AVG_MAX_N 345 /* number of full periods to produce LOAD_MAX_AVG */
#endif  /* #if defined(CONFIG_SMP) && defined(CONFIG_FAIR_GROUP_SCHED) */
////////////////////////////////////////////////////////////////////////////////
//  the information above is defined in fair.c but big_little_hmp need
////////////////////////////////////////////////////////////////////////////////

#include <linux/latencytop.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/profile.h>
#include <linux/interrupt.h>
#include <linux/mempolicy.h>
#include <linux/migrate.h>
#include <linux/task_work.h>

#ifdef CONFIG_HMP_PACK_STOP_MACHINE
#include <linux/stop_machine.h>
#endif  /*     #ifdef CONFIG_HMP_PACK_STOP_MACHINE      */

//#define CREATE_TRACE_POINTS
//#include <trace/events/sched.h>

/*
#define CONFIG_SMP
#define CONFIG_CPU_IDLE
#define CONFIG_HMP_VARIABLE_SCALE
#define CONFIG_SCHED_HMP
#define CONFIG_SCHED_HMP_ENHANCEMENT
#define CONFIG_HMP_PACK_SMALL_TASK
#define USE_HMP_DYNAMIC_THRESHOLD
#define CONFIG_HMP_PACK_BUDDY_INFO
#define CONFIG_FAIR_GROUP_SCHED
#define CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
#define CONFIG_SCHEDSTATS
#define CONFIG_HMP_TRACER
#define CONFIG_HMP_POWER_AWARE_CONTROLLER
#define CONFIG_HMP_PACK_SMALL_TASK
#define CONFIG_NO_HZ_COMMON
*/


#ifdef  CONFIG_HMP_VARIABLE_SCALE
#include <linux/sysfs.h>
#include <linux/vmalloc.h>
#endif

#ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE     /*  HMP 负载调度器需要DVFS调频支持   */
/* Include cpufreq header to add a notifier so that cpu frequency
 * scaling can track the current CPU frequency
 */
#include <linux/cpufreq.h>
#endif /* CONFIG_HMP_FREQUENCY_INVARIANT_SCALE */

#ifdef CONFIG_SCHED_HMP                         /*  HMP 负载调度器需要 cpuidle 和cpuhotplug 支持  */
#include <linux/cpuidle.h>
#endif



#ifdef CONFIG_HEVTASK_INTERFACE /*  add by gatieme(ChengJean)   */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#ifdef CONFIG_KGDB_KDB
#include <linux/kdb.h>
#endif      /*  end of CONFIG_KGDB_KDB                          */
#endif      /*  end of CONFIG_HEAVTEST_INTERFACE                */




/*  add by gatieme(ChengJean) @ 2012-12-01 23:35    */
#if defined (CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK) || defined (CONFIG_HMP_PACK_SMALL_TASK)
/*
 * Save the id of the optimal CPU that should be used to pack small tasks
 * The value -1 is used when no buddy has been found
 */
DEFINE_PER_CPU(int, sd_pack_buddy) = {-1};

#ifdef CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK
struct cpumask buddy_cpu_map = { {0} };
#endif  /*  CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK    */


#ifdef CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER
extern DEFINE_PER_CPU(u32, BUDDY_CPU_RQ_USAGE);
extern DEFINE_PER_CPU(u32, BUDDY_CPU_RQ_PERIOD);
extern DEFINE_PER_CPU(u32, BUDDY_CPU_RQ_NR);
extern DEFINE_PER_CPU(u32, TASK_USGAE);
extern DEFINE_PER_CPU(u32, TASK_PERIOD);
extern u32 PACK_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 TASK_PACK_CPU_COUNT[4][NR_CPUS];     // = { {0} };
extern u32 PA_ENABLE;   // = 1;
extern u32 PA_MON_ENABLE;       // = 0;
extern char PA_MON[4][TASK_COMM_LEN];   // = { {0} };
#endif /* CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER */

#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER
extern DEFINE_PER_CPU(u32, BUDDY_CPU_RQ_USAGE);
extern DEFINE_PER_CPU(u32, BUDDY_CPU_RQ_PERIOD);
extern DEFINE_PER_CPU(u32, BUDDY_CPU_RQ_NR);
extern DEFINE_PER_CPU(u32, TASK_USGAE);
extern DEFINE_PER_CPU(u32, TASK_PERIOD);
extern u32 PACK_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 HMP_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
extern u32 PA_ENABLE;           // = 1;
extern u32 LB_ENABLE;           // = 1;
extern u32 PA_MON_ENABLE;       // = 0;
extern char PA_MON[TASK_COMM_LEN];

#ifdef CONFIG_HMP_TRACER
#define POWER_AWARE_ACTIVE_MODULE_PACK_FORM_CPUX_TO_CPUY (0)
#define POWER_AWARE_ACTIVE_MODULE_AVOID_WAKE_UP_FORM_CPUX_TO_CPUY (1)
#define POWER_AWARE_ACTIVE_MODULE_AVOID_BALANCE_FORM_CPUX_TO_CPUY (2)
#define POWER_AWARE_ACTIVE_MODULE_AVOID_FORCE_UP_FORM_CPUX_TO_CPUY (3)
#endif /* CONFIG_HMP_TRACER */

#endif /* CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER */


/*public*/bool is_buddy_busy(int cpu);

//static inline bool is_light_task(struct task_struct *p);

/*public*/int check_pack_buddy(int cpu, struct task_struct *p);


#endif /* CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK || CONFIG_HMP_PACK_SMALL_TASK*/



/**************************************************************
 * CFS operations on generic schedulable entities:
 */


/**************************************************************
 * Scheduling class tree data structure manipulation methods:
 */

/**************************************************
 * Scheduling class queueing methods:
 */




#ifdef CONFIG_SCHED_HMP


/*
 * Migration thresholds should be in the range [0..1023]
 * hmp_up_threshold: min. load required for migrating tasks to a faster cpu
 * hmp_down_threshold: max. load allowed for tasks migrating to a slower cpu
 *
 * hmp_up_prio: Only up migrate task with high priority (<hmp_up_prio)
 * hmp_next_up_threshold: Delay before next up migration (1024 ~= 1 ms)
 * hmp_next_down_threshold: Delay before next down migration (1024 ~= 1 ms)
 *
 * Small Task Packing:
 * We can choose to fill the littlest CPUs in an HMP system rather than
 * the typical spreading mechanic. This behavior is controllable using
 * two variables.
 * hmp_packing_enabled: runtime control over pack/spread
 * hmp_full_threshold: Consider a CPU with this much unweighted load full
 */
#ifdef CONFIG_HMP_DYNAMIC_THRESHOLD
extern unsigned int hmp_up_threshold;// = 1023;
extern unsigned int hmp_down_threshold;// = 0;
#else
//unsigned int hmp_up_threshold = 512;
//unsigned int hmp_down_threshold = 256;
extern unsigned int hmp_up_threshold;// = 700;
extern unsigned int hmp_down_threshold;// = 512;
#endif


#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
extern unsigned int hmp_up_prio;        // = NICE_TO_PRIO(CONFIG_SCHED_HMP_PRIO_FILTER_VAL);
#endif
extern unsigned int hmp_next_up_threshold;//     = 4096;
extern unsigned int hmp_next_down_threshold;//   = 4096;



#ifdef CONFIG_SCHED_HMP_ENHANCEMENT

/* Schedule entity */
#define se_pid(se) ((se != NULL && entity_is_task(se)) ? \
                        container_of(se, struct task_struct, se)->pid : -1)
#define se_load(se) se->avg.load_avg_ratio
#define se_contrib(se) se->avg.load_avg_contrib

/* CPU related : load information */
#define cfs_pending_load(cpu) cpu_rq(cpu)->cfs.avg.pending_load
#define cfs_load(cpu) cpu_rq(cpu)->cfs.avg.load_avg_ratio
#define cfs_contrib(cpu) cpu_rq(cpu)->cfs.avg.load_avg_contrib

/* CPU related : the number of tasks */
#define cfs_nr_normal_prio(cpu) cpu_rq(cpu)->cfs.avg.nr_normal_prio
//#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 100)
#define cfs_nr_pending(cpu) cpu_rq(cpu)->cfs.avg.nr_pending
#define cfs_length(cpu) cpu_rq(cpu)->cfs.h_nr_running
#define rq_length(cpu) (cpu_rq(cpu)->nr_running + cfs_nr_pending(cpu))

#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
#define task_low_priority(prio) ((prio >= hmp_up_prio)?1:0)
#define cfs_nr_dequeuing_low_prio(cpu) \
                        cpu_rq(cpu)->cfs.avg.nr_dequeuing_low_prio
#define cfs_reset_nr_dequeuing_low_prio(cpu) \
                        (cfs_nr_dequeuing_low_prio(cpu) = 0)
#else
#define task_low_priority(prio)                 (0)
#define cfs_reset_nr_dequeuing_low_prio(cpu)    /*      NOP     */
#endif /* CONFIG_SCHED_HMP_PRIO_FILTER */

#endif /* CONFIG_SCHED_HMP_ENHANCEMENT */


#endif  /*      CONFIG_SCHED_HMP        */



/* Only depends on SMP, FAIR_GROUP_SCHED may be removed when useful in lb */
#if defined(CONFIG_SMP) && defined(CONFIG_FAIR_GROUP_SCHED)


#ifdef CONFIG_SCHED_HMP

#define HMP_VARIABLE_SCALE_SHIFT 16ULL

struct hmp_global_attr {
        struct attribute attr;
        ssize_t (*show)(struct kobject *kobj,
                        struct attribute *attr, char *buf);
        ssize_t (*store)(struct kobject *a, struct attribute *b,
                        const char *c, size_t count);
        int *value;
        int (*to_sysfs)(int);
        int (*from_sysfs)(int);
        ssize_t (*to_sysfs_text)(char *buf, int buf_size);
};

#define HMP_DATA_SYSFS_MAX 8
/*
 *  hmp_domains
 *  up_threshold
 *  down_threshold
 *  #ifdef CONFIG_HMP_VARIABLE_SCALE
 *  load_avg_period_ms              default(LOAD_AVG_PERIOD)
 *  #endif
 *  #ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
 *  frequency-invariant scaling     default(ON)
 *  #endif
 *  #ifdef CONFIG_SCHED_HMP_LITTLE_PACKING
 *  packing_enable                  default(ON)
 *  packing_limit                   default(hmp_full_threshold)
 *  #endif
 *  hmp
 * */
struct hmp_data_struct {            /*  sysfs 文件系统的操作接口    */
#ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
        int freqinvar_load_scale_enabled;
#endif
        int multiplier; /* used to scale the time delta */
        struct attribute_group attr_group;
        struct attribute *attributes[HMP_DATA_SYSFS_MAX + 1];
        struct hmp_global_attr attr[HMP_DATA_SYSFS_MAX];
} hmp_data;



#ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
/* Frequency-Invariant Load Modification:
 * Loads are calculated as in PJT's patch however we also scale the current
 * contribution in line with the frequency of the CPU that the task was
 * executed on.
 * In this version, we use a simple linear scale derived from the maximum
 * frequency reported by CPUFreq. As an example:
 *
 * Consider that we ran a task for 100% of the previous interval.
 *
 * Our CPU was under asynchronous frequency control through one of the
 * CPUFreq governors.
 *
 * The CPUFreq governor reports that it is able to scale the CPU between
 * 500MHz and 1GHz.
 *
 * During the period, the CPU was running at 1GHz.
 *
 * In this case, our load contribution for that period is calculated as
 * 1 * (number_of_active_microseconds)
 *
 * This results in our task being able to accumulate maximum load as normal.
 *
 *
 * Consider now that our CPU was executing at 500MHz.
 *
 * We now scale the load contribution such that it is calculated as
 * 0.5 * (number_of_active_microseconds)
 *
 * Our task can only record 50% maximum load during this period.
 *
 * This represents the task consuming 50% of the CPU's *possible* compute
 * capacity. However the task did consume 100% of the CPU's *available*
 * compute capacity which is the value seen by the CPUFreq governor and
 * user-side CPU Utilization tools.
 *
 * Restricting tracked load to be scaled by the CPU's frequency accurately
 * represents the consumption of possible compute capacity and allows the
 * HMP migration's simple threshold migration strategy to interact more
 * predictably with CPUFreq's asynchronous compute capacity changes.
 */
#define SCHED_FREQSCALE_SHIFT 10
struct cpufreq_extents {
        u32 curr_scale;
        u32 min;
        u32 max;
        u32 flags;
/*      add by gatieme(ChengJean) @ 2012-12-1 19:11     for CONFIG_SCHED_HMP_ENHANCEMENT      */
#ifdef  CONFIG_SCHED_HMP_ENHANCEMENT            /*      改进的 HMP 负载均衡调度器       */
        u32 const_max;
        u32 throttling;
#endif                          /*      endif   CONFIG_SCHED_HMP_ENHANCEMENT    */
};
/* Flag set when the governor in use only allows one frequency.
 * Disables scaling.
 */
#define SCHED_LOAD_FREQINVAR_SINGLEFREQ 0x01

static struct cpufreq_extents freq_scale[CONFIG_NR_CPUS];
#endif /* CONFIG_HMP_FREQUENCY_INVARIANT_SCALE */


#endif /* CONFIG_SCHED_HMP */


#ifdef CONFIG_MTK_SCHED_CMP
void get_cluster_cpus(struct cpumask *cpus, int cluster_id, bool exclusive_offline);

/*  add by gatieme(ChengJean) @ 2012-12-01 23:45    */
#ifdef CONFIG_MTK_SCHED_CMP_TGS
//static int nr_cpus_in_cluster(int cluster_id, bool exclusive_offline);
#endif  /*  CONFIG_MTK_SCHED_CMP_TGS    */

/*
 * generic entry point for cpu mask construction, dedicated for
 * mediatek scheduler.
 */
static __init __inline void cmp_cputopo_domain_setup(void);
#endif /* CONFIG_MTK_SCHED_CMP */

/*  add by gatieme(ChengJean) @ 2012-12-01 23:45    */
#ifdef CONFIG_SCHED_HMP_ENHANCEMENT
void sched_get_big_little_cpus(struct cpumask *big, struct cpumask *little);
EXPORT_SYMBOL(sched_get_big_little_cpus);
#endif  /*  CONFIG_SCHED_HMP_ENHANCEMENT    */

/*  add by gatieme(ChengJean) @ 2012-12-01 23:45    */
#ifdef CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY
#define VARIABLE_SCALE_SHIFT 16ULL
static u64 __inline variable_scale_convert(u64 delta);
#endif  /*  CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY    */




#if defined(CONFIG_MTK_SCHED_CMP) || defined(CONFIG_SCHED_HMP_ENHANCEMENT)
/* usage_avg_sum & load_avg_ratio are based on Linaro 12.11. */
//static long __update_task_entity_ratio(struct sched_entity *se);
#endif


#endif  /*  #if defined(CONFIG_SMP) && defined(CONFIG_FAIR_GROUP_SCHED) */




#ifdef CONFIG_SMP

#if defined(CONFIG_SCHED_HMP) || defined(CONFIG_MTK_SCHED_CMP)

/* CPU cluster statistics for task migration control */
#define HMP_GB (0x1000)
#define HMP_SELECT_RQ (0x2000)
#define HMP_LB (0x4000)
#define HMP_MAX_LOAD (NICE_0_LOAD - 1)


struct clb_env {
        struct clb_stats bstats;
        struct clb_stats lstats;
        int btarget, ltarget;

        struct cpumask bcpus;
        struct cpumask lcpus;

        unsigned int flags;
        struct mcheck {
                int status; /* Details of this migration check */
                int result; /* Indicate whether we should perform this task migration */
        } mcheck;
};

unsigned long __weak arch_scale_freq_power(struct sched_domain *sd, int cpu);

//static void collect_cluster_stats(struct clb_stats *clbs, struct cpumask *cluster_cpus, int target);

#define HMP_RESOLUTION_SCALING (4)
#define hmp_scale_down(w) ((w) >> HMP_RESOLUTION_SCALING)


/* #define USE_HMP_DYNAMIC_THRESHOLD */
//#if defined(CONFIG_SCHED_HMP) && defined(USE_HMP_DYNAMIC_THRESHOLD)
//static inline void hmp_dynamic_threshold(struct clb_env *clbenv);
//#endif  /*  #if defined(CONFIG_SCHED_HMP) && defined(USE_HMP_DYNAMIC_THRESHOLD) */

/*
 * Task Dynamic Migration Threshold Adjustment.
 *
 * If the workload between clusters is not balanced, adjust migration
 * threshold in an attempt to move task precisely.
 *
 * Diff. = Max Threshold - Min Threshold
 *
 * Dynamic UP-Threshold =
 *                               B_nacap               B_natask
 * Max Threshold - Diff. x  -----------------  x  -------------------
 *                          B_nacap + L_nacap     B_natask + L_natask
 *
 *
 * Dynamic Down-Threshold =
 *                               L_nacap               L_natask
 * Min Threshold + Diff. x  -----------------  x  -------------------
 *                          B_nacap + L_nacap     B_natask + L_natask
 */
//static void adj_threshold(struct clb_env *clbenv);

//static void sched_update_clbstats(struct clb_env *clbenv);
#endif /* #if defined(CONFIG_SCHED_HMP) || defined(CONFIG_SCHED_CMP) */


/*
 * Heterogenous multiprocessor (HMP) optimizations
 *
 * The cpu types are distinguished using a list of hmp_domains
 * which each represent one cpu type using a cpumask.
 * The list is assumed ordered by compute capacity with the
 * fastest domain first.
 */
DEFINE_PER_CPU(struct hmp_domain *, hmp_cpu_domain);

#ifdef CONFIG_SCHED_HMP_ENHANCEMENT
/* We need to know which cpus are fast and slow. */
extern struct cpumask hmp_fast_cpu_mask;
extern struct cpumask hmp_slow_cpu_mask;
#else   /*  CONFIG_SCHED_HMP_ENHANCEMENT    */
static const int hmp_max_tasks = 5;
#endif  /*  CONFIG_SCHED_HMP_ENHANCEMENT    */

extern void __init arch_get_hmp_domains(struct list_head *hmp_domains_list);

#ifdef CONFIG_CPU_IDLE
/*
 * hmp_idle_pull:
 *
 * In this version we have stopped using forced up migrations when we
 * detect that a task running on a little CPU should be moved to a bigger
 * CPU. In most cases, the bigger CPU is in a deep sleep state and a forced
 * migration means we stop the task immediately but need to wait for the
 * target CPU to wake up before we can restart the task which is being
 * moved. Instead, we now wake a big CPU with an IPI and ask it to pull
 * a task when ready. This allows the task to continue executing on its
 * current CPU, reducing the amount of time that the task is stalled for.
 *
 * keepalive timers:
 *
 * The keepalive timer is used as a way to keep a CPU engaged in an
 * idle pull operation out of idle while waiting for the source
 * CPU to stop and move the task. Ideally this would not be necessary
 * and we could impose a temporary zero-latency requirement on the
 * current CPU, but in the current QoS framework this will result in
 * all CPUs in the system being unable to enter idle states which is
 * not desirable. The timer does not perform any work when it expires.
 */
struct hmp_keepalive {
        bool init;
        ktime_t delay;  /* if zero, no need for timer */
        struct hrtimer timer;
};
DEFINE_PER_CPU(struct hmp_keepalive, hmp_cpu_keepalive);

/* setup per-cpu keepalive timers */
static enum hrtimer_restart hmp_cpu_keepalive_notify(struct hrtimer *hrtimer);
/*
 * Work out if any of the idle states have an exit latency too high for us.
 * ns_delay is passed in containing the max we are willing to tolerate.
 * If there are none, set ns_delay to zero.
 * If there are any, set ns_delay to
 * ('target_residency of state with shortest too-big latency' - 1) * 1000.
 */
static void hmp_keepalive_delay(int cpu, unsigned int *ns_delay);

static void hmp_cpu_keepalive_trigger(void);

static void hmp_cpu_keepalive_cancel(int cpu);

#endif  /*  CONFIG_CPU_IDLE */

/* Setup hmp_domains
 *
 * 调用关系
 * init_sched_fair_class( )
 *  ->  hmp_cpu_mask_setup( )
 * */
/*public*/int __init hmp_cpu_mask_setup(void);

//static inlien struct hmp_domain *hmp_get_hmp_domain_for_cpu(int cpu);

/*public*/void hmp_online_cpu(int cpu);
/*public*/void hmp_offline_cpu(int cpu);



/*
 * Needed to determine heaviest tasks etc.
 */
/* Check if cpu is in fastest hmp_domain */
/*public*/unsigned int hmp_cpu_is_fastest(int cpu);
/* Check if cpu is in slowest hmp_domain */
/*public*/unsigned int hmp_cpu_is_slowest(int cpu);
/*public*/struct hmp_domain *hmp_slower_domain(int cpu);
/*public*/struct hmp_domain *hmp_faster_domain(int cpu);


#ifndef  CONFIG_SCHED_HMP_ENHANCEMENT   /*      #if 0   #endif */

/* must hold runqueue lock for queue se is currently on
 * 查找并返回CPU(target_cpu)上最繁忙的进程,
 *
 * 参数描述
 * se           该 CPU 的当前进程curr的调度实体
 * target_cpu   该 CPU 的编号, 如果传入-1, 则表示目标运行的cpu为大核调度域中任何一个合适的大核
 *                             如果传入>0, 则表示任务将要迁移到的目标大核cpu
 *
 * 返回值
 *  如果调度实体 se 所处的 CPU 是大核, 则直接返回  se
 *  如果target_cpu > 0, 但是却不在大核的调度域中, 即该目标 CPU 是小核, 则返回NULL
 *  其他情况参数正确且合法(se为小核上的某个调度实体, target_cpu 为-1或者某个大核),
 *  则返回se所在的CPU上负载最大的那个调度实体 max_se
 * */
static struct sched_entity *hmp_get_heaviest_task(
                                struct sched_entity *se, int target_cpu);

/*  hmp_get_lightest_task()函数和 hmp_get_heaviest_task()函数类似,
 *  返回 se 调度实体对应的运行队列中任务最轻的调度实体 min_se
 *
 *  参数
 *      se              调度实体指示了待处理的调度实体, 其所在的运行队列 和 CPU
 *      migrate_down    控制变量, 传入1, 表示检查是否可以进行向下迁移(将任务从大核迁移到小核)
 *
 *  调用关系
 *      run_rebalance_domains( )
 *          ->  hmp_force_up_migration( )
 *              ->  hmp_get_lightest_task( )
 * */
static struct sched_entity *hmp_get_lightest_task(
                                struct sched_entity *se, int migrate_down);

/*
 * Select the 'best' candidate little CPU to wake up on.
 * Implements a packing strategy which examines CPU in
 * logical CPU order, and selects the first which will
 * be loaded less than hmp_full_threshold according to
 * the sum of the tracked load of the runqueue and the task.
 */
static inline unsigned int hmp_best_little_cpu(struct task_struct *tsk,
                int cpu);
#endif /* #ifndef  CONFIG_SCHED_HMP_ENHANCEMENT */


#ifdef CONFIG_SCHED_HMP_LITTLE_PACKING
/* 小任务封包补丁, 将负载小于NICE_0 80% 的进程, 看做是小任务
 * 将这些小任务打包成一个任务来看待, 他们共享负载和 CPU 频率
 * 可以查看文档 Documentation/arm/small_task_packing.txt
 * 如果系统中小任务比较多, 那么会将这些小任务进行封包调整, 迁移到一个小核 CPU 上
 * 直到该核上所有运行的小任务的总负载达到了 /sys/kernel/hmp/packing_limit
 * 这些小任务就被封装成了一个任务包
 *
 * Set the default packing threshold to try to keep little
 * CPUs at no more than 80% of their maximum frequency if only
 * packing a small number of small tasks. Bigger tasks will
 * raise frequency as normal.
 * In order to pack a task onto a CPU, the sum of the
 * unweighted runnable_avg load of existing tasks plus the
 * load of the new task must be less than hmp_full_threshold.
 *
 * This works in conjunction with frequency-invariant load
 * and DVFS governors. Since most DVFS governors aim for 80%
 * utilisation, we arrive at (0.8*0.8*(max_load=1024))=655
 * and use a value slightly lower to give a little headroom
 * in the decision.
 * Note that the most efficient frequency is different for
 * each system so /sys/kernel/hmp/packing_limit should be
 * configured at runtime for any given platform to achieve
 * optimal energy usage. Some systems may not benefit from
 * packing, so this feature can also be disabled at runtime
 * with /sys/kernel/hmp/packing_enable
 */
unsigned int hmp_packing_enabled = 1;
unsigned int hmp_full_threshold = 650;
#endif


#ifdef CONFIG_SCHED_HMP_ENHANCEMENT
#define hmp_last_up_migration(cpu) \
                        cpu_rq(cpu)->cfs.avg.hmp_last_up_migration
#define hmp_last_down_migration(cpu) \
                        cpu_rq(cpu)->cfs.avg.hmp_last_down_migration
/* This function enhances the original task selection function */
/*public*/int hmp_select_task_rq_fair(int sd_flag, struct task_struct *p,
                        int prev_cpu, int new_cpu);
#else   /*  CONFIG_SCHED_HMP_ENHANCEMENT    */
unsigned int hmp_up_migration(int cpu, int *target_cpu, struct sched_entity *se);
unsigned int hmp_down_migration(int cpu, struct sched_entity *se);
#endif  /*  CONFIG_SCHED_HMP_ENHANCEMENT    */


/*public*/unsigned int hmp_domain_min_load(struct hmp_domain *hmpd,
                                                int *min_cpu, struct cpumask *affinity);

//static struct hmp_domain *hmp_smallest_domain(void);





/* Next (slower) hmp_domain relative to cpu */
struct hmp_domain *hmp_slower_domain(int cpu);



/* Previous (faster) hmp_domain relative to cpu */
struct hmp_domain *hmp_faster_domain(int cpu);



/*
 * Selects a cpu in previous (faster) hmp_domain
 * Note that cpumask_any_and() returns the first cpu in the cpumask
 */
/*public*/unsigned int hmp_select_faster_cpu(struct task_struct *tsk, int cpu);

/*
 * Selects a cpu in next (slower) hmp_domain
 * Note that cpumask_any_and() returns the first cpu in the cpumask
 */
//static inline unsigned int hmp_select_slower_cpu(struct task_struct *tsk, int cpu);






/*public*/void hmp_next_up_delay(struct sched_entity *se, int cpu);


/*public*/void hmp_next_down_delay(struct sched_entity *se, int cpu);



/*
 * Heterogenous multiprocessor (HMP) optimizations
 *
 * These functions allow to change the growing speed of the load_avg_ratio
 * by default it goes from 0 to 0.5 in LOAD_AVG_PERIOD = 32ms
 * This can now be changed with /sys/kernel/hmp/load_avg_period_ms.
 *
 * These functions also allow to change the up and down threshold of HMP
 * using /sys/kernel/hmp/{up,down}_threshold.
 * Both must be between 0 and 1023. The threshold that is compared
 * to the load_avg_ratio is up_threshold/1024 and down_threshold/1024.
 *
 * For instance, if load_avg_period = 64 and up_threshold = 512, an idle
 * task with a load of 0 will reach the threshold after 64ms of busy loop.
 *
 * Changing load_avg_periods_ms has the same effect than changing the
 * default scaling factor Y=1002/1024 in the load_avg_ratio computation to
 * (1002/1024.0)^(LOAD_AVG_PERIOD/load_avg_period_ms), but the last one
 * could trigger overflows.
 * For instance, with Y = 1023/1024 in __update_task_entity_contrib()
 * "contrib = se->avg.runnable_avg_sum * scale_load_down(se->load.weight);"
 * could be overflowed for a weight > 2^12 even is the load_avg_contrib
 * should still be a 32bits result. This would not happen by multiplicating
 * delta time by 1/22 and setting load_avg_period_ms = 706.
 */

/*
 * By scaling the delta time it end-up increasing or decrease the
 * growing speed of the per entity load_avg_ratio
 * The scale factor hmp_data.multiplier is a fixed point
 * number: (32-HMP_VARIABLE_SCALE_SHIFT).HMP_VARIABLE_SCALE_SHIFT
 */
/*public*/u64 hmp_variable_scale_convert(u64 delta);

//ssize_t hmp_show(struct kobject *kobj, struct attribute *attr, char *buf);


//ssize_t hmp_store(struct kobject *a, struct attribute *attr, const char *buf, size_t count);

//ssize_t hmp_print_domains(char *outbuf, int outbufsize);


//int hmp_period_tofrom_sysfs(int value);

#if defined(CONFIG_SCHED_HMP_LITTLE_PACKING) || \
                defined(CONFIG_HMP_FREQUENCY_INVARIANT_SCALE)
/* toggle control is only 0,1 off/on */
//static int hmp_toggle_from_sysfs(int value);

#endif          /*              defined(CONFIG_SCHED_HMP_LITTLE_PACKING) || defined(CONFIG_HMP_FREQUENCY_INVARIANT_SCALE))  */

#ifdef CONFIG_SCHED_HMP_LITTLE_PACKING
/* packing value must be non-negative */
//int hmp_packing_from_sysfs(int value);
#endif  /*      #ifdef CONFIG_SCHED_HMP_LITTLE_PACKING  */

/*
void hmp_attr_add(
        const char *name,
        int *value,
        int (*to_sysfs)(int),
        int (*from_sysfs)(int),
        ssize_t (*to_sysfs_text)(char *, int),
        umode_t mode);
*/

//int hmp_attr_init(void);

/*
 * return the load of the lowest-loaded CPU in a given HMP domain
 * min_cpu optionally points to an int to receive the CPU.
 * affinity optionally points to a cpumask containing the
 * CPUs to be considered. note:
 *   + min_cpu = NR_CPUS only if no CPUs are in the set of
 *     affinity && hmp_domain cpus
 *   + min_cpu will always otherwise equal one of the CPUs in
 *     the hmp domain
 *   + when more than one CPU has the same load, the one which
 *     is least-recently-disturbed by an HMP migration will be
 *     selected
 *   + if all CPUs are equally loaded or idle and the times are
 *     all the same, the first in the set will be used
 *   + if affinity is not set, cpu_online_mask is used
 *
 *   参数
 *   hmpd       HMP调度域, 一般来说传入的是大核的调度域
 *   min_cpu    是一个指针变量用来传递结果给调用者
 *   affinify   是另外一个cpumask位图, 在我们上下文中是刚才讨论的进程可以运行的 CPU 位图,
 *              一般来说进程通常都运行在所有的CPU上运行
 *
 *   返回值
 *   0      返回0, 表示找到了空闲的CPU, 并用 min_cpu 返回找到的 cpu 的编号
 *   1023   返回1023, 表示该调度域没有空闲CPU也即是都在繁忙中
 *
 *   调用关系
 *   run_rebalance_domains( )
 *      ->  hmp_force_up_migration( )
 *          ->  hmp_up_migration( )
 *              -> hmp_domain_min_load
 */
/*public*/unsigned int hmp_domain_min_load(struct hmp_domain *hmpd,
                                                int *min_cpu, struct cpumask *affinity);



/*
 * Calculate the task starvation
 * This is the ratio of actually running time vs. runnable time.
 * If the two are equal the task is getting the cpu time it needs or
 * it is alone on the cpu and the cpu is fully utilized.
 */
//static inline unsigned int hmp_task_starvation(struct sched_entity *se);


/*
 *  为(大核上)负载最轻的进程调度实体 se 选择一个合适的迁移目标CPU(小核)
 *
 *
 *  调用关系
 *      run_rebalance_domains( )
 *          ->  hmp_force_up_migration( )
 *              ->  hmp_offload_down( )
 *  */
//static inline unsigned int hmp_offload_down(int cpu, struct sched_entity *se);









#if defined (CONFIG_MTK_SCHED_CMP_LAZY_BALANCE) && !defined(CONFIG_HMP_LAZY_BALANCE)
int need_lazy_balance(int dst_cpu, int src_cpu, struct task_struct *p);
#endif  /*      (CONFIG_MTK_SCHED_CMP_LAZY_BALANCE) && !defined(CONFIG_HMP_LAZY_BALANCE)        */





#ifdef CONFIG_SCHED_HMP
/*public*/unsigned int hmp_idle_pull(int this_cpu);
/*
 * move_specific_task tries to move a specific task.
 * Returns 1 if successful and 0 otherwise.
 * Called with both runqueues locked.
 */
/*public*/int move_specific_task(struct lb_env *env, struct task_struct *pm);
#endif  /*      CONFIG_SCHED_HMP        */

//int __do_active_load_balance_cpu_stop(void *data, bool check_sd_lb_flag);


/*
 * active_load_balance_cpu_stop is run by cpu stopper. It pushes
 * running tasks off the busiest CPU onto idle CPUs. It requires at
 * least 1 task to be running on each physical CPU where possible, and
 * avoids physical / logical imbalances.
 */
//int active_load_balance_cpu_stop(void *data);





#ifdef CONFIG_SCHED_HMP_LITTLE_PACKING
/*
 * Decide if the tasks on the busy CPUs in the
 * littlest domain would benefit from an idle balance
 */
int hmp_packing_ilb_needed(int cpu, int ilb_needed);

#endif  /*      CONFIG_HMP_PACK_SMALL_TASK      */


DEFINE_PER_CPU(cpumask_var_t, ilb_tmpmask);


#ifdef CONFIG_NO_HZ_COMMON

#ifdef CONFIG_HMP_PACK_SMALL_TASK

//inline int find_new_ilb(int call_cpu);

#endif  /*      CONFIG_HMP_PACK_SMALL_TASK      */

#endif  /*      CONFIG_NO_HZ_COMMON     */












////////////////////////////////////////////////////////////////////////////////
#ifdef  CONFIG_SCHED_HMP
////////////////////////////////////////////////////////////////////////////////
//static DEFINE_SPINLOCK(hmp_force_migration);


/////////////////////
/// common functions for HMP and CONFIG_SCHED_HMP_ENHANCEMENT
/////////////////////


/*
 * hmp_active_task_migration_cpu_stop is run by cpu stopper and used to
 * migrate a specific task from one runqueue to another.
 * hmp_force_up_migration uses this to push a currently running task
 * off a runqueue. hmp_idle_pull uses this to pull a currently
 * running task to an idle runqueue.
 * Reuses __do_active_load_balance_cpu_stop to actually do the work.
 */
//static inline int hmp_active_task_migration_cpu_stop(void *data);

/*
 * hmp_can_migrate_task - may task p from runqueue rq be migrated to this_cpu?
 * Ideally this function should be merged with can_migrate_task() to avoid
 * redundant code.
 */
//static int hmp_can_migrate_task(struct task_struct *p, struct lb_env *env);



#ifdef CONFIG_SCHED_HMP_ENHANCEMENT     /*      &&      defined(CONFIG_SCHED_HMP)       */

/*
 * Heterogenous Multi-Processor (HMP) - Declaration and Useful Macro
 */

/* Function Declaration */
/*
 * Heterogenous Multi-Processor (HMP) - Utility Function
 */

/*
 * These functions add next up/down migration delay that prevents the task from
 * doing another migration in the same direction until the delay has expired.
 */
//static int hmp_up_stable(int cpu);
//static int hmp_down_stable(int cpu);

//static unsigned int hmp_up_migration(int cpu, int *target_cpu, struct sched_entity *se,
//                                     struct clb_env *clbenv);
//static unsigned int hmp_down_migration(int cpu, int *target_cpu, struct sched_entity *se,
//                                       struct clb_env *clbenv);

#define hmp_caller_is_gb(caller) ((HMP_GB == caller)?1:0)

#define hmp_cpu_is_fast(cpu) cpumask_test_cpu(cpu, &hmp_fast_cpu_mask)
#define hmp_cpu_is_slow(cpu) cpumask_test_cpu(cpu, &hmp_slow_cpu_mask)
#define hmp_cpu_stable(cpu) (hmp_cpu_is_fast(cpu) ? \
                        hmp_up_stable(cpu) : hmp_down_stable(cpu))

#define hmp_inc(v) ((v) + 1)
#define hmp_dec(v) ((v) - 1)
#define hmp_pos(v) ((v) < (0) ? (0) : (v))

#define task_created(f) ((SD_BALANCE_EXEC == f || SD_BALANCE_FORK == f)?1:0)
#define task_cpus_allowed(mask, p) cpumask_intersects(mask, tsk_cpus_allowed(p))
#define task_slow_cpu_allowed(p) task_cpus_allowed(&hmp_slow_cpu_mask, p)
#define task_fast_cpu_allowed(p) task_cpus_allowed(&hmp_fast_cpu_mask, p)





/*
 * If the workload between clusters is not balanced, adjust migration
 * threshold in an attempt to move task to the cluster where the workload
 * is not heavy
 */

/*
 * According to ARM's cpu_efficiency table, the computing power of CA15 and
 * CA7 are 3891 and 2048 respectively. Thus, we assume big has twice the
 * computing power of LITTLE
 */

#define HMP_RATIO(v) ((v)*17/10)

#define hmp_fast_cpu_has_spare_cycles(B, cpu_load) (cpu_load < \
                        (HMP_RATIO(B->cpu_capacity) - (B->cpu_capacity >> 2)))

#define hmp_task_fast_cpu_afford(B, se, cpu) (B->acap > 0 \
                        && hmp_fast_cpu_has_spare_cycles(B, se_load(se) + cfs_load(cpu)))

#define hmp_fast_cpu_oversubscribed(caller, B, se, cpu) \
                        (hmp_caller_is_gb(caller) ? \
                        !hmp_fast_cpu_has_spare_cycles(B, cfs_load(cpu)) : \
                        !hmp_task_fast_cpu_afford(B, se, cpu))

#define hmp_task_slow_cpu_afford(L, se) \
                        (L->acap > 0 && L->acap >= se_load(se))

/* Macro used by low-priorty task filter */
#define hmp_low_prio_task_up_rejected(p, B, L) \
                        (task_low_priority(p->prio) && \
                        (B->ntask >= B->ncpu || 0 != L->nr_normal_prio_task) && \
                         (p->se.avg.load_avg_ratio < 800))

#define hmp_low_prio_task_down_allowed(p, B, L) \
                        (task_low_priority(p->prio) && !B->nr_dequeuing_low_prio && \
                        B->ntask >= B->ncpu && 0 != L->nr_normal_prio_task && \
                        (p->se.avg.load_avg_ratio < 800))

/* Migration check result */
#define HMP_BIG_NOT_OVERSUBSCRIBED           (0x01)
#define HMP_BIG_CAPACITY_INSUFFICIENT        (0x02)
#define HMP_LITTLE_CAPACITY_INSUFFICIENT     (0x04)
#define HMP_LOW_PRIORITY_FILTER              (0x08)
#define HMP_BIG_BUSY_LITTLE_IDLE             (0x10)
#define HMP_BIG_IDLE                         (0x20)
#define HMP_MIGRATION_APPROVED              (0x100)
#define HMP_TASK_UP_MIGRATION               (0x200)
#define HMP_TASK_DOWN_MIGRATION             (0x400)

/* Migration statistics */
#ifdef CONFIG_HMP_TRACER
struct hmp_statisic hmp_stats;
#endif

//inline void hmp_dynamic_threshold(struct clb_env *clbenv);

/*
 * Check whether this task should be migrated to big
 * Briefly summarize the flow as below;
 * 1) Migration stabilizing
 * 1.5) Keep all cpu busy
 * 2) Filter low-priorty task
 * 3) Check CPU capacity
 * 4) Check dynamic migration threshold
 */
unsigned int hmp_up_migration(int cpu, int *target_cpu, struct sched_entity *se,
                                                                        struct clb_env *clbenv);



/*
 * hmp_force_up_migration checks runqueues for tasks that need to
 * be actively migrated to a faster cpu.
 *
 * 调用关系
 * run_rebalance_domains( )
 *  ->  hmp_force_up_migration( )
 */
/*public*/void hmp_force_up_migration(int this_cpu);



/*
 * Heterogenous Multi-Processor (HMP) Global Load Balance
 */

/*
 * According to Linaro's comment, we should only check the currently running
 * tasks because selecting other tasks for migration will require extensive
 * book keeping.
 */
#ifdef CONFIG_HMP_GLOBAL_BALANCE
// static void hmp_force_down_migration(int this_cpu);
#endif /* CONFIG_HMP_GLOBAL_BALANCE */


#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER
u32 AVOID_FORCE_UP_MIGRATION_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
#endif /* CONFIG_HMP_POWER_AWARE_CONTROLLER */



/*
 * hmp_idle_pull looks at little domain runqueues to see
 * if a task should be pulled.
 *
 * Reuses hmp_force_migration spinlock.
 *
 */
unsigned int hmp_idle_pull(int this_cpu);

#elif   defined(CONFIG_SCHED_HMP)   /*  && !defined(CONFIG_SCHED_HMP__ENHANCEMENT)      */

/*  判断进程实体 se 的平均负载是否高于 hmp_up_threshold 这个阀值
 *  hmp_up_threshold 也是一个过滤作用
 *  目前 HMP负载均衡调度器有几个负载
 *
 *  #ifdef CONFIG_SCHED_HMP_PRIO_FILTER
 *  一个是优先级            hmp_up_prio, 优先级值高于hmp_up_prio(即优先级较低)的进程无需从小核迁移到大核
 *  #endif                  该阀值由 CONFIG_SCHED_HMP_PRIO_FILTER 宏开启
 *
 *  一个是负载              hmp_up_threshold,   只有负载高于 hmp_up_threshold 的进程才需要迁移
 *
 *  一个是迁移的间隔        hmp_next_up_threshold,  只有上次迁移距离当前时间的间隔大于hmp_next_up_threshold 的进程才可以进行迁移
 *                          该阀值避免进程被迁移来迁移去
 *  */
unsigned int hmp_task_eligible_for_up_migration(struct sched_entity *se);



/* Check if task should migrate to a faster cpu
 * 检查 cpu 上的调度实体 se 是否需要迁移到大核上
 *
 * 调用关系
 * run_rebalance_domains( )
 *  ->  hmp_force_up_migration( )
 *      ->  hmp_up_migration( )
 *
 * 返回值
 *
 *  如果 se 本来就是在大核上运行, 则无需进行向上迁移, return 0
 *
 *  如果开启了CONFIG_SCHED_HMP_PRIO_FILTER宏,
 *  而se->prio > hmp_up_prio, 即 se 的优先级过于低, 不适宜向上迁移,
 *  这点是符合现实情况的, 低优先级的进程无关紧要, 用户也不会在意其更快的完成和响应,
 *  自然没必要需要迁移到大核上去获得更高的性能, return 0
 *
 *  该进程上一次迁移离现在的时间间隔小于 hmp_next_up_threshold 阀值的也不需要迁移, 避免被迁移来迁移去, return 0
 *
 *  其他情况, 如果进程满足向上迁移的需求, 则从大核的调度域中找到一个空闲的CPU
 * */
unsigned int hmp_up_migration(int cpu, int *target_cpu, struct sched_entity *se);

/* Check if task should migrate to a slower cpu
 * 检查当前进程调度实体是否可以被迁移至一个小核
 * */
/*
 * Check whether this task should be migrated to LITTLE
 * Briefly summarize the flow as below;
 * 1) Migration stabilizing
 * 1.5) Keep all cpu busy
 * 2) Filter low-priorty task
 * 3) Check CPU capacity
 * 4) Check dynamic migration threshold
 */
//static unsigned int hmp_down_migration(int cpu, int *target_cpu, struct sched_entity *se, struct clb_env *clbenv);
//unsigned int hmp_down_migration(int cpu, struct sched_entity *se);


/*
 * Move task in a runnable state to another CPU.
 *
 * Tailored on 'active_load_balance_cpu_stop' with slight
 * modification to locking and pre-transfer checks.  Note
 * rq->lock must be held before calling.
 *
 * 迁移源 CPU 是 for 循环遍历到的 CPU,  cpu_of(rq)
 * 迁移进程是之前找到的那个负载比较轻的进程,    rq->migrate_task
 * 迁移目的地 CPU 是在小核调度域中找到的空闲 CPU,   rq->push_cpu
 *
 * 调用关系
 * run_rebalance_domains( )
 *  ->  hmp_force_up_migration( )
 *      ->  hmp_migrate_runnable_task( )
 */
void hmp_migrate_runnable_task(struct rq *rq);


/*
 * hmp_force_up_migration checks runqueues for tasks that need to
 * be actively migrated to a faster cpu.
 *
 * 调用关系
 * run_rebalance_domains( )
 *  ->  hmp_force_up_migration( )
 */
/*public*/void hmp_force_up_migration(int this_cpu);


/*
 * hmp_idle_pull looks at little domain runqueues to see
 * if a task should be pulled.
 *
 * Reuses hmp_force_migration spinlock.
 *
 */
unsigned int hmp_idle_pull(int this_cpu);





////////////////////////////////////////////////////////////////////////////////
#endif  /*      CONFIG_SCHED_HMP        */
////////////////////////////////////////////////////////////////////////////////

#endif
#endif /* CONFIG_SMP */



#endif  /*      #ifndef __BIT_LITTLE_HMP_H_INCLUDE__    */
