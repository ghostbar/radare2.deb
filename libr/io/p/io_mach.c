/* radare - LGPL - Copyright 2009-2016 - pancake */

#include <r_userconf.h>

#include <r_io.h>
#include <r_lib.h>
#include <r_cons.h>

#if __APPLE__ && DEBUGGER

#define EXCEPTION_PORT 0

#include <mach/exception_types.h>
//no available for ios #include <mach/mach_vm.h>
#include <mach/mach_host.h>
#include <mach/host_priv.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/processor_set.h>
#include <mach/mach_error.h>
#include <mach/task.h>
#include <mach/task_info.h>
#include <mach/thread_act.h>
#include <mach/thread_info.h>
#include <mach/vm_map.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/wait.h>
#include <errno.h>

#define MACH_ERROR_STRING(ret) \
	(mach_error_string (ret) ? mach_error_string (ret) : "(unknown)")

typedef struct {
	int pid;
	task_t task;
} RIOMach;
#define RIOMACH_PID(x) (x ? ((RIOMach*)(x))->pid : -1)
#define RIOMACH_TASK(x) (x ? ((RIOMach*)(x))->task : -1)

#undef R_IO_NFDS
#define R_IO_NFDS 2
extern int errno;

static task_t task_for_pid_workaround(int Pid) {
	host_t myhost = mach_host_self();
	mach_port_t psDefault = 0;
	mach_port_t psDefault_control = 0;
	task_array_t tasks = NULL;
	mach_msg_type_number_t numTasks = 0;
	kern_return_t kr = -1;
	int i;
	if (Pid == -1) return -1;

	kr = processor_set_default (myhost, &psDefault);
	if (kr != KERN_SUCCESS) {
		return -1;
	}
	kr = host_processor_set_priv (myhost, psDefault, &psDefault_control);
	if (kr != KERN_SUCCESS) {
//		eprintf ("host_processor_set_priv failed with error 0x%x\n", kr);
		//mach_error ("host_processor_set_priv",kr);
		return -1;
	}

	numTasks = 0;
	kr = processor_set_tasks (psDefault_control, &tasks, &numTasks);
	if (kr != KERN_SUCCESS) {
//		eprintf ("processor_set_tasks failed with error %x\n", kr);
		return -1;
	}
	if (Pid == 0) {
		/* kernel task */
		return tasks[0];
	}
	for (i = 0; i < numTasks; i++) {
		int pid;
		pid_for_task (i, &pid);
		if (pid == Pid) {
			return (tasks[i]);
		}
	}
	return -1;
}


static task_t pid_to_task(int pid) {
	task_t task = 0;
	static task_t old_task = 0;
	static int old_pid = -1;
	kern_return_t kr;
	if (old_task != 0 && old_pid == pid) {
		return old_task;
	} else if (old_task != 0 && old_pid != pid) {
		//we changed the process pid so deallocate a ref from the old_task
		//since we are going to get a new task
		kr = mach_port_deallocate (mach_task_self (), old_task);
		if (kr != KERN_SUCCESS) {
			eprintf ("fail to deallocate port %s-%d\n", __FILE__, __LINE__);
			return 0;
		}
	}
	int err = task_for_pid (mach_task_self (), (pid_t)pid, &task);
	if ((err != KERN_SUCCESS) || !MACH_PORT_VALID (task)) {
		task = task_for_pid_workaround (pid);
		if (task == -1) {
			//eprintf ("Failed to get task %d for pid %d.\n", (int)task, (int)pid);
			//eprintf ("Missing priviledges? 0x%x: %s\n", err, MACH_ERROR_STRING (err));
			return -1;
		}
	}
	old_task = task;
	old_pid = pid;
	return task;
}

static bool task_is_dead (int pid) {
	unsigned int count = 0;
	kern_return_t kr = mach_port_get_refs (mach_task_self(),
		pid_to_task (pid), MACH_PORT_RIGHT_SEND, &count);
	return (kr != KERN_SUCCESS || !count);
}

static ut64 the_lower = 0LL;

static ut64 getNextValid(RIO *io, RIODesc *fd, ut64 addr) {
	struct vm_region_submap_info_64 info;
	vm_address_t address = MACH_VM_MIN_ADDRESS;
	vm_size_t size = (vm_size_t) 0;
	vm_size_t osize = (vm_size_t) 0;
	natural_t depth = 0;
	kern_return_t kr;
	int tid = RIOMACH_PID (fd->data);
	task_t task = pid_to_task (tid);
	ut64 lower = addr;
#if __arm64__ || __aarch64__
	size = osize = 16384; // acording to frida
#else
	size = osize = 4096;
#endif
	if (the_lower) {
		if (addr < the_lower)
			return the_lower;
		return addr;
	}

	for (;;) {
		mach_msg_type_number_t info_count;
		info_count = VM_REGION_SUBMAP_INFO_COUNT_64;
		memset (&info, 0, sizeof (info));
		kr = vm_region_recurse_64 (task, &address, &size,
			&depth, (vm_region_recurse_info_t) &info, &info_count);
		if (kr != KERN_SUCCESS) {
			break;
		}
		if (lower == addr) {
			lower = address;
		}
		if (info.is_submap) {
			depth++;
			continue;
		}
		if (addr >= address && addr < address + size) {
			return addr;
		}
		if (address < lower) {
			lower = address;
		}
		if (size < 1) {
			size = osize; // fuck
		}
		address += size;
		size = 0;
	}
	the_lower = lower;
	return lower;
}

