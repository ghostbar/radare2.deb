/* radare - LGPL - Copyright 2008-2009 pancake<nopcode.org> */

#include "r_types.h"
#include "r_lib.h"
#include <stdio.h>
#include <dirent.h>

/* TODO: support for nested plugins ?? here */

#if __UNIX__
#include <dlfcn.h>
  #define DLOPEN(x)  dlopen(x, RTLD_GLOBAL | RTLD_NOW)
  #define DLSYM(x,y) dlsym(x,y)
  #define DLCLOSE(x) dlclose(x)
#elif __WINDOWS__ && !__CYGWIN__
#include <windows.h>
  #define DLOPEN(x)  LoadLibrary(x)
  #define DLSYM(x,y) GetProcAddress(x,y)
  #define DLCLOSE(x) 0//(x)
//CloseLibrary(x)
#else
  #define DLOPEN(x)  NULL
  #define DLSYM(x,y) NULL
  #define DLCLOSE(x) NULL
#endif

/* XXX : this must be registered in runtime */
static const char *r_lib_types[] = {
	"io", "dbg", "lang", "asm", "anal", "parse", "bin", //"bininfo", 
	"bp", "syscall", "fastcall", NULL
};

/* XXX: Rename this helper function */
R_API const char *r_lib_types_get(int idx)
{
	if (idx<0||idx>R_LIB_TYPE_LAST)
		return "unk";
	return r_lib_types[idx];
}

R_API void *r_lib_dl_open(const char *libname)
{
	void *ret;
	IFRTDBG fprintf(stderr, "Opening '%s'\n", libname);
	ret = DLOPEN(libname);
	if (ret == NULL)
#if __UNIX__
		IFDBG eprintf("dlerror: %s\n", dlerror());
#else
		eprintf ("r_lib_dl_open: Cannot open '%s'\n", libname);
#endif
	return ret;
}

R_API void *r_lib_dl_sym(void *handle, const char *name)
{
	return DLSYM(handle, name);
}

R_API int r_lib_dl_close(void *handle)
{
	return DLCLOSE(handle);
}

/* ---- */

R_API struct r_lib_t *r_lib_init(struct r_lib_t *lib, const char *symname)
{
	if (lib) {
		INIT_LIST_HEAD(&lib->handlers);
		INIT_LIST_HEAD(&lib->plugins);
		strncpy(lib->symname, symname, sizeof(lib->symname)-1);
	}
	return lib;
}

R_API struct r_lib_t *r_lib_new(const char *symname)
{
	struct r_lib_t *lib = R_NEW(struct r_lib_t);
	return r_lib_init(lib, symname);
}

R_API struct r_lib_t *r_lib_free(struct r_lib_t *lib)
{
	/* TODO: iterate over libraries and free them all */
	/* TODO: iterate over handlers and free them all */
	r_lib_close(lib, NULL);
	free (lib);
	return NULL;
}

/* THIS IS WRONG */
R_API int r_lib_dl_check_filename(const char *file)
{
	/* skip hidden files */
	if (file[0]=='.')
		return R_FALSE;
	else
	/* per SO dylib filename extensions */
	if (strstr(file, ".so"))
		return R_TRUE;
	if (strstr(file, ".dll"))
		return R_TRUE;
	if (strstr(file, ".dylib"))
		return R_TRUE;
	return R_FALSE;
}

/* high level api */

R_API int r_lib_run_handler(struct r_lib_t *lib, struct r_lib_plugin_t *plugin, struct r_lib_struct_t *symbol)
{
	struct r_lib_handler_t *h = plugin->handler;
	if (h && h->constructor != NULL)
		return h->constructor(plugin, h->user, symbol->data);
	return R_FAIL;
}

R_API struct r_lib_handler_t *r_lib_get_handler(struct r_lib_t *lib, int type)
{
	struct list_head *pos;
	list_for_each_prev(pos, &lib->handlers) {
		struct r_lib_handler_t *h = list_entry(pos, struct r_lib_handler_t, list);
		if (h->type == type)
			return h;
	}
	return NULL;
}

R_API R_API int r_lib_close(struct r_lib_t *lib, const char *file)
{
	struct list_head *pos;
	list_for_each_prev(pos, &lib->plugins) {
		struct r_lib_plugin_t *h = list_entry(pos, struct r_lib_plugin_t, list);
		if ((file==NULL || (!strcmp(file, h->file))) && h->handler->destructor != NULL) {
			int ret = h->handler->destructor(h, h->handler->user, h->data);
			free(h->file);
			list_del(&h->list);
			free(h);
			return ret;
		}
	}
	return -1;
}

// XXX ugly hack ?
static int samefile(const char *a, const char *b)
{
	char *sa = strdup(a);
	char *sb = strdup(b);
	char *ptr;
	int ret;

	if (sa != NULL && sb != NULL) {
		do {
			ptr = strstr(sa, "//");
			if (ptr) strcpy(ptr, ptr+1);	
		} while(ptr);

		do {
			ptr = strstr(sb, "//");
			if (ptr) strcpy(ptr, ptr+1);	
		} while(ptr);
		ret = strcmp(sa,sb)?R_FALSE:R_TRUE;
	}

	free(sa);
	free(sb);
	return ret;
}

