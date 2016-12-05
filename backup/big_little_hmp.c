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

#include <trace/events/sched.h>


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

#include "sched.h"

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

//#include "sched.h"


#ifdef CONFIG_HEVTASK_INTERFACE /*  add by gatieme(ChengJean)   */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#ifdef CONFIG_KGDB_KDB
#include <linux/kdb.h>
#endif      /*  end of CONFIG_KGDB_KDB                          */
#endif      /*  end of CONFIG_HEAVTEST_INTERFACE                */

struct lb_env;
#include "./big_little_hmp.h"



/*  add by gatieme(ChengJean) @ 2012-12-01 23:35    */
#if defined (CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK) || defined (CONFIG_HMP_PACK_SMALL_TASK)
/*
 * Save the id of the optimal CPU that should be used to pack small tasks
 * The value -1 is used when no buddy has been found
 */
extern DEFINE_PER_CPU(int, sd_pack_buddy);// = {-1};

#ifdef CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK
extern struct cpumask buddy_cpu_map;// = { {0} };
#endif  /*  CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK    */

/* Look for the best buddy CPU that can be used to pack small tasks
 * We make the assumption that it doesn't wort to pack on CPU that share the
 * same powerline. We looks for the 1st sched_domain without the
 * SD_SHARE_POWERLINE flag. Then We look for the sched_group witht the lowest
 * power per core based on the assumption that their power efficiency is
 * better */
void update_packing_domain(int cpu)
{
    struct sched_domain *sd;
    int id = -1;

#ifdef CONFIG_HMP_PACK_BUDDY_INFO
    pr_debug("[PACK] update_packing_domain() CPU%d\n", cpu);
#endif /* CONFIG_MTK_SCHED_CMP_PACK_BUDDY_INFO || CONFIG_HMP_PACK_BUDDY_INFO */
    mt_sched_printf(sched_pa, "[PACK] update_packing_domain() CPU%d", cpu);

    sd = highest_flag_domain(cpu, SD_SHARE_POWERLINE);
    if (!sd) {
        sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);
    } else if (cpumask_first(sched_domain_span(sd)) == cpu || !sd->parent)
            sd = sd->parent;

    while (sd) {
        struct sched_group *sg = sd->groups;
        struct sched_group *pack = sg;
        struct sched_group *tmp = sg->next;

#ifdef CONFIG_HMP_PACK_BUDDY_INFO
        pr_debug("[PACK]  sd = 0x%08lx, flags = %d\n", (unsigned long)sd, sd->flags);
#endif /* CONFIG_HMP_PACK_BUDDY_INFO */

#ifdef CONFIG_HMP_PACK_BUDDY_INFO
        pr_debug("[PACK]  sg = 0x%08lx\n", (unsigned long)sg);
#endif /* CONFIG_HMP_PACK_BUDDY_INFO */

        /* 1st CPU of the sched domain is a good candidate */
        if (id == -1)
            id = cpumask_first(sched_domain_span(sd));

#ifdef CONFIG_HMP_PACK_BUDDY_INFO
        pr_debug("[PACK]  First cpu in this sd id = %d\n", id);
#endif /* CONFIG_HMP_PACK_BUDDY_INFO */

        /* Find sched group of candidate */
        tmp = sd->groups;
        do {
            if (cpumask_test_cpu(id, sched_group_cpus(tmp))) {
                sg = tmp;
                break;
            }
        } while (tmp = tmp->next, tmp != sd->groups);

#ifdef CONFIG_HMP_PACK_BUDDY_INFO
        pr_debug("[PACK]  pack = 0x%08lx\n", (unsigned long)sg);
#endif /* CONFIG_HMP_PACK_BUDDY_INFO */

        pack = sg;
        tmp = sg->next;

        /* loop the sched groups to find the best one */
        /* Stop find the best one in the same Load Balance Domain */
        /* while (tmp != sg) { */
        while (tmp != sg && !(sd->flags & SD_LOAD_BALANCE)) {
            if (tmp->sgp->power * sg->group_weight <
                    sg->sgp->power * tmp->group_weight) {

#ifdef CONFIG_HMP_PACK_BUDDY_INFO
                pr_debug("[PACK]  Now sg power = %u, weight = %u, mask = %lu\n", sg->sgp->power, sg->group_weight, sg->cpumask[0]);
                pr_debug("[PACK]  Better sg power = %u, weight = %u, mask = %lu\n", tmp->sgp->power, tmp->group_weight, tmp->cpumask[0]);
#endif /* CONFIG_MTK_SCHED_CMP_PACK_BUDDY_INFO || CONFIG_HMP_PACK_BUDDY_INFO */

                pack = tmp;
            }
            tmp = tmp->next;
        }

        /* we have found a better group */
        if (pack != sg) {
            id = cpumask_first(sched_group_cpus(pack));

#ifdef CONFIG_HMP_PACK_BUDDY_INFO
            pr_debug("[PACK]  Better sg, first cpu id = %d\n", id);
#endif /* CONFIG_HMP_PACK_BUDDY_INFO */

        }

#ifdef CONFIG_HMP_PACK_BUDDY_INFO
        if (sd->parent) {
            pr_debug("[PACK]  cpu = %d, id = %d, sd->parent = 0x%08lx, flags = %d, SD_LOAD_BALANCE = %d\n", cpu, id, (unsigned long)sd->parent, sd->parent->flags, SD_LOAD_BALANCE);
            pr_debug("[PACK]  %d\n", (id != cpu));
            pr_debug("[PACK]  0x%08lx\n", (unsigned long)(sd->parent));
            pr_debug("[PACK]  %d\n", (sd->parent->flags & SD_LOAD_BALANCE));
        } else {
            pr_debug("[PACK]  cpu = %d, id = %d, sd->parent = 0x%08lx\n", cpu, id, (unsigned long)sd->parent);
        }
#endif /* CONFIG_HMP_PACK_BUDDY_INFO */


        /* Look for another CPU than itself */
        if ((id != cpu) ||
                ((sd->parent) && (sd->parent->flags & SD_LOAD_BALANCE))) {

#ifdef CONFIG_HMP_PACK_BUDDY_INFO
            pr_debug("[PACK]  Break\n");
#endif /*CONFIG_HMP_PACK_BUDDY_INFO */

            break;
        }
        sd = sd->parent;
    }

#ifdef CONFIG_HMP_PACK_BUDDY_INFO
    pr_debug("[PACK] CPU%d packing on CPU%d\n", cpu, id);
#endif /* CONFIG_MTK_SCHED_CMP_PACK_BUDDY_INFO || CONFIG_HMP_PACK_BUDDY_INFO */
    mt_sched_printf(sched_pa, "[PACK] CPU%d packing on CPU%d", cpu, id);

#ifdef CONFIG_HMP_PACK_SMALL_TASK
    per_cpu(sd_pack_buddy, cpu) = id;
#else /* CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK */
    if (per_cpu(sd_pack_buddy, cpu) != -1)
        cpu_clear(per_cpu(sd_pack_buddy, cpu), buddy_cpu_map);
    per_cpu(sd_pack_buddy, cpu) = id;
    if (id != -1)
        cpumask_set_cpu(id, &buddy_cpu_map);
#endif
}

#ifdef CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER
DEFINE_PER_CPU(u32, BUDDY_CPU_RQ_USAGE);
DEFINE_PER_CPU(u32, BUDDY_CPU_RQ_PERIOD);
DEFINE_PER_CPU(u32, BUDDY_CPU_RQ_NR);
DEFINE_PER_CPU(u32, TASK_USGAE);
DEFINE_PER_CPU(u32, TASK_PERIOD);
u32 PACK_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
u32 AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
u32 AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
u32 TASK_PACK_CPU_COUNT[4][NR_CPUS] = { {0} };
u32 PA_ENABLE = 1;
u32 PA_MON_ENABLE = 0;
char PA_MON[4][TASK_COMM_LEN] = { {0} };
#endif /* CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER */

#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER
DEFINE_PER_CPU(u32, BUDDY_CPU_RQ_USAGE);
DEFINE_PER_CPU(u32, BUDDY_CPU_RQ_PERIOD);
DEFINE_PER_CPU(u32, BUDDY_CPU_RQ_NR);
DEFINE_PER_CPU(u32, TASK_USGAE);
DEFINE_PER_CPU(u32, TASK_PERIOD);
u32 PACK_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
u32 AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
u32 AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
u32 HMP_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
u32 PA_ENABLE = 1;
u32 LB_ENABLE = 1;
u32 PA_MON_ENABLE = 0;
char PA_MON[TASK_COMM_LEN];
#endif /* CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER */


/*public*/bool is_buddy_busy(int cpu)
{
#ifdef CONFIG_HMP_PACK_SMALL_TASK
    struct rq *rq;

    if (cpu < 0)
        return 0;

    rq = cpu_rq(cpu);
#else  /* CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK */
        struct rq *rq = cpu_rq(cpu);
#endif
    /*
     * A busy buddy is a CPU with a high load or a small load with a lot of
     * running tasks.
     */

#if defined (CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER) || defined (CONFIG_HMP_POWER_AWARE_CONTROLLER)
    per_cpu(BUDDY_CPU_RQ_USAGE, cpu) = rq->avg.usage_avg_sum;
    per_cpu(BUDDY_CPU_RQ_PERIOD, cpu) = rq->avg.runnable_avg_period;
    per_cpu(BUDDY_CPU_RQ_NR, cpu) = rq->nr_running;
#endif /*(CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER) || defined (CONFIG_HMP_POWER_AWARE_CONTROLLER) */

    return ((rq->avg.usage_avg_sum << rq->nr_running) >
            rq->avg.runnable_avg_period);

}

static inline bool is_light_task(struct task_struct *p)
{
#if defined (CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER) || defined (CONFIG_HMP_POWER_AWARE_CONTROLLER)
    per_cpu(TASK_USGAE, task_cpu(p)) = p->se.avg.usage_avg_sum;
    per_cpu(TASK_PERIOD, task_cpu(p)) = p->se.avg.runnable_avg_period;
#endif /* CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER || CONFIG_HMP_POWER_AWARE_CONTROLLER*/

    /* A light task runs less than 25% in average */
    return (p->se.avg.usage_avg_sum << 2) < p->se.avg.runnable_avg_period;
}


/*public*/int check_pack_buddy(int cpu, struct task_struct *p)
{
#ifdef CONFIG_HMP_PACK_SMALL_TASK
    int buddy;

    if (cpu >= NR_CPUS || cpu < 0)
        return false;
    buddy = per_cpu(sd_pack_buddy, cpu);
#else /* CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK */
    int buddy = cpu;
#endif

    /* No pack buddy for this CPU */
    if (buddy == -1)
        return false;

    /*
     * If a task is waiting for running on the CPU which is its own buddy,
     * let the default behavior to look for a better CPU if available
     * The threshold has been set to 37.5%
     */
#ifdef CONFIG_HMP_PACK_SMALL_TASK
    if ((buddy == cpu)
     && ((p->se.avg.usage_avg_sum << 3) < (p->se.avg.runnable_avg_sum * 5)))
        return false;
#endif

    /* buddy is not an allowed CPU */
    if (!cpumask_test_cpu(buddy, tsk_cpus_allowed(p)))
        return false;

    /*
     * If the task is a small one and the buddy is not overloaded,
     * we use buddy cpu
     */
     if (!is_light_task(p) || is_buddy_busy(buddy))
        return false;

    return true;
}
#endif /* CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK || CONFIG_HMP_PACK_SMALL_TASK*/




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
unsigned int hmp_up_threshold = 1023;
unsigned int hmp_down_threshold = 0;
#else
//unsigned int hmp_up_threshold = 512;
//unsigned int hmp_down_threshold = 256;
unsigned int hmp_up_threshold = 700;
unsigned int hmp_down_threshold = 512;
#endif


#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
unsigned int hmp_up_prio = NICE_TO_PRIO(CONFIG_SCHED_HMP_PRIO_FILTER_VAL);
#endif
unsigned int hmp_next_up_threshold = 4096;
unsigned int hmp_next_down_threshold = 4096;

#endif  /*      CONFIG_SCHED_HMP        */



/* Only depends on SMP, FAIR_GROUP_SCHED may be removed when useful in lb */
#if defined(CONFIG_SMP) && defined(CONFIG_FAIR_GROUP_SCHED)


#ifdef CONFIG_SCHED_HMP
#ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
static struct cpufreq_extents freq_scale[CONFIG_NR_CPUS];
#endif /* CONFIG_HMP_FREQUENCY_INVARIANT_SCALE */
#endif /* CONFIG_SCHED_HMP */


#ifdef CONFIG_MTK_SCHED_CMP
void get_cluster_cpus(struct cpumask *cpus, int cluster_id,
                bool exclusive_offline)
{
    struct cpumask cls_cpus;

    arch_get_cluster_cpus(&cls_cpus, cluster_id);
    if (exclusive_offline) {
        cpumask_and(cpus, cpu_online_mask, &cls_cpus);
    } else
        cpumask_copy(cpus, &cls_cpus);
}

/*  add by gatieme(ChengJean) @ 2012-12-01 23:45    */
#ifdef CONFIG_MTK_SCHED_CMP_TGS
static int nr_cpus_in_cluster(int cluster_id, bool exclusive_offline)
{
    struct cpumask cls_cpus;
    int nr_cpus;

    arch_get_cluster_cpus(&cls_cpus, cluster_id);
    if (exclusive_offline) {
        struct cpumask online_cpus;
        cpumask_and(&online_cpus, cpu_online_mask, &cls_cpus);
        nr_cpus = cpumask_weight(&online_cpus);
    } else
        nr_cpus = cpumask_weight(&cls_cpus);

    return nr_cpus;
}
#endif  /*  CONFIG_MTK_SCHED_CMP_TGS    */

/*
 * generic entry point for cpu mask construction, dedicated for
 * mediatek scheduler.
 */
static __init __inline void cmp_cputopo_domain_setup(void)
{
    WARN(smp_processor_id() != 0, "%s is supposed runs on CPU0 "
                      "while kernel init", __func__);
    /*
     * sched_init
     *   |-> cmp_cputopo_domain_seutp()
     * ...
     * rest_init
     *   ^ fork kernel_init
     *       |-> kernel_init_freeable
     *        ...
     *           |-> arch_build_cpu_topology_domain
     *
     * here, we focus to build up cpu topology and domain before scheduler runs.
     */
    pr_debug("[CPUTOPO][%s] build CPU topology and cluster.\n", __func__);
    arch_build_cpu_topology_domain();
}
#endif /* CONFIG_MTK_SCHED_CMP */

/*  add by gatieme(ChengJean) @ 2012-12-01 23:45    */
#ifdef CONFIG_SCHED_HMP_ENHANCEMENT
void sched_get_big_little_cpus(struct cpumask *big, struct cpumask *little)
{
    arch_get_big_little_cpus(big, little);
}
//EXPORT_SYMBOL(sched_get_big_little_cpus);

#endif  /*  CONFIG_SCHED_HMP_ENHANCEMENT    */

