/* SPDX-License-Identifier: GPL-2.0 */
/* ARM EABI, static and libc-free. Run on the target kernel, not the build host. */
#include <linux/types.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <linux/prctl.h>
#include <linux/random.h>
#include <asm/unistd.h>
typedef __kernel_pid_t pid_t;
typedef __kernel_uid32_t uid_t;
#define BINDER_IPC_32BIT 1
#include "uapi/binder.h"

typedef char binder_protocol_must_be_v7[BINDER_CURRENT_PROTOCOL_VERSION == 7 ? 1 : -1];
typedef char binder_word_must_be_32bit[sizeof(binder_uintptr_t) == 4 ? 1 : -1];
typedef char binder_write_read_layout[sizeof(struct binder_write_read) == 24 ? 1 : -1];

static long call5(long nr, long a, long b, long c, long d, long e)
{
	register long r0 __asm__("r0") = a;
	register long r1 __asm__("r1") = b;
	register long r2 __asm__("r2") = c;
	register long r3 __asm__("r3") = d;
	register long r4 __asm__("r4") = e;
	register long r7 __asm__("r7") = nr;
	__asm__ volatile("svc 0" : "+r" (r0)
		: "r" (r1), "r" (r2), "r" (r3), "r" (r4), "r" (r7)
		: "memory", "cc");
	return r0;
}

static void say(const char *s)
{
	long n = 0;
	while (s[n])
		n++;
	call5(__NR_write, 1, (long)s, n, 0, 0);
}

static void finish(int code) __attribute__((noreturn));
static void finish(int code)
{
	call5(__NR_exit, code, 0, 0, 0, 0);
	for (;;) { }
}

static int wait_ok(long pid)
{
	int status = -1;
	long ret;
	do {
		ret = call5(__NR_wait4, pid, (long)&status, 0, 0, 0);
	} while (ret == -4); /* EINTR */
	return ret == pid && status == 0;
}

static int sandbox_child(void)
{
	struct sock_filter insns[] = {
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, 0), /* seccomp_data.nr */
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_getpid, 0, 1),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | 13),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = { 4, insns };
	long pid;
	if (call5(__NR_prctl, PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0))
		return 9;
	if (call5(__NR_prctl, PR_CAP_AMBIENT, PR_CAP_AMBIENT_IS_SET, 13, 0, 0))
		return 10;
	if (call5(__NR_prctl, PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
		return 1;
	if (call5(__NR_prctl, PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) != 1)
		return 2;
	/* Reject unknown flags without installing a filter. */
	if (call5(__NR_seccomp, SECCOMP_SET_MODE_FILTER, 2, (long)&prog, 0, 0) != -22)
		return 3;
	if (call5(__NR_seccomp, SECCOMP_SET_MODE_FILTER,
		  SECCOMP_FILTER_FLAG_TSYNC, (long)&prog, 0, 0))
		return 4;
	/* Regression: skipped syscalls must preserve EACCES, without SIGILL. */
	if (call5(__NR_getpid, 0, 0, 0, 0, 0) != -13)
		return 5;
	if (call5(__NR_prctl, PR_GET_SECCOMP, 0, 0, 0, 0) != SECCOMP_MODE_FILTER)
		return 6;
	pid = call5(__NR_fork, 0, 0, 0, 0, 0);
	if (!pid)
		finish(call5(__NR_getpid, 0, 0, 0, 0, 0) == -13 ? 0 : 7);
	if (pid < 0 || !wait_ok(pid))
		return 8;
	return 0;
}

void _start(void) __attribute__((noreturn));
void _start(void)
{
	const char *nodes[] = { "/dev/binder", "/dev/hwbinder", "/dev/vndbinder" };
	unsigned int i;
	long pid, fd;
	int failed = 0;
	char vers[8] = { 0 };
	char random_byte;
	long random_result;
	for (i = 0; i < 3; i++) {
		struct binder_version version = { 0 };
		fd = call5(__NR_open, (long)nodes[i], 2, 0, 0, 0);
		if (fd < 0 || call5(__NR_ioctl, fd, BINDER_VERSION, (long)&version, 0, 0)
		    || version.protocol_version != 7) {
			say("FAIL Binder access/protocol: ");
			say(nodes[i]);
			say("\n");
			failed = 1;
		}
		if (fd >= 0)
			call5(__NR_close, fd, 0, 0, 0, 0);
	}
	fd = call5(__NR_open, (long)"/sys/fs/selinux/policyvers", 0, 0, 0, 0);
	if (fd < 0 || call5(__NR_read, fd, (long)vers, 7, 0, 0) < 2
	    || vers[0] != '3' || vers[1] != '0') {
		say("FAIL SELinux policyvers (expected 30)\n");
		failed = 1;
	}
	if (fd >= 0)
		call5(__NR_close, fd, 0, 0, 0, 0);
	random_result = call5(__NR_getrandom, (long)&random_byte, 1,
			      GRND_NONBLOCK, 0, 0);
	if (random_result != 1 && random_result != -11) {
		say("FAIL getrandom (expected one byte or EAGAIN before entropy init)\n");
		failed = 1;
	}
	pid = call5(__NR_fork, 0, 0, 0, 0, 0);
	if (!pid)
		finish(sandbox_child());
	if (pid < 0 || !wait_ok(pid)) {
		say("FAIL seccomp/no_new_privs/TSYNC API/fork inheritance\n");
		failed = 1;
	}
	if (call5(__NR_getpid, 0, 0, 0, 0, 0) <= 0) {
		say("FAIL child filter affected parent\n");
		failed = 1;
	}
	say(failed ? "Oreo smoke checks: FAIL\n" : "Oreo smoke checks: PASS\n");
	finish(failed);
}
