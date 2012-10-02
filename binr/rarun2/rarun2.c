/* radare2 - Copyleft 2011 - pancake<nopcode.org> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <r_util.h>
#include <r_socket.h>
#if __UNIX__
#include <signal.h>
#endif

#define NARGS (sizeof (_args)/sizeof(*_args))
static char *_args[512] = {NULL};

static char *_program = NULL;
static char *_stdin = NULL;
static char *_stdout = NULL;
static char *_stderr = NULL;
static char *_chgdir = NULL;
static char *_chroot = NULL;
static char *_preload = NULL;
static char *_setuid = NULL;
static char *_seteuid = NULL;
static char *_setgid = NULL;
static char *_setegid = NULL;
static char *_input = NULL;
static char *_connect = NULL;
static char *_listen = NULL;
static int _timeout = 0;

static void parseline (char *b) {
	char *e = strchr (b, '=');
	if (!e) return;
	if (*b=='#') return;
	*e++ = 0;
	if (*e=='$') e = r_sys_getenv (e);
	if (e == NULL) return;
	if (!strcmp (b, "program")) _args[0] = _program = strdup (e);
	else if (!strcmp (b, "connect")) _connect = strdup (e);
	else if (!strcmp (b, "listen")) _listen = strdup (e);
	else if (!strcmp (b, "stdout")) _stdout = strdup (e);
	else if (!strcmp (b, "stdin")) _stdin = strdup (e);
	else if (!strcmp (b, "input")) _input = strdup (e);
	else if (!strcmp (b, "chdir")) _chgdir = strdup (e);
	else if (!strcmp (b, "chroot")) _chroot = strdup (e);
	else if (!strcmp (b, "preload")) _preload = strdup (e);
	else if (!strcmp (b, "setuid")) _setuid = strdup (e);
	else if (!strcmp (b, "seteuid")) _seteuid = strdup (e);
	else if (!strcmp (b, "setgid")) _setgid = strdup (e);
	else if (!strcmp (b, "setegid")) _setegid = strdup (e);
	else if (!memcmp (b, "arg", 3)) {
		int n = atoi (b+3);
		if (n>=0 && n<NARGS) {
			_args[n] = strdup (e);
		} else fprintf (stderr, "Out of bounds args index: %d\n", n);
	} else if (!strcmp (b, "timeout")) _timeout = atoi (e);
	else if (!strcmp (b, "setenv")) {
		char *v = strchr (e, '=');
		if (v) {
			*v++ = 0;
			r_sys_setenv (e, v);
		}
	}
}

#if __UNIX__
static void parseinput (char *s) {
	if (!*s) return;
	while (*s++) {
		if (s[0]=='\\' && s[1]=='n') {
			*s = '\n';
			strcpy (s+1, s+2);
		}
	}
}
#endif

static int runfile () {
	if (!_program) {
		printf ("No program rule defined\n");
		return 1;
	}
	if (_stdin) {
		int f = open (_stdin, O_RDONLY);
		close (0);
		dup2 (f, 0);
	}
	if (_stdout) {
		int f = open (_stdout, O_RDONLY);
		close (1);
		dup2 (f, 1);
	}
	if (_stderr) {
		int f = open (_stderr, O_RDONLY);
		close (2);
		dup2 (f, 2);
	}

	if (_connect) {
		char *p = strchr (_connect, ':');
		if (p) {
			RSocket *fd = r_socket_new (0);
			*p=0;
			if (!r_socket_connect_tcp (fd, _connect, p+1)) {
				eprintf ("Cannot connect\n");
				return 1;
			}
			eprintf ("connected\n");
			close (0);
			close (1);
			close (2);
			dup2 (fd->fd, 0);
			dup2 (fd->fd, 1);
			dup2 (fd->fd, 2);
		} else {
			eprintf ("Invalid format for connect. missing ':'\n");
			return 1;
		}
	}
	if (_listen) {
		RSocket *child, *fd = r_socket_new (0);
		if (!r_socket_listen (fd, _listen, NULL)) {
			eprintf ("Cannot listen\n");
			return 1;
		}
		child = r_socket_accept (fd);
		if (child) {
			eprintf ("connected\n");
			close (0);
			close (1);
			close (2);
			dup2 (child->fd, 0);
			dup2 (child->fd, 1);
			dup2 (child->fd, 2);
		}
	}
	if (_chgdir) chdir (_chgdir);
	if (_chroot) chdir (_chroot);
#if __UNIX__
	if (_setuid) setuid (atoi (_setuid));
	if (_seteuid) seteuid (atoi (_seteuid));
	if (_setgid) setgid (atoi (_setgid));
	if (_input) {
		int f2[2];
		pipe (f2);
		close (0);
		dup2 (f2[0], 0);
		parseinput (_input);
		write (f2[1], _input, strlen (_input));
	}
#endif
	if (_preload) {
#if __APPLE__
		r_sys_setenv ("DYLD_PRELOAD", _preload);
#else
		r_sys_setenv ("LD_PRELOAD", _preload);
#endif
	}
	if (_timeout) {
#if __UNIX__
		int mypid = getpid ();
		if (!fork ()) {
			sleep (_timeout);
			if (!kill (mypid, 0))
				fprintf (stderr, "\nInterrupted by timeout\n");
			kill (mypid, SIGKILL);
			exit (0);
		}
#else
		eprintf ("timeout not supported for this platform\n");
#endif
	}
	exit (execv (_program, _args));
}

int main(int argc, char **argv) {
	int i;
	FILE *fd;
	char *file, buf[1024];
	if (argc==1 || !strcmp (argv[1], "-h")) {
		fprintf (stderr, "Usage: rarun2 [''|script.rr2] [options ...]\n"
			"> options are file directives:\n");
		printf (
			"program=/bin/ls\n"
			"arg1=/bin\n"
			"# arg#=...\n"
			"setenv=FOO=BAR\n"
			"timeout=3\n"
			"# connect=localhost:8080\n"
			"# listen=8080\n"
			"# stdout=foo.txt\n"
			"# stdin=input.txt\n"
			"# input=input.txt\n"
			"# chdir=/\n"
			"# chroot=/mnt/chroot\n"
			"# preload=/lib/libfoo.so\n"
			"# setuid=2000\n"
			"# seteuid=2000\n"
			"# setgid=2001\n"
			"# setegid=2001\n");
		return 1;
	}
	file = argv[1];
	if (*file) {
		fd = fopen (file, "r");
		if (!fd) {
			fprintf (stderr, "Cannot open %s\n", file);
			return 1;
		}
		for (;;) {
			fgets (buf, sizeof (buf)-1, fd);
			if (feof (fd)) break;
			buf[strlen (buf)-1] = 0;
			parseline (buf);
		}
		fclose (fd);
	} else {
		for (i=2; i<argc; i++)
			parseline (argv[i]);
	}
	return runfile ();
}
