/* radare - LGPL - Copyright 2011 pancake<nopcode.org> */

#include <r_fs.h>
#include "../config.h"
#include <errno.h>
#include "p/grub/include/grub/msdos_partition.h"

static RFSPlugin *fs_static_plugins[] = { R_FS_STATIC_PLUGINS };

R_API RFS *r_fs_new () {
	int i;
	RFSPlugin *static_plugin;
	RFS *fs = R_NEW (RFS);
	if (fs) {
		fs->view = R_FS_VIEW_NORMAL;
		fs->roots = r_list_new ();
		fs->roots->free = (RListFree)r_fs_root_free;
		fs->plugins = r_list_new ();
		// XXX fs->roots->free = r_fs_plugin_free;
		for (i=0; fs_static_plugins[i]; i++) {
			static_plugin = R_NEW (RFSPlugin);
			memcpy (static_plugin, fs_static_plugins[i], sizeof (RFSPlugin));
			r_fs_add (fs, static_plugin);
		}
	}
	return fs;
}

R_API RFSPlugin *r_fs_plugin_get (RFS *fs, const char *name) {
	RListIter *iter;
	RFSPlugin *p;
	r_list_foreach (fs->plugins, iter, p) {
		if (!strcmp (p->name, name))
			return p;
	}
	return NULL;
}

R_API void r_fs_free (RFS* fs) {
	r_list_free (fs->plugins);
	r_list_free (fs->roots);
	free (fs);
}

/* plugins */
R_API void r_fs_add (RFS *fs, RFSPlugin *p) {
	// TODO: find coliding plugin name
	if (p && p->init)
		p->init ();
	r_list_append (fs->plugins, p);
}

R_API void r_fs_del (RFS *fs, RFSPlugin *p) {
	// TODO: implement r_fs_del
}

/* mountpoint */
R_API RFSRoot *r_fs_mount (RFS* fs, const char *fstype, const char *path, ut64 delta) {
	RFSPlugin *p;
	RFSRoot *root;
	RFSFile *file;
	RList *list;
	RListIter *iter;
	char *str;
	int len, lenstr;

	if (path[0] != '/') {
		eprintf ("r_fs_mount: invalid mountpoint\n");
		return NULL;
	}
	if (!(p = r_fs_plugin_get (fs, fstype))) {
		eprintf ("r_fs_mount: Invalid filesystem type\n");
		return NULL;
	}
	str = strdup (path);
	r_str_chop_path (str);
	/* Check if path exists */
	r_list_foreach (fs->roots, iter, root) {
		len = strlen (root->path);
		lenstr = strlen (str);
		if (!strncmp (str, root->path, len)) {
			if (len < lenstr && str[len] != '/')
				continue;
			else if (len > lenstr && root->path[lenstr] == '/')
				continue;
			eprintf ("r_fs_mount: Invalid mount point\n");
			return NULL;
		}
	}
	file = r_fs_open (fs, str);
	if (file) {
		r_fs_close (fs, file);
		eprintf ("r_fs_mount: Invalid mount point\n");
		return NULL;
	} else {
		list = r_fs_dir (fs, str);
		if (!r_list_empty (list)) {
			//XXX: list need free ??
			eprintf ("r_fs_mount: Invalid mount point\n");
			return NULL;
		}
	}
	root = r_fs_root_new (str, delta);
	root->p = p;
	//memcpy (&root->iob, &fs->iob, sizeof (root->iob));
	root->iob = fs->iob;
	if (!p->mount (root)) {
		eprintf ("r_fs_mount: Cannot mount partition\n");
		free (str);
		r_fs_root_free (root);
		return NULL;
	}
	r_list_append (fs->roots, root);
	eprintf ("Mounted %s on %s at 0x%llx\n", fstype, str, delta);
	free (str);
	return root;
}

static inline int r_fs_match (const char *root, const char *path, int len) {
	return (!strncmp (path, root, len));
}

R_API int r_fs_umount (RFS* fs, const char *path) {
	int len;
	RFSRoot *root;
	RListIter *iter, *riter = NULL;
	r_list_foreach (fs->roots, iter, root) {
		len = strlen (root->path);
		if (r_fs_match (path, root->path, len)) {
			riter = iter;
		}
	}
	if (riter) {
		r_list_delete (fs->roots, riter);
		return R_TRUE;
	}
	return R_FALSE;
}

