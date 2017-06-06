
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include <cutils/misc.h>
#include <cutils/sockets.h>
#include <cutils/multiuser.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/atomics.h>
#include <private/android_filesystem_config.h>

#include <selinux/selinux.h>
#include <selinux/label.h>
#include <sched.h> 

#include "log.h"
#include "property_service.h"

#define AW_BOOST_UP_CPUFREQ_GOVNOR_SYS  "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
#define AW_CPUFREQ_GOVNOR 		"interactive"
#define AW_BOOST_UP_CPUFAUTOHOTPLUG_SYS "/sys/kernel/autohotplug/enable"
#define AW_CPUFAUTOHOTPLUG_ENABLE 	"1"
#define AW_BOOST_UP_CPUS 		"/sys/devices/platform/sunxi-budget-cooling/roomage"
#define AW_BOOST_UP_MODE_NORMAL 	0xA5

#ifdef SUN8IW6P1
#define AW_BOOST_UP_GPU  	    	"/sys/devices/platform/pvrsrvkm/dvfs/android"
#define AW_BOOST_UP_CPUS_PERF       	"1608000 4 1608000 4 2016000 4 2016000 4"
#define AW_BOOST_UP_CPUS_NORMAL     	"0 2 0 0 2016000 4 2016000 4"
#define AW_BOOST_UP_CPUS_MEDIA_CDX2 	"1008000 4 0 0 2016000 4 2016000 4"
#define AW_BOOST_UP_MODE_CDX	    	0xF1
#define AW_BOOST_CPUFREQ_BOOT_LOCK_SYS  "/sys/devices/system/cpu/cpu0/cpufreq/boot_lock"
#define AW_BOOST_CPUFREQ_UNLOCK         "0\n"
#endif

#ifdef SUN8IW11P1
/** now do nothing **/

#endif

#ifdef SUN9IW1P1
#define AW_BOOST_SYSTEM_NONE        	0
#define AW_BOOST_SYSTEM_UP          	1
#define AW_SYSTEM_SYSCTR_MODE 		0xF1
#define AW_BOOST_UP_CPUS_PERF	    	"1200000 1 1608000 4 1200000 4 1800000 4 1"
#define AW_BOOST_UP_CPUS_SYSTEM     	"1200000 2 1608000 1 1200000 4 1800000 4 1"
#define AW_BOOST_UP_CPUS_NORMAL     	"0 0 0 0 1200000 4 1800000 4 0"
#endif

#ifdef SUN8IW7P1
#define AW_BOOST_UP_MODE_KARAOK         0xF1
#define AW_MEDIA_SYSCTR_MODE            0xF2
#define AW_MEDIA_SYSCTRL_PREF_INDEX     3
#define AW_SYSCTRL_THERMAL_TRIP         "sys/class/thermal/thermal_zone0/trip_offset"
#define AW_THERMAL_TRIP_NORMAL          "0"
#define AW_THERMAL_TRIP_ADJUST			"10"
#define AW_SYSCTRL_LMK_FAST_CALL      	"/sys/module/lowmemorykiller/parameters/fast_kill"
#define AW_SYSCTRL_LMK_PERF             "1"
#define AW_SYSCTRL_LMK_PROCESS          "/sys/module/lowmemorykiller/parameters/process_com"
#define AW_BOOST_SYSTEM_NONE            0
#define AW_BOOST_SYSTEM_UP              1
#define AW_BOOST_SYSTEM_SCHED				    2
#define AW_BOOST_CPUFREQ_BOOT_LOCK_SYS  "/sys/devices/system/cpu/cpu0/cpufreq/boot_lock"
#define AW_BOOST_CPUFREQ_UNLOCK         "0\n"
#define AW_CPU_FAST			0
#define AW_CPU_SLOW			1
#define AW_BOOST_UP_CPUS_SLOW_PERF      "1008000 4 0 0 1008000 4 0 0 0"
#define AW_BOOST_UP_CPUS_SLOW_NORMAL    "0 2 0 0 1008000 4 0 0 0"
#define AW_BOOST_UP_CPUS_FAST_PERF      "1200000 4 0 0 1200000 4 0 0 0"
#define AW_BOOST_UP_CPUS_FAST_NORMAL    "0 2 0 0 1200000 4 0 0 0"
#define AW_BOOST_UP_CPUS_KARAOK         "1008000 4 0 0 1008000 4 0 0 0"
static unsigned int bid = AW_CPU_SLOW;
#endif
static bool BOOST_UP_DEBUG = false;
#define B_LOG(fmt,...) if (BOOST_UP_DEBUG) ERROR(fmt,##__VA_ARGS__)

struct boost_list {
	int slot_active;
	int hold_pid;
	int mode_cur;
	int mode_pre;
};