/*  add by gatieme(ChengJean) @ 2012-12-01 23:45    */
#ifdef CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY
#define VARIABLE_SCALE_SHIFT 16ULL
static u64 __inline variable_scale_convert(u64 delta)
{
    u64 high = delta >> 32ULL;
    u64 low = delta & 0xffffffffULL;
    /* change 32ms -> 50% load to (512/32) ms -> 50% load */
    low *= (LOAD_AVG_PERIOD << VARIABLE_SCALE_SHIFT) / LOAD_AVG_VARIABLE_PERIOD;
    high *= (LOAD_AVG_PERIOD << VARIABLE_SCALE_SHIFT) / LOAD_AVG_VARIABLE_PERIOD;
    return (low >> VARIABLE_SCALE_SHIFT) + (high << (32ULL - VARIABLE_SCALE_SHIFT));
}
#endif  /*  CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY    */











#if defined(CONFIG_MTK_SCHED_CMP) || defined(CONFIG_SCHED_HMP_ENHANCEMENT)
/* usage_avg_sum & load_avg_ratio are based on Linaro 12.11. */
static long __update_task_entity_ratio(struct sched_entity *se)
{
        long old_ratio = se->avg.load_avg_ratio;
        u32 ratio;

        ratio = se->avg.runnable_avg_sum * scale_load_down(NICE_0_LOAD);
        ratio /= (se->avg.runnable_avg_period + 1);
        se->avg.load_avg_ratio = scale_load(ratio);

        return se->avg.load_avg_ratio - old_ratio;
}
#else   /*  #if defined(CONFIG_MTK_SCHED_CMP) || defined(CONFIG_SCHED_HMP_ENHANCEMENT)  */
static inline long __update_task_entity_ratio(struct sched_entity *se) { return 0; }
#endif  /*  #if defined(CONFIG_MTK_SCHED_CMP) || defined(CONFIG_SCHED_HMP_ENHANCEMENT)  */



#endif  /*  #if defined(CONFIG_SMP) && defined(CONFIG_FAIR_GROUP_SCHED) */





#ifdef CONFIG_SMP




#if defined(CONFIG_SCHED_HMP) || defined(CONFIG_MTK_SCHED_CMP)


unsigned long __weak arch_scale_freq_power(struct sched_domain *sd, int cpu);

static void collect_cluster_stats(struct clb_stats *clbs, struct cpumask *cluster_cpus, int target)
{
#define HMP_RESOLUTION_SCALING (4)
#define hmp_scale_down(w) ((w) >> HMP_RESOLUTION_SCALING)

        /* Update cluster informatics */
        int cpu;
        for_each_cpu(cpu, cluster_cpus) {
                if (cpu_online(cpu)) {
                        clbs->ncpu++;
                        clbs->ntask += cpu_rq(cpu)->cfs.h_nr_running;
                        clbs->load_avg += cpu_rq(cpu)->cfs.avg.load_avg_ratio;
#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
                        clbs->nr_normal_prio_task += cfs_nr_normal_prio(cpu);
                        clbs->nr_dequeuing_low_prio += cfs_nr_dequeuing_low_prio(cpu);
#endif
                }
        }

        if (!clbs->ncpu || NR_CPUS == target || !cpumask_test_cpu(target, cluster_cpus))
                return;

        clbs->cpu_power = (int)arch_scale_freq_power(NULL, target);

        /* Scale current CPU compute capacity in accordance with frequency */
        clbs->cpu_capacity = HMP_MAX_LOAD;
#ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
        if (hmp_data.freqinvar_load_scale_enabled) {
                cpu = cpumask_any(cluster_cpus);
                if (freq_scale[cpu].throttling == 1) {
                        clbs->cpu_capacity *= freq_scale[cpu].curr_scale;
                } else {
                        clbs->cpu_capacity *= freq_scale[cpu].max;
                }
                clbs->cpu_capacity >>= SCHED_FREQSCALE_SHIFT;

                if (clbs->cpu_capacity > HMP_MAX_LOAD) {
                        clbs->cpu_capacity = HMP_MAX_LOAD;
                }
        }
#elif defined(CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY)
        if (topology_cpu_inv_power_en()) {
                cpu = cpumask_any(cluster_cpus);
                if (topology_cpu_throttling(cpu))
                        clbs->cpu_capacity *= topology_cpu_capacity(cpu);
                else
                        clbs->cpu_capacity *= topology_max_cpu_capacity(cpu);
                clbs->cpu_capacity >>= CPUPOWER_FREQSCALE_SHIFT;

                if (clbs->cpu_capacity > HMP_MAX_LOAD) {
                        clbs->cpu_capacity = HMP_MAX_LOAD;
                }
        }
#endif

        /*
         * Calculate available CPU capacity
         * Calculate available task space
         *
         * Why load ratio should be multiplied by the number of task ?
         * The task is the entity of scheduling unit so that we should consider
         * it in scheduler. Only considering task load is not enough.
         * Thus, multiplying the number of tasks can adjust load ratio to a more
         * reasonable value.
         */
        clbs->load_avg /= clbs->ncpu;
        clbs->acap = clbs->cpu_capacity - cpu_rq(target)->cfs.avg.load_avg_ratio;
        clbs->scaled_acap = hmp_scale_down(clbs->acap);
        clbs->scaled_atask = cpu_rq(target)->cfs.h_nr_running * cpu_rq(target)->cfs.avg.load_avg_ratio;
        clbs->scaled_atask = clbs->cpu_capacity - clbs->scaled_atask;
        clbs->scaled_atask = hmp_scale_down(clbs->scaled_atask);

        mt_sched_printf(sched_log, "[%s] cpu/cluster:%d/%02lx load/len:%lu/%u stats:%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
                                        target, *cpumask_bits(cluster_cpus),
                                        cpu_rq(target)->cfs.avg.load_avg_ratio, cpu_rq(target)->cfs.h_nr_running,
                                        clbs->ncpu, clbs->ntask, clbs->load_avg, clbs->cpu_capacity,
                                        clbs->acap, clbs->scaled_acap, clbs->scaled_atask, clbs->threshold);
}

/* #define USE_HMP_DYNAMIC_THRESHOLD */
#if defined(CONFIG_SCHED_HMP) && defined(USE_HMP_DYNAMIC_THRESHOLD)
static inline void hmp_dynamic_threshold(struct clb_env *clbenv);
#endif  /*  #if defined(CONFIG_SCHED_HMP) && defined(USE_HMP_DYNAMIC_THRESHOLD) */

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
static void adj_threshold(struct clb_env *clbenv)
{
#define TSKLD_SHIFT (2)
#define POSITIVE(x) ((int)(x) < 0 ? 0 : (x))

        int bcpu, lcpu;
        unsigned long b_cap = 0, l_cap = 0;
        unsigned long b_load = 0, l_load = 0;
        unsigned long b_task = 0, l_task = 0;
        int b_nacap, l_nacap, b_natask, l_natask;

#if defined(CONFIG_SCHED_HMP) && defined(USE_HMP_DYNAMIC_THRESHOLD)
        hmp_dynamic_threshold(clbenv);
        return;
#endif

        bcpu = clbenv->btarget;
        lcpu = clbenv->ltarget;
        if (bcpu < nr_cpu_ids) {
                b_load = cpu_rq(bcpu)->cfs.avg.load_avg_ratio;
                b_task = cpu_rq(bcpu)->cfs.h_nr_running;
        }
        if (lcpu < nr_cpu_ids) {
                l_load = cpu_rq(lcpu)->cfs.avg.load_avg_ratio;
                l_task = cpu_rq(lcpu)->cfs.h_nr_running;
        }

#ifdef CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY
        if (bcpu < nr_cpu_ids) {
                b_cap = topology_cpu_capacity(bcpu);
        }
        if (lcpu < nr_cpu_ids) {
                l_cap = topology_cpu_capacity(lcpu);
        }

        b_nacap = POSITIVE(b_cap - b_load);
        b_natask = POSITIVE(b_cap - ((b_task * b_load) >> TSKLD_SHIFT));
        l_nacap = POSITIVE(l_cap - l_load);
        l_natask = POSITIVE(l_cap - ((l_task * l_load) >> TSKLD_SHIFT));
#else /* !CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY */
        b_cap = clbenv->bstats.cpu_power;
        l_cap = clbenv->lstats.cpu_power;
        b_nacap = POSITIVE(clbenv->bstats.scaled_acap *
                                           clbenv->bstats.cpu_power / (clbenv->lstats.cpu_power+1));
        b_natask = POSITIVE(clbenv->bstats.scaled_atask *
                                                clbenv->bstats.cpu_power / (clbenv->lstats.cpu_power+1));
        l_nacap = POSITIVE(clbenv->lstats.scaled_acap);
        l_natask = POSITIVE(clbenv->bstats.scaled_atask);

#endif /* CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY */

        clbenv->bstats.threshold = HMP_MAX_LOAD - HMP_MAX_LOAD * b_nacap * b_natask /
            ((b_nacap + l_nacap) * (b_natask + l_natask) + 1);
        clbenv->lstats.threshold = HMP_MAX_LOAD * l_nacap * l_natask /
            ((b_nacap + l_nacap) * (b_natask + l_natask) + 1);

        mt_sched_printf(sched_log, "[%s]\tup/dl:%4d/%4d L(%d:%4lu,%4lu/%4lu) b(%d:%4lu,%4lu/%4lu)\n", __func__,
                                        clbenv->bstats.threshold, clbenv->lstats.threshold,
                                        lcpu, l_load, l_task, l_cap,
                                        bcpu, b_load, b_task, b_cap);
}

static void sched_update_clbstats(struct clb_env *clbenv)
{
        collect_cluster_stats(&clbenv->bstats, &clbenv->bcpus, clbenv->btarget);
        collect_cluster_stats(&clbenv->lstats, &clbenv->lcpus, clbenv->ltarget);
        adj_threshold(clbenv);
}
#endif /* #if defined(CONFIG_SCHED_HMP) || defined(CONFIG_SCHED_CMP) */


#ifdef CONFIG_SCHED_HMP
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
static enum hrtimer_restart hmp_cpu_keepalive_notify(struct hrtimer *hrtimer)
{
        return HRTIMER_NORESTART;
}

/*
 * Work out if any of the idle states have an exit latency too high for us.
 * ns_delay is passed in containing the max we are willing to tolerate.
 * If there are none, set ns_delay to zero.
 * If there are any, set ns_delay to
 * ('target_residency of state with shortest too-big latency' - 1) * 1000.
 */
static void hmp_keepalive_delay(int cpu, unsigned int *ns_delay)
{
        struct cpuidle_device *dev = per_cpu(cpuidle_devices, cpu);
        struct cpuidle_driver *drv;

        drv = cpuidle_get_cpu_driver(dev);
        if (drv) {
                unsigned int us_delay = UINT_MAX;
                unsigned int us_max_delay = *ns_delay / 1000;
                int idx;
                /* if cpuidle states are guaranteed to be sorted we
                 * could stop at the first match.
                 */
                for (idx = 0; idx < drv->state_count; idx++) {
                        if (drv->states[idx].exit_latency > us_max_delay &&
                                drv->states[idx].target_residency < us_delay) {
                                us_delay = drv->states[idx].target_residency;
                        }
                }
                if (us_delay == UINT_MAX)
                        *ns_delay = 0; /* no timer required */
                else
                        *ns_delay = 1000 * (us_delay - 1);
        }
}

static void hmp_cpu_keepalive_trigger(void)
{
        int cpu = smp_processor_id();
        struct hmp_keepalive *keepalive = &per_cpu(hmp_cpu_keepalive, cpu);
        if (!keepalive->init) {
                unsigned int ns_delay = 100000; /* tolerate 100usec delay */

                hrtimer_init(&keepalive->timer,
                                CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
                keepalive->timer.function = hmp_cpu_keepalive_notify;

                hmp_keepalive_delay(cpu, &ns_delay);
                keepalive->delay = ns_to_ktime(ns_delay);
                keepalive->init = true;
        }
        if (ktime_to_ns(keepalive->delay))
                hrtimer_start(&keepalive->timer,
                        keepalive->delay, HRTIMER_MODE_REL_PINNED);
}

static void hmp_cpu_keepalive_cancel(int cpu)
{
        struct hmp_keepalive *keepalive = &per_cpu(hmp_cpu_keepalive, cpu);
        if (keepalive->init)
                hrtimer_cancel(&keepalive->timer);
}
#else /* !CONFIG_CPU_IDLE */
static void hmp_cpu_keepalive_trigger(void)
{
}

static void hmp_cpu_keepalive_cancel(int cpu)
{
}
#endif  /*  CONFIG_CPU_IDLE */

/* Setup hmp_domains
 *
 * 调用关系
 * init_sched_fair_class( )
 *  ->  hmp_cpu_mask_setup( )
 * */
/*public*/int __init hmp_cpu_mask_setup(void)
{
        char buf[64];
        struct hmp_domain *domain;
        struct list_head *pos;
        int dc, cpu;

#if defined(CONFIG_SCHED_HMP_ENHANCEMENT) || defined(CONFIG_MT_RT_SCHED)
        cpumask_clear(&hmp_fast_cpu_mask);
        cpumask_clear(&hmp_slow_cpu_mask);
#endif  /*  #if defined(CONFIG_SCHED_HMP_ENHANCEMENT) || defined(CONFIG_MT_RT_SCHED)    */

        pr_debug("Initializing HMP scheduler:\n");

        /* Initialize hmp_domains using platform code
     * see arch/arm[64]/kernel/topology.c   */
        arch_get_hmp_domains(&hmp_domains);
        if (list_empty(&hmp_domains)) {
                pr_debug("HMP domain list is empty!\n");
                return 0;
        }

        /* Print hmp_domains */
        dc = 0;
        list_for_each(pos, &hmp_domains) {
                domain = list_entry(pos, struct hmp_domain, hmp_domains);
                cpulist_scnprintf(buf, 64, &domain->possible_cpus);
                pr_debug("  HMP domain %d: %s\n", dc, buf);

                /*
                 * According to the description in "arch_get_hmp_domains",
                 * Fastest domain is at head of list. Thus, the fast-cpu mask should
                 * be initialized first, followed by slow-cpu mask.
                 */
#if defined(CONFIG_SCHED_HMP_ENHANCEMENT) || defined(CONFIG_MT_RT_SCHED)
                if (cpumask_empty(&hmp_fast_cpu_mask)) {
                        cpumask_copy(&hmp_fast_cpu_mask, &domain->possible_cpus);
                        for_each_cpu(cpu, &hmp_fast_cpu_mask)
                            pr_debug("  HMP fast cpu : %d\n", cpu);
                } else if (cpumask_empty(&hmp_slow_cpu_mask)) {
                        cpumask_copy(&hmp_slow_cpu_mask, &domain->possible_cpus);
                        for_each_cpu(cpu, &hmp_slow_cpu_mask)
                            pr_debug("  HMP slow cpu : %d\n", cpu);
                }
#endif

                for_each_cpu_mask(cpu, domain->possible_cpus) {
                        per_cpu(hmp_cpu_domain, cpu) = domain;
                }
                dc++;
        }

        return 1;
}

static struct hmp_domain *hmp_get_hmp_domain_for_cpu(int cpu)
{
        struct hmp_domain *domain;
        struct list_head *pos;

        list_for_each(pos, &hmp_domains) {
                domain = list_entry(pos, struct hmp_domain, hmp_domains);
                if(cpumask_test_cpu(cpu, &domain->possible_cpus))
                        return domain;
        }
        return NULL;
}

/*public*/void hmp_online_cpu(int cpu)
{
        struct hmp_domain *domain = hmp_get_hmp_domain_for_cpu(cpu);

        if(domain)
                cpumask_set_cpu(cpu, &domain->cpus);
}

/*public*/void hmp_offline_cpu(int cpu)
{
        struct hmp_domain *domain = hmp_get_hmp_domain_for_cpu(cpu);

        if(domain)
                cpumask_clear_cpu(cpu, &domain->cpus);

        hmp_cpu_keepalive_cancel(cpu);
}




/*
 * Needed to determine heaviest tasks etc.
 */
/*public*/unsigned int hmp_cpu_is_fastest(int cpu);
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
                                struct sched_entity *se, int target_cpu)
{
        int num_tasks = hmp_max_tasks;
        struct sched_entity *max_se = se;
        unsigned long int max_ratio = se->avg.load_avg_ratio;
        const struct cpumask *hmp_target_mask = NULL;
        struct hmp_domain *hmp;