R_API RList *r_fs_root (RFS *fs, const char *p) {
	RList *roots = r_list_new ();
	RFSRoot *root;
	RListIter *iter;
	int len, olen;
	char *path = strdup (p);
	r_str_chop_path (path);
	r_list_foreach (fs->roots, iter, root) {
		len = strlen (root->path);
		if (r_fs_match (path, root->path, len)) {
			olen = strlen (path);
			if (len == 1 || olen == len)
				r_list_append (roots, root);
			else if ( olen > len && path[len] == '/')
				r_list_append (roots, root);
		}
	}
	free (path);
	return roots;
}

/* filez */
R_API RFSFile *r_fs_open (RFS* fs, const char *p) {
	RFSRoot *root;
	RList *roots;
	RListIter *iter;
	RFSFile *f = NULL;
	const char *dir;
	char *path = strdup (p);
	//r_str_chop_path (path);
	roots = r_fs_root (fs, path);
	if (!r_list_empty (roots)) {
		r_list_foreach (roots, iter, root) {
			if (root && root->p && root->p->open) {
				if (strlen (root->path) == 1)
					dir = path;
				else
					dir = path + strlen (root->path);
				f = root->p->open (root, dir);
				if (f)
					break;
			}
		}
	}
	free (roots);
	free (path);
	return f;
}

// TODO: close or free?
R_API void r_fs_close (RFS* fs, RFSFile *file) {
	if (fs && file && file->p && file->p->close)
		file->p->close (file);
}

R_API int r_fs_read (RFS* fs, RFSFile *file, ut64 addr, int len) {
	if (len<1) {
		eprintf ("r_fs_read: too short read\n");
		return R_FALSE;
	}
	if (fs && file) {
		free (file->data);
		file->data = malloc (len+1);
		if (file->p && file->p->read) {
			file->p->read (file, addr, len);
			return R_TRUE;
		} else eprintf ("r_fs_read: file->p->read is null\n");
	}
	return R_FALSE;
}

R_API RList *r_fs_dir(RFS* fs, const char *p) {
	RList *roots, *ret = NULL;
	RFSRoot *root;
	RListIter *iter;
	const char *dir;
	char *path = strdup (p);
	r_str_chop_path (path);
	roots = r_fs_root (fs, path);
	r_list_foreach (roots, iter, root) {
		if (root) {
			if (strlen (root->path) == 1)
				dir = path;
			else
				dir = path + strlen (root->path);
			if (!*dir) dir = "/";
			ret = root->p->dir (root, dir, fs->view);
			if (ret)
				break;
		}
	}
	free (roots);
	free (path);
	return ret;
}

R_API int r_fs_dir_dump (RFS* fs, const char *path, const char *name) {
	RList *list;
	RListIter *iter;
	RFSFile *file, *item;
	char *str, *npath;

	list = r_fs_dir (fs, path);
	if (!list)
		return R_FALSE;
	if (!r_sys_mkdir (name)) {
		if (r_sys_mkdir_failed ()) {
			eprintf ("Cannot create \"%s\"\n", name);
			return R_FALSE;
		}
	}
	r_list_foreach (list, iter, file) {
		if (!strcmp (file->name, ".") || !strcmp (file->name, ".."))
			continue;
		str = (char *) malloc (strlen (name) + strlen (file->name) + 2);
		if (!str)
			return R_FALSE;
		strcpy (str, name);
		strcat (str, "/");
		strcat (str, file->name);
		npath = malloc (strlen (path) + strlen (file->name) + 2);
		if (!npath)
			return R_FALSE;
		strcpy (npath, path);
		strcat (npath, "/");
		strcat (npath, file->name);
		if (file->type != R_FS_FILE_TYPE_DIRECTORY) {
			item = r_fs_open (fs, npath);
			if (item) {
				r_fs_read (fs, item, 0, item->size);
				r_file_dump (str, item->data, item->size);
				free (item->data);
				r_fs_close (fs, item);
			}
		} else {
			r_fs_dir_dump (fs, npath, str);
		}
		free (npath);
		free (str);
	}
	return R_TRUE;
}

