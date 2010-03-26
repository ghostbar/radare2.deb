/* radare - LGPL - Copyright 2009-2010 pancake<@nopcode.org> */

#include <r_th.h>

static void *_r_th_launcher(void *_th) {
	int ret;
	struct r_th_t *th = _th;

	th->ready = R_TRUE;
#if __UNIX__
	if (th->delay>0) sleep(th->delay);
	else if (th->delay<0) r_th_lock_wait(&th->lock);
#else
	if (th->delay<0) r_th_lock_wait(&th->lock);
#endif

	do {
		r_th_lock_leave(&th->lock);
		th->running = R_TRUE;
		ret = th->fun(th);
		th->running = R_FALSE;
		r_th_lock_enter(&th->lock);
	} while(ret);

#if HAVE_PTHREAD
	pthread_exit(&ret);
#endif
	return 0;
}

R_API int r_th_init(struct r_th_t *th, R_TH_FUNCTION(fun), void *user, int delay) {
	int ret = R_TRUE;

	r_th_lock_init(&th->lock);
	th->running = R_FALSE;
	th->fun = fun;	
	th->user = user;
	th->delay = delay;
	th->breaked = R_FALSE;
	th->ready = R_FALSE;
#if HAVE_PTHREAD
	if (pthread_create(&th->tid, NULL, _r_th_launcher, th))
		ret = R_FALSE;
#elif __WIN32__
	th->tid = CreateThread(NULL, 0, _r_th_launcher, th, 0, &th->tid);
#endif
	return ret;
}

R_API int r_th_push_task(struct r_th_t *th, void *user) {
	int ret = R_TRUE;
	th->user = user;
	r_th_lock_leave(&th->lock);
	return ret;
}

R_API struct r_th_t *r_th_new(R_TH_FUNCTION(fun), void *user, int delay) {
	struct r_th_t *th = R_NEW (struct r_th_t);
	r_th_init (th, fun, user, delay);
	return th;
}

R_API void r_th_break(struct r_th_t *th) {
	th->breaked = R_TRUE;
}

R_API int r_th_kill(struct r_th_t *th, int force) {
	th->breaked = R_TRUE;
	r_th_break(th);
	r_th_wait(th);
#if HAVE_PTHREAD
	pthread_cancel (th->tid);
#endif
	return 0;
}

R_API int r_th_start(struct r_th_t *th, int enable) {
	int ret = R_TRUE;
	if (enable) {
		if (!th->running) {
			// start thread
			while (!th->ready);
			r_th_lock_leave (&th->lock);
		}
	} else {
		if (th->running) {
			// stop thread
			r_th_kill (th, 0);
			r_th_lock_enter (&th->lock); // deadlock?
		}
	}
	th->running = enable;
	return ret;
}

R_API int r_th_wait(struct r_th_t *th) {
	int ret = R_FALSE;
#if HAVE_PTHREAD
	void *thret;
	ret = pthread_join (th->tid, &thret);
	th->running = R_FALSE;
#endif
	return ret;
}

R_API int r_th_wait_async(struct r_th_t *th) {
	return th->running;
}

R_API void *r_th_free(struct r_th_t *th) {
	r_th_kill (th, R_TRUE);
	free (th);
	return NULL;
}