struct boostup_mode {
	int mode_cur;
	int mode_pre;
};

static struct boost_list blist;
static struct boostup_mode bmode ;
static int lowmem_limit = 0;

#define DETECT_INIT  0
#define DETECT_WORK 1
#define USER_ACTIVE 1
#define USER_SLOT	3
#define SYS_SLOT 	1
#define COMM_LEN	128

enum {
	USER_UNKOWN = -1,
	USER_CMDLINE,
	USER_COMM,
	USER_PID,
};

enum {
	RECLAIM_IGNORE = 1,
	RECLAIM_MIN,
	RECLAIM_LOW,
	RECLAIM_HIGH,
	RECLAIM_SERIOUS,
};

struct proc_info {
	char str[2][COMM_LEN];
	int pid;
	int state;	// = 1 rescore used; = 0 NULL or wait for clear
	int item;	// =1 cmdline used; =2 comm used; =3 pid used
};
//static struct proc_info user[USER_SLOT];
//static struct proc_info sys[SYS_SLOT];
static int user_detect = 0;   	 // if user[] be used, use this flag for distingush
static int sys_detect = 0;    	 // if sys[] be used, use this flag for distingush
static int detect_grap   = 3000; //for 3000ms detect rescan
//static int detect_update = 500;  //for 3000ms detect update proc info
//static int high_cpuload  = 0;    //record system kernel high cpu load
//static int ligh_cpuload  = 0;    //record system kernel high cpu load
//static int dram_size     = 0;    //record system total memory size

struct proc_usage {
	char state;
	long unsigned ut, st;
	long unsigned delta_ut, delta_st, delta_time;
};