static void r_fs_find_off_aux (RFS* fs, const char *name, ut64 offset, RList *list) {
	RList *dirs;
	RListIter *iter;
	RFSFile *item, *file;
	char *found;

	dirs = r_fs_dir (fs, name);
	r_list_foreach (dirs, iter, item) {
		if (!strcmp (item->name, ".") || !strcmp (item->name, ".."))
			continue;
		if (item->type == R_FS_FILE_TYPE_DIRECTORY) {
			found = (char *) malloc (strlen (name) + strlen (item->name) + 2);
			if (!found)
				break;
			strcpy (found, name);
			strcat (found, "/");
			strcat (found, item->name);
			r_fs_find_off_aux (fs, found, offset, list);
			free (found);
		} else {
			found = (char *) malloc (strlen (name) + strlen (item->name) + 2);
			if (!found)
				break;
			strcpy (found, name);
			strcat (found, "/");
			strcat (found, item->name);
			file = r_fs_open (fs, found);
			if (file) {
				r_fs_read (fs, file, 0, file->size);
				if (file->off == offset)
					r_list_append (list, found);
				free (file->data);
				r_fs_close (fs, file);
			}
		}
	}
}

R_API RList *r_fs_find_off (RFS* fs, const char *name, ut64 off) {
	RList *list =  r_list_new ();
	list->free = free;
	r_fs_find_off_aux (fs, name, off, list);
	return list;
}

static void r_fs_find_name_aux (RFS* fs, const char *name, const char *glob, RList *list) {
	RList *dirs;
	RListIter *iter;
	RFSFile *item;
	char *found;

	dirs = r_fs_dir (fs, name);
	r_list_foreach (dirs, iter, item) {
		if (r_str_glob (item->name, glob)) {
			found = (char *) malloc (strlen (name) + strlen (item->name) + 2);
			if (!found)
				break;
			strcpy (found, name);
			strcat (found, "/");
			strcat (found, item->name);
			r_list_append (list, found);
		}
		if (!strcmp (item->name, ".") || !strcmp (item->name, ".."))
			continue;
		if (item->type == R_FS_FILE_TYPE_DIRECTORY) {
			found = (char *) malloc (strlen (name) + strlen (item->name) + 2);
			if (!found)
				break;
			strcpy (found, name);
			strcat (found, "/");
			strcat (found, item->name);
			r_fs_find_name_aux (fs, found, glob, list);
			free (found);
		}
	}
}

R_API RList *r_fs_find_name (RFS* fs, const char *name, const char *glob) {
	RList *list = r_list_new ();
	list->free = free;
	r_fs_find_name_aux (fs, name, glob, list);
	return list;
}

R_API RFSFile *r_fs_slurp(RFS* fs, const char *path) {
	RFSFile *file = NULL;
	RFSRoot *root;
	RList * roots = r_fs_root (fs, path);
	RListIter *iter;
	r_list_foreach (roots, iter, root) {
		if (!root || !root->p)
			continue;
		if (root->p->open && root->p->read && root->p->close) {
			file = root->p->open (root, path);
			if (file) root->p->read (file, 0, file->size); //file->data
			else eprintf ("r_fs_slurp: cannot open file\n");
		} else {
			if (root->p->slurp)
				return root->p->slurp (root, path);
			eprintf ("r_fs_slurp: null root->p->slurp\n");
		}
	}
	free (roots);
	return file;
}

// TODO: move into grubfs
#include "p/grub/include/grubfs.h"
RList *list = NULL;
static int parhook (struct grub_disk *disk, struct grub_partition *par, void *closure) {
	RFSPartition *p = r_fs_partition_new (r_list_length (list), par->start*512, 512*par->len);
	p->type = par->msdostype;
	r_list_append (list, p);
	return 0;
}

