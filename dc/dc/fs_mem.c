/*

	fs_mem.c
	(c)2002 BERO

	under KOS licence(new BSD)
	based on fs_builtin.c
 	(c)2000-2001 Dan Potter

*/

/*

This is a simple VFS handler that implements an in-kernel file system. 
using realloc/free

*/

#include <stdlib.h>	/* for realloc/free */
#include <string.h>
#include <kos/fs.h>
#include <kos/thread.h>
#include <arch/spinlock.h>

CVSID("$Id: fs_builtin.c,v 1.2 2002/01/06 01:14:48 bardtx Exp $");

/* Static file table -- this should be set exactly once by the main()
   of the linking program. It can only be called once because changing
   tables mid-stream would break open files. */

/* Internal file structure; you can have eight files open at once on the
   built-in file system (for simplicity and to reduce malloc'ing). */
#define MAX_MEM_FILES	16

typedef struct {
	int	tblidx;		/* Table entry */
	int	mode;
	uint8	*data;		/* Quick access */
	size_t	size;		/* Ditto */
	off_t	ptr;		/* Offset ptr */
	size_t	alloced;	/* for write */
} mem_fd_t;
static mem_fd_t fh[MAX_MEM_FILES];

#define	MAX_NAMELEN	32
typedef struct {
	char	name[MAX_NAMELEN];
	uint8	*data;
	size_t	size;
} mem_ent_t;

#define	MAX_ENTRIES	32
static	mem_ent_t tbl[MAX_ENTRIES];
#define	tblcnt	MAX_ENTRIES

typedef struct {
	int	idx;
	dirent_t dirent;
} dir_t;
static dir_t dirfd;

/* FD access mutex */
static spinlock_t mutex;

#define	DPRINTF(fmt,args...)	dbglog(0,fmt,##args)

int search_file(const char *fn)
{
	int i;
	for(i=0;i<tblcnt && stricmp(tbl[i].name,fn)!=0;i++) ;
	if (i==tblcnt) return -1;
	return i;
}

/* Standard file ops */
uint32 mem_open(const char *fn, int mode) {
	int i, tblidx;

	DPRINTF("mem_open:%s %x\n",fn,mode);

	if (mode & O_DIR) {
		dirfd.idx = 0;
		return (uint32)&dirfd;
	}

	if (fn[0]=='/') fn++;
	/* Find the file entry (or not) */
	tblidx = search_file(fn);

	if (tblidx==-1) {
		if ((mode&O_MODE_MASK)==O_RDONLY) return 0;
		else {
			DPRINTF("new file\n");
			/* create new entry */
			for(i=0;i<tblcnt && tbl[i].name[0]!=0 ;i++) ;
			if (i==tblcnt) return 0;
			tblidx = i;
			tbl[i].size = 0;
			tbl[i].data = NULL;
			strcpy(tbl[i].name,fn);
		}
	}

	/* Allocate an FD for it */
	spinlock_lock(&mutex);
	for (i=0; i<MAX_MEM_FILES && fh[i].tblidx!=-1; i++) ;
	spinlock_unlock(&mutex);
	if (i >= MAX_MEM_FILES) return 0;

	fh[i].tblidx = tblidx;
	fh[i].mode = mode;
	fh[i].data = tbl[tblidx].data;
	fh[i].size = tbl[tblidx].size;
	fh[i].alloced = tbl[tblidx].size;
	fh[i].ptr = (mode&O_APPEND)?fh[i].size:0;

	DPRINTF("%x\n",i+1);
	return i+1;
}

void mem_close(uint32 hnd) {
	DPRINTF("mem_close:%x\n",hnd);

	if (hnd == 0 || hnd==(uint32)&dirfd) return;
	hnd--;
	if ((fh[hnd].mode&O_MODE_MASK)==O_WRONLY || (fh[hnd].mode&O_MODE_MASK)==O_RDWR) {
		int tblidx = fh[hnd].tblidx;
		tbl[tblidx].data = fh[hnd].data;
		tbl[tblidx].size = fh[hnd].size;
	}

	fh[hnd].tblidx = -1;
}

ssize_t mem_read(uint32 hnd, void *buf, size_t cnt) {
	DPRINTF("mem_read:%x %p %d\n",hnd,buf,cnt);
	if (hnd == 0 || fh[hnd-1].tblidx == -1) return -1;
	hnd--;

	if ((fh[hnd].ptr + cnt) > fh[hnd].size)
		cnt = fh[hnd].size - fh[hnd].ptr;
	if (cnt > 0)
		memcpy(buf, fh[hnd].data + fh[hnd].ptr, cnt);
	fh[hnd].ptr += cnt;
	return cnt;
}

