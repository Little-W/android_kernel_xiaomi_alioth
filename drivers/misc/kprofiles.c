/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021-2022 Dakkshesh <dakkshesh5@gmail.com>.
 */

#include <linux/kprofiles.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/moduleparam.h>
#ifdef CONFIG_AUTO_KPROFILES_MSM_DRM
#include <linux/msm_drm_notify.h>
#elif defined(CONFIG_AUTO_KPROFILES_MI_DRM)
#include <drm/drm_notifier_mi.h>
#elif defined(CONFIG_AUTO_KPROFILES_FB)
#include <linux/fb.h>
#endif
#include "version.h"

#ifdef CONFIG_AUTO_KPROFILES_MSM_DRM
#define KP_EVENT_BLANK MSM_DRM_EVENT_BLANK
#define KP_BLANK_POWERDOWN MSM_DRM_BLANK_POWERDOWN
#define KP_BLANK_UNBLANK MSM_DRM_BLANK_UNBLANK
#elif defined(CONFIG_AUTO_KPROFILES_MI_DRM)
#define KP_EVENT_BLANK MI_DRM_EVENT_BLANK
#define KP_BLANK_POWERDOWN MI_DRM_BLANK_POWERDOWN
#define KP_BLANK_UNBLANK MI_DRM_BLANK_UNBLANK
#elif defined(CONFIG_AUTO_KPROFILES_FB)
#define KP_EVENT_BLANK FB_EVENT_BLANK
#define KP_BLANK_POWERDOWN FB_BLANK_POWERDOWN
#define KP_BLANK_UNBLANK FB_BLANK_UNBLANK
#endif

#define GPU_INSTANT_HIGH_LOAD_STEP_UP_LIMIT 10
#define GPU_INSTANT_HIGH_LOAD_STEP_DOWN_LIMIT 30
#define GPU_LONGTIME_HIGH_LOAD_STEP_UP_LIMIT 2000
#define GPU_LONGTIME_HIGH_LOAD_STEP_DOWN_LIMIT 100
#define GPU_LOW_LOAD_TIME_LIMIT_1 1000
#define GPU_LOW_LOAD_TIME_LIMIT_2 1000
#define CPU_HIGH_LOAD_TIME_LIMIT_MS 1000
#define CPU_HIGH_LOAD_TARGET_LOAD_HP 85
#define CPU_HIGH_LOAD_TARGET_LOAD_PR 80
#define CPU_HIGH_LOAD_TARGET_FREQ_HP 2246400
#define CPU_HIGH_LOAD_TARGET_FREQ_PR 2265600
// #define CPU_LOW_LOAD_LIMIT 1000
const int GPU_LONGTIME_HIGH_LOAD_STEP_UP_BOUNDARY = 2 * GPU_LONGTIME_HIGH_LOAD_STEP_UP_LIMIT;
const int GPU_LONGTIME_HIGH_LOAD_STEP_DOWN_BOUNDARY = 2 * GPU_LONGTIME_HIGH_LOAD_STEP_DOWN_LIMIT;
const int GPU_INSTANT_HIGH_LOAD_STEP_UP_BOUNDARY = 2 * GPU_INSTANT_HIGH_LOAD_STEP_UP_LIMIT;
const int GPU_INSTANT_HIGH_LOAD_STEP_DOWN_BOUNDARY = 2 * GPU_INSTANT_HIGH_LOAD_STEP_DOWN_LIMIT;

static unsigned int kp_override_mode;
static bool kp_override = false;

static bool auto_sultan_pid = 1;

module_param(auto_kprofiles, bool, 0664);
module_param(auto_sultan_pid, bool, 0664);
module_param(kp_mode, int, 0664);


DEFINE_MUTEX(kplock);

extern bool lyb_sultan_pid;
extern bool lyb_sultan_pid_shrink;

struct cpu_high_load_long{
	bool hp;
	bool pr;
};

struct cpu_load_info_store{
	bool high_load_hp;
	bool high_load_pr;
	u64 high_load_start_time_hp;
	u64 high_load_start_time_pr;
};

struct gpu_load_info_store{
	s16 instant_high_load_step;
	s16 longtime_high_load_step;
	bool instant_high_load;
	bool longtime_high_load;
};

static struct gpu_load_info_store gpu_load_info = {0};
static struct cpu_load_info_store cpu_load_info = {0};
static struct cpu_high_load_long cpu_high_load_long = {0};