static int __read(RIO *io, RIODesc *fd, ut8 *buf, int len) {
	vm_size_t size = 0;
	int blen, err, copied = 0;
	int blocksize = 32;
	RIOMach *riom = (RIOMach *)fd->data;

	if (task_is_dead (riom->pid))
		return -1;

	memset (buf, 0xff, len);
	if (RIOMACH_PID (fd->data) == 0) {
		if (io->off < 4096)
			return len;
	}

	copied = getNextValid(io, fd, io->off) - io->off;
	if (copied < 0) copied = 0;

	while (copied < len) {
		blen = R_MIN ((len - copied), blocksize);
		//blen = len;
		err = vm_read_overwrite (RIOMACH_TASK (fd->data),
			(ut64)io->off + copied, blen,
			(pointer_t)buf + copied, &size);
		switch (err) {
		case KERN_PROTECTION_FAILURE:
			//eprintf ("r_io_mach_read: kern protection failure.\n");
			break;
		case KERN_INVALID_ADDRESS:
			if (blocksize == 1) {
				memset (buf+copied, 0xff, len-copied);
				return size+copied;
			}
			blocksize = 1;
			blen = 1;
			buf[copied] = 0xff;
			break;
		}
		if (err == -1 || size < 1) {
			return -1;
		}
		if (size == 0) {
			if (blocksize == 1) {
				memset (buf+copied, 0xff, len-copied);
				return len;
			}
			blocksize = 1;
			blen = 1;
			buf[copied] = 0xff;
		}
		copied += blen;
	}
	return len;
}

static int tsk_getperm(RIO *io, task_t task, vm_address_t addr) {
	kern_return_t kr;
	mach_port_t object;
	vm_size_t vmsize;
	mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
	vm_region_flavor_t flavor = VM_REGION_BASIC_INFO_64;
	vm_region_basic_info_data_64_t info;
	kr = vm_region_64 (task, &addr, &vmsize, flavor, (vm_region_info_t)&info, &info_count, &object);
	return (kr != KERN_SUCCESS ? 0 : info.protection);


}

static int tsk_pagesize(RIOMach *riom) {
	//cache the pagesize
	static vm_size_t pagesize = 0;
	kern_return_t kr;
	if (pagesize)
		return pagesize;
	kr = host_page_size (mach_host_self (), &pagesize);
	if (kr != KERN_SUCCESS)
		pagesize = 4096;
	return pagesize;
}

static vm_address_t tsk_getpagebase(RIOMach *riom, ut64 addr) {
	vm_address_t pagesize = tsk_pagesize (riom);
	return (addr & ~(pagesize - 1));
}


static bool tsk_setperm(RIO *io, task_t task, vm_address_t addr, int len, int perm) {
	kern_return_t kr;
	kr = vm_protect (task, addr, len, 0, perm);
	if (kr != KERN_SUCCESS) {
		eprintf ("failed to change perm %s:%d\n", __FILE__, __LINE__);
		perror ("tsk_setperm");
		return false;
	}
	return true;
}

static bool tsk_write(task_t task, vm_address_t addr, const ut8 *buf, int len) {
	kern_return_t kr = vm_write (task, addr, (vm_offset_t)buf, (mach_msg_type_number_t)len);
	if (kr != KERN_SUCCESS) {
		// perror ("vm_write");
		//the memory is not mapped
		return false;
	}
	return true;
}

static int mach_write_at(RIO *io, RIOMach *riom, const void *buf, int len, ut64 addr) {
	vm_address_t vaddr = addr;
	vm_address_t pageaddr;
	vm_size_t pagesize;
	vm_size_t total_size;
	int operms = 0;
	task_t task;

	if (!riom || len < 1)
		return 0;
	if (task_is_dead (riom->pid))
		return 0;
	task = riom->task;
	pageaddr = tsk_getpagebase (riom, addr);
	pagesize = tsk_pagesize (riom);
	if (len > pagesize)
		total_size = pagesize * (1 + (len / pagesize));
	else
		total_size = pagesize;

	if (tsk_write (task, vaddr, buf, len))
		return len;
	operms = tsk_getperm (io, task, pageaddr);
	if (!tsk_setperm (io, task, pageaddr, total_size, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY)) {
		eprintf ("io.mach: Cannot set page perms for %d bytes at 0x%08"
			PFMT64x"\n", (int)pagesize, (ut64)pageaddr);
		return -1;
	}
	if (!tsk_write (task, vaddr, buf, len)) {
		eprintf ("io.mach: Cannot write on memory\n");
		len = -1;
	}
	if (operms) {
		if (!tsk_setperm (io, task, pageaddr, total_size, operms)) {
			eprintf ("io.mach: Cannot restore page perms\n");
			return -1;
		}
	}
	return len;
}