        if (hmp_cpu_is_fastest(cpu_of(se->cfs_rq->rq))) /*  判断该 CPU 是否处于大核 CPU 的调度域中  */
                return max_se;                              /*  如果是则直接返回当前进程的调度实体      */

        hmp = hmp_faster_domain(cpu_of(se->cfs_rq->rq));
        hmp_target_mask = &hmp->cpus;                   /*  指向大核调度域中cpumask位图             */
        if (target_cpu >= 0) {                          /*  如果参数target_cpu传入的是实际的CPU编号 */
                /* idle_balance gets run on a CPU while
                 * it is in the middle of being hotplugged
                 * out. Bail early in that case.
                 */
                if(!cpumask_test_cpu(target_cpu, hmp_target_mask))  /*  判断target_cpu是否在大核的调度域内  */
                        return NULL;                                    /*  如果指定的target_cpu不是大核,
                                                                则当前操作为希望从一个小核上返回一个最繁忙的进程,
                                                                却要把该进程放到另外一个小核target_cpu上, 返回NULL    */
                hmp_target_mask = cpumask_of(target_cpu);           /*  设置mask信息为指定的target_cpu的mask信息    */
        }
        /* The currently running task is not on the runqueue
     * curr 进程是不在 CPU 的运行队列里面的 */
        se = __pick_first_entity(cfs_rq_of(se));

        while (num_tasks && se) {   /*  从当前CPU 的红黑树中找出负载最大的那个调度实体max_se    */
                if (entity_is_task(se) &&
                        se->avg.load_avg_ratio > max_ratio &&
                        cpumask_intersects(hmp_target_mask,
                                tsk_cpus_allowed(task_of(se)))) {
                        max_se = se;
                        max_ratio = se->avg.load_avg_ratio;
                }
                se = __pick_next_entity(se);
                num_tasks--;
        }

        return max_se;
}

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
                                struct sched_entity *se, int migrate_down)
{
        int num_tasks = hmp_max_tasks;
        struct sched_entity *min_se = se;
        unsigned long int min_ratio = se->avg.load_avg_ratio;
        const struct cpumask *hmp_target_mask = NULL;

        if (migrate_down) {         /*  进行向下迁移, 将任务从大核迁移到小核    */
                struct hmp_domain *hmp;
                if (hmp_cpu_is_slowest(cpu_of(se->cfs_rq->rq))) /*  检查当前se所在的cpu是不是大核, 如果不是, 则直接返回 min_se = se   */
                        return min_se;
                hmp = hmp_slower_domain(cpu_of(se->cfs_rq->rq));
                hmp_target_mask = &hmp->cpus;
        }
        /* The currently running task is not on the runqueue */
        se = __pick_first_entity(cfs_rq_of(se));

        while (num_tasks && se) {               /*  从 se 所在 CPU 的任务队列上找到那个负载最小的进程 */
                if (entity_is_task(se) &&
                        (se->avg.load_avg_ratio < min_ratio &&
                        hmp_target_mask &&
                                cpumask_intersects(hmp_target_mask,
                                tsk_cpus_allowed(task_of(se))))) {
                        min_se = se;
                        min_ratio = se->avg.load_avg_ratio;
                }
                se = __pick_next_entity(se);
                num_tasks--;
        }
        return min_se;
}


/*
 * Select the 'best' candidate little CPU to wake up on.
 * Implements a packing strategy which examines CPU in
 * logical CPU order, and selects the first which will
 * be loaded less than hmp_full_threshold according to
 * the sum of the tracked load of the runqueue and the task.
 */
static inline unsigned int hmp_best_little_cpu(struct task_struct *tsk,
                int cpu) {
        int tmp_cpu;
        unsigned long estimated_load;
        struct hmp_domain *hmp;
        struct sched_avg *avg;
        struct cpumask allowed_hmp_cpus;

        if(!hmp_packing_enabled ||
                        tsk->se.avg.load_avg_ratio > ((NICE_0_LOAD * 90)/100))
                return hmp_select_slower_cpu(tsk, cpu);

        if (hmp_cpu_is_slowest(cpu))
                hmp = hmp_cpu_domain(cpu);
        else
                hmp = hmp_slower_domain(cpu);

        /* respect affinity */
        cpumask_and(&allowed_hmp_cpus, &hmp->cpus,
                        tsk_cpus_allowed(tsk));

        for_each_cpu_mask(tmp_cpu, allowed_hmp_cpus) {
                avg = &cpu_rq(tmp_cpu)->avg;
                /* estimate new rq load if we add this task */
                estimated_load = avg->load_avg_ratio +
                                tsk->se.avg.load_avg_ratio;
                if (estimated_load <= hmp_full_threshold) {
                        cpu = tmp_cpu;
                        break;
                }
        }
        /* if no match was found, the task uses the initial value */
        return cpu;
}
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




static inline struct hmp_domain *hmp_smallest_domain(void)
{
        return list_entry(hmp_domains.prev, struct hmp_domain, hmp_domains);
}

/* Check if cpu is in fastest hmp_domain */
/*public*/unsigned int hmp_cpu_is_fastest(int cpu)
{
        struct list_head *pos;

        pos = &hmp_cpu_domain(cpu)->hmp_domains;
        return pos == hmp_domains.next;
}

/* Check if cpu is in slowest hmp_domain */
/*public*/unsigned int hmp_cpu_is_slowest(int cpu)
{
        struct list_head *pos;

        pos = &hmp_cpu_domain(cpu)->hmp_domains;
        return list_is_last(pos, &hmp_domains);
}

/* Next (slower) hmp_domain relative to cpu */
/*public*/struct hmp_domain *hmp_slower_domain(int cpu)
{
        struct list_head *pos;

        pos = &hmp_cpu_domain(cpu)->hmp_domains;
        return list_entry(pos->next, struct hmp_domain, hmp_domains);
}

/* Previous (faster) hmp_domain relative to cpu */
/*public*/struct hmp_domain *hmp_faster_domain(int cpu)
{
        struct list_head *pos;

        pos = &hmp_cpu_domain(cpu)->hmp_domains;
        return list_entry(pos->prev, struct hmp_domain, hmp_domains);
}

#ifdef CONFIG_SCHED_HMP_ENHANCEMENT


/*
 * Selects a cpu in previous (faster) hmp_domain
 * Note that cpumask_any_and() returns the first cpu in the cpumask
 */
unsigned int hmp_select_faster_cpu(struct task_struct *tsk,
                                                        int cpu)
{
        int lowest_cpu = NR_CPUS;
        __always_unused int lowest_ratio = hmp_domain_min_load(hmp_faster_domain(cpu), &lowest_cpu, NULL);
        /*
         * If the lowest-loaded CPU in the domain is allowed by the task affinity
         * select that one, otherwise select one which is allowed
         */
        if (lowest_cpu < nr_cpu_ids && cpumask_test_cpu(lowest_cpu, tsk_cpus_allowed(tsk)))
                return lowest_cpu;
        else
                return cpumask_any_and(&hmp_faster_domain(cpu)->cpus,
                                tsk_cpus_allowed(tsk));
}

/*
 * Selects a cpu in next (slower) hmp_domain
 * Note that cpumask_any_and() returns the first cpu in the cpumask
 */
static inline unsigned int hmp_select_slower_cpu(struct task_struct *tsk,
                                                        int cpu)
{
        int lowest_cpu = NR_CPUS;
        __always_unused int lowest_ratio = hmp_domain_min_load(hmp_slower_domain(cpu), &lowest_cpu, NULL);
        /*
         * If the lowest-loaded CPU in the domain is allowed by the task affinity
         * select that one, otherwise select one which is allowed
         */
        if (lowest_cpu < nr_cpu_ids && cpumask_test_cpu(lowest_cpu, tsk_cpus_allowed(tsk)))
                return lowest_cpu;
        else
                return cpumask_any_and(&hmp_slower_domain(cpu)->cpus,
                                tsk_cpus_allowed(tsk));
}


#else       /*  CONFIG_SCHED_HMP_ENHANCEMENT    */

/*
 * Selects a cpu in previous (faster) hmp_domain
 */
static inline unsigned int hmp_select_faster_cpu(struct task_struct *tsk,
                                                        int cpu)
{
        int lowest_cpu=NR_CPUS;
        __always_unused int lowest_ratio;
        struct hmp_domain *hmp;

        if (hmp_cpu_is_fastest(cpu))
                hmp = hmp_cpu_domain(cpu);
        else
                hmp = hmp_faster_domain(cpu);

        lowest_ratio = hmp_domain_min_load(hmp, &lowest_cpu,
                        tsk_cpus_allowed(tsk));

        return lowest_cpu;
}

/*
 * Selects a cpu in next (slower) hmp_domain
 * Note that cpumask_any_and() returns the first cpu in the cpumask
 */
static inline unsigned int hmp_select_slower_cpu(struct task_struct *tsk,
                                                        int cpu)
{
        int lowest_cpu=NR_CPUS;
        struct hmp_domain *hmp;
        __always_unused int lowest_ratio;

        if (hmp_cpu_is_slowest(cpu))
                hmp = hmp_cpu_domain(cpu);
        else
                hmp = hmp_slower_domain(cpu);

        lowest_ratio = hmp_domain_min_load(hmp, &lowest_cpu,
                        tsk_cpus_allowed(tsk));

        return lowest_cpu;
}

#endif  /*  CONFIG_SCHED_HMP_ENHANCEMENT    */



/*public*/void hmp_next_up_delay(struct sched_entity *se, int cpu)
{
        /* hack - always use clock from first online CPU */
        u64 now = cpu_rq(cpumask_first(cpu_online_mask))->clock_task;
        se->avg.hmp_last_up_migration = now;
        se->avg.hmp_last_down_migration = 0;
        cpu_rq(cpu)->avg.hmp_last_up_migration = now;
        cpu_rq(cpu)->avg.hmp_last_down_migration = 0;
}

/*public*/void hmp_next_down_delay(struct sched_entity *se, int cpu)
{
        /* hack - always use clock from first online CPU */
        u64 now = cpu_rq(cpumask_first(cpu_online_mask))->clock_task;
        se->avg.hmp_last_down_migration = now;
        se->avg.hmp_last_up_migration = 0;
        cpu_rq(cpu)->avg.hmp_last_down_migration = now;
        cpu_rq(cpu)->avg.hmp_last_up_migration = 0;
}




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
inline u64 hmp_variable_scale_convert(u64 delta)
{
#ifdef CONFIG_HMP_VARIABLE_SCALE
        u64 high = delta >> 32ULL;
        u64 low = delta & 0xffffffffULL;
        low *= hmp_data.multiplier;
        high *= hmp_data.multiplier;
        return (low >> HMP_VARIABLE_SCALE_SHIFT)
                        + (high << (32ULL - HMP_VARIABLE_SCALE_SHIFT));
#else
        return delta;
#endif
}

static ssize_t hmp_show(struct kobject *kobj,
                                struct attribute *attr, char *buf)
{
        struct hmp_global_attr *hmp_attr =
                container_of(attr, struct hmp_global_attr, attr);
        int temp;

        if (hmp_attr->to_sysfs_text != NULL)
                return hmp_attr->to_sysfs_text(buf, PAGE_SIZE);

        temp = *(hmp_attr->value);
        if (hmp_attr->to_sysfs != NULL)
                temp = hmp_attr->to_sysfs(temp);

        return (ssize_t)sprintf(buf, "%d\n", temp);
}

static ssize_t hmp_store(struct kobject *a, struct attribute *attr,
                                const char *buf, size_t count)
{
        int temp;
        ssize_t ret = count;
        struct hmp_global_attr *hmp_attr =
                container_of(attr, struct hmp_global_attr, attr);
        char *str = vmalloc(count + 1);
        if (str == NULL)
                return -ENOMEM;
        memcpy(str, buf, count);
        str[count] = 0;
        if (sscanf(str, "%d", &temp) < 1)
                ret = -EINVAL;
        else {
                if (hmp_attr->from_sysfs != NULL)
                        temp = hmp_attr->from_sysfs(temp);
                if (temp < 0)
                        ret = -EINVAL;
                else
                        *(hmp_attr->value) = temp;
        }
        vfree(str);
        return ret;
}

static ssize_t hmp_print_domains(char *outbuf, int outbufsize)
{
        char buf[64];
        const char nospace[] = "%s", space[] = " %s";
        const char *fmt = nospace;
        struct hmp_domain *domain;
        struct list_head *pos;
        int outpos = 0;
        list_for_each(pos, &hmp_domains) {
                domain = list_entry(pos, struct hmp_domain, hmp_domains);
                if (cpumask_scnprintf(buf, 64, &domain->possible_cpus)) {
                        outpos += sprintf(outbuf+outpos, fmt, buf);
                        fmt = space;
                }
        }
        strcat(outbuf, "\n");
        return outpos+1;
}

#ifdef CONFIG_HMP_VARIABLE_SCALE
static int hmp_period_tofrom_sysfs(int value)
{
        return (LOAD_AVG_PERIOD << HMP_VARIABLE_SCALE_SHIFT) / value;
}
#endif
/* max value for threshold is 1024 */
static int hmp_theshold_from_sysfs(int value)
{
        if (value > 1024)
                return -1;
        return value;
}
#if defined(CONFIG_SCHED_HMP_LITTLE_PACKING) || \
                defined(CONFIG_HMP_FREQUENCY_INVARIANT_SCALE)