#define	BLOCKSIZE	2048

ssize_t mem_write(uint32 hnd, const void *buf, size_t cnt) {
	DPRINTF("mem_write:%x %p %d\n",hnd,buf,cnt);
	if (hnd == 0 || fh[hnd-1].tblidx == -1) return -1;
	hnd--;

	if ((fh[hnd].ptr + cnt) > fh[hnd].size) {
		int alloc_size;
		fh[hnd].size = fh[hnd].ptr + cnt;
		alloc_size = (fh[hnd].size + BLOCKSIZE-1)&~(BLOCKSIZE-1);
		if (alloc_size>fh[hnd].alloced) {
			DPRINTF("realloc:%p,%d\n",fh[hnd].data,alloc_size);
			fh[hnd].alloced = alloc_size;
			fh[hnd].data = realloc(fh[hnd].data,alloc_size);
		}
	}
	DPRINTF("%p %d\n",fh[hnd].data , fh[hnd].ptr);
	if (cnt > 0)
		memcpy(fh[hnd].data + fh[hnd].ptr, buf, cnt);
	fh[hnd].ptr += cnt;
	DPRINTF("ok %d\n",cnt);
	return cnt;
}

off_t mem_seek(uint32 hnd, off_t offset, int whence) {
	DPRINTF("mem_seek:%x %d %d\n",hnd,offset,whence);
	if (hnd == 0 || fh[hnd-1].tblidx == -1) return -1;
	hnd--;

	switch(whence) {
	case SEEK_SET: break;
	case SEEK_CUR: offset += fh[hnd].ptr; break;
	case SEEK_END: offset = fh[hnd].size - offset; break;
	default: return -1;
	}

	if (offset < 0)
		offset = 0;
	if (offset >= fh[hnd].size)
		offset = fh[hnd].size;
	fh[hnd].ptr = offset;

	return fh[hnd].ptr;
}

off_t mem_tell(uint32 hnd) {
	if (hnd == 0 || fh[hnd-1].tblidx == -1) return -1;
	return fh[hnd-1].ptr;
}

size_t mem_total(uint32 hnd) {
	if (hnd == 0 || fh[hnd-1].tblidx == -1) return -1;
	return fh[hnd-1].size;
}

void *mem_mmap(uint32 hnd) {
	if (hnd == 0 || fh[hnd-1].tblidx == -1) return NULL;
	return fh[hnd-1].data;
}

dirent_t *mem_readdir(uint32 hnd)
{
	dir_t *dir = (dir_t*)hnd;
	int i;
	DPRINTF("mem_readdir:%x\n",hnd);

	for(i=dir->idx;i<tblcnt && tbl[i].name[0]==0;i++) ;
	dir->idx = i+1;
	if (i>=tblcnt) return NULL;

	dir->dirent.size = tbl[i].size;
	dir->dirent.time = 0;
	dir->dirent.attr = 0;
	strcpy(dir->dirent.name,tbl[i].name);

	DPRINTF("%s %d\n",dir->dirent.name,dir->dirent.size);

	return &dir->dirent;
}

int mem_rename(const char *fn,const char *fn2)
{
	int i = search_file(fn);
	if (i==-1) return -1;
	strcpy(tbl[i].name,fn2);
	return 0;
}

int mem_unlink(const char *fn)
{
	DPRINTF("mem_unlink:%s\n",fn);
	int i;
	i = search_file(fn);
	if (i==-1) return -1;
	tbl[i].name[0] = 0;
	if (tbl[i].data) {
		free(tbl[i].data);
		printf("free\n");
	}
	return 0;
}


/* Might implement this eventually */
#if 0
static dirent_t *mem_readdir(uint32 hnd) {
}
#endif

/* Pull all that together */
static vfs_handler vh = {
	{ 0 },			/* Prefix */
	0, 0, NULL,		/* In-kernel, no cacheing, next */
	
	mem_open,
	mem_close,
	mem_read,
	mem_write,
	mem_seek,
	mem_tell,
	mem_total,
	mem_readdir,		/* readdir */
	NULL,			/* ioctl */
	mem_rename,		/* rename */
	mem_unlink,			/* unlink */
	mem_mmap
};

/* This is called before any main() gets to take a whack */
int fs_mem_init() {
	int i;

	/* Start with no table */

	/* Clear open files list */
	for (i=0; i<MAX_MEM_FILES; i++)
		fh[i].tblidx = -1;

	/* Init thread mutex */
	spinlock_init(&mutex);

	/* Register with VFS */
	return fs_handler_add("/mem", &vh);
}

int fs_mem_shutdown() {
	return fs_handler_remove(&vh);
}