bool instant_high_load_show;
bool longtime_high_load_show;
bool high_load_hp_show;
bool high_load_pr_show;
int longtime_high_load_step_show;
int instant_high_load_step_show;
ulong instant_high_load_time_store_show;
ulong longtime_high_load_time_store_show;
ulong instant_high_load_time_gap_show;
module_param(instant_high_load_show, bool, 0664);
module_param(longtime_high_load_show, bool, 0664);
module_param(longtime_high_load_step_show, int, 0664);
module_param(instant_high_load_step_show, int, 0664);
module_param(high_load_hp_show, bool, 0664);
module_param(high_load_pr_show, bool, 0664);

/*module_param(high_load_pr_show, bool, 0664);
* This function can be used to change profile to any given mode 
* for a specific period of time during any in-kernel event,
* then return to the previously active mode.
*
* usage example: kp_set_mode_rollback(3, 55)
*/
void kp_get_cpu_load(unsigned int freq, u16 load ,int cpu ,u64 time)
{
	high_load_hp_show = cpu_load_info.high_load_hp;
	high_load_pr_show = cpu_load_info.high_load_pr;
	if (cpumask_test_cpu(cpu, cpu_perf_mask))
	{
		if(!cpu_load_info.high_load_hp)
		{
			if(load >= CPU_HIGH_LOAD_TARGET_LOAD_HP && freq >= CPU_HIGH_LOAD_TARGET_FREQ_HP)
			{
				cpu_load_info.high_load_hp = true;
				cpu_load_info.high_load_start_time_hp = time;
			}
		}
		else
		{
			if(load >= CPU_HIGH_LOAD_TARGET_LOAD_HP && freq >= CPU_HIGH_LOAD_TARGET_FREQ_HP)
			{
				if(time - cpu_load_info.high_load_start_time_hp > CPU_HIGH_LOAD_TIME_LIMIT_MS * NSEC_PER_MSEC)
				{
					cpu_high_load_long.hp = true;
				}
			}
			else
			{
				cpu_load_info.high_load_hp = false;
				cpu_high_load_long.hp= false;
			}
		}
	}
	else if (cpumask_test_cpu(cpu, cpu_prime_mask))
	{
		if(!cpu_load_info.high_load_pr)
		{
			if(load >= CPU_HIGH_LOAD_TARGET_LOAD_PR && freq >= CPU_HIGH_LOAD_TARGET_FREQ_PR)
			{
				cpu_load_info.high_load_pr = true;
				cpu_load_info.high_load_start_time_pr = time;
			}
		}
		else
		{
			if(load >= CPU_HIGH_LOAD_TARGET_LOAD_PR && freq >= CPU_HIGH_LOAD_TARGET_FREQ_PR)
			{
				if(time - cpu_load_info.high_load_start_time_pr > CPU_HIGH_LOAD_TIME_LIMIT_MS * NSEC_PER_MSEC)
				{
					cpu_high_load_long.pr = true;
				}
			}
			else
			{
				cpu_load_info.high_load_pr = false;
				cpu_high_load_long.pr = false;
			}
		}
	}
}

