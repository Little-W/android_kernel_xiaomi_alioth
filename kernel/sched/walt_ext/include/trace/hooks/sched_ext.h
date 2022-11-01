/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM sched_ext
#define TRACE_INCLUDE_PATH ../../../kernel/sched/walt_ext/include/trace/hooks
#if !defined(_TRACE_HOOK_SCHED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_SCHED_H
#include <linux/tracepoint.h>
#include "vendor_hooks.h"

/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */
#if defined(CONFIG_TRACEPOINTS)
struct task_struct;
DECLARE_RESTRICTED_HOOK(android_rvh_select_task_rq_fair,
	TP_PROTO(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flags, int *new_cpu),
	TP_ARGS(p, prev_cpu, sd_flag, wake_flags, new_cpu), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_select_task_rq_rt,
	TP_PROTO(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flags, int *new_cpu),
	TP_ARGS(p, prev_cpu, sd_flag, wake_flags, new_cpu), 1);


DECLARE_RESTRICTED_HOOK(android_rvh_select_fallback_rq,
	TP_PROTO(int cpu, struct task_struct *p, int *new_cpu),
	TP_ARGS(cpu, p, new_cpu), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_find_lowest_rq,
	TP_PROTO(struct task_struct *p, struct cpumask *local_cpu_mask,
			int ret, int *lowest_cpu),
	TP_ARGS(p, local_cpu_mask, ret, lowest_cpu), 1);

struct cgroup_subsys_state;
DECLARE_RESTRICTED_HOOK(android_rvh_cpu_cgroup_online,
	TP_PROTO(struct cgroup_subsys_state *css),
	TP_ARGS(css), 1);

struct cgroup_taskset;
DECLARE_RESTRICTED_HOOK(android_rvh_cpu_cgroup_attach,
	TP_PROTO(struct cgroup_taskset *tset),
	TP_ARGS(tset), 1);


DECLARE_RESTRICTED_HOOK(android_rvh_uclamp_eff_get,
	TP_PROTO(struct task_struct *p, enum uclamp_id clamp_id,
		 struct uclamp_se *uclamp_max, struct uclamp_se *uclamp_eff, int *ret),
	TP_ARGS(p, clamp_id, uclamp_max, uclamp_eff, ret), 1);

DECLARE_HOOK(android_vh_setscheduler_uclamp,
	TP_PROTO(struct task_struct *tsk, int clamp_id, unsigned int value),
	TP_ARGS(tsk, clamp_id, value));

DECLARE_RESTRICTED_HOOK(android_rvh_cpu_cgroup_can_attach,
	TP_PROTO(struct cgroup_taskset *tset, int *retval),
	TP_ARGS(tset, retval), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_sched_balance_rt,
	TP_PROTO(struct rq *rq, struct task_struct *p, int *done),
	TP_ARGS(rq, p, done), 1);


EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_sched_balance_rt);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_cpu_cgroup_online);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_cpu_cgroup_attach);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_find_lowest_rq);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_select_fallback_rq);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_select_task_rq_rt);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_uclamp_eff_get);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_setscheduler_uclamp);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_cpu_cgroup_can_attach);

#endif
#endif /* _TRACE_HOOK_SCHED_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

