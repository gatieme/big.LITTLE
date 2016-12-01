/*
 * arch/arm64/kernel/topology.c
 *
 * Copyright (C) 2011,2013,2014 Linaro Limited.
 *
 * Based on the arm32 version written by Vincent Guittot in turn based on
 * arch/sh/kernel/topology.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/percpu.h>
#include <linux/node.h>
#include <linux/nodemask.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>
#include <trace/events/sched.h>	/*	add by gatieme(ChengJean)	*/


/*
 * cpu power table
 * This per cpu data structure describes the relative capacity of each core.
 * On a heteregenous system, cores don't have the same computation capacity
 * and we reflect that difference in the cpu_power field so the scheduler can
 * take this difference into account during load balance. A per cpu structure
 * is preferred because each CPU updates its own cpu_power field during the
 * load balance except for idle cores. One idle core is selected to run the
 * rebalance_domains for all idle cores and the cpu_power can be updated
 * during this sequence.
 */
/*	add by gatieme(ChengJean)	for CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY	*/
/* when CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY is in use, a new measure of
 * compute capacity is available. This is limited to a maximum of 1024 and
 * scaled between 0 and 1023 according to frequency.
 * Cores with different base CPU powers are scaled in line with this.
 * CPU capacity for each core represents a comparable ratio to maximum
 * achievable core compute capacity for a core in this system.
 *
 * e.g.1 If all cores in the system have a base CPU power of 1024 according to
 * efficiency calculations and are DVFS scalable between 500MHz and 1GHz, the
 * cores currently at 1GHz will have CPU power of 1024 whilst the cores
 * currently at 500MHz will have CPU power of 512.
 *
 * e.g.2
 * If core 0 has a base CPU power of 2048 and runs at 500MHz & 1GHz whilst
 * core 1 has a base CPU power of 1024 and runs at 100MHz and 200MHz, then
 * the following possibilities are available:
 *
 * cpu power\| 1GHz:100Mhz | 1GHz : 200MHz | 500MHz:100MHz | 500MHz:200MHz |
 * ----------|-------------|---------------|---------------|---------------|
 *    core 0 |    1024     |     1024      |     512       |     512       |
 *    core 1 |     256     |      512      |     256       |     512       |
 *
 * This information may be useful to the scheduler when load balancing,
 * so that the compute capacity of the core a task ran on can be baked into
 * task load histories.
 */
static DEFINE_PER_CPU(unsigned long, cpu_scale);
#ifdef CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY /*      体系结构自定义的 CPU 负载和频率计算     */
static DEFINE_PER_CPU(unsigned long, base_cpu_capacity);
static DEFINE_PER_CPU(unsigned long, invariant_cpu_capacity);
static DEFINE_PER_CPU(unsigned long, prescaled_cpu_capacity);
#endif /* CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY */

static int frequency_invariant_power_enabled = 1;
/* >0=1, <=0=0 */
void arch_set_invariant_power_enabled(int val)
{
	if(val>0)
		frequency_invariant_power_enabled = 1;
	else
		frequency_invariant_power_enabled = 0;
}

int arch_get_invariant_power_enabled(void)
{
	return frequency_invariant_power_enabled;
}

unsigned long arch_scale_freq_power(struct sched_domain *sd, int cpu)
{
	return per_cpu(cpu_scale, cpu);
}

static void set_power_scale(unsigned int cpu, unsigned long power)
{
	per_cpu(cpu_scale, cpu) = power;
}

#ifdef CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY
unsigned long arch_get_cpu_capacity(int cpu)
{
	return per_cpu(invariant_cpu_capacity, cpu);
}
unsigned long arch_get_max_cpu_capacity(int cpu)
{
	return per_cpu(base_cpu_capacity, cpu);
}
#else
unsigned long arch_get_cpu_capacity(int cpu)
{
	return per_cpu(cpu_scale, cpu);
}
unsigned long arch_get_max_cpu_capacity(int cpu)
{
	return per_cpu(cpu_scale, cpu);
}
#endif /* CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY */





/*      add by gatieme for CONFIG_SCHED_HMP_ENHANCEMENT @ 2016-12-01 20:12      */
//#if define(CONFIG_SCHED_HMP)    &&      !defined(CONFIG_SCHED_HMP_ENHANCEMENT)
#ifndef CONFIG_SCHED_HMP_ENHANCEMENT
static int __init get_cpu_for_node(struct device_node *node)
{
	struct device_node *cpu_node;
	int cpu;

	cpu_node = of_parse_phandle(node, "cpu", 0);
	if (!cpu_node)
		return -1;

	for_each_possible_cpu(cpu) {
		if (of_get_cpu_node(cpu, NULL) == cpu_node) {
			of_node_put(cpu_node);
			return cpu;
		}
	}

	pr_crit("Unable to find CPU node for %s\n", cpu_node->full_name);

	of_node_put(cpu_node);
	return -1;
}

