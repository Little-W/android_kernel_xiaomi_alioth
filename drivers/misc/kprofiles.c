/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021-2022 Dakkshesh <dakkshesh5@gmail.com>.
 */

#include <linux/kprofiles.h>
#include <linux/module.h>
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

#define target_cpu_util_freq 3985297
#define GPU_STEP_UPPER_BOUND 9223372036854775000

static unsigned int kp_override_mode;
static bool kp_override = false,is_gpu_high_load = false;
long gpu_load_show;
long cpu_time_show;
long gpu_freq_show;
long cpu_time_up_sub_show;
long cpu_time_down_sub_show;
int up_delay_show;
int last_time_up_show;
int last_time_down_show;
int is_cpu_high_load_show;
int gpu_up_time_delay = 1000;
int gpu_down_time_delay = 60;
bool auto_sultan_pid = 1;
unsigned long gpu_busy_perc_show;
module_param(auto_kprofiles, bool, 0664);
module_param(auto_sultan_pid, bool, 0664);
module_param(kp_mode, int, 0664);
module_param(gpu_load_show, long, 0664);
module_param(gpu_up_time_delay, int, 0664);
module_param(gpu_down_time_delay, int, 0664);
module_param(gpu_freq_show, long, 0664);
module_param(is_gpu_high_load, bool, 0664);
module_param(gpu_busy_perc_show, ulong, 0664);

DEFINE_MUTEX(kplock);

extern bool lyb_sultan_pid;
extern bool lyb_sultan_pid_shrink;

struct last_time_store{
	u64 lp;
	u64 hp;
	u64 pr;
};
struct last_step_store{
	int lp;
	int hp;
	int pr;
};
struct boost_request{
	bool lp;
	bool hp;
	bool pr;
};

struct boost_request is_cpu_high_load;

bool is_cpu_high_load_lp_show,is_cpu_high_load_hp_show,is_cpu_high_load_pr_show;
int last_step_lp_show,last_step_hp_show,last_step_pr_show;