/* toggle control is only 0,1 off/on */
static int hmp_toggle_from_sysfs(int value)
{
        if (value < 0 || value > 1)
                return -1;
        return value;
}
#endif          /*              defined(CONFIG_SCHED_HMP_LITTLE_PACKING) || defined(CONFIG_HMP_FREQUENCY_INVARIANT_SCALE))  */
#ifdef CONFIG_SCHED_HMP_LITTLE_PACKING
/* packing value must be non-negative */
static int hmp_packing_from_sysfs(int value)
{
        if (value < 0)
                return -1;
        return value;
}
#endif  /*      #ifdef CONFIG_SCHED_HMP_LITTLE_PACKING  */
static void hmp_attr_add(
        const char *name,
        int *value,
        int (*to_sysfs)(int),
        int (*from_sysfs)(int),
        ssize_t (*to_sysfs_text)(char *, int),
        umode_t mode)
{
        int i = 0;
        while (hmp_data.attributes[i] != NULL) {
                i++;
                if (i >= HMP_DATA_SYSFS_MAX)
                        return;
        }
        if (mode)
                hmp_data.attr[i].attr.mode = mode;
        else
                hmp_data.attr[i].attr.mode = 0644;
        hmp_data.attr[i].show = hmp_show;
        hmp_data.attr[i].store = hmp_store;
        hmp_data.attr[i].attr.name = name;
        hmp_data.attr[i].value = value;
        hmp_data.attr[i].to_sysfs = to_sysfs;
        hmp_data.attr[i].from_sysfs = from_sysfs;
        hmp_data.attr[i].to_sysfs_text = to_sysfs_text;
        hmp_data.attributes[i] = &hmp_data.attr[i].attr;
        hmp_data.attributes[i + 1] = NULL;
}

static int hmp_attr_init(void)
{
        int ret;
        memset(&hmp_data, sizeof(hmp_data), 0);
        hmp_attr_add("hmp_domains",
                NULL,
                NULL,
                NULL,
                hmp_print_domains,
                0444);
        hmp_attr_add("up_threshold",
                &hmp_up_threshold,
                NULL,
                hmp_theshold_from_sysfs,
                NULL,
                0);
        hmp_attr_add("down_threshold",
                &hmp_down_threshold,
                NULL,
                hmp_theshold_from_sysfs,
                NULL,
                0);
#ifdef CONFIG_HMP_VARIABLE_SCALE
        /* by default load_avg_period_ms == LOAD_AVG_PERIOD
         * meaning no change
         */
        hmp_data.multiplier = hmp_period_tofrom_sysfs(LOAD_AVG_PERIOD);
        hmp_attr_add("load_avg_period_ms",
                &hmp_data.multiplier,
                hmp_period_tofrom_sysfs,
                hmp_period_tofrom_sysfs,
                NULL,
                0);
#endif
#ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
        /* default frequency-invariant scaling ON */
        hmp_data.freqinvar_load_scale_enabled = 1;
        hmp_attr_add("frequency_invariant_load_scale",
                &hmp_data.freqinvar_load_scale_enabled,
                NULL,
                hmp_toggle_from_sysfs,
                NULL,
                0);
#endif
#ifdef CONFIG_SCHED_HMP_LITTLE_PACKING
        hmp_attr_add("packing_enable",
                &hmp_packing_enabled,
                NULL,
                hmp_toggle_from_sysfs,
                NULL,
                0);
        hmp_attr_add("packing_limit",
                &hmp_full_threshold,
                NULL,
                hmp_packing_from_sysfs,
                NULL,
                0);
#endif
        hmp_data.attr_group.name = "hmp";
        hmp_data.attr_group.attrs = hmp_data.attributes;
        ret = sysfs_create_group(kernel_kobj,
                &hmp_data.attr_group);
        return 0;
}
late_initcall(hmp_attr_init);
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
                                                int *min_cpu, struct cpumask *affinity)
{
        int cpu;
        int min_cpu_runnable_temp = NR_CPUS;
        u64 min_target_last_migration = ULLONG_MAX;
        u64 curr_last_migration;
        unsigned long min_runnable_load = INT_MAX;
        unsigned long contrib;
        struct sched_avg *avg;
        struct cpumask temp_cpumask;
        /*
         * only look at CPUs allowed if specified,
         * otherwise look at all online CPUs in the
         * right HMP domain
     *
     * 检查hmpd 调度域上 cpumask 和 affinity 位图
         */
        cpumask_and(&temp_cpumask, &hmpd->cpus, affinity ? affinity : cpu_online_mask);

        for_each_cpu_mask(cpu, temp_cpumask) {  /*  遍历 temp_cpumask 位图上的所有 CPU      */
                avg = &cpu_rq(cpu)->avg;            /*  获取当前检查的 cpu 上运行队列的平均负载 */
                /* used for both up and down migration */
                curr_last_migration = avg->hmp_last_up_migration ?
                        avg->hmp_last_up_migration : avg->hmp_last_down_migration;

                contrib = avg->load_avg_ratio;      /*  获取平均负载信息 avg->load_avg_ratio    */
                /*
                 * Consider a runqueue completely busy if there is any load
                 * on it. Definitely not the best for overall fairness, but
                 * does well in typical Android use cases.
                 */
                if (contrib)                        /*  如果当前 cpu 上有负载(load_avg_ratio)   */
                        contrib = 1023;                 /*  那么 contrib 统统设置为 1023,
            为何这样做呢?   因为该函数的目的就是找一个空闲CPU, 如果当前CPU有负载, 说明不空闲,
            因此统一设置为 1023, 仅仅是为了表示该CPU不是空闲而已
            同时一趟遍历下来, 如果没有空闲CPU, 则最小负载值值都是 1023  */

                if ((contrib < min_runnable_load) ||
                        (contrib == min_runnable_load &&                        /*  如果有多个 CPU 的 contrib 值相同            */
                         curr_last_migration < min_target_last_migration)) {    /*  那么选择该调度域中最近一个发生过迁移的CPU   */
                        /*
                         * if the load is the same target the CPU with
                         * the longest time since a migration.
                         * This is to spread migration load between
                         * members of a domain more evenly when the
                         * domain is fully loaded
                         */
                        min_runnable_load = contrib;
                        min_cpu_runnable_temp = cpu;
                        min_target_last_migration = curr_last_migration;
                }
        }

        if (min_cpu)
                *min_cpu = min_cpu_runnable_temp;

        return min_runnable_load;
}

/*
 * Calculate the task starvation
 * This is the ratio of actually running time vs. runnable time.
 * If the two are equal the task is getting the cpu time it needs or
 * it is alone on the cpu and the cpu is fully utilized.
 */
static inline unsigned int hmp_task_starvation(struct sched_entity *se)
{
        u32 starvation;

        starvation = se->avg.usage_avg_sum * scale_load_down(NICE_0_LOAD);
        starvation /= (se->avg.runnable_avg_sum + 1);

        return scale_load(starvation);
}

/*  为(大核上)负载最轻的进程调度实体 se 选择一个合适的迁移目标CPU(小核)
 *
 *
 *  调用关系
 *      run_rebalance_domains( )
 *          ->  hmp_force_up_migration( )
 *              ->  hmp_offload_down( )
 *  */
static inline unsigned int hmp_offload_down(int cpu, struct sched_entity *se)
{
        int min_usage;
        int dest_cpu = NR_CPUS;

        if (hmp_cpu_is_slowest(cpu))    /*  如果当前CPU就是一个小核, 则无需迁移, */
                return NR_CPUS;

        /* Is there an idle CPU in the current domain
     * 查找当前 cpu 调度域(大核)是否有空闲 CPU  */
        min_usage = hmp_domain_min_load(hmp_cpu_domain(cpu), NULL, NULL);
        if (min_usage == 0) {           /*  如果大核中有空闲的 CPU, 则也不做迁移    */
                trace_sched_hmp_offload_abort(cpu, min_usage, "load");
                return NR_CPUS;
        }

        /* Is the task alone on the cpu? */
        if (cpu_rq(cpu)->cfs.h_nr_running < 2) {    /*  如果该 CPU 的运行队列中正在运行的进程只有一个或者没有, 那么也不需要迁移 */
                trace_sched_hmp_offload_abort(cpu,
                        cpu_rq(cpu)->cfs.h_nr_running, "nr_running");
                return NR_CPUS;
        }

        /* Is the task actually starving? 判断当前进程是否饥饿  */
        /* >=25% ratio running/runnable = starving */
        if (hmp_task_starvation(se) > 768) {    /*  当starving > 75% 时说明该进程一直渴望获得更多的 CPU 时间, 这样的进程也不适合迁移    */
                trace_sched_hmp_offload_abort(cpu, hmp_task_starvation(se),
                        "starvation");
                return NR_CPUS;
        }

        /* Does the slower domain have any idle CPUs?
     * 检查小核的调度域中是否有空闲的CPU
     * 如果有的话返回该空闲 CPU,
     * 如果没有的话返回 NR_CPUS, 说明没找到合适的CPU用做迁移的目的地
     * */
        min_usage = hmp_domain_min_load(hmp_slower_domain(cpu), &dest_cpu,
                        tsk_cpus_allowed(task_of(se)));

        if (min_usage == 0) {
                trace_sched_hmp_offload_succeed(cpu, dest_cpu);
                return dest_cpu;
        } else
                trace_sched_hmp_offload_abort(cpu,min_usage,"slowdomain");
        return NR_CPUS;
}
#endif /* CONFIG_SCHED_HMP */





////#endif /* CONFIG_SMP */


////#ifdef CONFIG_SMP




#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER
        if (PA_MON_ENABLE) {
                if (strcmp(p->comm, PA_MON) == 0) {
                        pr_warn("[PA] %s Balance From CPU%d to CPU%d\n", p->comm, env->src_rq->cpu, env->dst_rq->cpu);
                }
        }
#endif /* CONFIG_HMP_POWER_AWARE_CONTROLLER */
}




/* in second round load balance, we migrate heavy load_weight task
     as long as RT tasks exist in busy cpu*/
#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
        #define over_imbalance(lw, im) \
                (((lw)/2 > (im)) && \
                ((env->mt_check_cache_in_idle == 1) || \
                (env->src_rq->rt.rt_nr_running == 0) || \
                (pulled > 0)))
#else
        #define over_imbalance(lw, im) (((lw) / 2) > (im))
#endif