struct cpus_usage {
	long unsigned ut, nt, st, itime;
	long unsigned iot, irt, sirt;
};
#define MS_SLEEP(time) usleep((unsigned int)time*1000)
/*
static int aw_get_usage(int pid, struct cpus_usage *cpu, struct proc_usage *proc)
{
	FILE *file = NULL;
	char *start_p = NULL;
	int ret = -1;
	char buf[256];
	char filename[64];
	long unsigned temp[10];

	file = fopen("/proc/stat", "r");
	if (!file) {
		ERROR("Could not open /proc/stat.\n");
		goto out;
	}
	fscanf(file, "cpu %lu %lu %lu %lu %lu %lu %lu", &cpu->ut, &cpu->nt,
			&cpu->st, &cpu->itime, &cpu->iot, &cpu->irt, &cpu->sirt);
	fclose(file);

	sprintf(filename, "/proc/%d/stat", pid);
	file = fopen(filename, "r");
	if (!file || !proc) {
		ERROR("Could not open %s.\n", filename);
		goto out;
	}
	fgets(buf, 256, file);
	fclose(file);

	start_p = strrchr(buf, ')');
	if (!start_p)
		goto out;

	//sscanf(start_p + 1, " %c %ld %ld",
	//	&proc->state, &proc->ut, &proc->st);
	//sscanf(start_p + 1, " %c %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d "
	//					"%lu %lu %*d %*d %*d %*d %*d %*d %*d %lu %ld "
	//					"%*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %d",
	//					&proc->state, &proc->ut, &proc->st);

	sscanf(start_p + 1, " %c %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
		&proc->state, &temp[0], &temp[1], &temp[2], &temp[3], &temp[4], &temp[5], &temp[6], &temp[7], &temp[8], &temp[9],
	    &proc->ut, &proc->st);
	//B_LOG("%c %lu %lu \n ", proc->state, proc->ut, proc->st);

	ret = 0;
out:
	return ret;
}
static int aw_get_process_usage(int pid, int grap_ms)
{
	int prev, nextv;
	struct proc_usage np, op;
	struct cpus_usage nc, oc;

	unsigned long toatl_delta_time = 0;
	unsigned long proc_usage = 0;

	memset(&np, 0, sizeof(np));
	memset(&op, 0, sizeof(op));
	memset(&nc, 0, sizeof(nc));
	memset(&oc, 0, sizeof(oc));

	prev =  aw_get_usage(pid, &oc, &op);
	MS_SLEEP(grap_ms);
	nextv = aw_get_usage(pid, &nc, &np);
	if (!prev && !nextv) {
		np.delta_ut = np.ut - op.ut;
		np.delta_st = np.st - op.st;
		np.delta_time = np.delta_ut + np.delta_st;
		toatl_delta_time = (nc.ut + nc.nt + nc.st + nc.itime + nc.iot + nc.irt + nc.sirt) -
			(oc.ut + oc.nt + oc.st + oc.itime + oc.iot + oc.irt + oc.sirt);
		proc_usage = ((np.delta_time <=0) || (toatl_delta_time <=0))? (0):
							(np.delta_time * 100 / toatl_delta_time);
		B_LOG("process:%d usage %lu%%\n", pid, proc_usage);
	}

	return (int)proc_usage;
}
static int aw_get_process_info(struct proc_info *user)
{
    int id;
	int ret = -1;
	FILE *fp;
   // pid_t pid = -1;
    DIR* proc_dir;
    char proc_note[32];
    char proc_str[COMM_LEN];
	char *proc_dest = NULL; //{ "cmdline", "comm" };
	char *proc_src	= NULL;
	char *user_dest = NULL;
	char *user_src = NULL;
	int user_buf_size = 0;
	int user_id_start = 0;

	struct dirent *proc_entry;

    if ((user == NULL) || (user->item == USER_UNKOWN))
        goto fail;

    proc_dir = opendir("/proc");
    if (proc_dir == NULL)
        goto fail;

	if (user->item == USER_CMDLINE) {                //for Android APP
		proc_dest = (char *)("cmdline");
		proc_src  = (char *)("comm");
		user_dest = user->str[USER_CMDLINE];
		user_src  = user->str[USER_COMM];
		user_buf_size = sizeof(user->str[USER_COMM]);
		user_id_start = 300;
	} else {										////for System therad
		proc_dest = (char *)("comm");
		proc_src  = (char *)("cmdline");
		user_dest = user->str[USER_COMM];
		user_src  = user->str[USER_CMDLINE];
		user_buf_size = sizeof(user->str[USER_CMDLINE]);
		user_id_start = 0;
	}

	B_LOG("user_dest[%s] proc_dest[%s],  size %d\n", user_dest, proc_dest, user_buf_size);
    while((proc_entry = readdir(proc_dir)) != NULL) {
        id = atoi(proc_entry->d_name);
        if ((id != 0) && (id > user_id_start)){
            snprintf(proc_note, sizeof(proc_note), "/proc/%d/%s", id, proc_dest);
            fp = fopen(proc_note, "r");
            if (fp) {
                fgets(proc_str, sizeof(proc_str), fp);
				//B_LOG("path:[%s] [%s]\n", proc_note, proc_str);
                fclose(fp);
				fp = NULL;
				if (strcmp(user_dest, proc_str) == 0) {
					user->pid = id;
                    break;
                }
            }
        }
    }
    closedir(proc_dir);

	if ((user->pid > 0)) {
		memset(proc_note, 0 ,sizeof(proc_note));
		snprintf(proc_note, sizeof(proc_note), "/proc/%d/%s", user->pid, proc_src);
		fp = fopen(proc_note, "r");
		if (fp) {
			 fgets(user_src, user_buf_size, fp);
			 fclose(fp);
			 fp = NULL;
		} else {
			B_LOG("open %s fail! \n", proc_note);
			goto fail;
		}
		B_LOG("prcoss-%d cmdline[%s], comm[%s]\n", user->pid, user->str[USER_CMDLINE], user->str[USER_COMM]);
	}
	ret = 0;
fail:
    return ret;
}

*/
static int aw_get_para(const char *value, int *pid, int *index)
{
	//value like: modem1008:2
	char buf[32];
	int mPid = 0;
	int mIndex = 0;

	if (!value || !pid || !index) {
		ERROR("%s invalid para! \n", __func__);
		return -1;
	}

	sscanf(value, "%[a-z] %d:%d", buf, &mPid, &mIndex);
	*pid = mPid;
	*index = mIndex;
	ERROR("%s  magic %s pid %d : index %d \n", __func__, buf, *pid, *index);
	return 0;
}
/*
static int aw_get_str(const char *value, char *str, int *index)
{
	//value like: modem1:xxx
	char buf[32];

	if (!value || !str || !index) {
		ERROR("%s invalid para! \n", __func__);
		return -1;
	}

	sscanf(value, "%[a-z] %d:%s", buf, index, str);
	B_LOG("%s  magic %s index %d str %s \n", __func__, buf, *index, str);
	return 0;
}
*/

int aw_sysctrl_set(const char *path, char *buf)
{
        FILE *sysctrl_fd;
        unsigned int ret = 0;

        sysctrl_fd = fopen(path, "w");
        if (sysctrl_fd == NULL){
                ERROR(" ERR: boost sysctrl open fail,%s!\n",strerror(errno));
                goto err_out;
        }

	ret = fwrite(buf, 1, strlen(buf), sysctrl_fd);
	if (ret != strlen(buf)) {
		ERROR(" ERR: boost sysctrl write fail,%s!\n",strerror(errno));
		fclose(sysctrl_fd);
		goto err_out;
	}

	fflush(sysctrl_fd);
	fclose(sysctrl_fd);
	B_LOG("%s : %s %s ok \n",__func__, path, buf);
        return 0;
err_out:
        return -1;
}