static int __init parse_core(struct device_node *core, int cluster_id,
			     int core_id)
{
	char name[10];
	bool leaf = true;
	int i = 0;
	int cpu;
	struct device_node *t;

	do {
		snprintf(name, sizeof(name), "thread%d", i);
		t = of_get_child_by_name(core, name);
		if (t) {
			leaf = false;
			cpu = get_cpu_for_node(t);
			if (cpu >= 0) {
				cpu_topology[cpu].cluster_id = cluster_id;
				cpu_topology[cpu].core_id = core_id;
				cpu_topology[cpu].thread_id = i;
			} else {
				pr_err("%s: Can't get CPU for thread\n",
				       t->full_name);
				of_node_put(t);
				return -EINVAL;
			}
			of_node_put(t);
		}
		i++;
	} while (t);

	cpu = get_cpu_for_node(core);
	if (cpu >= 0) {
		if (!leaf) {
			pr_err("%s: Core has both threads and CPU\n",
			       core->full_name);
			return -EINVAL;
		}

		cpu_topology[cpu].cluster_id = cluster_id;
		cpu_topology[cpu].core_id = core_id;
	} else if (leaf) {
		pr_err("%s: Can't get CPU for leaf core\n", core->full_name);
		return -EINVAL;
	}

	return 0;
}

static int __init parse_cluster(struct device_node *cluster, int depth)
{
	char name[10];
	bool leaf = true;
	bool has_cores = false;
	struct device_node *c;
	static int cluster_id __initdata;
	int core_id = 0;
	int i, ret;

	/*
	 * First check for child clusters; we currently ignore any
	 * information about the nesting of clusters and present the
	 * scheduler with a flat list of them.
	 */
	i = 0;
	do {
		snprintf(name, sizeof(name), "cluster%d", i);
		c = of_get_child_by_name(cluster, name);
		if (c) {
			leaf = false;
			ret = parse_cluster(c, depth + 1);
			of_node_put(c);
			if (ret != 0)
				return ret;
		}
		i++;
	} while (c);

	/* Now check for cores */
	i = 0;
	do {
		snprintf(name, sizeof(name), "core%d", i);
		c = of_get_child_by_name(cluster, name);
		if (c) {
			has_cores = true;

			if (depth == 0) {
				pr_err("%s: cpu-map children should be clusters\n",
				       c->full_name);
				of_node_put(c);
				return -EINVAL;
			}

			if (leaf) {
				ret = parse_core(c, cluster_id, core_id++);
			} else {
				pr_err("%s: Non-leaf cluster with core %s\n",
				       cluster->full_name, name);
				ret = -EINVAL;
			}

			of_node_put(c);
			if (ret != 0)
				return ret;
		}
		i++;
	} while (c);

	if (leaf && !has_cores)
		pr_warn("%s: empty cluster\n", cluster->full_name);

	if (leaf)
		cluster_id++;

	return 0;
}
//#endif          /*      end     CONFIG_SCHED_HMP && !CONFIG_SCHED_HMP_ENHANCEMENT       */
#endif          /*      #ifndef CONFIG_SCHED_HMP_ENHANCEMENT                            */

struct cpu_efficiency {
	const char *compatible;
	unsigned long efficiency;
};

/*
 * Table of relative efficiency of each processors
 * The efficiency value must fit in 20bit and the final
 * cpu_scale value must be in the range
 *   0 < cpu_scale < 3*SCHED_POWER_SCALE/2
 * in order to return at most 1 when DIV_ROUND_CLOSEST
 * is used to compute the capacity of a CPU.
 * Processors that are not defined in the table,
 * use the default SCHED_POWER_SCALE value for cpu_scale.
 */
static const struct cpu_efficiency table_efficiency[] = {
	{ "arm,cortex-a57", 3891 },
	{ "arm,cortex-a53", 2048 },
	{ NULL, },
};

#ifdef  CONFIG_SCHED_HMP_ENHANCEMENT    /*      add by gatieme(ChengJean) @ 2016-12-01 20:08*/
struct cpu_capacity{
        unsigned long hwid;
        unsigned long capacity;
};
struct cpu_capacity      *__cpu_capacity;
#define cpu_capacity(cpu)       __cpu_capacity[cpu].capacity
#else
static unsigned long *__cpu_capacity;
#define cpu_capacity(cpu)	__cpu_capacity[cpu]
#endif

static unsigned long middle_capacity = 1;
#ifdef  CONFIG_SCHED_HMP_ENHANCEMENT    /*      add by gatieme(ChengJean) @ 2016-12-01 20:15    */
unsigned long min_capacity = (unsigned long)(-1);
unsigned long max_capacity = 0;
#endif

/*
 * Iterate all CPUs' descriptor in DT and compute the efficiency
 * (as per table_efficiency). Also calculate a middle efficiency
 * as close as possible to  (max{eff_i} - min{eff_i}) / 2
 * This is later used to scale the cpu_power field such that an
 * 'average' CPU is of middle power. Also see the comments near
 * table_efficiency[] and update_cpu_power().
 */
static int __init parse_dt_topology(void)
{
	struct device_node *cn, *map;
	int ret = 0;
	int cpu;

	cn = of_find_node_by_path("/cpus");
	if (!cn) {
		pr_err("No CPU information found in DT\n");
		return 0;
	}

	/*
	 * When topology is provided cpu-map is essentially a root
	 * cluster with restricted subnodes.
	 */
	map = of_get_child_by_name(cn, "cpu-map");
	if (!map)
		goto out;

	ret = parse_cluster(map, 0);
	if (ret != 0)
		goto out_map;

	/*
	 * Check that all cores are in the topology; the SMP code will
	 * only mark cores described in the DT as possible.
	 */
	for_each_possible_cpu(cpu) {
		if (cpu_topology[cpu].cluster_id == -1) {
			pr_err("CPU%d: No topology information specified\n",
			       cpu);
			ret = -EINVAL;
		}
	}

out_map:
	of_node_put(map);
out:
	of_node_put(cn);
	return ret;
}