R_API int r_lib_open(struct r_lib_t *lib, const char *file)
{
	struct r_lib_plugin_t *p;
	struct list_head *pos;
	struct r_lib_struct_t *stru;
	void * handler;
	int ret;

	/* ignored by filename */
	if (!r_lib_dl_check_filename(file)) {
		IFDBG fprintf(stderr, "Invalid library extension: %s\n", file);
		return R_FAIL;
	}

	handler = r_lib_dl_open(file);
	if (handler == NULL) {
		IFDBG fprintf(stderr, "Cannot open library: '%s'\n", file);
		return R_FAIL;
	}
	
	stru = (struct r_lib_struct_t *) r_lib_dl_sym(handler, lib->symname);
	if (stru == NULL) {
		IFDBG fprintf(stderr, "No root symbol '%s' found in library '%s'\n", lib->symname, file);
		return R_FAIL;
	}

	list_for_each_prev(pos, &lib->plugins) {
		struct r_lib_plugin_t *p = list_entry(pos, struct r_lib_plugin_t, list);
		if (samefile(file, p->file)) {
			r_lib_dl_close(handler);
			return R_FAIL;
		}
	}

	p = R_NEW(struct r_lib_plugin_t);
	p->type = stru->type;
	p->data = stru->data;
	p->file = strdup(file);
	p->dl_handler = handler;
	p->handler = r_lib_get_handler(lib, p->type);
	
	ret = r_lib_run_handler(lib, p, stru);
	if (ret == R_FAIL) {
		IFDBG fprintf(stderr, "Library handler has failed for '%s'\n", file);
		free(p->file);
		free(p);
		r_lib_dl_close(handler);
	} else list_add(&p->list, &lib->plugins);

	return ret;
}

R_API int r_lib_opendir(struct r_lib_t *lib, const char *path)
{
	char file[1024];
	struct dirent *de;
	DIR *dh;

#ifdef LIBR_PLUGINS
	if (path == NULL)
		path = LIBR_PLUGINS;
#endif
	if (path == NULL)
		return R_FALSE;

	dh = opendir(path);
	if (dh == NULL) {
		IFDBG fprintf(stderr, "Cannot open directory '%s'\n", path);
		return R_FALSE;
	}
	while((de = (struct dirent *)readdir(dh))) {
		snprintf(file, 1023, "%s/%s", path, de->d_name);
		r_lib_open(lib, file);
	}
	closedir(dh);
	return R_TRUE;
}

R_API int r_lib_add_handler(struct r_lib_t *lib,
	int type, const char *desc,
	int (*cb)(struct r_lib_plugin_t *, void *, void *),  /* constructor */
	int (*dt)(struct r_lib_plugin_t *, void *, void *),  /* destructor */
	void *user)
{
	struct list_head *pos;
	struct r_lib_handler_t *handler = NULL;

	list_for_each_prev(pos, &lib->handlers) {
		struct r_lib_handler_t *h = list_entry(pos, struct r_lib_handler_t, list);
		if (type == h->type) {
			IFDBG fprintf(stderr, "Redefining library handler constructor for %d\n", type);
			handler = h;
			break;
		}
	}
	if (handler == NULL) {
		handler = R_NEW(struct r_lib_handler_t);
		if (handler == NULL)
			return R_FALSE;
		handler->type = type;
		list_add(&handler->list, &lib->handlers);
	}
	strncpy(handler->desc, desc, sizeof(handler->desc));
	handler->user = user;
	handler->constructor = cb;
	handler->destructor = dt;

	return R_TRUE;
}

R_API int r_lib_del_handler(struct r_lib_t *lib, int type)
{
	struct list_head *pos;
	// TODO: remove all handlers for that type? or only one?
	list_for_each_prev(pos, &lib->handlers) {
		struct r_lib_handler_t *h = list_entry(pos, struct r_lib_handler_t, list);
		if (type == h->type) {
			list_del(&(h->list));
			return R_TRUE;
		}
	}
	return R_FALSE;
}

/* XXX _list methods must be deprecated before r2-1.0 */
R_API void r_lib_list(struct r_lib_t *lib)
{
	struct list_head *pos;
#if 0
	printf("Plugin Handlers:\n");
	list_for_each_prev(pos, &lib->handlers) {
		struct r_lib_handler_t *h = list_entry(pos, struct r_lib_handler_t, list);
		printf(" - %d: %s\n", h->type, h->desc);
	}
#endif
	//printf("Loaded plugins:\n");
	list_for_each_prev(pos, &lib->plugins) {
		struct r_lib_plugin_t *p = list_entry(pos, struct r_lib_plugin_t, list);
		printf(" %5s %p %s \n", r_lib_types_get(p->type), p->handler->destructor, p->file);
	}
}