int aw_sysctrl_get(const char *path, char *buf, int slen)
{
	FILE *sysctrl_fd;
	int ret = 0;

	sysctrl_fd = fopen(path, "r");
	if (sysctrl_fd == NULL){
		ERROR(" ERR: boost sysctrl open fail,%s!\n",strerror(errno));
		goto err_out;
	}

	ret = fread(buf, slen, 1, sysctrl_fd);
	if (ret < 0) {
		ERROR(" ERR: boost sysctrl read fail,%s!\n",strerror(errno));
		fclose(sysctrl_fd);
		goto err_out;
	}

	fclose(sysctrl_fd);
	return 0;
err_out:
	return -1;
}


#ifdef SUN8IW6P1
static int aw_boost_normal(void)
{
	aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_NORMAL);
        if (lowmem_limit == 0) {
                dram_size = get_dram_size();
                if (dram_size <= 512) {
                        aw_sysctrl_set("/proc/sys/vm/dirty_ratio","5");
                        aw_sysctrl_set("/proc/sys/vm/dirty_background_ratio","2");
			aw_sysctrl_set("/proc/sys/vm/min_free_kbytes","8192");
                } else {
                        aw_sysctrl_set("/proc/sys/vm/extra_free_kbytes","86400");
			aw_sysctrl_set("/proc/sys/vm/min_free_kbytes","16384");
                }
		aw_sysctrl_set(AW_BOOST_UP_CPUFREQ_GOVNOR_SYS, AW_CPUFREQ_GOVNOR);
		aw_sysctrl_set(AW_BOOST_UP_CPUFAUTOHOTPLUG_SYS, AW_CPUFAUTOHOTPLUG_ENABLE);
                lowmem_limit = 1;
        }
	aw_sysctrl_set(AW_BOOST_CPUFREQ_BOOT_LOCK_SYS, AW_BOOST_CPUFREQ_UNLOCK);

	bmode.mode_cur = AW_BOOST_UP_MODE_NORMAL;
	bmode.mode_pre = AW_BOOST_UP_MODE_NORMAL;
	blist.hold_pid = 0;
	blist.mode_cur = AW_BOOST_UP_MODE_NORMAL;
	blist.mode_pre = AW_BOOST_UP_MODE_NORMAL;
	blist.slot_active = 0;
	return 0;
err_out:
	return -1;
}

static int aw_media_cdx_boost_cpus(int mpid, int mindex)
{
        FILE *boost_up_cpus_fd;
        char *sbuf = AW_BOOST_UP_CPUS_MEDIA_CDX2;
        char *path = AW_BOOST_UP_CPUS;
        unsigned int ret;

        boost_up_cpus_fd = fopen(path, "w+");
        if (boost_up_cpus_fd == NULL) {
                ERROR(" ERR: boostup access sysfs fail,%s!\n",strerror(errno));
                goto err_out;
        } else {
                ret = fwrite(sbuf, 1, strlen(sbuf), boost_up_cpus_fd);
                if (ret != strlen(sbuf))
                {
                        ERROR(" ERR: boostup switch fail,%s!\n",strerror(errno));
                        fclose(boost_up_cpus_fd);
                        goto err_out;
                } else {
                        fflush(boost_up_cpus_fd);
                        fclose(boost_up_cpus_fd);
                }
        }

        bmode.mode_cur = AW_BOOST_UP_MODE_CDX;
        bmode.mode_pre = AW_BOOST_UP_MODE_NORMAL;
        blist.hold_pid = mpid;
        blist.mode_cur = AW_BOOST_UP_MODE_CDX;
        blist.mode_pre = AW_BOOST_UP_MODE_NORMAL;
        blist.slot_active = 1;
        return 0;
err_out:
        return -1;
}

#endif

#ifdef SUN8IW11P1
static int aw_boost_normal(void)
{
	if (lowmem_limit == 0) {
#if 0
		dram_size = get_dram_size();
		if (dram_size <= 512) {
			aw_sysctrl_set("/proc/sys/vm/dirty_ratio","5");
			aw_sysctrl_set("/proc/sys/vm/dirty_background_ratio","2");
			aw_sysctrl_set("/proc/sys/vm/min_free_kbytes","8192");
		} else {
			aw_sysctrl_set("/proc/sys/vm/extra_free_kbytes","86400");
			aw_sysctrl_set("/proc/sys/vm/min_free_kbytes","16384");
		}
#endif
		aw_sysctrl_set(AW_BOOST_UP_CPUFREQ_GOVNOR_SYS,(char*)(AW_CPUFREQ_GOVNOR));
		lowmem_limit = 1;
	}

	bmode.mode_cur = AW_BOOST_UP_MODE_NORMAL;
	bmode.mode_pre = AW_BOOST_UP_MODE_NORMAL;
	blist.hold_pid = 0;
	blist.mode_cur = AW_BOOST_UP_MODE_NORMAL;
	blist.mode_pre = AW_BOOST_UP_MODE_NORMAL;
	blist.slot_active = 0;
	return 0;
}
#endif