static void __init parse_dt_cpu_power(void)
{
	const struct cpu_efficiency *cpu_eff;
	struct device_node *cn = NULL;

#ifndef  CONFIG_SCHED_HMP_ENHANCEMENT    /*      add by gatieme(ChengJean) @ 2016-12-01 20:08*/
        unsigned long min_capacity = ULONG_MAX;
	unsigned long max_capacity = 0;
#endif
        unsigned long capacity = 0;
	int cpu;

	__cpu_capacity = kcalloc(nr_cpu_ids, sizeof(*__cpu_capacity),
				 GFP_NOWAIT);

	for_each_possible_cpu(cpu) {
		const u32 *rate;
#ifndef  CONFIG_SCHED_HMP_ENHANCEMENT    /*      add by gatieme(ChengJean) @ 2016-12-01 20:08*/
                const u32 *reg;
#endif
		int len;

		/* Too early to use cpu->of_node */
		cn = of_get_cpu_node(cpu, NULL);
		if (!cn) {
			pr_err("Missing device node for CPU %d\n", cpu);
			continue;
		}

		for (cpu_eff = table_efficiency; cpu_eff->compatible; cpu_eff++)
			if (of_device_is_compatible(cn, cpu_eff->compatible))
				break;

		if (cpu_eff->compatible == NULL) {
			pr_warn("%s: Unknown CPU type\n", cn->full_name);
			continue;
		}

		rate = of_get_property(cn, "clock-frequency", &len);
		if (!rate || len != 4) {
			pr_err("%s: Missing clock-frequency property\n",
				cn->full_name);
			continue;
		}

#ifdef  CONFIG_SCHED_HMP_ENHANCEMENT    /*      add by gatieme(ChengJean) @ 2016-12-01 20:08*/
                reg = of_get_property(cn, "reg", &len);
                if(!reg || len != 4) {
                        pr_err("%s Missing reg property\n",
                                cn->full_name);
                }
#endif
                capacity = ((be32_to_cpup(rate)) >> 20) * cpu_eff->efficiency;

		/* Save min capacity of the system */
		if (capacity < min_capacity)
			min_capacity = capacity;

		/* Save max capacity of the system */
		if (capacity > max_capacity)
			max_capacity = capacity;

                cpu_capacity(cpu) = capacity;
#ifdef  CONFIG_SCHED_HMP_ENHANCEMENT    /*      add by gatieme(ChengJean) @ 2016-12-01 20:08*/
                cpu_topology[cpu].core_id = be32_to_cpup(reg) & 0xFF;
                cpu_topology[cpu].socket_id = be32_to_cpup(reg) >> 0x8 & 0xFF;
                pr_debug("CPU%u: hwid(0x%0x) socket_id(%i) core_id(%i) cap(%lu)\n",
                                cpu, be32_to_cpup(reg), cpu_topology[cpu].socket_id, cpu_topology[cpu].core_id, capacity);

                __cpu_capacity[cpu].hwid = be32_to_cpup(reg);
#endif
        }
        /* If min and max capacities are equals, we bypass the update of the
         * cpu_scale because all CPUs have the same capacity. Otherwise, we
         * compute a middle_capacity factor that will ensure that the capacity
	 * of an 'average' CPU of the system will be as close as possible to
	 * SCHED_POWER_SCALE, which is the default value, but with the
	 * constraint explained near table_efficiency[].
	 */
#ifdef  CONFIG_SCHED_HMP_ENHANCEMENT    /*      add by gatieme(ChengJean) @ 2016-12-01 20:08*/
        if (min_capacity == max_capacity)
                        __cpu_capacity[0].hwid = (unsigned long)(-1);
#endif
        if (4 * max_capacity < (3 * (max_capacity + min_capacity)))
		middle_capacity = (min_capacity + max_capacity)
				>> (SCHED_POWER_SHIFT+1);
	else
		middle_capacity = ((max_capacity / 3)
				>> (SCHED_POWER_SHIFT-1)) + 1;
}

/*
 * Look for a customed capacity of a CPU in the cpu_topo_data table during the
 * boot. The update of all CPUs is in O(n^2) for heteregeneous system but the
 * function returns directly for SMP system.
 */
#ifdef  CONFIG_SCHED_HMP_ENHANCEMENT    /*      add by gatieme(ChengJean) @ 2016-12-01 20:08*/
void update_cpu_power(unsigned int cpu, unsigned long hwid)
{
        unsigned int idx = 0;

        /* look for the cpu's hwid in the cpu capacity table */
        for (idx = 0; idx < num_possible_cpus(); idx++) {
                if (__cpu_capacity[idx].hwid == (hwid&0xFFFF))
                        break;

                if (__cpu_capacity[idx].hwid == -1)
                                return;
        }

        if (idx == num_possible_cpus())
                return;

        set_power_scale(cpu, (__cpu_capacity[idx].capacity << SCHED_POWER_SHIFT) / max_capacity);

        printk(KERN_INFO "CPU%u: update cpu_power %lu\n",
                                           cpu, arch_scale_freq_power(NULL, cpu));
}
#else
static void update_cpu_power(unsigned int cpu)
{
	if (!cpu_capacity(cpu))
		return;

	set_power_scale(cpu, cpu_capacity(cpu) / middle_capacity);

	pr_info("CPU%u: update cpu_power %lu\n",
		cpu, arch_scale_freq_power(NULL, cpu));
}
#endif


