/*
 * pprofmod.c
 *
 * NOTE:
 * 	Register callback function to process every state
 * 	change of threads. Meanwhile, this module sends
 * 	the state change data through TCP socket that is
 * 	prepared during initialization phase of this module
 * 	to user-space memory. This module also allows user
 * 	to specify a PID to record its state changes.
 *
 * AUTHOR:
 * 	Akira Yokokawa - akira.yokokawa@gmail.com
 *
 * DATE:
 * 	23.Jul.2012
 */


/*
 * kernel module
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/sched.h>

/*
 * File I/O
 */
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>


/*
 * TCP/UDP connection
 */
#include <linux/net.h>
#include <net/sock.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <asm/uaccess.h>
#include <linux/file.h>
#include <linux/socket.h>
#include <linux/slab.h>

/*
 * Netlink connection
 */
#include <linux/netlink.h>
#include <linux/skbuff.h>

/*
 * custom header
 */
#include "../include/pprof.h"

/*
 * data structure representing each state change with timestamp
 */
struct process_struct {
	long sec;
	long usec;
	pid_t pid;	// pid_t is alias of int
	pid_t tgid;
	//int coreid;
	long state;
};

#define NUM 1000
//#define LIMIT_SIZE sizeof(struct process_struct)*NUM

/*
 * default port
 */
#define PORT 20000

int profile_pid;
int pprofd_pid;
unsigned long total_data_num = 0;
struct process_struct *ptr[NUM];
struct proc_dir_entry *proc_entry;
struct task_struct *kthread;

/*
 * netlink connection
 */
struct sock *nl_sk = NULL;

void pprof_callback(struct task_struct *);

void pprof_callback(struct task_struct *task)
{
	if(task->tgid == profile_pid) {
		printk(KERN_INFO "%d %d %ld", task->pid, task->tgid, task->state);

		// struct process_struct process;
		// struct timeval tv;
		// struct sk_buff *skb_out;
		// struct nlmsghdr *nlh;
		// 
		// do_gettimeofday(&tv);

		// process.sec = tv.tv_sec;
		// process.usec = tv.tv_usec;
		// process.pid = task->pid;
		// process.tgid = task->tgid;
		// //process.coreid = task_rq(task)->cpu;
		// process.state = task->state;

		// printk(KERN_INFO "pid(%d) tgid(%d) filtered(%d)", task->pid, task->tgid, profile_pid);

		// skb_out = nlmsg_new(sizeof(struct process_struct), 0);
		// if(!skb_out) {
		// 	printk(KERN_ERR "skb_out error\n");
		// 	return;
		// }
		// nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, sizeof(struct process_struct), 0);
		// NETLINK_CB(skb_out).dst_group = 0;
		// memcpy(NLMSG_DATA(nlh), &process, sizeof(struct process_struct));
		// nlmsg_unicast(nl_sk, skb_out, netlink_client_pid);
	}
}

ssize_t proc_write(struct file *filp, const char __user *buff, unsigned long len, void *data)
{
	unsigned long ret = 0;
	char tmp[10];

	memset(tmp, '\0', 10);
	if(copy_from_user(tmp, buff, len-1)) {
		printk(KERN_INFO "[ERROR][proc_write]: copy_from_user\n");
		return 0;
	}
	
	profile_pid = (int)simple_strtol(tmp, NULL, 10);

	set_pprof_callback(pprof_callback);
	printk(KERN_INFO "[init_pprof]: callback registered\n");

	printk(KERN_INFO "tgid=%d(%s)", profile_pid, tmp);
}

ssize_t proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	return 0;
}

/*
 * register_pprofd_pid
 *
 * NOTE:
 * 	This function is used only once during the initialization phase,
 * 	in order to notify the PID of pprofd to this kernel module.
 */
static void register_pprofd_pid(struct sk_buff *skb)
{
	printk(KERN_INFO "[register_pprofd_pid]: some packet received by the pprof_mod\n");
	struct nlmsghdr *nlh;
	nlh=(struct nlmsghdr*)skb->data;
	printk(KERN_INFO "[register_pprofd_pid]: extracting the PID of pprofd\n");
	pprofd_pid = nlh->nlmsg_pid; /*pid of sending process */
	
	printk(KERN_INFO "[register_pprofd_pid]: pprofd_pid = %d\n", pprofd_pid);
}

static int __init init_pprof(void)
{

	printk(KERN_INFO "[init_pprof]: register\n");

	// create a proc device
	proc_entry = create_proc_entry("pprof", 0666, NULL);
	if(proc_entry == NULL) {
		printk(KERN_INFO "[init_pprof][Error]: Couldn't create a proc entry\n");
		return -ENOMEM;
	}
	else {
		proc_entry->read_proc = proc_read;
		proc_entry->write_proc = proc_write;
		printk(KERN_INFO "[init_pprof]: proc entry created\n");
	}

	// create a netlink socket for netlink_client 
	printk(KERN_INFO "[init_pprof]: create a netlink socket in order to know the PID of pprofd\n");
	nl_sk=netlink_kernel_create(&init_net, NETLINK_PPROF, 0, register_pprofd_pid, NULL, THIS_MODULE);
	if(!nl_sk) {
	
	    printk(KERN_ALERT "Error creating socket.\n");
		remove_proc_entry("pprof", NULL);
	    return -10;
	
	}
	else {
		printk(KERN_INFO "[init_pprof]: netlink socket for pprofd is created\n");
	}

	return 0;
}

static void __exit exit_pprof(void)
{
	printk(KERN_INFO "[exit_pprof]: unregister\n");
	remove_proc_entry("pprof", NULL);
	del_pprof_callback();
}

module_init(init_pprof);
module_exit(exit_pprof);