static int __write(RIO *io, RIODesc *fd, const ut8 *buf, int len) {
	return mach_write_at (io, (RIOMach*)fd->data, buf, len, io->off);
}

static int __plugin_open(RIO *io, const char *file, ut8 many) {
	return (!strncmp (file, "attach://", 9) \
		|| !strncmp (file, "mach://", 7));
}

// s/inferior_task/port/
static unsigned int debug_attach(int pid) {
	task_t task = pid_to_task (pid);
	if (!task)
		return 0;
	//eprintf ("pid: %d\ntask: %d\n", pid, task);
	return task;
}

static RIODesc *__open(RIO *io, const char *file, int rw, int mode) {
	RIODesc *ret = NULL;
	RIOMach *riom;
	const char *pidfile;
	char *pidpath, *endptr;
	int pid;
	task_t task;
	if (!__plugin_open (io, file, 0))
		return NULL;
	pidfile = file + (file[0] == 'a' ? 9:7);
	pid = (int)strtol (pidfile, &endptr, 10);
	if (endptr == pidfile || pid < 0)
		return NULL;

	task = debug_attach (pid);
	if (task == -1) {
		return NULL;
	}
	if (!task) {
		if (pid > 0 && io->referer && !strncmp (io->referer, "dbg://", 6)) {
			eprintf ("Child killed\n");
			kill (pid, 9);
		}
		switch (errno) {
		case EPERM:
			eprintf ("Operation not permitted\n");
			break;
		case EINVAL:
			perror ("ptrace: Cannot attach");
			eprintf ("ERRNO: %d (EINVAL)\n", errno);
			break;
		default:
			eprintf ("unknown error in debug_attach\n");
			break;
		}
		return NULL;
	}
	riom = R_NEW0 (RIOMach);
	riom->pid = pid;
	riom->task = task;
	// sleep 1s to get proper path (program name instead of ls) (racy)
	if (!pid)
		pidpath = strdup ("kernel");
	else
		pidpath = r_sys_pid_to_path (pid);
	ret = r_io_desc_new (&r_io_plugin_mach, riom->pid,
			    pidpath, rw | R_IO_EXEC, mode, riom);
	free (pidpath);
	return ret;
}

static ut64 __lseek(RIO *io, RIODesc *fd, ut64 offset, int whence) {
	switch (whence) {
	case 0: // abs
		io->off = offset;
		break;
	case 1: // cur
		io->off += (int)offset;
		break;
	case 2: // end
		io->off = UT64_MAX;
		break;
	}
	return io->off;
}

static int __close(RIODesc *fd) {
	RIOMach *riom = (RIOMach*)fd->data;
	kern_return_t kr;
	if (!riom)
		return false;
	kr = mach_port_deallocate (mach_task_self (), riom->task);
	if (kr != KERN_SUCCESS)
		perror ("__close io_mach");
	R_FREE (fd->data);
	return kr == KERN_SUCCESS;
}

static int __system(RIO *io, RIODesc *fd, const char *cmd) {
	RIOMach *riom = (RIOMach*)fd->data;
	/* XXX ugly hack for testing purposes */
	if (!strncmp (cmd, "perm", 4)) {
		int perm = r_str_rwx (cmd + 4);
		if (perm) {
			int pagesize = tsk_pagesize(riom);
			tsk_setperm (io, riom->task, io->off, pagesize, perm);
		} else {
			eprintf ("Usage: =!perm [rwx]\n");
		}
		return 0;
	}
	if (!strncmp (cmd, "pid", 3)) {
		const char *pidstr = cmd + 4;
		int pid = -1;
		if (!cmd[3]) {
			int pid = RIOMACH_PID (fd->data);
			eprintf ("%d\n", pid);
			return 0;
		}
		if (!strcmp (pidstr, "0")) {
			pid = 0;
		} else {
			pid = atoi (cmd+4);
			if (!pid) pid = -1;
		}
		if (pid != -1) {
			task_t task = pid_to_task (pid);
			if (task != -1) {
				eprintf ("PID=%d\n", pid);
				riom->pid = pid;
				riom->task = task;
				return 0;
			}
		}
		eprintf ("io_mach_system: Invalid pid %d\n", pid);
	} else {
		eprintf ("Try: '=!pid' or '=!perm'\n");
	}
	return 1;
}

// TODO: rename ptrace to io_mach .. err io.ptrace ??
RIOPlugin r_io_plugin_mach = {
	.name = "mach",
	.desc = "mach debugger io plugin (mach://pid)",
	.license = "LGPL",
	.open = __open,
	.close = __close,
	.read = __read,
	.plugin_open = __plugin_open,
	.lseek = __lseek,
	.system = __system,
	.write = __write,
	.isdbg = true
};

#else
RIOPlugin r_io_plugin_mach = {
	.name = "mach",
	.desc = "mach debug io (unsupported in this platform)",
	.license = "LGPL"
};
#endif

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_IO,
	.data = &r_io_plugin_mach,
	.version = R2_VERSION
};
#endif