/*
 * cpu topology table
 */
/*      2016-12-01 21:29        modify by gatieme use cpu_topology instead of cputopol_arm @arm64
 *      3.10/arch/arm/include/asm/topology.h    defined cputopo_arm     in arm
 *      3.10/arch/arm64/include/asm/topology.h  defined cpu_topology    in arm64
 *      */
//#ifdef  CONFIG_SCHED_HMP_ENHANCEMENT    /*      add by gatieme(ChengJean) @ 2016-12-01 20:08*/
//struct cputopol_arm cpu_topology[NR_CPUS];      /*      use socket_id   */
//#else
struct cpu_topology cpu_topology[NR_CPUS];      /*      use cluster_id  */
//#endif
EXPORT_SYMBOL_GPL(cpu_topology);

const struct cpumask *cpu_coregroup_mask(int cpu)
{
	return &cpu_topology[cpu].core_sibling;
}

static void update_siblings_masks(unsigned int cpuid)
{
	struct cpu_topology *cpu_topo, *cpuid_topo = &cpu_topology[cpuid];
	int cpu;

	if (cpuid_topo->cluster_id == -1) {
		/*
		 * DT does not contain topology information for this cpu.
		 */
		pr_debug("CPU%u: No topology information configured\n", cpuid);
		return;
	}

	/* update core and thread sibling masks */
	for_each_possible_cpu(cpu) {
		cpu_topo = &cpu_topology[cpu];

		if (cpuid_topo->cluster_id != cpu_topo->cluster_id)
			continue;

		cpumask_set_cpu(cpuid, &cpu_topo->core_sibling);
		if (cpu != cpuid)
			cpumask_set_cpu(cpu, &cpuid_topo->core_sibling);

		if (cpuid_topo->core_id != cpu_topo->core_id)
			continue;

		cpumask_set_cpu(cpuid, &cpu_topo->thread_sibling);
		if (cpu != cpuid)
			cpumask_set_cpu(cpu, &cpuid_topo->thread_sibling);
	}
#ifdef  CONFIG_SCHED_HMP_ENHANCEMENT    /*      add by gatieme(ChengJean) @ 2016-12-01 201:09   */
        smp_wmb( );
#endif
}


/*
 * cluster_to_logical_mask - return cpu logical mask of CPUs in a cluster
 * @socket_id:		cluster HW identifier
 * @cluster_mask:	the cpumask location to be initialized, modified by the
 *			function only if return value == 0
 *
 * Return:
 *
 * 0 on success
 * -EINVAL if cluster_mask is NULL or there is no record matching socket_id
 */
int cluster_to_logical_mask(unsigned int socket_id, cpumask_t *cluster_mask)
{
	int cpu;

	if (!cluster_mask)
		return -EINVAL;

	for_each_online_cpu(cpu) {
		if (socket_id == topology_physical_package_id(cpu)) {
			cpumask_copy(cluster_mask, topology_core_cpumask(cpu));
			return 0;
		}
	}

	return -EINVAL;
}




#ifdef CONFIG_SCHED_HMP_ENHANCEMENT

void store_cpu_topology(unsigned int cpuid)
{
	struct cpu_topology *cpuid_topo = &cpu_topology[cpuid];
	//struct cputopo_arm *cpuid_topo = &cpu_topology[cpuid];        /*      modify by gatieme use cpu_topology instead of cputopol_arm @arm64       */
	unsigned int mpidr;

	mpidr = read_cpuid_mpidr();

	/* If the cpu topology has been already set, just return */
	if (cpuid_topo->cluster_id != -1)
        //if (cpuid_topo->socket_id != -1)       /*      modify by gatieme use cpu_topology instead of cputopol_arm @arm64       */
		goto topology_populated;

	/* create cpu topology mapping */
	if ((mpidr & MPIDR_SMP_BITMASK) == MPIDR_SMP_VALUE) {
		/*
		 * This is a multiprocessor system
		 * multiprocessor format & multiprocessor mode field are set
		 */

		if (mpidr & MPIDR_MT_BITMASK) {
			/* core performance interdependency */
			cpuid_topo->thread_id = MPIDR_AFFINITY_LEVEL(mpidr, 0);
			cpuid_topo->core_id = MPIDR_AFFINITY_LEVEL(mpidr, 1);
			cpuid_topo->cluster_id = MPIDR_AFFINITY_LEVEL(mpidr, 2);
			//cpuid_topo->socket_id = MPIDR_AFFINITY_LEVEL(mpidr, 2);
		} else {
			/* largely independent cores */
			cpuid_topo->thread_id = -1;
			cpuid_topo->core_id = MPIDR_AFFINITY_LEVEL(mpidr, 0);
			cpuid_topo->cluster_id = MPIDR_AFFINITY_LEVEL(mpidr, 1);
			//cpuid_topo->socket_id = MPIDR_AFFINITY_LEVEL(mpidr, 1);
		}
	} else {
		/*
		 * This is an uniprocessor system
		 * we are in multiprocessor format but uniprocessor system
		 * or in the old uniprocessor format
		 */
		cpuid_topo->thread_id = -1;
		cpuid_topo->core_id = 0;
		cpuid_topo->cluster_id = -1;
		//cpuid_topo->socket_id = -1;
	}

topology_populated:
	update_siblings_masks(cpuid);

	update_cpu_power(cpuid, mpidr & MPIDR_HWID_BITMASK);

	printk(KERN_INFO "CPU%u: thread %d, cpu %d, socket %d, mpidr %x\n",
		   cpuid, cpu_topology[cpuid].thread_id,
		   cpu_topology[cpuid].core_id,
		   cpu_topology[cpuid].cluster_id, mpidr);
		   //cpu_topology[cpuid].socket_id, mpidr);
}