#if defined (CONFIG_MTK_SCHED_CMP_LAZY_BALANCE) && !defined(CONFIG_HMP_LAZY_BALANCE)
static int need_lazy_balance(int dst_cpu, int src_cpu, struct task_struct *p)
{
        /* Lazy balnace for small task
           1. src cpu is buddy cpu
           2. src cpu is not busy cpu
           3. p is light task
         */
#ifdef CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER
        if (PA_ENABLE && cpumask_test_cpu(src_cpu, &buddy_cpu_map) &&
            !is_buddy_busy(src_cpu) && is_light_task(p)) {
#else
        if (cpumask_test_cpu(src_cpu, &buddy_cpu_map) &&
            !is_buddy_busy(src_cpu) && is_light_task(p)) {
#endif
#ifdef CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER
                unsigned int i;
                AVOID_LOAD_BALANCE_FROM_CPUX_TO_CPUY_COUNT[src_cpu][dst_cpu]++;
                mt_sched_printf(sched_pa, "[PA]pid=%d, Lazy balance from CPU%d to CPU%d\n)\n",
                                p->pid, src_cpu, dst_cpu);
                for (i = 0; i < 4; i++) {
                        if (PA_MON_ENABLE && (strcmp(p->comm, &PA_MON[i][0]) == 0)) {
                                pr_warn("[PA] %s Lazy balance from CPU%d to CPU%d\n", p->comm,
                                        src_cpu, dst_cpu);
                                /*
                                pr_warn("[PA]   src_cpu RQ Usage = %u, Period = %u, NR = %u\n",
                                        per_cpu(BUDDY_CPU_RQ_USAGE, src_cpu),
                                        per_cpu(BUDDY_CPU_RQ_PERIOD, src_cpu),
                                        per_cpu(BUDDY_CPU_RQ_NR, src_cpu));
                                pr_warn("[PA]   Task Usage = %u, Period = %u\n",
                                        p->se.avg.usage_avg_sum,
                                        p->se.avg.runnable_avg_period);
                                */
                        }
                }
#endif
                return 1;
        } else
                return 0;
}
#endif  /*      (CONFIG_MTK_SCHED_CMP_LAZY_BALANCE) && !defined(CONFIG_HMP_LAZY_BALANCE)        */





#ifdef CONFIG_SCHED_HMP
/*public*/unsigned int hmp_idle_pull(int this_cpu);

/*public*/int move_specific_task(struct lb_env *env, struct task_struct *pm);
#else
/*public*/int move_specific_task(struct lb_env *env, struct task_struct *pm)
{
        return 0;
}
#endif  /*      CONFIG_SCHED_HMP        */






#ifdef CONFIG_SCHED_HMP_LITTLE_PACKING
/*
 * Decide if the tasks on the busy CPUs in the
 * littlest domain would benefit from an idle balance
 */
static int hmp_packing_ilb_needed(int cpu, int ilb_needed)
{
        struct hmp_domain *hmp;
        /* allow previous decision on non-slowest domain */
        if (!hmp_cpu_is_slowest(cpu))
                return ilb_needed;

        /* if disabled, use normal ILB behaviour */
        if (!hmp_packing_enabled)
                return ilb_needed;

        hmp = hmp_cpu_domain(cpu);
        for_each_cpu_and(cpu, &hmp->cpus, nohz.idle_cpus_mask) {
                /* only idle balance if a CPU is loaded over threshold */
                if (cpu_rq(cpu)->avg.load_avg_ratio > hmp_full_threshold)
                        return 1;
        }
        return 0;
}
#endif  /*      CONFIG_HMP_PACK_SMALL_TASK      */


DEFINE_PER_CPU(cpumask_var_t, ilb_tmpmask);


/////////////////
#ifdef CONFIG_NO_HZ_COMMON
//////////////

#ifdef CONFIG_HMP_PACK_SMALL_TASK

static inline int find_new_ilb(int call_cpu)
{
#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER

        struct sched_domain *sd;

        int ilb_new = nr_cpu_ids;

        int ilb_return = 0;

        int ilb = cpumask_first(nohz.idle_cpus_mask);


        if (PA_ENABLE) {
                int buddy = per_cpu(sd_pack_buddy, call_cpu);

                /*
                 * If we have a pack buddy CPU, we try to run load balance on a CPU
                 * that is close to the buddy.
                 */
                if (buddy != -1)
                        for_each_domain(buddy, sd) {
                                if (sd->flags & SD_SHARE_CPUPOWER)
                                        continue;

                                ilb_new = cpumask_first_and(sched_domain_span(sd),
                                                nohz.idle_cpus_mask);

                                if (ilb_new < nr_cpu_ids)
                                        break;

                        }
        }

        if (ilb < nr_cpu_ids && idle_cpu(ilb)) {
                ilb_return = 1;
        }

        if (ilb_new < nr_cpu_ids) {
                if (idle_cpu(ilb_new)) {
                        if (PA_ENABLE && ilb_return && ilb_new != ilb) {
                                AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[call_cpu][ilb]++;

#ifdef CONFIG_HMP_TRACER
                                trace_sched_power_aware_active(POWER_AWARE_ACTIVE_MODULE_AVOID_WAKE_UP_FORM_CPUX_TO_CPUY, 0, call_cpu, ilb);
#endif /* CONFIG_HMP_TRACER */

                        }
                        return ilb_new;
                }
        }

        if (ilb_return) {
                return ilb;
        }

        return nr_cpu_ids;

#else                           /* CONFIG_HMP_POWER_AWARE_CONTROLLER */

        struct sched_domain *sd;
        int ilb = cpumask_first(nohz.idle_cpus_mask);
        int buddy = per_cpu(sd_pack_buddy, call_cpu);

        /*
         * If we have a pack buddy CPU, we try to run load balance on a CPU
         * that is close to the buddy.
         */
        if (buddy != -1)
                for_each_domain(buddy, sd) {
                        if (sd->flags & SD_SHARE_CPUPOWER)
                                continue;

                        ilb = cpumask_first_and(sched_domain_span(sd),
                                        nohz.idle_cpus_mask);

                        if (ilb < nr_cpu_ids)
                                break;
                }

        if (ilb < nr_cpu_ids && idle_cpu(ilb))
                return ilb;

        return nr_cpu_ids;

#endif                          /* CONFIG_HMP_POWER_AWARE_CONTROLLER */
}

#else   /*      #ifdef CONFIG_HMP_PACK_SMALL_TASK       */

static inline int find_new_ilb(int call_cpu)
{

//        int ilb = cpumask_first(nohz.idle_cpus_mask);
//#ifdef CONFIG_MTK_SCHED_CMP_TGS
//        /* Find nohz balancing to occur in the same cluster firstly */
//        int new_ilb;
//        struct cpumask tmp;
//        /* Find idle cpu with online one */
//        get_cluster_cpus(&tmp, arch_get_cluster_id(call_cpu), true);
//        new_ilb = cpumask_first_and(nohz.idle_cpus_mask, &tmp);
//        if (new_ilb < nr_cpu_ids && idle_cpu(new_ilb)) {
//#ifdef CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER
//               if (new_ilb != ilb) {
//                       mt_sched_printf(sched_pa, "[PA]find_new_ilb(cpu%x), new_ilb = %d, ilb = %d\n", call_cpu, new_ilb, ilb);
//                        AVOID_WAKE_UP_FROM_CPUX_TO_CPUY_COUNT[call_cpu][ilb]++;
//                }
//#endif
//                return new_ilb;
//        }
//#endif /* CONFIG_MTK_SCHED_CMP_TGS */
//
//       if (ilb < nr_cpu_ids && idle_cpu(ilb))
//                return ilb;
//
//        return nr_cpu_ids;
//
//#endif /* CONFIG_HMP_PACK_SMALL_TASK */
        int ilb = cpumask_first(nohz.idle_cpus_mask);
#ifdef CONFIG_SCHED_HMP
        int ilb_needed = 0;
        int cpu;
        struct cpumask* tmp = per_cpu(ilb_tmpmask, smp_processor_id());

        /* restrict nohz balancing to occur in the same hmp domain */
        ilb = cpumask_first_and(nohz.idle_cpus_mask,
                        &((struct hmp_domain *)hmp_cpu_domain(call_cpu))->cpus);

        /* check to see if it's necessary within this domain */
        cpumask_andnot(tmp,
                        &((struct hmp_domain *)hmp_cpu_domain(call_cpu))->cpus,
                        nohz.idle_cpus_mask);
        for_each_cpu(cpu, tmp) {
                if (cpu_rq(cpu)->nr_running > 1) {
                        ilb_needed = 1;
                        break;
                }
        }

#ifdef CONFIG_SCHED_HMP_LITTLE_PACKING
        if (ilb < nr_cpu_ids)
                ilb_needed = hmp_packing_ilb_needed(ilb, ilb_needed);
#endif  /*      #ifdef CONFIG_SCHED_HMP_LITTLE_PACKING  */

        if (ilb_needed && ilb < nr_cpu_ids && idle_cpu(ilb))
                return ilb;
#else   /*      #ifdef CONFIG_SCHED_HMP */
        if (ilb < nr_cpu_ids && idle_cpu(ilb))
                return ilb;
#endif  /*     #ifdef CONFIG_SCHED_HMP  */

        return nr_cpu_ids;
}
#endif  /*      CONFIG_HMP_PACK_SMALL_TASK      */

#endif  /*      CONFIG_NO_HZ_COMMON     */














////////////////////////////////////////////////////////////////////////////////
#ifdef  CONFIG_SCHED_HMP
////////////////////////////////////////////////////////////////////////////////
static DEFINE_SPINLOCK(hmp_force_migration);


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
/*
static int hmp_active_task_migration_cpu_stop(void *data)
{
        return __do_active_load_balance_cpu_stop(data, false);
}
*/

/*
 * hmp_can_migrate_task - may task p from runqueue rq be migrated to this_cpu?
 * Ideally this function should be merged with can_migrate_task() to avoid
 * redundant code.
 */
static int hmp_can_migrate_task(struct task_struct *p, struct lb_env *env)
{
        int tsk_cache_hot = 0;

        /*
         * We do not migrate tasks that are:
         * 1) running (obviously), or
         * 2) cannot be migrated to this CPU due to cpus_allowed
         */
        if (!cpumask_test_cpu(env->dst_cpu, tsk_cpus_allowed(p))) {
                schedstat_inc(p, se.statistics.nr_failed_migrations_affine);
                return 0;
        }
        env->flags &= ~LBF_ALL_PINNED;

        if (task_running(env->src_rq, p)) {
                schedstat_inc(p, se.statistics.nr_failed_migrations_running);
                return 0;
        }

        /*
         * Aggressive migration if:
         * 1) task is cache cold, or
         * 2) too many balance attempts have failed.
         */

#if defined(CONFIG_MT_LOAD_BALANCE_ENHANCEMENT)
        tsk_cache_hot = task_hot(p, env->src_rq->clock_task, env->sd, env->mt_check_cache_in_idle);
#else
        tsk_cache_hot = task_hot(p, env->src_rq->clock_task, env->sd);
#endif
        if (!tsk_cache_hot ||
                env->sd->nr_balance_failed > env->sd->cache_nice_tries) {
#ifdef CONFIG_SCHEDSTATS
                if (tsk_cache_hot) {
                        schedstat_inc(env->sd, lb_hot_gained[env->idle]);
                        schedstat_inc(p, se.statistics.nr_forced_migrations);
                }
#endif
                return 1;
        }

        return 1;
}


/*
 * move_specific_task tries to move a specific task.
 * Returns 1 if successful and 0 otherwise.
 * Called with both runqueues locked.
 */
int move_specific_task(struct lb_env *env, struct task_struct *pm)
{
        struct task_struct *p, *n;

        list_for_each_entry_safe(p, n, &env->src_rq->cfs_tasks, se.group_node) {
        if (throttled_lb_pair(task_group(p), env->src_rq->cpu,
                                env->dst_cpu))
                continue;

                if (!hmp_can_migrate_task(p, env))
                        continue;
                /* Check if we found the right task */
                if (p != pm)
                        continue;

                move_task(p, env);
                /*
                 * Right now, this is only the third place move_task()
                 * is called, so we can safely collect move_task()
                 * stats here rather than inside move_task().
                 */
                schedstat_inc(env->sd, lb_gained[env->idle]);
                return 1;
        }
        return 0;
}




#ifdef CONFIG_SCHED_HMP_ENHANCEMENT     /*      &&      defined(CONFIG_SCHED_HMP)       */

/*
 * Heterogenous Multi-Processor (HMP) - Declaration and Useful Macro
 */

/* Function Declaration */
int hmp_up_stable(int cpu);
int hmp_down_stable(int cpu);
unsigned int hmp_up_migration(int cpu, int *target_cpu, struct sched_entity *se,
                                     struct clb_env *clbenv);
unsigned int hmp_down_migration(int cpu, int *target_cpu, struct sched_entity *se,
                                       struct clb_env *clbenv);

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
 * Heterogenous Multi-Processor (HMP) - Utility Function
 */

/*
 * These functions add next up/down migration delay that prevents the task from
 * doing another migration in the same direction until the delay has expired.
 */
int hmp_up_stable(int cpu)
{
        struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
        u64 now = cfs_rq_clock_task(cfs_rq);
        if (((now - hmp_last_up_migration(cpu)) >> 10) < hmp_next_up_threshold)
                return 0;
        return 1;
}

int hmp_down_stable(int cpu)
{
        struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
        u64 now = cfs_rq_clock_task(cfs_rq);
        if (((now - hmp_last_down_migration(cpu)) >> 10) < hmp_next_down_threshold)
                return 0;
        return 1;
}

/* Select the most appropriate CPU from hmp cluster */
static unsigned int hmp_select_cpu(unsigned int caller, struct task_struct *p,
                        struct cpumask *mask, int prev)
{
        int curr = 0;
        int target = NR_CPUS;
        unsigned long curr_wload = 0;
        unsigned long target_wload = 0;
        struct cpumask srcp;
        cpumask_and(&srcp, cpu_online_mask, mask);
        target = cpumask_any_and(&srcp, tsk_cpus_allowed(p));
        if (NR_CPUS == target)
                goto out;

        /*
         * RT class is taken into account because CPU load is multiplied
         * by the total number of CPU runnable tasks that includes RT tasks.
         */
        target_wload = hmp_inc(cfs_load(target));
        target_wload += cfs_pending_load(target);
        target_wload *= rq_length(target);
        for_each_cpu(curr, mask) {
                /* Check CPU status and task affinity */
                if (!cpu_online(curr) || !cpumask_test_cpu(curr, tsk_cpus_allowed(p)))
                        continue;

                /* For global load balancing, unstable CPU will be bypassed */
                if (hmp_caller_is_gb(caller) && !hmp_cpu_stable(curr))
                        continue;

                curr_wload = hmp_inc(cfs_load(curr));
                curr_wload += cfs_pending_load(curr);
                curr_wload *= rq_length(curr);
                if (curr_wload < target_wload) {
                        target_wload = curr_wload;
                        target = curr;
                } else if (curr_wload == target_wload && curr == prev) {
                        target = curr;
                }
        }

out:
        return target;
}

/*
 * Heterogenous Multi-Processor (HMP) - Task Runqueue Selection
 */

/* This function enhances the original task selection function */
int hmp_select_task_rq_fair(int sd_flag, struct task_struct *p,
                        int prev_cpu, int new_cpu)
{
#ifdef CONFIG_HMP_TASK_ASSIGNMENT
        int step = 0;
        struct sched_entity *se = &p->se;
        int B_target = NR_CPUS;
        int L_target = NR_CPUS;
        struct clb_env clbenv;

#ifdef CONFIG_HMP_TRACER
        int cpu = 0;
        for_each_online_cpu(cpu)
            trace_sched_cfs_runnable_load(cpu, cfs_load(cpu), cfs_length(cpu));
#endif

        /* error handling */
        if (prev_cpu >= NR_CPUS)
                return new_cpu;

        /*
         * Skip all the checks if only one CPU is online.
         * Otherwise, select the most appropriate CPU from cluster.
         */
        if (num_online_cpus() == 1)
                goto out;
        B_target = hmp_select_cpu(HMP_SELECT_RQ, p, &hmp_fast_cpu_mask, prev_cpu);
        L_target = hmp_select_cpu(HMP_SELECT_RQ, p, &hmp_slow_cpu_mask, prev_cpu);

        /*
         * Only one cluster exists or only one cluster is allowed for this task
         * Case 1: return the runqueue whose load is minimum
         * Case 2: return original CFS runqueue selection result
         */
#ifdef CONFIG_HMP_DISCARD_CFS_SELECTION_RESULT
        if (NR_CPUS == B_target && NR_CPUS == L_target)
                goto out;
        if (NR_CPUS == B_target)
                goto select_slow;
        if (NR_CPUS == L_target)
                goto select_fast;
#else
        if (NR_CPUS == B_target || NR_CPUS == L_target)
                goto out;
#endif

        /*
         * Two clusters exist and both clusters are allowed for this task
         * Step 1: Move newly created task to the cpu where no tasks are running
         * Step 2: Migrate heavy-load task to big
         * Step 3: Migrate light-load task to LITTLE
         * Step 4: Make sure the task stays in its previous hmp domain
         */
        step = 1;
        if (task_created(sd_flag) && !task_low_priority(p->prio)) {
                if (!rq_length(B_target))
                        goto select_fast;
                if (!rq_length(L_target))
                        goto select_slow;
        }
        memset(&clbenv, 0, sizeof(clbenv));
        clbenv.flags |= HMP_SELECT_RQ;
        cpumask_copy(&clbenv.lcpus, &hmp_slow_cpu_mask);
        cpumask_copy(&clbenv.bcpus, &hmp_fast_cpu_mask);
        clbenv.ltarget = L_target;
        clbenv.btarget = B_target;
        sched_update_clbstats(&clbenv);
        step = 2;
        if (hmp_up_migration(L_target, &B_target, se, &clbenv))
                goto select_fast;
        step = 3;
        if (hmp_down_migration(B_target, &L_target, se, &clbenv))
                goto select_slow;
        step = 4;
        if (hmp_cpu_is_slow(prev_cpu))
                goto select_slow;
        goto select_fast;

select_fast:
        new_cpu = B_target;
        goto out;
select_slow:
        new_cpu = L_target;
        goto out;

out:

        /* it happens when num_online_cpus=1 */
        if (new_cpu >= nr_cpu_ids) {
                /* BUG_ON(1); */
                new_cpu = prev_cpu;
        }

        cfs_nr_pending(new_cpu)++;
        cfs_pending_load(new_cpu) += se_load(se);
#ifdef CONFIG_HMP_TRACER
        trace_sched_hmp_load(clbenv.bstats.load_avg, clbenv.lstats.load_avg);
        trace_sched_hmp_select_task_rq(p, step, sd_flag, prev_cpu, new_cpu,
                                       se_load(se), &clbenv.bstats, &clbenv.lstats);
#endif
#ifdef CONFIG_MET_SCHED_HMP
        HmpLoad(clbenv.bstats.load_avg, clbenv.lstats.load_avg);
#endif
#endif /* CONFIG_HMP_TASK_ASSIGNMENT */
        return new_cpu;
}

/*
 * Heterogenous Multi-Processor (HMP) - Task Dynamic Migration Threshold
 */

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

static inline void hmp_dynamic_threshold(struct clb_env *clbenv)
{
        struct clb_stats *L = &clbenv->lstats;
        struct clb_stats *B = &clbenv->bstats;
        unsigned int hmp_threshold_diff = hmp_up_threshold - hmp_down_threshold;
        unsigned int B_normalized_acap = hmp_pos(HMP_RATIO(B->scaled_acap));
        unsigned int B_normalized_atask = hmp_pos(HMP_RATIO(B->scaled_atask));
        unsigned int L_normalized_acap = hmp_pos(L->scaled_acap);
        unsigned int L_normalized_atask = hmp_pos(L->scaled_atask);

#ifdef CONFIG_HMP_DYNAMIC_THRESHOLD
        L->threshold = hmp_threshold_diff;
        L->threshold *= hmp_inc(L_normalized_acap) * hmp_inc(L_normalized_atask);
        L->threshold /= hmp_inc(B_normalized_acap + L_normalized_acap);
        L->threshold /= hmp_inc(B_normalized_atask + L_normalized_atask);
        L->threshold = hmp_down_threshold + L->threshold;

        B->threshold = hmp_threshold_diff;
        B->threshold *= hmp_inc(B_normalized_acap) * hmp_inc(B_normalized_atask);
        B->threshold /= hmp_inc(B_normalized_acap + L_normalized_acap);
        B->threshold /= hmp_inc(B_normalized_atask + L_normalized_atask);
        B->threshold = hmp_up_threshold - B->threshold;
#else                           /* !CONFIG_HMP_DYNAMIC_THRESHOLD */
        clbenv->lstats.threshold = hmp_down_threshold;  /* down threshold */
        clbenv->bstats.threshold = hmp_up_threshold;    /* up threshold */
#endif                          /* CONFIG_HMP_DYNAMIC_THRESHOLD */

        mt_sched_printf(sched_log, "[%s]\tup/dl:%4d/%4d bcpu(%d):%d/%d, lcpu(%d):%d/%d\n", __func__,
                        B->threshold, L->threshold,
                        clbenv->btarget, clbenv->bstats.cpu_capacity, clbenv->bstats.cpu_power,
                        clbenv->ltarget, clbenv->lstats.cpu_capacity, clbenv->lstats.cpu_power);
}

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
                                                                        struct clb_env *clbenv)
{
        struct task_struct *p = task_of(se);
        struct clb_stats *L, *B;
        struct mcheck *check;
        int curr_cpu = cpu;
        unsigned int caller = clbenv->flags;