#ifdef SUN9IW1P1

static int aw_boost_normal(void)
{
	if (lowmem_limit == 0) {
		aw_sysctrl_set(AW_BOOST_UP_CPUFREQ_GOVNOR_SYS, AW_CPUFREQ_GOVNOR);
		aw_sysctrl_set(AW_BOOST_UP_CPUFAUTOHOTPLUG_SYS, AW_CPUFAUTOHOTPLUG_ENABLE);
		lowmem_limit = 1;
	}
	aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_NORMAL);

	bmode.mode_cur = AW_BOOST_UP_MODE_NORMAL;
	bmode.mode_pre = AW_BOOST_UP_MODE_NORMAL;
	blist.hold_pid = 0;
	blist.mode_cur = AW_BOOST_UP_MODE_NORMAL;
	blist.mode_pre = AW_BOOST_UP_MODE_NORMAL;
	blist.slot_active = 0;

    return 0;
}

static int aw_boost_system(int mpid, int mindex)
{
	int cur_mode = 0;
	int pre_mode = 0;
	int active = 0;
	int detect_pid = 0;

	if (mindex == AW_BOOST_SYSTEM_UP) {
		aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_SYSTEM);
		cur_mode = AW_SYSTEM_SYSCTR_MODE;
		pre_mode = AW_BOOST_UP_MODE_NORMAL;
		detect_pid = mpid;
		active = 1;
	}
	else if (mindex == AW_BOOST_SYSTEM_NONE) {
		aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_NORMAL);
		cur_mode = AW_BOOST_UP_MODE_NORMAL;
		pre_mode = AW_BOOST_UP_MODE_NORMAL;
		detect_pid = 0;
		active = 0;
	}

    bmode.mode_cur = cur_mode;
    bmode.mode_pre = pre_mode;
    blist.hold_pid = detect_pid;
    blist.mode_cur = cur_mode;
    blist.mode_pre = pre_mode;
    blist.slot_active = active;

	return 0;
err_out:
	return -1;
}
#endif


#ifdef SUN8IW7P1

static int aw_system_watch_userspace(int index, char *str)
{
	int ret = -1;

	if ((index >= USER_SLOT) || (str == NULL))
		goto out;

	memset(&user[index], 0 ,sizeof(user[index]));
	user[index].state = USER_ACTIVE;
	user[index].item = USER_CMDLINE;
	strcpy(user[index].str[user[index].item], str);

out:
	return ret;

}

static int aw_system_watch_kernelspace(int index, char *str)
{
	int ret = -1;

	if ((index >= SYS_SLOT) || (str == NULL))
		goto out;

	memset(&sys[index], 0 ,sizeof(sys[index]));
	sys[index].state = USER_ACTIVE;
	sys[index].item  = USER_COMM;
	snprintf(sys[index].str[USER_COMM], sizeof(sys[index].str[USER_COMM]), "%s\n", str);

out:
	return ret;

}