static RFSPartitionType partitions[] = {
	{ "msdos", &grub_msdos_partition_map },
	{ "apple", &grub_apple_partition_map },
	{ "sun", &grub_sun_partition_map },
	{ "sunpc", &grub_sun_pc_partition_map },
	{ "amiga", &grub_amiga_partition_map },
	{ "bsdlabel", &grub_bsdlabel_partition_map },
	{ "gpt", &grub_gpt_partition_map },
// XXX: In BURG all bsd partition map are in bsdlabel
	//{ "openbsdlabel", &grub_openbsd_partition_map },
	//{ "netbsdlabel", &grub_netbsd_partition_map },
	//{ "acorn", &grub_acorn_partition_map },
	{ NULL }
};

R_API const char *r_fs_partition_type_get (int n) {
	if (n<0 || n>=R_FS_PARTITIONS_LENGTH)
		return NULL;
	return partitions[n].name;
}

R_API int r_fs_partition_get_size () {
	return R_FS_PARTITIONS_LENGTH;
}

R_API RList *r_fs_partitions (RFS *fs, const char *ptype, ut64 delta) {
	int i;
	struct grub_partition_map *gpm = NULL;
	for (i=0; partitions[i].name; i++) {
		if (!strcmp (ptype, partitions[i].name)) {
			gpm = partitions[i].ptr;
			break;
		}
	}
	if (gpm) {
		list = r_list_new ();
		list->free = (RListFree)r_fs_partition_free;
		grubfs_bind_io (NULL, 0);
		struct grub_disk *disk = grubfs_disk (&fs->iob);
		gpm->iterate (disk, parhook, 0);
		return list;
	}
	if (ptype && *ptype)
		eprintf ("Unknown partition type '%s'.\n", ptype);
	eprintf ("Supported types:\n");
	for (i=0; partitions[i].name; i++)
		eprintf (" %s", partitions[i].name);
	eprintf ("\n");
	return NULL;
}

//TODO: Complete!!
R_API const char *r_fs_partition_type (const char *part, int type) {
	switch (type) {
		case GRUB_PC_PARTITION_TYPE_FAT12:
		case GRUB_PC_PARTITION_TYPE_FAT16_GT32M:
		case GRUB_PC_PARTITION_TYPE_FAT16_LT32M:
		case GRUB_PC_PARTITION_TYPE_FAT32:
		case GRUB_PC_PARTITION_TYPE_FAT32_LBA:
		case GRUB_PC_PARTITION_TYPE_FAT16_LBA:
			return strdup ("fat");
		case GRUB_PC_PARTITION_TYPE_EXT2FS:
			return strdup ("ext2");
		case GRUB_PC_PARTITION_TYPE_MINIX:
		case GRUB_PC_PARTITION_TYPE_LINUX_MINIX:
			return strdup ("minix");
		case GRUB_PC_PARTITION_TYPE_NTFS:
			return strdup ("ntfs");
		case GRUB_PC_PARTITION_TYPE_EXTENDED:
		case GRUB_PC_PARTITION_TYPE_LINUX_EXTENDED:
			return strdup ("ext3");
		case GRUB_PC_PARTITION_TYPE_HFS:
			return strdup ("hfs");
		case GRUB_PC_PARTITION_TYPE_WIN95_EXTENDED:
		case GRUB_PC_PARTITION_TYPE_EZD:
		case GRUB_PC_PARTITION_TYPE_VSTAFS:
		case GRUB_PC_PARTITION_TYPE_FREEBSD:
		case GRUB_PC_PARTITION_TYPE_OPENBSD:
		case GRUB_PC_PARTITION_TYPE_NETBSD:
		case GRUB_PC_PARTITION_TYPE_GPT_DISK:
		case GRUB_PC_PARTITION_TYPE_LINUX_RAID:
		case GRUB_PC_PARTITION_TYPE_NONE:
		default:
			return NULL;
	}
}