        L = &clbenv->lstats;
        B = &clbenv->bstats;
        check = &clbenv->mcheck;

        check->status = clbenv->flags;
        check->status |= HMP_TASK_UP_MIGRATION;
        check->result = 0;

        /*
         * No migration is needed if
         * 1) There is only one cluster
         * 2) Task is already in big cluster
         * 3) It violates task affinity
         */
        if (!L->ncpu || !B->ncpu
                || cpumask_test_cpu(curr_cpu, &clbenv->bcpus)
                || !cpumask_intersects(&clbenv->bcpus, tsk_cpus_allowed(p)))
                goto out;

        /*
         * [1] Migration stabilizing
         * Let the task load settle before doing another up migration.
         * It can prevent a bunch of tasks from migrating to a unstable CPU.
         */
        if (!hmp_up_stable(*target_cpu))
                goto out;

        /* [2] Filter low-priorty task */
#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
        if (hmp_low_prio_task_up_rejected(p, B, L)) {
                check->status |= HMP_LOW_PRIORITY_FILTER;
                goto trace;
        }
#endif

        /* [2.5]if big is idle, just go to big */
        if (rq_length(*target_cpu) == 0) {
                check->status |= HMP_BIG_IDLE;
                check->status |= HMP_MIGRATION_APPROVED;
                check->result = 1;
                goto trace;
        }

        /*
         * [3] Check CPU capacity
         * Forbid up-migration if big CPU can't handle this task
         */
        if (!hmp_task_fast_cpu_afford(B, se, *target_cpu)) {
                check->status |= HMP_BIG_CAPACITY_INSUFFICIENT;
                goto trace;
        }

        /*
         * [4] Check dynamic migration threshold
         * Migrate task from LITTLE to big if load is greater than up-threshold
         */
        if (se_load(se) > B->threshold) {
                check->status |= HMP_MIGRATION_APPROVED;
                check->result = 1;
        }

trace:
#ifdef CONFIG_HMP_TRACER
        if (check->result && hmp_caller_is_gb(caller))
                hmp_stats.nr_force_up++;
        trace_sched_hmp_stats(&hmp_stats);
        trace_sched_dynamic_threshold(task_of(se), B->threshold, check->status,
                                      curr_cpu, *target_cpu, se_load(se), B, L);
#endif
#ifdef CONFIG_MET_SCHED_HMP
        TaskTh(B->threshold, L->threshold);
        HmpStat(&hmp_stats);
#endif
out:
        return check->result;
}

/*
 * Check whether this task should be migrated to LITTLE
 * Briefly summarize the flow as below;
 * 1) Migration stabilizing
 * 1.5) Keep all cpu busy
 * 2) Filter low-priorty task
 * 3) Check CPU capacity
 * 4) Check dynamic migration threshold
 */
unsigned int hmp_down_migration(int cpu, int *target_cpu, struct sched_entity *se,
                                                                                struct clb_env *clbenv)
{
        struct task_struct *p = task_of(se);
        struct clb_stats *L, *B;
        struct mcheck *check;
        int curr_cpu = cpu;
        unsigned int caller = clbenv->flags;

        L = &clbenv->lstats;
        B = &clbenv->bstats;
        check = &clbenv->mcheck;

        check->status = caller;
        check->status |= HMP_TASK_DOWN_MIGRATION;
        check->result = 0;

        /*
         * No migration is needed if
         * 1) There is only one cluster
         * 2) Task is already in LITTLE cluster
         * 3) It violates task affinity
         */
        if (!L->ncpu || !B->ncpu
                || cpumask_test_cpu(curr_cpu, &clbenv->lcpus)
            || !cpumask_intersects(&clbenv->lcpus, tsk_cpus_allowed(p)))
                goto out;

        /*
         * [1] Migration stabilizing
         * Let the task load settle before doing another down migration.
         * It can prevent a bunch of tasks from migrating to a unstable CPU.
         */
        if (!hmp_down_stable(*target_cpu))
                goto out;

        /* [1.5]if big is busy and little is idle, just go to little */
        if (rq_length(*target_cpu) == 0 && caller == HMP_SELECT_RQ && rq_length(curr_cpu) > 0) {
                check->status |= HMP_BIG_BUSY_LITTLE_IDLE;
                check->status |= HMP_MIGRATION_APPROVED;
                check->result = 1;
                goto trace;
        }

        /* [2] Filter low-priorty task */
#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
        if (hmp_low_prio_task_down_allowed(p, B, L)) {
                cfs_nr_dequeuing_low_prio(curr_cpu)++;
                check->status |= HMP_LOW_PRIORITY_FILTER;
                check->status |= HMP_MIGRATION_APPROVED;
                check->result = 1;
                goto trace;
        }
#endif

        /*
         * [3] Check CPU capacity
         * Forbid down-migration if either of the following conditions is true
         * 1) big cpu is not oversubscribed (if big CPU seems to have spare
         *    cycles, do not force this task to run on LITTLE CPU, but
         *    keep it staying in its previous cluster instead)
         * 2) LITTLE cpu doesn't have available capacity for this new task
         */
        if (!hmp_fast_cpu_oversubscribed(caller, B, se, curr_cpu)) {
                check->status |= HMP_BIG_NOT_OVERSUBSCRIBED;
                goto trace;
        }

        if (!hmp_task_slow_cpu_afford(L, se)) {
                check->status |= HMP_LITTLE_CAPACITY_INSUFFICIENT;
                goto trace;
        }

        /*
         * [4] Check dynamic migration threshold
         * Migrate task from big to LITTLE if load ratio is less than
         * or equal to down-threshold
         */
        if (L->threshold >= se_load(se)) {
                check->status |= HMP_MIGRATION_APPROVED;
                check->result = 1;
        }

trace:
#ifdef CONFIG_HMP_TRACER
        if (check->result && hmp_caller_is_gb(caller))
                hmp_stats.nr_force_down++;
        trace_sched_hmp_stats(&hmp_stats);
        trace_sched_dynamic_threshold(task_of(se), L->threshold, check->status,
                                      curr_cpu, *target_cpu, se_load(se), B, L);
#endif
#ifdef CONFIG_MET_SCHED_HMP
        TaskTh(B->threshold, L->threshold);
        HmpStat(&hmp_stats);
#endif
out:
        return check->result;
}




/*
 * Heterogenous Multi-Processor (HMP) Global Load Balance
 */

/*
 * According to Linaro's comment, we should only check the currently running
 * tasks because selecting other tasks for migration will require extensive
 * book keeping.
 */
#ifdef CONFIG_HMP_GLOBAL_BALANCE
static void hmp_force_down_migration(int this_cpu)
{
        int curr_cpu, target_cpu;
        struct sched_entity *se;
        struct rq *target;
        unsigned long flags;
        unsigned int force;
        struct task_struct *p;
        struct clb_env clbenv;

        /* Migrate light task from big to LITTLE */
        for_each_cpu(curr_cpu, &hmp_fast_cpu_mask) {
                /* Check whether CPU is online */
                if (!cpu_online(curr_cpu))
                        continue;

                force = 0;
                target = cpu_rq(curr_cpu);
                raw_spin_lock_irqsave(&target->lock, flags);
                se = target->cfs.curr;
                if (!se) {
                        raw_spin_unlock_irqrestore(&target->lock, flags);
                        continue;
                }

                /* Find task entity */
                if (!entity_is_task(se)) {
                        struct cfs_rq *cfs_rq;
                        cfs_rq = group_cfs_rq(se);
                        while (cfs_rq) {
                                se = cfs_rq->curr;
                                cfs_rq = group_cfs_rq(se);
                        }
                }

                p = task_of(se);
                target_cpu = hmp_select_cpu(HMP_GB, p, &hmp_slow_cpu_mask, -1);
                if (NR_CPUS == target_cpu) {
                        raw_spin_unlock_irqrestore(&target->lock, flags);
                        continue;
                }

                /* Collect cluster information */
                memset(&clbenv, 0, sizeof(clbenv));
                clbenv.flags |= HMP_GB;
                clbenv.btarget = curr_cpu;
                clbenv.ltarget = target_cpu;
                cpumask_copy(&clbenv.lcpus, &hmp_slow_cpu_mask);
                cpumask_copy(&clbenv.bcpus, &hmp_fast_cpu_mask);
                sched_update_clbstats(&clbenv);

                /* Check migration threshold */
                if (!target->active_balance &&
                        hmp_down_migration(curr_cpu, &target_cpu, se, &clbenv) &&
                        !cpu_park(cpu_of(target))) {
                        if (p->state != TASK_DEAD) {
                                target->active_balance = 1; /* force down */
                                target->push_cpu = target_cpu;
                                target->migrate_task = p;
                                force = 1;
                                trace_sched_hmp_migrate(p, target->push_cpu, 1);
                                hmp_next_down_delay(&p->se, target->push_cpu);
                        }
                }
                raw_spin_unlock_irqrestore(&target->lock, flags);
                if (force) {
                        if (stop_one_cpu_dispatch(cpu_of(target),
                                hmp_active_task_migration_cpu_stop,
                                target, &target->active_balance_work)) {
                                raw_spin_lock_irqsave(&target->lock, flags);
                                target->active_balance = 0;
                                force = 0;
                                raw_spin_unlock_irqrestore(&target->lock, flags);
                        }
                }
        }
}
#endif /* CONFIG_HMP_GLOBAL_BALANCE */


#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER
u32 AVOID_FORCE_UP_MIGRATION_FROM_CPUX_TO_CPUY_COUNT[NR_CPUS][NR_CPUS];
#endif /* CONFIG_HMP_POWER_AWARE_CONTROLLER */


/*public */void hmp_force_up_migration(int this_cpu)
{
        int curr_cpu, target_cpu;
        struct sched_entity *se;
        struct rq *target;
        unsigned long flags;
        unsigned int force;
        struct task_struct *p;
        struct clb_env clbenv;
#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER
        int push_cpu;
#endif  /*     ONFIG_HMP_POWER_AWARE_CONTROLLER */

        if (!spin_trylock(&hmp_force_migration))
                return;

#ifdef CONFIG_HMP_TRACER
        for_each_online_cpu(curr_cpu)
            trace_sched_cfs_runnable_load(curr_cpu, cfs_load(curr_cpu), cfs_length(curr_cpu));
#endif  /*      #ifdef CONFIG_HMP_TRACER        */

        /* Migrate heavy task from LITTLE to big */
        for_each_cpu(curr_cpu, &hmp_slow_cpu_mask) {
                /* Check whether CPU is online */
                if (!cpu_online(curr_cpu))
                        continue;

                force = 0;
                target = cpu_rq(curr_cpu);
                raw_spin_lock_irqsave(&target->lock, flags);
                se = target->cfs.curr;
                if (!se) {
                        raw_spin_unlock_irqrestore(&target->lock, flags);
                        continue;
                }

                /* Find task entity */
                if (!entity_is_task(se)) {
                        struct cfs_rq *cfs_rq;
                        cfs_rq = group_cfs_rq(se);
                        while (cfs_rq) {
                                se = cfs_rq->curr;
                                cfs_rq = group_cfs_rq(se);
                        }
                }

                p = task_of(se);
                target_cpu = hmp_select_cpu(HMP_GB, p, &hmp_fast_cpu_mask, -1);
                if (NR_CPUS == target_cpu) {
                        raw_spin_unlock_irqrestore(&target->lock, flags);
                        continue;
                }

                /* Collect cluster information */
                memset(&clbenv, 0, sizeof(clbenv));
                clbenv.flags |= HMP_GB;
                clbenv.ltarget = curr_cpu;
                clbenv.btarget = target_cpu;
                cpumask_copy(&clbenv.lcpus, &hmp_slow_cpu_mask);
                cpumask_copy(&clbenv.bcpus, &hmp_fast_cpu_mask);
                sched_update_clbstats(&clbenv);

#ifdef CONFIG_HMP_LAZY_BALANCE
#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER
                if (PA_ENABLE && LB_ENABLE) {
#endif                          /* CONFIG_HMP_POWER_AWARE_CONTROLLER */
                        if (is_light_task(p) && !is_buddy_busy(per_cpu(sd_pack_buddy, curr_cpu))) {
#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER
                                push_cpu = hmp_select_cpu(HMP_GB, p, &hmp_fast_cpu_mask, -1);
                                if (hmp_cpu_is_fast(push_cpu)) {
                                        AVOID_FORCE_UP_MIGRATION_FROM_CPUX_TO_CPUY_COUNT[curr_cpu][push_cpu]++;
#ifdef CONFIG_HMP_TRACER
                                        trace_sched_power_aware_active(POWER_AWARE_ACTIVE_MODULE_AVOID_FORCE_UP_FORM_CPUX_TO_CPUY, p->pid, curr_cpu, push_cpu);
#endif                          /* CONFIG_HMP_TRACER */
                                }
#endif                          /* CONFIG_HMP_POWER_AWARE_CONTROLLER */
                                goto out_force_up;
                        }
#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER
                }
#endif                          /* CONFIG_HMP_POWER_AWARE_CONTROLLER */
#endif                          /* CONFIG_HMP_LAZY_BALANCE */

                /* Check migration threshold */
                if (!target->active_balance &&
                        hmp_up_migration(curr_cpu, &target_cpu, se, &clbenv) &&
                        !cpu_park(cpu_of(target))) {
                        if (p->state != TASK_DEAD) {
                                target->active_balance = 1; /* force up */
                                target->push_cpu = target_cpu;
                                target->migrate_task = p;
                                force = 1;
                                trace_sched_hmp_migrate(p, target->push_cpu, 1);
                                hmp_next_up_delay(&p->se, target->push_cpu);
                        }
                }

#ifdef CONFIG_HMP_LAZY_BALANCE
out_force_up:
#endif                          /* CONFIG_HMP_LAZY_BALANCE */

                raw_spin_unlock_irqrestore(&target->lock, flags);
                if (force) {
                        if (stop_one_cpu_dispatch(cpu_of(target),
                                hmp_active_task_migration_cpu_stop,
                                target, &target->active_balance_work)) {
                                raw_spin_lock_irqsave(&target->lock, flags);
                                target->active_balance = 0;
                                force = 0;
                                raw_spin_unlock_irqrestore(&target->lock, flags);
                        }
                }
        }

#ifdef CONFIG_HMP_GLOBAL_BALANCE
        hmp_force_down_migration(this_cpu);
#endif  /*      CONFIG_HMP_GLOBAL_BALANCE       */

#ifdef CONFIG_HMP_TRACER
        trace_sched_hmp_load(clbenv.bstats.load_avg, clbenv.lstats.load_avg);
#endif  /*      #ifdef CONFIG_HMP_TRACER        */
        spin_unlock(&hmp_force_migration);
}


