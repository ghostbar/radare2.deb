/* XXX : move to r_util??? rename method names.. to long? */
/* proc IO is not related to socket io.. this is shitty!! */

#include <r_socket.h>
#include <signal.h>

#if __UNIX__
#include <sys/wait.h>
#endif

R_API struct r_socket_proc_t *r_socket_proc_open(char *const argv[])
{
#if __UNIX__
	struct r_socket_proc_t *sp = R_NEW(struct r_socket_proc_t);
#ifdef O_CLOEXEC
	const int flags = O_CLOEXEC; //O_NONBLOCK|O_CLOEXEC;
#else
	const int flags = 0; //O_NONBLOCK|O_CLOEXEC;
#endif

	if (pipe (sp->fd0)==-1) {
		perror("pipe");
		free(sp);
		return NULL;
	}
	fcntl (sp->fd0[0], flags);
	fcntl (sp->fd0[1], flags);

	if (pipe (sp->fd1)==-1) {
		perror("pipe");
		free(sp);
		return NULL;
	}
	fcntl (sp->fd1[0], flags);
	fcntl (sp->fd1[1], flags);

	sp->pid = fork ();
	switch (sp->pid) {
	case 0:
		close (0);
		dup2 (sp->fd0[0], 0);
		close (1);
		dup2 (sp->fd1[1], 1);
		execv (argv[0], argv);
		exit (1);
	case -1:
		perror ("fork");
		r_socket_proc_close (sp);
		free (sp);
		break;
		//r_socket_block(sp, R_FALSE);
	}
	return sp;
#endif
}

R_API int r_socket_proc_close(struct r_socket_proc_t *sp) {
#if __UNIX__
	/* this is wrong */
	kill(sp->pid, 9);
	waitpid(sp->pid, NULL, 0); //WNOHANG);
	close(sp->fd0[0]);
	close(sp->fd0[1]);
	//close(sp->fd1[0]);
	close(sp->fd1[1]);
	//sp->fd[0] = -1;
	//sp->fd[1] = -1;
#endif
	return 0;
}