static int aw_system_watch_group_memory(int detect_stage, int index)
{
	int ret = -1;
	int i;
	char proc_str[256];
	char *pproc = proc_str;
	int blen = 0;
	int btemp = 0;

	if (detect_stage == DETECT_INIT)
	{
		for(i=0; (i < USER_SLOT) && (user[i].state == USER_ACTIVE); i++)
		{
			if(!aw_get_process_info(&user[i]))
				user_detect = USER_ACTIVE;
		}

		for(i=0; (i < SYS_SLOT) && (sys[i].state == USER_ACTIVE); i++)
		{
			if(!aw_get_process_info(&sys[i]))
				sys_detect = USER_ACTIVE;
		}
	}
	else if (detect_stage == DETECT_WORK)
	{
		if (user_detect == USER_ACTIVE)
		{
			memset(proc_str, 0, sizeof(proc_str));
			for(i=0; (i < USER_SLOT) && (user[i].state == USER_ACTIVE); i++)
			{
				btemp = snprintf(pproc + blen, sizeof(proc_str), "%s ", user[i].str[USER_COMM]);
				blen += btemp;
			}
			aw_sysctrl_set(AW_SYSCTRL_LMK_PROCESS, pproc);
			user_detect = 0;
		}

		if (sys_detect == USER_ACTIVE)
		{
			int pid = sys[index].pid;
			int usage = aw_get_process_usage(pid, detect_grap);
			if (usage > 20)
			{
				detect_update = 250;  //speed up to 250 ms
				detect_grap   = 800;  //speed up to 1000 ms
				ligh_cpuload  = 0;
				switch(++high_cpuload)
				{
					case RECLAIM_SERIOUS:
						if (dram_size <= 512)
						{
							aw_sysctrl_set("/proc/sys/vm/min_free_kbytes", "4096"); //adjust water_mark
							aw_sysctrl_set("/proc/sys/vm/extra_free_kbytes", "14400");
						}
					case RECLAIM_HIGH:
						aw_sysctrl_set(AW_SYSCTRL_LMK_FAST_CALL,AW_SYSCTRL_LMK_PERF); //kill background lanuch
					case RECLAIM_LOW:
						aw_sysctrl_set("/proc/sys/vm/drop_caches", "2"); //flush fs inote and dentry cache
					case RECLAIM_MIN:
						aw_sysctrl_set("/proc/sys/vm/drop_caches", "1"); //flsuh cpu-cacge
					case RECLAIM_IGNORE:
						break;
					default:
						aw_sysctrl_set("/proc/sys/vm/drop_caches", "1"); //flsuh cpu-cacge
						B_LOG("invalid stage !\n");
						break;
				}
				B_LOG(" memory recliam stage : %d \n", high_cpuload);
			}
			else
			{
				detect_update = 500;
				detect_grap   = 3000;
				ligh_cpuload ++;
				if ((high_cpuload >0) && (ligh_cpuload == 100))  //winthin 300s no high-cpuload
				{
					aw_sysctrl_set(AW_SYSCTRL_LMK_FAST_CALL, "0");
					if (dram_size <= 512)
					{
						aw_sysctrl_set("/proc/sys/vm/min_free_kbytes", "8192"); //adjust water_mark
						aw_sysctrl_set("/proc/sys/vm/extra_free_kbytes", "28800");
					}
					high_cpuload  = 0;
				}
			}

		}
	}

out:
	return ret;
}

static int aw_media_karaok_boost_cpus(int pid, int index)
{
		int cur_mode = 0;
		int pre_mode = 0;
		int active = 0;
		int detect_pid = 0;
		int min_priority;
		struct sched_param param;
        if (index == AW_BOOST_SYSTEM_UP) {
			aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_KARAOK);
			aw_sysctrl_set(AW_SYSCTRL_THERMAL_TRIP, AW_THERMAL_TRIP_ADJUST);
			cur_mode = AW_BOOST_UP_MODE_KARAOK;
			pre_mode = AW_BOOST_UP_MODE_NORMAL;
			detect_pid = pid;
			active = 1;
			ERROR(" AW_BOOST_SYSTEM_UP \n");
        }
        else if (index == AW_BOOST_SYSTEM_NONE) {
                if (bid == AW_CPU_FAST)
                        aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_FAST_NORMAL);
                else
                        aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_SLOW_NORMAL);
				aw_sysctrl_set(AW_SYSCTRL_THERMAL_TRIP, AW_THERMAL_TRIP_NORMAL);
				cur_mode = AW_BOOST_UP_MODE_NORMAL;
				pre_mode = AW_BOOST_UP_MODE_NORMAL;
				detect_pid = 0;
				active = 0;
				ERROR(" AW_BOOST_SYSTEM_NONE \n");
        }
        else if (index == AW_BOOST_SYSTEM_SCHED) {
	        min_priority = sched_get_priority_min(SCHED_FIFO);
	        param.sched_priority = min_priority;
	        if(sched_setscheduler(pid, SCHED_FIFO, &param) == -1)
	        	ERROR("pid %d set priority fail!!\n");
        	ERROR("pid:%d set priority %d\n",pid, min_priority);
        	return 0;
        }
        bmode.mode_cur = cur_mode;
        bmode.mode_pre = pre_mode;
        blist.hold_pid = detect_pid;
        blist.mode_cur = cur_mode;
        blist.mode_pre = pre_mode;
        blist.slot_active = active;
        return 0;
err_out:
        return -1;

}