static int cpu_topology_init;
/*
 * init_cpu_topology is called at boot when only one cpu is running
 * which prevent simultaneous write access to cpu_topology array
 */
void __init init_cpu_topology(void)
{
	unsigned int cpu;

	if (cpu_topology_init)
		return;
	/* init core mask and power*/
	for_each_possible_cpu(cpu) {
		struct cpu_topology *cpu_topo = &(cpu_topology[cpu]);

		cpu_topo->thread_id = -1;
		cpu_topo->core_id =  -1;
		cpu_topo->cluster_id = -1;
		//cpu_topo->socket_id = -1;
		cpumask_clear(&cpu_topo->core_sibling);
		cpumask_clear(&cpu_topo->thread_sibling);

		set_power_scale(cpu, SCHED_POWER_SCALE);
	}
	smp_wmb();

	parse_dt_topology();

	cpu_topology_init = 1;

}

void __init arch_build_cpu_topology_domain(void)
{
	init_cpu_topology();
	cpu_topology_init = 1;
}

#else

void store_cpu_topology(unsigned int cpuid)
{
	update_siblings_masks(cpuid);
	update_cpu_power(cpuid);
	printk(KERN_INFO "CPU%u: thread %d, cpu %d, socket %d\n",
			cpuid, cpu_topology[cpuid].thread_id,
			cpu_topology[cpuid].core_id,
 			cpu_topology[cpuid].cluster_id);
 			//cpu_topology[cpuid].socket_id);
}

static void __init reset_cpu_topology(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];

		cpu_topo->thread_id = -1;
		cpu_topo->core_id = 0;
		cpu_topo->cluster_id = -1;

		cpumask_clear(&cpu_topo->core_sibling);
		cpumask_set_cpu(cpu, &cpu_topo->core_sibling);
		cpumask_clear(&cpu_topo->thread_sibling);
		cpumask_set_cpu(cpu, &cpu_topo->thread_sibling);
	}
}

static void __init reset_cpu_power(void)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu)
		set_power_scale(cpu, SCHED_POWER_SCALE);
}




void __init init_cpu_topology(void)
{
	reset_cpu_topology();

	/*
	 * Discard anything that was parsed if we hit an error so we
	 * don't use partial information.
	 */
	if (parse_dt_topology())
		reset_cpu_topology();

	reset_cpu_power();
	parse_dt_cpu_power();
}


void __init arch_build_cpu_topology_domain(void)
{
	init_cpu_topology();
}
#endif





#ifdef CONFIG_SCHED_HMP


/*      add by gatieme for 改进的 HMP 调度器   2016-12-01 21:47 */
#ifndef CONFIG_SCHED_HMP_ENHANCEMENT
/*	defined(CONFIG_SCHED_HMP)  && !defined(CONFIG_SCHED_HMP_ENHANCEMENT)		*/

/*
 * Retrieve logical cpu index corresponding to a given MPIDR[23:0]
 *  - mpidr: MPIDR[23:0] to be used for the look-up
 *
 * Returns the cpu logical index or -EINVAL on look-up error
 */
static inline int get_logical_index(u32 mpidr)
{
	int cpu;
	for (cpu = 0; cpu < nr_cpu_ids; cpu++)
		if (cpu_logical_map(cpu) == mpidr)
			return cpu;
	return -EINVAL;
}

static const char * const little_cores[] = {
	"arm,cortex-a53",
	NULL,
};

static bool is_little_cpu(struct device_node *cn)
{
	const char * const *lc;
	for (lc = little_cores; *lc; lc++)
		if (of_device_is_compatible(cn, *lc))
			return true;
	return false;
}

void __init arch_get_fast_and_slow_cpus(struct cpumask *fast,
					struct cpumask *slow)
{
	struct device_node *cn = NULL;
	int cpu;

	cpumask_clear(fast);
	cpumask_clear(slow);

	/*
	 * Use the config options if they are given. This helps testing
	 * HMP scheduling on systems without a big.LITTLE architecture.
	 */
	if (strlen(CONFIG_HMP_FAST_CPU_MASK) && strlen(CONFIG_HMP_SLOW_CPU_MASK)) {
		if (cpulist_parse(CONFIG_HMP_FAST_CPU_MASK, fast))
			WARN(1, "Failed to parse HMP fast cpu mask!\n");
		if (cpulist_parse(CONFIG_HMP_SLOW_CPU_MASK, slow))
			WARN(1, "Failed to parse HMP slow cpu mask!\n");
		return;
	}