void kp_get_gpu_load(unsigned long *freq, u8 busy_perc)
{
	int target_load_balance = 5*cpu_high_load_long.hp + 10*cpu_high_load_long.pr;
	instant_high_load_show = gpu_load_info.instant_high_load;
	longtime_high_load_show = gpu_load_info.longtime_high_load;
	longtime_high_load_step_show = gpu_load_info.longtime_high_load_step;
	instant_high_load_step_show = gpu_load_info.instant_high_load_step;

	if (*freq >= 600000000) {
		if(gpu_load_info.instant_high_load_step < 0)
		{
			gpu_load_info.instant_high_load_step = 0;
		}
		gpu_load_info.instant_high_load_step += 1;
	}
	else 
	{
		gpu_load_info.instant_high_load_step -= 1;
	}

	if(gpu_load_info.instant_high_load_step > GPU_INSTANT_HIGH_LOAD_STEP_UP_BOUNDARY)
		gpu_load_info.instant_high_load_step = GPU_INSTANT_HIGH_LOAD_STEP_UP_BOUNDARY;
	else if(gpu_load_info.instant_high_load_step < -GPU_INSTANT_HIGH_LOAD_STEP_DOWN_BOUNDARY)
		gpu_load_info.instant_high_load_step = -GPU_INSTANT_HIGH_LOAD_STEP_DOWN_BOUNDARY;
	if(gpu_load_info.instant_high_load_step > GPU_INSTANT_HIGH_LOAD_STEP_UP_LIMIT)
	{
		gpu_load_info.instant_high_load = true;
	}
	else if(gpu_load_info.instant_high_load_step < -GPU_INSTANT_HIGH_LOAD_STEP_DOWN_LIMIT)
	{
		gpu_load_info.instant_high_load = false;
	}
	

	if(*freq >= 600000000 && busy_perc > 70 - target_load_balance)
	{
		if(*freq >= 600000000 && busy_perc > 80 - target_load_balance)
			gpu_load_info.longtime_high_load_step += 5;
		else
			gpu_load_info.longtime_high_load_step += 4;
	}
	else if(*freq >= 550000000 && busy_perc > 70 - target_load_balance)
	{
		gpu_load_info.longtime_high_load_step += 2;
	}
	else if(*freq >= 500000000 && busy_perc > 70 - target_load_balance)
	{
		gpu_load_info.longtime_high_load_step += 1;
	}
	else if(*freq < 500000000 && busy_perc >= 40 + target_load_balance)
	{
		gpu_load_info.longtime_high_load_step -= 2;
	}
	else if(*freq < 450000000 && busy_perc < 40 + target_load_balance)
	{
		gpu_load_info.longtime_high_load_step -= 5;
	}
	else if(*freq < 400000000)
	{	
		if(gpu_load_info.longtime_high_load_step > 0)
		{
			gpu_load_info.longtime_high_load_step = 0;
		}
		gpu_load_info.longtime_high_load_step -=
			GPU_LONGTIME_HIGH_LOAD_STEP_DOWN_LIMIT / (10 + 10 * gpu_load_info.longtime_high_load);
	}

	if(gpu_load_info.longtime_high_load_step > GPU_LONGTIME_HIGH_LOAD_STEP_UP_BOUNDARY)
		gpu_load_info.longtime_high_load_step = GPU_LONGTIME_HIGH_LOAD_STEP_UP_BOUNDARY;
	else if(gpu_load_info.longtime_high_load_step < -GPU_LONGTIME_HIGH_LOAD_STEP_DOWN_BOUNDARY)
		gpu_load_info.longtime_high_load_step = -GPU_LONGTIME_HIGH_LOAD_STEP_DOWN_BOUNDARY;

	if(gpu_load_info.longtime_high_load_step > GPU_LONGTIME_HIGH_LOAD_STEP_UP_LIMIT)
	{
		gpu_load_info.longtime_high_load = true;
	}
	else if(gpu_load_info.longtime_high_load_step < -GPU_LONGTIME_HIGH_LOAD_STEP_DOWN_LIMIT)
	{
		gpu_load_info.longtime_high_load = false;
	}

}


void kp_set_mode_rollback(unsigned int level, unsigned int duration_ms)
{
#ifdef CONFIG_AUTO_KPROFILES
	if (!screen_on)
		return;
#endif

	mutex_lock(&kplock);
	if (level && duration_ms && auto_kprofiles) {
		kp_override_mode = level;
		kp_override = true;
		msleep(duration_ms);
		kp_override = false;
	}
	mutex_unlock(&kplock);
}

EXPORT_SYMBOL(kp_set_mode_rollback);

/*
* This function can be used to change profile to 
* any given mode during any in-kernel event.
*
* usage example: kp_set_mode(3)
*/
void kp_set_mode(unsigned int level)
{
#ifdef CONFIG_AUTO_KPROFILES
	if (!screen_on)
		return;
#endif

	if (level && auto_kprofiles)
		kp_mode = level;
}

EXPORT_SYMBOL(kp_set_mode);


/*
* This function returns a number from 0 and 3 depending on the profile 
* selected. The former can be used in conditions to disable/enable 
* or tune kernel features according to a profile mode.
*
* usage exmaple:
*
* if (kp_active_mode() == 3) {
*	  things to be done when performance profile is active
* } else if (kp_active_mode() == 2) {
*	  things to be done when balanced profile is active
* } else if (kp_active_mode() == 1) {
*	  things to be done when battery profile is active
* } else {
*	  things to be done when kprofiles is disabled
* }
*
*/