static int aw_borad_feature_detect(void)
{
	int vendor = 0x00;
	char itend[64];
	char spt[64];
	char buf[100];
	char head[60];
	char *items = "113";
	char *bpt = "/sys/class/gpio/export";
	char *dpt = "/sys/class/gpio/unexport";
	char *ept = "/sys/class/gpio/gpio";
	char *sys_info = "/sys/class/sunxi_info/sys_info";
	int board_flag = 0;
	int cpu_flag = 0;

	memset(itend, 0, sizeof(itend));
	memset(spt, 0, sizeof(spt));
	snprintf(spt, sizeof(spt), "%s%s/value", ept, items);
	aw_sysctrl_set(bpt, items);
	aw_sysctrl_get(spt, itend, sizeof(itend));
	aw_sysctrl_set(dpt, items);
	vendor = (int)strtoul(itend, NULL, 10);

	FILE *file = fopen(sys_info, "r");
	if (!file) {
		ERROR("Could not open %s .\n", sys_info);
		goto fail;
	}
	fseek(file,0,SEEK_SET);
	while(fgets(buf,100,file) != NULL)
	{
		if (!strncmp(buf, "sunxi_board_id", strlen("sunxi_board_id"))) {
			if(sscanf(buf, "%s : %d(%d)", head, &board_flag, &cpu_flag) < 0) {
				ERROR("sunxi board od sscan  erroe!");
				fclose(file);
				goto fail;
			}
		B_LOG("boost sscan %s board flag %d ,cpu flag %d \n", head, board_flag, cpu_flag);
		}
	}
	fclose(file);

	if ((board_flag < 0) && (cpu_flag < 0)) { // all flags get -1 , because system_config.fex vid_used = 0
		vendor = AW_CPU_FAST;
	}
	if ((vendor == cpu_flag) && (cpu_flag >=0)) {
		vendor = (vendor == AW_CPU_FAST)?AW_CPU_FAST:AW_CPU_SLOW;
	}
	return vendor;

fail:
	return -1;
}


static int aw_media_adjust_lmk_params(int mpid, int mindex)
{
	if (mindex == AW_MEDIA_SYSCTRL_PREF_INDEX)
		aw_sysctrl_set(AW_SYSCTRL_LMK_FAST_CALL,AW_SYSCTRL_LMK_PERF);
	else if (mindex == 0)
		aw_sysctrl_set(AW_SYSCTRL_LMK_FAST_CALL,"0");

	return 0;
}

static int aw_boost_system(int mpid, int mindex)
{
	sleep(3);
	if (mindex == AW_BOOST_SYSTEM_UP) {
		if (bid == AW_CPU_FAST)
			aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_FAST_PERF);
		else
			aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_SLOW_PERF);
	}
	else if (mindex == AW_BOOST_SYSTEM_NONE) {
		if (bid == AW_CPU_FAST)
			aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_FAST_NORMAL);
		else
			aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_SLOW_NORMAL);
	}
	return 0;
err_out:
	return -1;
}

static int aw_boost_normal(void)
{
	if (lowmem_limit == 0) {
		dram_size = get_dram_size();
		if (dram_size <= 512) {
			aw_sysctrl_set("/proc/sys/vm/dirty_ratio","5");
			aw_sysctrl_set("/proc/sys/vm/dirty_background_ratio","2");
			aw_sysctrl_set("/proc/sys/vm/min_free_kbytes","8192");
		} else {
			aw_sysctrl_set("/proc/sys/vm/extra_free_kbytes","86400");
			aw_sysctrl_set("/proc/sys/vm/min_free_kbytes","16384");
		}
		bid = aw_borad_feature_detect();
		aw_system_watch_kernelspace(0, "kswapd0");
		aw_system_watch_group_memory(DETECT_INIT, 0);
		aw_sysctrl_set(AW_BOOST_UP_CPUFREQ_GOVNOR_SYS, AW_CPUFREQ_GOVNOR);
		aw_sysctrl_set(AW_BOOST_UP_CPUFAUTOHOTPLUG_SYS, AW_CPUFAUTOHOTPLUG_ENABLE);
		aw_sysctrl_set(AW_SYSCTRL_THERMAL_TRIP, AW_THERMAL_TRIP_NORMAL);
		lowmem_limit = 1;
	}

	if (bid == AW_CPU_FAST)
		aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_FAST_NORMAL);
	else
		aw_sysctrl_set(AW_BOOST_UP_CPUS, AW_BOOST_UP_CPUS_SLOW_NORMAL);

	aw_sysctrl_set(AW_BOOST_CPUFREQ_BOOT_LOCK_SYS, AW_BOOST_CPUFREQ_UNLOCK);

	bmode.mode_cur = AW_BOOST_UP_MODE_NORMAL;
        bmode.mode_pre = AW_BOOST_UP_MODE_NORMAL;
        blist.hold_pid = 0;
        blist.mode_cur = AW_BOOST_UP_MODE_NORMAL;
        blist.mode_pre = AW_BOOST_UP_MODE_NORMAL;
        blist.slot_active = 0;
        return 0;
err_out:
        return -1;

}
#endif