/*
 * hmp_idle_pull looks at little domain runqueues to see
 * if a task should be pulled.
 *
 * Reuses hmp_force_migration spinlock.
 *
 */
unsigned int hmp_idle_pull(int this_cpu)
{
    //  need rewrite
    return -1;
}


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
static unsigned int hmp_task_eligible_for_up_migration(struct sched_entity *se)
{
        /* below hmp_up_threshold, never eligible */
        if (se->avg.load_avg_ratio < hmp_up_threshold)
                return 0;
        return 1;
}

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
unsigned int hmp_up_migration(int cpu, int *target_cpu, struct sched_entity *se)
{
        struct task_struct *p = task_of(se);
        int temp_target_cpu;
        u64 now;

        if (hmp_cpu_is_fastest(cpu))        /*  首先判断 se 原来所运行的 cpu 是否在大核的调度域  */
                return 0;                       /*  如果已经在大核的调度域内, 则没必要进行迁移繁忙的进程　se 到大核 target_cpu 上   */

#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
        /* Filter by task priority
     * hmp_up_prio 用于过滤优先级高于该值的进程,
     * 如果待迁移进程 p 优先级大于 hmp_up_prio ,
     * 那么也没必要迁移到大核 CPU 上
     *
     * 这个要打开 CONFIG_SCHED_HMP_PRIO_FILTER  这个宏才能开始此功能,
     * 另外注意优先级的数值是越低优先级越高,
     * 因此此功能类似于如果任务优先级比较低,
     * 可以设置一个阀值 hmp_up_prio,
     * 优先级低于阀值的进程即使再繁重我们也不需要对他进行迁移,
     * 从而保证高优先级进行优先得到响应和迁移   */
        if (p->prio >= hmp_up_prio)
                return 0;
#endif
        if (!hmp_task_eligible_for_up_migration(se))    /*  判断该进程实体se的平均负载是否高于 hmp_up_threshold 这个阀值   */
                return 0;

        /* Let the task load settle before doing another up migration */
        /* hack - always use clock from first online CPU */
    /*  迁移时间上的过滤,
     *  该进程上一次迁移离现在的时间间隔小于 hmp_next_up_threshold 阀值的也不需要迁移,
     *  从而避免进程被迁移来迁移去  */
        now = cpu_rq(cpumask_first(cpu_online_mask))->clock_task;
        if (((now - se->avg.hmp_last_up_migration) >> 10)
                                        < hmp_next_up_threshold)
                return 0;

        /* hmp_domain_min_load only returns 0 for an
         * idle CPU or 1023 for any partly-busy one.
         * Be explicit about requirement for an idle CPU.
     *
     * 查找大核调度域中是否有空闲的CPU
         */
        if (hmp_domain_min_load(hmp_faster_domain(cpu), &temp_target_cpu,
                        tsk_cpus_allowed(p)) == 0 && temp_target_cpu != NR_CPUS) {  /*  如果在大核调度域中找到了一个空闲的CPU, 即target_cpu */
                if(target_cpu)
                        *target_cpu = temp_target_cpu;  /*  设置 target_cpu 为查找到的 CPU                              */
                return 1;                           /*  返回1, 在大核调度域中发现了空闲的 CPU, 用 target_cpu 返回   */
        }
        return 0;
}

/* Check if task should migrate to a slower cpu
 * 检查当前进程调度实体是否可以被迁移至一个小核
 * */
unsigned int hmp_down_migration(int cpu, struct sched_entity *se)
{
        struct task_struct *p = task_of(se);
        u64 now;

        if (hmp_cpu_is_slowest(cpu)) {          /*  如果当前调度实体所在的 CPU 已经是小核       */
#ifdef CONFIG_SCHED_HMP_LITTLE_PACKING      /*  小任务封包补丁, 运行小任务在小核之间迁移    */
                if(hmp_packing_enabled)
                        return 1;
                else
#endif
                return 0;
        }

#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
        /* Filter by task priority */
        if ((p->prio >= hmp_up_prio) &&
                cpumask_intersects(&hmp_slower_domain(cpu)->cpus,
                                        tsk_cpus_allowed(p))) {
                return 1;
        }
#endif

        /* Let the task load settle before doing another down migration */
        /* hack - always use clock from first online CPU */
        now = cpu_rq(cpumask_first(cpu_online_mask))->clock_task;
        if (((now - se->avg.hmp_last_down_migration) >> 10)
                                        < hmp_next_down_threshold)
                return 0;

        if (cpumask_intersects(&hmp_slower_domain(cpu)->cpus,
                                        tsk_cpus_allowed(p))
                && se->avg.load_avg_ratio < hmp_down_threshold) {
                return 1;
        }
        return 0;
}


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
static void hmp_migrate_runnable_task(struct rq *rq)
{
        struct sched_domain *sd;
        int src_cpu = cpu_of(rq);                   /*  迁移的源cpu     */
        struct rq *src_rq = rq;
        int dst_cpu = rq->push_cpu;                 /*  迁移的目的cpu   */
        struct rq *dst_rq = cpu_rq(dst_cpu);
        struct task_struct *p = rq->migrate_task;
        /*
         * One last check to make sure nobody else is playing
         * with the source rq.
         */
        if (src_rq->active_balance)
                goto out;

        if (src_rq->nr_running <= 1)    /*  源 CPU 的运行队列上的任务数目 <= 1, 则不应该迁移    */
                goto out;

        if (task_rq(p) != src_rq)       /*  如果待迁移进程 p 并不在源 CPU (的任务队列)上, 则也不能迁移    */
                goto out;
        /*
         * Not sure if this applies here but one can never
         * be too cautious
         */
        BUG_ON(src_rq == dst_rq);

        double_lock_balance(src_rq, dst_rq);        /*  对源迁移任务队列src_rq 和 目的任务 dst_rq 加锁  */

        rcu_read_lock();
        for_each_domain(dst_cpu, sd) {
                if (cpumask_test_cpu(src_cpu, sched_domain_span(sd)))
                        break;
        }
    /*  这里和内核默认的负载均衡调度器的 load_balance( ) 函数一样使用 struct lb_env 结构体来描述迁移信息
     *  迁移的动作是 move_specific_task()函数
     *
     *  move_specific_task( ) 函数的实现和 load_balance( )函数里实现的类似  */
        if (likely(sd)) {
                struct lb_env env = {
                        .sd             = sd,
                        .dst_cpu        = dst_cpu,
                        .dst_rq         = dst_rq,
                        .src_cpu        = src_cpu,
                        .src_rq         = src_rq,
                        .idle           = CPU_IDLE,
                };

                schedstat_inc(sd, alb_count);

                if (move_specific_task(&env, p))
                        schedstat_inc(sd, alb_pushed);
                else
                        schedstat_inc(sd, alb_failed);
        }

        rcu_read_unlock();
        double_unlock_balance(src_rq, dst_rq);
out:
        put_task_struct(p);
}


/*
 * hmp_force_up_migration checks runqueues for tasks that need to
 * be actively migrated to a faster cpu.
 *
 * 调用关系
 * run_rebalance_domains( )
 *  ->  hmp_force_up_migration( )
 */
static void hmp_force_up_migration(int this_cpu)
{
        int cpu, target_cpu;
        struct sched_entity *curr, *orig;
        struct rq *target;
        unsigned long flags;
        unsigned int force, got_target;
        struct task_struct *p;

    /*  hmp_force_migration 是一个 HMP 定义的锁  */
        if (!spin_trylock(&hmp_force_migration))
                return;
    /*  从头开始遍历 所有在线(活跃)的 CPU, 由cpu_online_mask 标识   */
        for_each_online_cpu(cpu) {
                force = 0;
                got_target = 0;
                target = cpu_rq(cpu);   /*  获取到 CPU 的运行队列   */
                raw_spin_lock_irqsave(&target->lock, flags);    /*  对运行队列加锁  */
                curr = target->cfs.curr;    /*  获取到 CPU 上当前的调度实体 */
                if (!curr || target->active_balance) {  /*  如果当前CPU上正在做负载均衡, 则跳过该CPU    */
                        raw_spin_unlock_irqrestore(&target->lock, flags);
                        continue;
                }
                if (!entity_is_task(curr)) {    /*  如果当前运行的调度实体不是进程, 而是一个进程组, 则从中取出正在运行的进, 而是一个进程组, 则从中取出正在运行的进程程  */
                        struct cfs_rq *cfs_rq;

                        cfs_rq = group_cfs_rq(curr);
                        while (cfs_rq) {
                                curr = cfs_rq->curr;
                                cfs_rq = group_cfs_rq(curr);
                        }
                }
        /*  任务迁移分为向上迁移和向下迁移,
         *
         *  向上迁移(hmp_up_migration)      -=> 将小核上负载最大且符合迁移要求的进程调度实体迁移至空闲的大核target_cpu
         *  向下迁移(hmp_down_migration)    -=> 将大核上负载最小且符合迁移要求的进程调度实体迁移至空闲的小核target_cpu
         *
         *  首先检查是否可以进行向上迁移
         *
         *  如果当前遍历到的CPU核是小核, 则
         *      hmp_get_heaviest_task   从该小核CPU上找到一个负载最大的进程调度实体curr
         *      hmp_up_migration        判断负载最大的进程是否满足迁移到大核上所需的条件
         *                              如果满足迁移的要求, 而当前所有的大核中也有空闲的大核
         *                              则返回 1, 并用target_cpu 标记空闲的大核
         *      下面通知内核可以将任务的向上迁移
         *      cpu_rq(target_cpu)->wake_for_idle_pull = 1; 设置将要迁移的目标 CPU(target_cpu) 运行队列上的 wake_for_idle_pull 标志位
         *      smp_send_reschedule(target_cpu);            发送一个IPI_RESCHEDULE 的 IPI 中断给 target_cpu
         *
         *  */
                orig = curr;
                curr = hmp_get_heaviest_task(curr, -1); /*  从当前 CPU(小核) 中找到负载 se->avg.load_avg_ratio 最大(即最繁忙)的那个进程*/
                if (!curr) {                            /*  没有需要迁移的进程*/
                        raw_spin_unlock_irqrestore(&target->lock, flags);
                        continue;
                }
                p = task_of(curr);                      /*  获取到找到的大负载进行的 task_struct 结构 */
                if (hmp_up_migration(cpu, &target_cpu, curr)) {         /*  判断刚才取得的最大负载的调度实体 curr 是否需要迁移到大核 CPU 上, 如果需要则找到大核中一个空闲的 CPU(target_cpu)  */
                        cpu_rq(target_cpu)->wake_for_idle_pull = 1;         /*  设置将要迁移的目标 CPU(target_cpu) 运行队列上的 wake_for_idle_pull 标志位  */
                        raw_spin_unlock_irqrestore(&target->lock, flags);
                        spin_unlock(&hmp_force_migration);
                        smp_send_reschedule(target_cpu);                    /*  发送一个IPI_RESCHEDULE 的 IPI 中断给 target_cpu */
                        return;
                }
                /*  刚才那个 CPU 真是小幸运,
         *      正好它是小核上的 CPU
         *      并且有合适迁移到大核上的进程(负载大于阀值, 且近期未发生迁移, 进程优先级也满足阀值要求)
         *      最重要的是大核调度域上有空闲的 CPU,
         *  这叫作无巧不成书.
         *
         *  我们下面看看没那么好运气的其他 CPU 的情况, 一般来说有如下几种情况
         *      hmp_up_migration( )返回了 0
         *          一种是调度实体curr需要向上迁移, 但是大核的调度域内没有空闲的的大核CPU
         *          另外一种是调度实体不需要迁移(不满足阀值要求, 或者curr本身就在大核上)
         *
         *  我们首先考虑curr本身就在大核上运行的情况其他的情况,
         *  curr运行在大核上时hmp_get_heaviest_task直接返回了curr(此时orig == curr)
         *  而hmp_up_migration( )在判断进程在大核上运行时也直接返回了0,
         */

        /*  任务迁移分为向上迁移和向下迁移,
         *
         *  向上迁移(hmp_up_migration)      -=> 将小核上负载最大且符合迁移要求的进程调度实体迁移至空闲的大核target_cpu
         *  向下迁移(hmp_down_migration)    -=> 将大核上负载最小且符合迁移要求的进程调度实体迁移至空闲的小核target_cpu
         *
         *  接着检查是否可以进行向下迁移
         *
         *  如果当前遍历到的CPU核是大核, 则
         *      hmp_get_lightest_task   从该大核 CPU 上找到一个负载最小的进程调度实体 curr
         *      hmp_offload_down        判断负载最小的进程是否满足迁移到小核上所需的条件
         *                              如果满足迁移的要求, 而当前所有的小核中也有空闲的小核
         *                              则返回可以迁移的空闲的小核
         *
         *      接着完善迁移信息, 填充迁移源 CPU 的任务队列 rq(target) 的信息,
         *      包括如下信息    :
         *          迁移目标cpu(target->push_cpu)
         *          待迁移进程target->migrate_task = p;
         *
         *      最后内核完成任务的向下迁移
         *          如果待迁移的任务没在运行, 直接调用 hmp_migrate_runnable_task(target)完成任务迁移
         *          如果带迁移的任务在运行, 则调用 stop_one_cpu_nowait(cpu_of(target) 暂停迁移源 CPU 后, 强行进行迁移
         */
        if (!got_target) {                                      /*  如果此时没有*/
                        /*
                         * For now we just check the currently running task.
                         * Selecting the lightest task for offloading will
                         * require extensive book keeping.
                         */
                        curr = hmp_get_lightest_task(orig, 1);          /*  返回 orig 调度实体对应的运行队列中任务最轻的调度实体min_se  */
                        p = task_of(curr);
                        target->push_cpu = hmp_offload_down(cpu, curr); /*  查询刚才找到的最轻负载的进程能迁移到哪个 CPU上去, 返回迁移目标target->push_cpu  */
                        if (target->push_cpu < NR_CPUS) {               /*  如果返回值是NR_CPUS, 则表示没有找到合适的迁移目标CPU    */
                                get_task_struct(p);
                                target->migrate_task = p;
                                got_target = 1;
                                trace_sched_hmp_migrate(p, target->push_cpu, HMP_MIGRATE_OFFLOAD);
                                hmp_next_down_delay(&p->se, target->push_cpu);      /*  更新调度实体的hmp_last_down_migration和hmp_last_up_migration 记录为现在时刻的时间   */
                        }
                }
                /*
                 * We have a target with no active_balance.  If the task
                 * is not currently running move it, otherwise let the
                 * CPU stopper take care of it.
                 */
                if (got_target) {
                        if (!task_running(target, p)) {     /*  如果要迁移的进程p没有正在运行, 即p->on_cpu == 0, 则可以进行迁移 */
                                trace_sched_hmp_migrate_force_running(p, 0);
                                hmp_migrate_runnable_task(target);  /*  完成任务迁移*/
                        } else {                            /*  否则待迁移的进程如果正在运行            */
                                target->active_balance = 1;     /*  设置 target 运行队列的 avtive_balance   */
                                force = 1;                      /*  设置强制迁移标示为1                     */
                        }
                }

                raw_spin_unlock_irqrestore(&target->lock, flags);

                if (force)              /*  依照前面所述, 待迁移的进程 p 正在运行中, 则 force被置为 1 */
                        stop_one_cpu_nowait(cpu_of(target),
                                hmp_active_task_migration_cpu_stop,
                                target, &target->active_balance_work);  /*  暂停迁移源 CPU 后, 强行进行迁移 */
        /*  自此我们了解到, 如果*/
        }
        spin_unlock(&hmp_force_migration);
}