/*
* kp_active_mode() == 4 : gaming
* kp_active_mode() == 3 : performance
* kp_active_mode() == 2 : balance
* kp_active_mode() == 1 : powersave
*/
int kp_active_mode(void)
{
#ifdef CONFIG_AUTO_KPROFILES
	if(auto_kprofiles)
	{
		if (!screen_on)
		{
			if(auto_sultan_pid)
			{
				if(lyb_sultan_pid)
				{
					lyb_sultan_pid=false;
					lyb_sultan_pid_shrink=false;
				}
			}
			kp_mode = 1;
		}
		else 
		{
			int cpu_boost = 0;

			if(cpu_high_load_long.hp || cpu_high_load_long.pr )
			{
				cpu_boost = 1;
			}
			if(auto_sultan_pid)
			{
				if(gpu_load_info.longtime_high_load)
				{

					lyb_sultan_pid=true;
					lyb_sultan_pid_shrink=true;
				}
				else
				{
					lyb_sultan_pid=false;
					lyb_sultan_pid_shrink=false;
				}
			}
			if(gpu_load_info.longtime_high_load && cpu_boost == 1)
			{
				kp_mode = 4;
			}
			else if(gpu_load_info.instant_high_load)
			{
				kp_mode = 3;
			}
			else
			{
				kp_mode = 2;
			}
		}
			
	}
#endif

	if (kp_override)
		return kp_override_mode;

	if (kp_mode > 4) {
		kp_mode = 0;
		pr_info("Invalid value passed, falling back to level 0\n");
	}

	return kp_mode;
}

EXPORT_SYMBOL(kp_active_mode);

#ifdef CONFIG_AUTO_KPROFILES
static inline int kp_notifier_callback(struct notifier_block *self,
				       unsigned long event, void *data)
{
#ifdef CONFIG_AUTO_KPROFILES_MSM_DRM
	struct msm_drm_notifier *evdata = data;
#elif defined(CONFIG_AUTO_KPROFILES_MI_DRM)
	struct mi_drm_notifier *evdata = data;
#elif defined(CONFIG_AUTO_KPROFILES_FB)
	struct fb_event *evdata = data;
#endif
	int *blank;

	if (event != KP_EVENT_BLANK
#ifdef CONFIG_AUTO_KPROFILES_MSM_DRM
		|| !evdata || !evdata->data
		|| evdata->id != MSM_DRM_PRIMARY_DISPLAY
#endif
	)
		return NOTIFY_OK;

	blank = evdata->data;
	switch (*blank) {
	case KP_BLANK_POWERDOWN:
		if (!screen_on)
			break;
		screen_on = false;
		break;
	case KP_BLANK_UNBLANK:
		if (screen_on)
			break;
		screen_on = true;
		break;
	}

	return NOTIFY_OK;
}


static struct notifier_block kp_notifier_block = {
	.notifier_call = kp_notifier_callback,
};

static int kprofiles_register_notifier(void)
{
	int ret = 0;

#ifdef CONFIG_AUTO_KPROFILES_MSM_DRM
	ret = msm_drm_register_client(&kp_notifier_block);
#elif defined(CONFIG_AUTO_KPROFILES_MI_DRM)
	ret = mi_drm_register_client(&kp_notifier_block);
#elif defined(CONFIG_AUTO_KPROFILES_FB)
	ret = fb_register_client(&kp_notifier_block);
#endif

	return ret;
}


static void kprofiles_unregister_notifier(void)
{
#ifdef CONFIG_AUTO_KPROFILES_MSM_DRM
	msm_drm_unregister_client(&kp_notifier_block);
#elif defined(CONFIG_AUTO_KPROFILES_MI_DRM)
	mi_drm_unregister_client(&kp_notifier_block);
#elif defined(CONFIG_AUTO_KPROFILES_FB)
	fb_unregister_client(&kp_notifier_block);
#endif
}

#else
static inline int kprofiles_register_notifier(void) { return 0; }
static inline void kprofiles_unregister_notifier(void) { }
#endif

static int __init kp_init(void)
{
	int ret = 0;

	ret = kprofiles_register_notifier();
	if (ret)
		pr_err("Failed to register notifier, err: %d\n", ret);

	pr_info("Kprofiles " KPROFILES_VERSION
		" loaded. Visit https://github.com/dakkshesh07/Kprofiles/blob/main/README.md for information.\n");
	pr_info("Copyright (C) 2021-2022 Dakkshesh <dakkshesh5@gmail.com>.\n");

	return ret;
}
module_init(kp_init);

static void __exit kp_exit(void)
{
	kprofiles_unregister_notifier();
}
module_exit(kp_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("KernelSpace Profiles");
MODULE_AUTHOR("Dakkshesh <dakkshesh5@gmail.com>");
MODULE_VERSION(KPROFILES_VERSION);