	/*
	 * Else, parse device tree for little cores.
	 */
	while ((cn = of_find_node_by_type(cn, "cpu"))) {

		const u32 *mpidr;
		int len;

		mpidr = of_get_property(cn, "reg", &len);
		if (!mpidr || len != 8) {
			pr_err("%s missing reg property\n", cn->full_name);
			continue;
		}

		cpu = get_logical_index(be32_to_cpup(mpidr+1));
		if (cpu == -EINVAL) {
			pr_err("couldn't get logical index for mpidr %x\n",
							be32_to_cpup(mpidr+1));
			break;
		}

		if (is_little_cpu(cn))
			cpumask_set_cpu(cpu, slow);
		else
			cpumask_set_cpu(cpu, fast);
	}

	if (!cpumask_empty(fast) && !cpumask_empty(slow))
		return;

	/*
	 * We didn't find both big and little cores so let's call all cores
	 * fast as this will keep the system running, with all cores being
	 * treated equal.
	 */
	cpumask_setall(fast);
	cpumask_clear(slow);
}

struct cpumask hmp_slow_cpu_mask;


/*
 *  HMP负载均衡调度器实现了自己的CPU拓扑结构,
 *  该函数用于初始化HMP的cpu拓扑结构
 *
 *  调用关系
 *  init_sched_fair_class( )
 *      ->hmp_cpu_mask_setup( )
 *          ->arch_get_hmp_domains( )
 *  */
void __init arch_get_hmp_domains(struct list_head *hmp_domains_list)
{
	struct cpumask hmp_fast_cpu_mask;
	struct hmp_domain *domain;

    /*  获取大小核的 CPU MASK (hmp_hast_cpu_mask, hmp_slow_cpu_mask)    */
	arch_get_fast_and_slow_cpus(&hmp_fast_cpu_mask, &hmp_slow_cpu_mask);

	/*
	 * Initialize hmp_domains
	 * Must be ordered with respect to compute capacity.
	 * Fastest domain at head of list.
	 */
	if(!cpumask_empty(&hmp_slow_cpu_mask)) {
		domain = (struct hmp_domain *)
			kmalloc(sizeof(struct hmp_domain), GFP_KERNEL);
		cpumask_copy(&domain->possible_cpus, &hmp_slow_cpu_mask);
		cpumask_and(&domain->cpus, cpu_online_mask, &domain->possible_cpus);
		list_add(&domain->hmp_domains, hmp_domains_list);
	}
	domain = (struct hmp_domain *)
		kmalloc(sizeof(struct hmp_domain), GFP_KERNEL);
	cpumask_copy(&domain->possible_cpus, &hmp_fast_cpu_mask);
	cpumask_and(&domain->cpus, cpu_online_mask, &domain->possible_cpus);
	list_add(&domain->hmp_domains, hmp_domains_list);
}

#else /*	ifdef  CONFIG_SCHED_HMP_ENHANCEMENT	*/

void __init arch_get_fast_and_slow_cpus(struct cpumask *fast, struct cpumask *slow)
{
	unsigned int cpu;

	cpumask_clear(fast);
	cpumask_clear(slow);

	/*
	 * Use the config options if they are given. This helps testing
	 * HMP scheduling on systems without a big.LITTLE architecture.
	 */
	if (strlen(CONFIG_HMP_FAST_CPU_MASK) && strlen(CONFIG_HMP_SLOW_CPU_MASK)) {
		if (cpulist_parse(CONFIG_HMP_FAST_CPU_MASK, fast))
			WARN(1, "Failed to parse HMP fast cpu mask!\n");
		if (cpulist_parse(CONFIG_HMP_SLOW_CPU_MASK, slow))
			WARN(1, "Failed to parse HMP slow cpu mask!\n");
		return;
	}

	/* check by capacity */
	for_each_possible_cpu(cpu) {
		if (__cpu_capacity[cpu].capacity > min_capacity)
			cpumask_set_cpu(cpu, fast);
		else
			cpumask_set_cpu(cpu, slow);
	}

	if (!cpumask_empty(fast) && !cpumask_empty(slow))
		return;

	/*
	 * We didn't find both big and little cores so let's call all cores
	 * fast as this will keep the system running, with all cores being
	 * treated equal.
	 */
	cpumask_setall(slow);
	cpumask_clear(fast);
}

struct cpumask hmp_fast_cpu_mask;
struct cpumask hmp_slow_cpu_mask;

void __init arch_get_hmp_domains(struct list_head *hmp_domains_list)
{
	struct hmp_domain *domain;

	arch_get_fast_and_slow_cpus(&hmp_fast_cpu_mask, &hmp_slow_cpu_mask);

	/*
	 * Initialize hmp_domains
	 * Must be ordered with respect to compute capacity.
	 * Fastest domain at head of list.
	 */
	if(!cpumask_empty(&hmp_slow_cpu_mask)) {
		domain = (struct hmp_domain *)
			kmalloc(sizeof(struct hmp_domain), GFP_KERNEL);
		cpumask_copy(&domain->possible_cpus, &hmp_slow_cpu_mask);
		cpumask_and(&domain->cpus, cpu_online_mask, &domain->possible_cpus);
		list_add(&domain->hmp_domains, hmp_domains_list);
	}
	domain = (struct hmp_domain *)
		kmalloc(sizeof(struct hmp_domain), GFP_KERNEL);
	cpumask_copy(&domain->possible_cpus, &hmp_fast_cpu_mask);
	cpumask_and(&domain->cpus, cpu_online_mask, &domain->possible_cpus);
	list_add(&domain->hmp_domains, hmp_domains_list);
}