R_API int r_fs_prompt (RFS *fs, char *root) {
	char buf[1024];
	char path[1024];
	char str[2048];
	char *input;
	RList *list;
	RListIter *iter;
	RFSFile *file;

	if (root && *root) {
		r_str_chop_path (root);
		list = r_fs_root (fs, root);
		if (r_list_empty (list)) {
			printf ("Unknown root\n");
			return R_FALSE;
		}
		strncpy (path, root, sizeof (path)-1);
	} else strcpy (path, "/");

	for (;;) {
		printf ("[%s]> ", path);
		fflush (stdout);
		fgets (buf, sizeof (buf)-1, stdin);
		if (feof (stdin)) break;
		buf[strlen (buf)-1] = '\0';
		if (!strcmp (buf, "q") || !strcmp (buf, "exit"))
			return R_TRUE;
		if (buf[0]=='!') {
			system (buf+1);
		} else
		if (!memcmp (buf, "ls", 2)) {
			if (buf[2]==' ') {
				if (buf[3] != '/') {
					strncpy (str, path, sizeof (str)-1);
					strcat (str, "/");
					strncat (str, buf+3, sizeof (buf)-1);
					list = r_fs_dir (fs, str);
				} else
					list = r_fs_dir (fs, buf+3);
			} else list = r_fs_dir (fs, path);
			if (list) {
				r_list_foreach (list, iter, file)
					printf ("%c %s\n", file->type, file->name);
				r_list_free (list);
			} else eprintf ("Unknown path: %s\n", path);
		} else if (!strncmp (buf, "pwd", 3)) {
			eprintf ("%s\n", path);
		} else if (!memcmp (buf, "cd ", 3)) {
			char opath[4096];
			strncpy (opath, path, sizeof (opath)-1);
			input = buf+3;
			while (*input == ' ')
				input++;
			if (!strcmp (input, "..")) {
				char *p = r_str_lchr (path, '/');
				if (p) p[(p==path)?1:0]=0;
			} else {
				strcat (path, "/");
				if (*input=='/')
					strcpy (path, input);
				else strcat (path, input);
			}
			r_str_chop_path (path);
			list = r_fs_dir (fs, path);
			if (r_list_empty (list)) {
				strcpy (path, opath);
				eprintf ("cd: unknown path: %s\n", path);
			} else r_list_free (list);
		} else if (!memcmp (buf, "cat ", 4)) {
			input = buf+3;
			while (input[0] == ' ')
				input++;
			if (input[0] == '/')
				strncpy (str, root, sizeof (str)-1);
			else strncpy (str, path, sizeof (str)-1);
			strcat (str, "/");
			strcat (str, input);
			file = r_fs_open (fs, str);
			if (file) {
				r_fs_read (fs, file, 0, file->size);
				write (1, file->data, file->size);
				free (file->data);
				r_fs_close (fs, file);
			} else eprintf ("Cannot open file\n");
		} else if (!memcmp (buf, "mount", 5)) {
			RFSRoot *root;
			r_list_foreach (fs->roots, iter, root) {
				eprintf ("%s %s\n", root->path, root->p->name);
			}
		} else if (!memcmp (buf, "get ", 4)) {
			input = buf+3;
			while (input[0] == ' ')
				input++;
			if (input[0] == '/')
				strncpy (str, root, sizeof (str)-1);
			else strncpy (str, path, sizeof (str)-1);
			strcat (str, "/");
			strcat (str, input);
			file = r_fs_open (fs, str);
			if (file) {
				r_fs_read (fs, file, 0, file->size);
				r_file_dump (input, file->data, file->size);
				free (file->data);
				r_fs_close (fs, file);
			} else {
				input -= 2; //OMFG!!!! O_O
				memcpy (input, "./", 2);
				if (!r_fs_dir_dump (fs, str, input))
					printf ("Cannot open file\n");
			}
		} else if (!memcmp (buf, "help", 4) || !strcmp (buf, "?")) {
			eprintf (
			"Commands:\n"
			" !cmd        ; escape to system\n"
			" ls [path]   ; list current directory\n"
			" cd path     ; change current directory\n"
			" cat file    ; print contents of file\n"
			" get file    ; dump file to disk\n"
			" mount       ; list mount points\n"
			" q/exit      ; leave prompt mode\n"
			" ?/help      ; show this help\n"
			);
		} else eprintf ("Unknown command %s\n", buf);
	}
	clearerr (stdin);
	printf ("\n");
	return R_TRUE;
}

R_API void r_fs_view (RFS *fs, int view) {
	fs->view = view;
}