static void* aw_boostup_mode_detect(void* arg)
{
	int init;
	char dirname[20];
	DIR *dir;

	init = (int)arg;
	B_LOG("%s: boost thread pid %d tid %d arg %d init %d \n",__func__, getpid(), (int)pthread_self(), (int)arg, init);
	while(1)
	{
		if (init == 1)
		{
			init++;
			B_LOG("%s wake up and boost init normal mode \n",__func__);
			#ifdef SUN8IW6P1
				aw_boost_normal();
			#endif
			#ifdef SUN8IW7P1
				aw_boost_normal();
			#endif
			#ifdef SUN8IW11P1
				aw_boost_normal();
			#endif
			#ifdef SUN9IW1P1
				aw_boost_normal();
			#endif
		}
		else
		{
			if (blist.slot_active)
			{
				B_LOG("%s: boost mode_index %d pid %d\n",__func__,blist.mode_cur, blist.hold_pid);
				snprintf(dirname,sizeof(dirname),"/proc/%d",blist.hold_pid);
				dir = opendir(dirname);
				if(dir == NULL)
				{
					#ifdef SUN8IW6P1
					if (bmode.mode_cur != AW_BOOST_UP_MODE_NORMAL)
						aw_boost_normal();
					#endif
					#ifdef SUN8IW7P1
					if (bmode.mode_cur != AW_BOOST_UP_MODE_NORMAL)
						 aw_boost_normal();
					#endif
				}
				else
					closedir(dir);
			}

			if ((user_detect == USER_ACTIVE) || (sys_detect = USER_ACTIVE))
			{
				#ifdef SUN8IW7P1
				aw_system_watch_group_memory(DETECT_WORK, 0);
				#endif
			}

			MS_SLEEP(detect_grap);
		}
	}

	return NULL;
}

int aw_init_boostup(void)
{
	int err = 0;
	pthread_t bst = 0;
	int init = 1;
	err = pthread_create(&bst, NULL, aw_boostup_mode_detect, (void *)init);
	if (err != 0)
	{
		ERROR(" ERR: boostup thread init fail, %s !\n", strerror(err));
		return -1;
	}
	pthread_detach(bst);
	return 0;
}



/**
 * mode_media_cdx2.0_deinterlace
 * mode_debug
 */
int aw_boost_up_perf(const char *name, const char *value)
{
	int pid = 0;
	int index = 0;
	int enable = 0;
//	char string[COMM_LEN];
	if(!strncmp(name, "sys.boot_completed", strlen("sys.boot_completed"))) {
		sscanf(value, "%d", &enable);
		if (enable == 1)
			aw_init_boostup();
		B_LOG("%s %d \n", name, enable);
	}

	if(!strncmp(name, "media.boost.pref", strlen("media.boost.pref"))){
		B_LOG("property get :%s, value:%s\n", name, value);
		aw_get_para(value, &pid, &index);
		if(!strncmp(value, "mode", strlen("mode"))){
			switch(value[4]){
				case 'n':
				case 'N':
					#ifdef SUN8IW6P1
					if (bmode.mode_cur != AW_BOOST_UP_MODE_NORMAL)
						aw_boost_normal();
					#endif
					break;
				case 'm':
				case 'M':
					#ifdef SUN8IW6P1
					if (bmode.mode_cur != AW_BOOST_UP_MODE_CDX)
						aw_media_cdx_boost_cpus(pid,index);
					#endif
					#ifdef SUN8IW7P1
					aw_media_karaok_boost_cpus(pid,index);
					break;
					#endif
				case 'l':
				case 'L':
					#ifdef SUN8IW7P1
					aw_media_adjust_lmk_params(pid,index);
					#endif
					break;
				case 'd':
				case 'D':
					BOOST_UP_DEBUG = true;
					break;
				default:
					B_LOG("media.boost.pref set invalid value:%s\n", value);
					break;
			}
		}
	}
	if(!strncmp(name, "sys.boost.pref", strlen("sys.boost.pref"))) {
		B_LOG("property get :%s, value:%s\n", name, value);
		if(!strncmp(value, "mode", strlen("mode"))){
			switch(value[4]){
				case 'n':
				case 'N':
					aw_get_para(value, &pid, &index);
					#ifdef SUN8IW7P1
					aw_boost_system(pid, index);
					#endif
					#ifdef SUN9IW1P1
					aw_boost_system(pid, index);
					#endif
					break;
				case 'm':
				case 'M':
					#ifdef SUN8IW7P1
					aw_get_str(value, string, &index);
					aw_system_watch_userspace(index, string);
					#endif
					break;
				default:
					B_LOG("sys.boost.pref set invalid value:%s\n", value);
					break;
			}
		}
	}

	return 0;
}