#ifdef CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY
#include <linux/cpufreq.h>
#define ARCH_SCALE_INVA_CPU_CAP_PERCLS 1

struct cpufreq_extents {
	u32 max;
	u32 flags;
	u32 const_max;
	u32 throttling;
};
/* Flag set when the governor in use only allows one frequency.
 * Disables scaling.
 */
#define CPUPOWER_FREQINVAR_SINGLEFREQ 0x01
static struct cpufreq_extents freq_scale[CONFIG_NR_CPUS];

static unsigned long get_max_cpu_power(void)
{
	unsigned long max_cpu_power = 0;
	int cpu;
	for_each_online_cpu(cpu){
		if( per_cpu(cpu_scale, cpu) > max_cpu_power)
			max_cpu_power = per_cpu(cpu_scale, cpu);
	}
	return max_cpu_power;
}

int arch_get_cpu_throttling(int cpu)
{
	return freq_scale[cpu].throttling;
}

/* Called when the CPU Frequency is changed.
 * Once for each CPU.
 */
static int cpufreq_callback(struct notifier_block *nb,
							unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	int cpu = freq->cpu;
	struct cpufreq_extents *extents;
	unsigned int curr_freq;
#ifdef ARCH_SCALE_INVA_CPU_CAP_PERCLS
	int i = 0;
#endif

	if (freq->flags & CPUFREQ_CONST_LOOPS)
		return NOTIFY_OK;

	if (val != CPUFREQ_POSTCHANGE)
		return NOTIFY_OK;

	/* if dynamic load scale is disabled, set the load scale to 1.0 */
	if (!frequency_invariant_power_enabled) {
		per_cpu(invariant_cpu_capacity, cpu) = per_cpu(base_cpu_capacity, cpu);
		return NOTIFY_OK;
	}

	extents = &freq_scale[cpu];
	if (extents->max < extents->const_max) {
		extents->throttling = 1;
	} else {
		extents->throttling = 0;
	}
	/* If our governor was recognised as a single-freq governor,
	 * use curr = max to be sure multiplier is 1.0
	 */
	if (extents->flags & CPUPOWER_FREQINVAR_SINGLEFREQ)
		curr_freq = extents->max;
	else
		curr_freq = freq->new >> CPUPOWER_FREQSCALE_SHIFT;

#ifdef ARCH_SCALE_INVA_CPU_CAP_PERCLS
	for_each_cpu(i, topology_core_cpumask(cpu)) {
			per_cpu(invariant_cpu_capacity, i) = DIV_ROUND_UP(
				(curr_freq * per_cpu(prescaled_cpu_capacity, i)), CPUPOWER_FREQSCALE_DEFAULT);
	}
#else
	per_cpu(invariant_cpu_capacity, cpu) = DIV_ROUND_UP(
		(curr_freq * per_cpu(prescaled_cpu_capacity, cpu)), CPUPOWER_FREQSCALE_DEFAULT);
#endif

	mt_sched_printf(sched_lb_info, "[%s] CPU%d: pre/inv(%lu/%lu) flags:0x%x freq: cur/new/max/const_max(%u/%u/%u/%u)", __func__,
		cpu, per_cpu(prescaled_cpu_capacity, cpu), per_cpu(invariant_cpu_capacity, cpu),
		extents->flags, curr_freq, freq->new, extents->max, extents->const_max);

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
	int cpu, singleFreq = 0, cpu_capacity;
	static const char performance_governor[] = "performance";
	static const char powersave_governor[] = "powersave";
	unsigned long max_cpu_power;
#ifdef ARCH_SCALE_INVA_CPU_CAP_PERCLS
	int i = 0;
#endif

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

	max_cpu_power = get_max_cpu_power();
	/* Make sure that all CPUs impacted by this policy are
	 * updated since we will only get a notification when the
	 * user explicitly changes the policy on a CPU.
	 */
	for_each_cpu(cpu, policy->cpus) {
		/* scale cpu_power to max(1024) */
		cpu_capacity = (per_cpu(cpu_scale, cpu) << CPUPOWER_FREQSCALE_SHIFT)
			/ max_cpu_power;
		extents = &freq_scale[cpu];
		extents->max = policy->max >> CPUPOWER_FREQSCALE_SHIFT;
		extents->const_max = policy->cpuinfo.max_freq >> CPUPOWER_FREQSCALE_SHIFT;
		if (!frequency_invariant_power_enabled) {
			/* when disabled, invariant_cpu_scale = cpu_scale */
			per_cpu(base_cpu_capacity, cpu) = CPUPOWER_FREQSCALE_DEFAULT;
			per_cpu(invariant_cpu_capacity, cpu) = CPUPOWER_FREQSCALE_DEFAULT;
			/* unused when disabled */
			per_cpu(prescaled_cpu_capacity, cpu) = CPUPOWER_FREQSCALE_DEFAULT;
		} else {
			if (singleFreq)
				extents->flags |= CPUPOWER_FREQINVAR_SINGLEFREQ;
			else
				extents->flags &= ~CPUPOWER_FREQINVAR_SINGLEFREQ;
			per_cpu(base_cpu_capacity, cpu) = cpu_capacity;
#ifdef CONFIG_SCHED_HMP_ENHANCEMENT
			per_cpu(prescaled_cpu_capacity, cpu) =
				((cpu_capacity << CPUPOWER_FREQSCALE_SHIFT) / extents->const_max);
#else
			per_cpu(prescaled_cpu_capacity, cpu) =
				((cpu_capacity << CPUPOWER_FREQSCALE_SHIFT) / extents->max);
#endif

#ifdef ARCH_SCALE_INVA_CPU_CAP_PERCLS
			for_each_cpu(i, topology_core_cpumask(cpu)) {
					per_cpu(invariant_cpu_capacity, i) = DIV_ROUND_UP(
						((policy->cur>>CPUPOWER_FREQSCALE_SHIFT) *
						 per_cpu(prescaled_cpu_capacity, i)), CPUPOWER_FREQSCALE_DEFAULT);
			}
#else
			per_cpu(invariant_cpu_capacity, cpu) = DIV_ROUND_UP(
				((policy->cur>>CPUPOWER_FREQSCALE_SHIFT) *
				 per_cpu(prescaled_cpu_capacity, cpu)), CPUPOWER_FREQSCALE_DEFAULT);
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

static int __init register_topology_cpufreq_notifier(void)
{
	int ret;

	/* init safe defaults since there are no policies at registration */
	for (ret = 0; ret < CONFIG_NR_CPUS; ret++) {
		/* safe defaults */
		freq_scale[ret].max = CPUPOWER_FREQSCALE_DEFAULT;
		per_cpu(base_cpu_capacity, ret) = CPUPOWER_FREQSCALE_DEFAULT;
		per_cpu(invariant_cpu_capacity, ret) = CPUPOWER_FREQSCALE_DEFAULT;
		per_cpu(prescaled_cpu_capacity, ret) = CPUPOWER_FREQSCALE_DEFAULT;
	}

	pr_info("topology: registering cpufreq notifiers for scale-invariant CPU Power\n");
	ret = cpufreq_register_notifier(&cpufreq_policy_notifier,
									CPUFREQ_POLICY_NOTIFIER);

	if (ret != -EINVAL)
		ret = cpufreq_register_notifier(&cpufreq_notifier,
										CPUFREQ_TRANSITION_NOTIFIER);

	return ret;
}

core_initcall(register_topology_cpufreq_notifier);

#else /* !CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY */

int arch_get_cpu_throttling(int cpu)
{
	return 0;
}

#endif /* CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY */


/*
 * Extras of CPU & Cluster functions
 */
/* range: 1 ~  (1 << SCHED_POWER_SHIFT) */
int arch_cpu_cap_ratio(unsigned int cpu)
{
	unsigned long ratio = (cpu_capacity[cpu].capacity << SCHED_POWER_SHIFT) / max_capacity;
	BUG_ON(cpu >= num_possible_cpus());
	return (int)ratio;
}

int arch_is_smp(void)
{
	static int __arch_smp = -1;

	if (__arch_smp != -1)
		return __arch_smp;

	__arch_smp = (max_capacity != min_capacity) ? 0 : 1;

	return __arch_smp;
}

int arch_get_nr_clusters(void)
{
	static int __arch_nr_clusters = -1;
	int max_id = 0;
	unsigned int cpu;

	if (__arch_nr_clusters != -1)
		return __arch_nr_clusters;

	/* assume socket id is monotonic increasing without gap. */
	for_each_possible_cpu(cpu) {
		struct cpu_topology *arm_cputopo = &cpu_topology[cpu];
		if (arm_cputopo->cluster_id > max_id)
			max_id = arm_cputopo->cluster_id;
	}
	__arch_nr_clusters = max_id + 1;
	return __arch_nr_clusters;
}

int arch_is_multi_cluster(void)
{
	return arch_get_nr_clusters() > 1 ? 1 : 0;
}

int arch_get_cluster_id(unsigned int cpu)
{
	struct cpu_topology *arm_cputopo = &cpu_topology[cpu];
	return arm_cputopo->cluster_id < 0 ? 0 : arm_cputopo->cluster_id;
}

void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	unsigned int cpu;

	cpumask_clear(cpus);
	for_each_possible_cpu(cpu) {
		struct cputopo_arm *arm_cputopo = &cpu_topology[cpu];
		if (arm_cputopo->cluster_id == cluster_id)
			cpumask_set_cpu(cpu, cpus);
	}
}

void arch_get_big_little_cpus(struct cpumask *big, struct cpumask *little)
{
	unsigned int cpu;
	cpumask_clear(big);
	cpumask_clear(little);
	for_each_possible_cpu(cpu) {
		if (__cpu_capacity[cpu].capacity > min_capacity)
			cpumask_set_cpu(cpu, big);
		else
			cpumask_set_cpu(cpu, little);
	}
}

int arch_better_capacity(unsigned int cpu)
{
	BUG_ON(cpu >= num_possible_cpus());
	return __cpu_capacity[cpu].capacity > min_capacity;
}



#endif /* CONFIG_SCHED_HMP */