module_param(is_cpu_high_load_lp_show, bool, 0664);
module_param(is_cpu_high_load_hp_show, bool, 0664);
module_param(is_cpu_high_load_pr_show, bool, 0664);
module_param(last_step_lp_show, int, 0664);
module_param(last_step_hp_show, int, 0664);
module_param(last_step_pr_show, int, 0664);
/*
* This function can be used to change profile to any given mode 
* for a specific period of time during any in-kernel event,
* then return to the previously active mode.
*
* usage example: kp_set_mode_rollback(3, 55)
*/
void get_cpu_load(unsigned int *freq, int cpu ,u64 time , int target_up_delay , int target_down_delay ,
				  u32 target_util_freq ,bool *boost_request)
{
	static struct last_time_store last_time_up_st,last_time_down_st;
	static struct last_step_store last_step_st;
	int *last_step;
	u64 *last_time_up,*last_time_down;
	bool *is_cpu_high_load_cur;
	is_cpu_high_load_lp_show = is_cpu_high_load.lp,is_cpu_high_load_hp_show = is_cpu_high_load.hp,is_cpu_high_load_pr_show= is_cpu_high_load.pr;
	last_step_lp_show = last_step_st.lp;
	last_step_hp_show = last_step_st.hp;
	last_step_pr_show = last_step_st.pr;
	if (cpumask_test_cpu(cpu, cpu_lp_mask))
	{
		last_time_up = &last_time_up_st.lp;
		last_time_down = &last_time_down_st.lp;
		is_cpu_high_load_cur = &is_cpu_high_load.lp;
		last_step = &last_step_st.lp;
	}
	else if (cpumask_test_cpu(cpu, cpu_perf_mask))
	{
		last_time_up = &last_time_up_st.hp;
		last_time_down = &last_time_down_st.hp;
		is_cpu_high_load_cur = &is_cpu_high_load.hp;
		last_step = &last_step_st.hp;
	}
	else if (cpumask_test_cpu(cpu, cpu_prime_mask))
	{
		last_time_up = &last_time_up_st.pr;
		last_time_down = &last_time_down_st.pr;
		is_cpu_high_load_cur = &is_cpu_high_load.pr;
		last_step = &last_step_st.pr;
	}
	
	if(target_up_delay > 0 && target_down_delay > 0 )
	{
		cpu_time_show = time;	
		if(!(*last_time_up))
		{
			if(*freq>=target_util_freq)
			{
				*last_time_up = time;
				*last_time_down = 0;
			}
		}
		if(!(*last_time_down))
		{
			if(*freq < target_util_freq)
			{
				*last_time_up = 0;
				*last_time_down = time;
			}
		}
		if(*last_time_up)
		{	
			last_time_up_show = *last_time_up;
			cpu_time_up_sub_show = (time - *last_time_up)/NSEC_PER_MSEC;
			if(time - *last_time_up > 10 * NSEC_PER_MSEC)
			{ 
				(*last_step)++;
				*last_time_up = 0;
				*last_time_down = 0;
			}
		}
		if(*last_time_down)
		{	
			last_time_down_show=*last_time_down;
			cpu_time_down_sub_show = (time - *last_time_down)/NSEC_PER_MSEC;
			if(time - *last_time_down > 10 * NSEC_PER_MSEC)
			{
				if(*is_cpu_high_load_cur && *last_step > 0)	
					*last_step = 0;
				(*last_step)--;
				*last_time_down = 0;
				*last_time_up = 0;
			}
		}
		if(*last_step > target_up_delay)
		{
			*boost_request = true;
			*is_cpu_high_load_cur = true;
			*last_step = 0;
		}
		else 	if(*last_step < -target_down_delay)
		{
			*boost_request = false;
			*is_cpu_high_load_cur = false;
			*last_step = 0;
		}
	}
}
void get_gpu_load(unsigned long *freq, unsigned long busy_perc)
{
	if(auto_sultan_pid)
	{
	gpu_freq_show = *freq;
	gpu_busy_perc_show = busy_perc;
	static s64 high_load_step = 0;
	static is_high_load_for_long = 0;
	gpu_load_show = high_load_step;
	
	if(*freq >= 600000000 && busy_perc > 60 - 5*(is_cpu_high_load.hp+is_cpu_high_load.pr + 2*is_high_load_for_long))
	{
		if(!is_gpu_high_load)
		{
			if(is_high_load_for_long && high_load_step < gpu_up_time_delay - 50)
				high_load_step = gpu_up_time_delay - 50;
			else if(high_load_step < 0)
				high_load_step = 0;
			
		}
		high_load_step++;
		if(busy_perc > 80 - 5*(is_cpu_high_load.hp + is_cpu_high_load.pr))
			high_load_step+=5;
		if(busy_perc > 90 - 5*(is_cpu_high_load.hp + is_cpu_high_load.pr))
			high_load_step+=20;	
	}
	else if(*freq >= 500000000 && busy_perc > 60 - 5*(is_cpu_high_load.hp+is_cpu_high_load.pr + 2*is_high_load_for_long))
	{
		if(!is_gpu_high_load)
		{
			if(is_high_load_for_long && high_load_step < gpu_up_time_delay - 100)
				high_load_step = gpu_up_time_delay - 100;
			else if(high_load_step < 0)
				high_load_step = 0;
			
		}
		  high_load_step++;
		if(busy_perc > 70 - 5*(is_cpu_high_load.hp+is_cpu_high_load.pr))
			high_load_step+=3;
		if(busy_perc > 80 - 5*(is_cpu_high_load.hp+is_cpu_high_load.pr))
			high_load_step+=5;
		if(busy_perc > 90 - 5*(is_cpu_high_load.hp+is_cpu_high_load.pr))
			high_load_step+=10;	
		if(busy_perc > 95 - 5*(is_cpu_high_load.hp+is_cpu_high_load.pr))
			high_load_step+=100;	
	}
	else if(*freq > 400000000 && busy_perc > 60 - 5*(is_cpu_high_load.hp+is_cpu_high_load.pr + 2*is_high_load_for_long))
	{
		high_load_step++;
		if(busy_perc > 80 - 5*(is_cpu_high_load.hp+is_cpu_high_load.pr))
			high_load_step+=5;
		if(busy_perc > 90 - 5*(is_cpu_high_load.hp+is_cpu_high_load.pr))
			high_load_step+=10;	
	}
	else if(*freq <= 400000000  && busy_perc < 40)
	{	
		if(is_gpu_high_load && high_load_step > 0)
			high_load_step = -gpu_down_time_delay + 20;
		high_load_step-=2;
		if(busy_perc < 10)
			high_load_step-=500;
	}
	else if(*freq < 400000000 && busy_perc < 40)
	{
		if(is_gpu_high_load && high_load_step > 0)
			high_load_step = 0;
		high_load_step--;
		if(busy_perc < 25)
			high_load_step-=10;
		if(busy_perc < 15)
			high_load_step-=100;
	}	
	else
	{
		high_load_step--;
	}

	if(high_load_step > gpu_up_time_delay)	
	{	
		is_gpu_high_load=true;
	}
	else if(high_load_step < -gpu_down_time_delay)
	{
		is_gpu_high_load = false;
		//high_load_step = 0;
	}
	if(high_load_step > 4000)
		is_high_load_for_long = true;
	else if(high_load_step < -5000)
		is_high_load_for_long = false;
	if(high_load_step > GPU_STEP_UPPER_BOUND )
		high_load_step = GPU_STEP_UPPER_BOUND;
	else if(high_load_step < -GPU_STEP_UPPER_BOUND)   
		high_load_step = -GPU_STEP_UPPER_BOUND;
	}	
}
static bool adaptive_boost(void)
{
	bool boost;
	if(is_cpu_high_load.hp || is_cpu_high_load.pr )
	{
		if(is_cpu_high_load.hp && is_cpu_high_load.pr )
			boost=true;
	}
	else
	{
		boost=false;
	}
	if(is_gpu_high_load)
	{
		lyb_sultan_pid=true;
		lyb_sultan_pid_shrink=true;
	}
	else
	{
		lyb_sultan_pid=false;
		lyb_sultan_pid_shrink=false;
	}
	return boost;

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
int kp_active_mode(void)
{
#ifdef CONFIG_AUTO_KPROFILES
	if (!screen_on && auto_kprofiles)
	{
		if(lyb_sultan_pid)
		{
			lyb_sultan_pid=false;
			lyb_sultan_pid_shrink=false;
		}
		return 1;
	}
	if(auto_kprofiles)
		if (adaptive_boost() || lyb_sultan_pid) 
			return 3;
#endif

	if (kp_override)
		return kp_override_mode;

	if (kp_mode > 3) {
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