/*
 * hmp_idle_pull looks at little domain runqueues to see
 * if a task should be pulled.
 *
 * Reuses hmp_force_migration spinlock.
 *
 */
unsigned int hmp_idle_pull(int this_cpu)
{
        int cpu;
        struct sched_entity *curr, *orig;
        struct hmp_domain *hmp_domain = NULL;
        struct rq *target = NULL, *rq;
        unsigned long flags, ratio = 0;
        unsigned int force = 0;
        struct task_struct *p = NULL;

        if (!hmp_cpu_is_slowest(this_cpu))      /*  检查当前 CPU 是不是大核 */
                hmp_domain = hmp_slower_domain(this_cpu);   /*  如果是大核(不是小核), 则取出小核的 domain 结构*/
        if (!hmp_domain)
                return 0;

        if (!spin_trylock(&hmp_force_migration))    /*  加锁    */
                return 0;

        /* first select a task
     * for循环遍历小核调度域上的所有 CPU, 找出该 CPU 的运行队列中负载最重的进程 curr    */
        for_each_cpu(cpu, &hmp_domain->cpus) {
                rq = cpu_rq(cpu);
                raw_spin_lock_irqsave(&rq->lock, flags);
                curr = rq->cfs.curr;
                if (!curr) {
                        raw_spin_unlock_irqrestore(&rq->lock, flags);
                        continue;
                }
                if (!entity_is_task(curr)) {
                        struct cfs_rq *cfs_rq;

                        cfs_rq = group_cfs_rq(curr);
                        while (cfs_rq) {
                                curr = cfs_rq->curr;
                                if (!entity_is_task(curr))
                                        cfs_rq = group_cfs_rq(curr);
                                else
                                        cfs_rq = NULL;
                        }
                }
                orig = curr;
                curr = hmp_get_heaviest_task(curr, this_cpu);   /*  从当前CPU中找到负载最大(即最繁忙)的那个进程 */
                /* check if heaviest eligible task on this
                 * CPU is heavier than previous task
                 */
                if (curr && hmp_task_eligible_for_up_migration(curr) && /*  先判断这个负载重的进程是否合适迁移到大核 CPU 上(比较其负载是否大于阀值)   */
                        curr->avg.load_avg_ratio > ratio &&
                        cpumask_test_cpu(this_cpu,
                                        tsk_cpus_allowed(task_of(curr)))) {
                        p = task_of(curr);
                        target = rq;
                        ratio = curr->avg.load_avg_ratio;
                }
                raw_spin_unlock_irqrestore(&rq->lock, flags);
        }

        if (!p)
                goto done;

        /* now we have a candidate
     * 迁移进程 migrate_task    :   是刚才找到的 p = task_of(curr) 进程
     * 迁移源 CPU               :   迁移进程对应的 CPU
     * 迁移目的地 CPU           :   当前 CPU, 当前 CPU 是大核调度域中的一个
     * */
        raw_spin_lock_irqsave(&target->lock, flags);
        if (!target->active_balance && task_rq(p) == target) {
                get_task_struct(p);
                target->push_cpu = this_cpu;        /*  迁移的目的 CPU  */
                target->migrate_task = p;           /*  待迁移的进程    */
                trace_sched_hmp_migrate(p, target->push_cpu, HMP_MIGRATE_IDLE_PULL);
                hmp_next_up_delay(&p->se, target->push_cpu);    /*  更新当前迁移的时间为 now    */
                /*
                 * if the task isn't running move it right away.
                 * Otherwise setup the active_balance mechanic and let
                 * the CPU stopper do its job.
                 */
                if (!task_running(target, p)) {                     /*  如果待迁移的进程不在运行    */
                        trace_sched_hmp_migrate_idle_running(p, 0);
                        hmp_migrate_runnable_task(target);              /*  直接对进程进行迁移          */
                } else {
                        target->active_balance = 1;
                        force = 1;                                      /*  否则设置强制迁移标识 force 为 1 */
                }
        }
        raw_spin_unlock_irqrestore(&target->lock, flags);

        if (force) {
                /* start timer to keep us awake */
                hmp_cpu_keepalive_trigger();                    /*  为大核设置保活监控定时器  */
                stop_one_cpu_nowait(cpu_of(target),             /*  如果迁移进程正在运行, 那么和之前一样, 调用 stop_one_cpu_nowait( ) 函数强行迁移  */
                        hmp_active_task_migration_cpu_stop,
                        target, &target->active_balance_work);
        }
done:
        spin_unlock(&hmp_force_migration);
        return force;
}

#else   /* CONFIG_SCHED_HMP_ENHANCEMENT */

static void hmp_force_up_migration(int this_cpu) { }

#endif /* CONFIG_SCHED_HMP_ENHANCEMENT */





////////////////////////////////////////////////////////////////////////////////
#endif  /*      CONFIG_SCHED_HMP        */
////////////////////////////////////////////////////////////////////////////////

#endif /* CONFIG_SMP */











#ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
static u32 cpufreq_calc_scale(u32 min, u32 max, u32 curr)
{
        u32 result = curr / max;
        return result;
}

#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER
DEFINE_PER_CPU(u32, FREQ_CPU);
#endif /* CONFIG_HMP_POWER_AWARE_CONTROLLER */


/* Called when the CPU Frequency is changed.
 * Once for each CPU.
 */
static int cpufreq_callback(struct notifier_block *nb,
                                        unsigned long val, void *data)
{
        struct cpufreq_freqs *freq = data;
        int cpu = freq->cpu;
        struct cpufreq_extents *extents;
#ifdef CONFIG_SCHED_HMP_ENHANCEMENT
        struct cpumask *mask;
        int id;
#endif

        if (freq->flags & CPUFREQ_CONST_LOOPS)
                return NOTIFY_OK;

        if (val != CPUFREQ_POSTCHANGE)
                return NOTIFY_OK;

        /* if dynamic load scale is disabled, set the load scale to 1.0 */
        if (!hmp_data.freqinvar_load_scale_enabled) {
                freq_scale[cpu].curr_scale = 1024;
                return NOTIFY_OK;
        }

        extents = &freq_scale[cpu];
#ifdef CONFIG_SCHED_HMP_ENHANCEMENT
        if (extents->max < extents->const_max) {
                extents->throttling = 1;
        } else {
                extents->throttling = 0;
        }
#endif
        if (extents->flags & SCHED_LOAD_FREQINVAR_SINGLEFREQ) {
                /* If our governor was recognised as a single-freq governor,
                 * use 1.0
                 */
                extents->curr_scale = 1024;
        } else {
#ifdef CONFIG_SCHED_HMP_ENHANCEMENT
                extents->curr_scale = cpufreq_calc_scale(extents->min,
                                extents->const_max, freq->new);
#else
                extents->curr_scale = cpufreq_calc_scale(extents->min,
                                extents->max, freq->new);
#endif
        }
#ifdef CONFIG_SCHED_HMP_ENHANCEMENT
        mask = arch_better_capacity(cpu) ? &hmp_fast_cpu_mask : &hmp_slow_cpu_mask;
        for_each_cpu(id, mask)
            freq_scale[id].curr_scale = extents->curr_scale;
#endif

#if NR_CPUS == 4
#ifdef CONFIG_SCHED_HMP_ENHANCEMENT
        switch (cpu) {
        case 0:
        case 2:
                (extents + 1)->curr_scale = extents->curr_scale;
                break;

        case 1:
        case 3:
                (extents - 1)->curr_scale = extents->curr_scale;
                break;

        default:

                break;
        }
#endif  /*  CONFIG_SCHED_HMP_ENHANCEMENT    */
#endif  /*  NR_CPUS == 4                    */

#ifdef CONFIG_HMP_POWER_AWARE_CONTROLLER
        per_cpu(FREQ_CPU, cpu) = freq->new;
#endif /* CONFIG_HMP_POWER_AWARE_CONTROLLER */

        return NOTIFY_OK;
}

/* Called when the CPUFreq governor is changed.
 * Only called for the CPUs which are actually changed by the
 * userspace.
 */
static int cpufreq_policy_callback(struct notifier_block *nb,
                                       unsigned long event, void *data)
{
        struct cpufreq_policy *policy = data;
        struct cpufreq_extents *extents;
        int cpu, singleFreq = 0;
        static const char performance_governor[] = "performance";
        static const char powersave_governor[] = "powersave";

        if (event == CPUFREQ_START)
                return 0;

        if (event != CPUFREQ_INCOMPATIBLE)
                return 0;

        /* CPUFreq governors do not accurately report the range of
         * CPU Frequencies they will choose from.
         * We recognise performance and powersave governors as
         * single-frequency only.
         */
        if (!strncmp(policy->governor->name, performance_governor,
                        strlen(performance_governor)) ||
                !strncmp(policy->governor->name, powersave_governor,
                                strlen(powersave_governor)))
                singleFreq = 1;

        /* Make sure that all CPUs impacted by this policy are
         * updated since we will only get a notification when the
         * user explicitly changes the policy on a CPU.
         */
        for_each_cpu(cpu, policy->cpus) {
                extents = &freq_scale[cpu];
                extents->max = policy->max >> SCHED_FREQSCALE_SHIFT;
                extents->min = policy->min >> SCHED_FREQSCALE_SHIFT;
 #ifdef CONFIG_SCHED_HMP_ENHANCEMENT
                extents->const_max = policy->cpuinfo.max_freq >> SCHED_FREQSCALE_SHIFT;
#endif
                if (!hmp_data.freqinvar_load_scale_enabled) {
                        extents->curr_scale = 1024;
                } else if (singleFreq) {
                        extents->flags |= SCHED_LOAD_FREQINVAR_SINGLEFREQ;
                        extents->curr_scale = 1024;
                } else {
                        extents->flags &= ~SCHED_LOAD_FREQINVAR_SINGLEFREQ;
#ifdef CONFIG_SCHED_HMP_ENHANCEMENT
                        extents->curr_scale = cpufreq_calc_scale(extents->min,
                                        extents->const_max, policy->cur);
#else
                        extents->curr_scale = cpufreq_calc_scale(extents->min,
                                        extents->max, policy->cur);
#endif
                }
        }

        return 0;
}

static struct notifier_block cpufreq_notifier = {
        .notifier_call  = cpufreq_callback,
};
static struct notifier_block cpufreq_policy_notifier = {
        .notifier_call  = cpufreq_policy_callback,
};

static int __init register_sched_cpufreq_notifier(void)
{
        int ret = 0;

        /* init safe defaults since there are no policies at registration */
        for (ret = 0; ret < CONFIG_NR_CPUS; ret++) {
                /* safe defaults */
                freq_scale[ret].max = 1024;
                freq_scale[ret].min = 1024;
                freq_scale[ret].curr_scale = 1024;
        }

        pr_info("sched: registering cpufreq notifiers for scale-invariant loads\n");
        ret = cpufreq_register_notifier(&cpufreq_policy_notifier,
                        CPUFREQ_POLICY_NOTIFIER);

        if (ret != -EINVAL)
                ret = cpufreq_register_notifier(&cpufreq_notifier,
                        CPUFREQ_TRANSITION_NOTIFIER);

        return ret;
}

core_initcall(register_sched_cpufreq_notifier);
#endif /* CONFIG_HMP_FREQUENCY_INVARIANT_SCALE */


#ifdef CONFIG_HEVTASK_INTERFACE /*  add by gatieme(ChengJean)*/
/*  add by gatieme(ChengJean)
 *  for /proc/task_detect
 *  to show the hmp information
 *  per online cpu, show the task_struct in it
 *  p->se.avg.load_avg_ratio, p->pid, task_cpu(p), (p->utime + p->stime), p->comm
 *  see https://github.com/meizuosc/m681/blob/master/kernel/sched/fair.c
 *  */


/*
 *  * This allows printing both to /proc/task_detect and
 *   * to the console
 *    */
#ifndef CONFIG_KGDB_KDB
#define SEQ_printf(m, x...)                     \
        do {                                            \
                if (m)                                  \
                seq_printf(m, x);               \
                else                                    \
                pr_debug(x);                    \
        } while (0)
#else
#define SEQ_printf(m, x...)                     \
        do {                                            \
                if (m)                                  \
                seq_printf(m, x);               \
                else if (__get_cpu_var(kdb_in_use) == 1)                \
                kdb_printf(x);                  \
                else                                            \
                pr_debug(x);                            \
        } while (0)
#endif

static int task_detect_show(struct seq_file *m, void *v)
{
        struct task_struct *p;
        unsigned long flags;
        unsigned int i;

#ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
        for (i = 0; i < NR_CPUS; i++) {
                SEQ_printf(m, "%5d ", freq_scale[i].curr_scale);
        }
#endif

        SEQ_printf(m, "\n%lu\n ", jiffies_to_cputime(jiffies));

        for (i = 0; i < NR_CPUS; i++) {
                raw_spin_lock_irqsave(&cpu_rq(i)->lock, flags);
                if (cpu_online(i)) {
                        list_for_each_entry(p, &cpu_rq(i)->cfs_tasks, se.group_node) {
                                SEQ_printf(m, "%lu %5d %5d %lu (%15s)\n ",
                                           p->se.avg.load_avg_ratio, p->pid, task_cpu(p),
                                           (p->utime + p->stime), p->comm);
                        }
                }
                raw_spin_unlock_irqrestore(&cpu_rq(i)->lock, flags);

        }

        return 0;
}

static int task_detect_open(struct inode *inode, struct file *filp)
{
        return single_open(filp, task_detect_show, NULL);
}

static const struct file_operations task_detect_fops = {
        .open           = task_detect_open,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = single_release,
};

static int __init init_task_detect_procfs(void)
{
        struct proc_dir_entry *pe;

        pe = proc_create("task_detect", 0444, NULL, &task_detect_fops);
        if (!pe)
                return -ENOMEM;
        return 0;
}

__initcall(init_task_detect_procfs);
#endif /* CONFIG_HEVTASK_INTERFACE */
