#include <_ansi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <kos/fs.h>
#include <arch/dbgio.h>
#include <arch/arch.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
/* int errno; */

/* This is used by _sbrk.  */
register char *stack_ptr asm ("r15");

/*
   kos return 0 when err,
   newlib fd return -1 when err
   newlib fd must >=0 and short
*/

#define	MAX_OPEN	64
static file_t fh[MAX_OPEN];

#define	CHECKFILE(file)	if ((unsigned)file<3) return -1
#define	FD2KOS(fd)	fh[fd]

int
_read (int file,
       char *ptr,
       int len)
{
  CHECKFILE(file);
  return fs_read(FD2KOS(file),ptr,len);
}

int
_lseek (int file,
	int ptr,
	int dir)
{
  CHECKFILE(file);
//  dbgio_printf("seek %x,%d,%d\n",FD2KOS(file),ptr,dir);
  return fs_seek(FD2KOS(file),ptr,dir);
}

int
_write ( int file,
	 const char *ptr,
	 int len)
{
  switch(file) {
  case 0: /* stdin */
    return -1;
  case 1: /* stdout */
  case 2: /* stderr */
    dbgio_writek(ptr,len);
    return len;
  default:
    return fs_write(FD2KOS(file),ptr,len);
  }
}

int
_close (int file)
{
  CHECKFILE(file);
  fs_close(FD2KOS(file));
  fh[file]=0;
  return 0;
}

int
_open (const char *path,
	int flags)
{
  file_t f;
  int fd;

//  dbgio_printf(path);

  for(fd=3;fd<MAX_OPEN && fh[fd];fd++) ;
  if (fd==MAX_OPEN) {
    return -1;
  }
  f = fs_open(path,flags);
  if (f==0) {
//	dbgio_printf("\n");
  	return -1;
  }
//  dbgio_printf(":%x\n",f);
  fh[fd] = f;
  return fd;
}

int
_fstat (int file,
	struct stat *st)
{
  int size;
  CHECKFILE(file);
  size = fs_total(FD2KOS(file));
  if (size==-1) {
  	dbglog(0,"fstat:file %d\n",file);
  	return -1;
  }

  memset(st,0,sizeof(*st));
  st->st_mode = S_IFREG;
  st->st_size = size;
  return 0;
}

int
_creat (const char *path,
	int mode)
{
  return _open(path,O_WRONLY|O_TRUNC /*|O_CREAT*/);
}

int
_rename (const char *oldpath,const char *newpath)
{
  return fs_rename(oldpath,newpath);
}

int
rename (const char *oldpath,const char *newpath)
{
  return fs_rename(oldpath,newpath);
}

int
_unlink (const char *path)
{
  return fs_unlink(path);
}

int chdir(const char *path)
{
  return fs_chdir(path);
}

_exit (int n)
{
  arch_exit();
}

#include <sys/dirent.h>

DIR* opendir(const char *path)
{
	return (DIR*)fs_open(path,O_DIR);
}

struct dirent *readdir (DIR *dir)
{
	return (struct dirent*)fs_readdir((file_t)dir);
}

int closedir(DIR *dir)
{
	fs_close((file_t)dir);
	return 0;
}

#define	NEWHEAP	(char*)(0x85000000+((320*240*2+4095)&~4095))
#define	NEWHEAP_END	((char*)0x85800000)
/* cachable address */

caddr_t
_sbrk (int incr)
{
  extern char end;		/* Defined by the linker */
  static char *heap_end;
  static int total;
  char *prev_heap_end;
  static char *ram_heap_end;

  total += incr;
  if (heap_end == 0)
    {
      heap_end = &end;
    }
  prev_heap_end = heap_end;
  dbgio_printf("sbrk:%d\n",incr);
  if (!ram_heap_end) {
    if ( heap_end + incr > stack_ptr)
    {
      _write (1, "Heap and stack collision\n", 25);
	dbgio_printf("current %p stack %p rest %d\n",heap_end,stack_ptr,stack_ptr-heap_end);
//      abort ();
      /* use vram */
      dbgio_printf("use vram as heap\n");
      ram_heap_end = heap_end;
      prev_heap_end = heap_end = NEWHEAP;
    }
  } else {
    if (incr<0 && (heap_end + incr <= NEWHEAP)) {
        dbgio_printf("back to mem\n");
        incr+=heap_end-NEWHEAP;
        heap_end = ram_heap_end;
        ram_heap_end = 0;
    }

    else if ( heap_end + incr > NEWHEAP_END)
    {
      _write (1, "Heap exhausted          \n", 25);
	dbgio_printf("current %p rest %d\n",heap_end,NEWHEAP_END-heap_end);
        abort ();
    }
  
  }
  heap_end += incr;
  dbgio_printf("heap:%x end:%x: total:%d\n",prev_heap_end,heap_end,total);
  return (caddr_t) prev_heap_end;
}

caddr_t sbrk(int incr) { return _sbrk(incr); }
int mm_init() {return 0;}

/*
void * _malloc_r(void *reent,size_t sz) { return malloc(sz); }
void  _free_r(void *reent,void *p) { free(p); }
void * _calloc_r(void *reent,size_t sz,size_t sz2) { return calloc(sz,sz2); }
void * _realloc_r(void *reent,void *p,size_t sz) { return realloc(p,sz); }
*/

int
_link (char *old, char *new)
{
  return -1;
}

int
isatty (int fd)
{
//  if ((unsigned)fd<3) return 1;
  return 0;
}

_kill (n, m)
{
  return -1;
}

_getpid (n)
{
  return 1;
}

_raise ()
{
}

/*
int
_stat (const char *path, struct stat *st)

{
  return __trap34 (SYS_stat, path, st, 0);
}
*/

int
_chmod (const char *path, short mode)
{
  return -1;
}

int
_chown (const char *path, short owner, short group)
{
  return -1;
}

int
_utime (path, times)
     const char *path;
     char *times;
{
  return -1;
}

int
_fork ()
{
  return -1;
}

int
_wait (statusp)
     int *statusp;
{
  return -1;
}

int
_execve (const char *path, char *const argv[], char *const envp[])
{
  return -1;
}

int
_execv (const char *path, char *const argv[])
{
  return -1;
}

int
_pipe (int *fd)
{
  return -1;
}

#if 0
/* This is only provided because _gettimeofday_r and _times_r are
   defined in the same module, so we avoid a link error.  */
clock_t
_times (struct tms *tp)
{
  return -1;
}

int
_gettimeofday (struct timeval *tv, struct timezone *tz)
{
  tv->tv_usec = 0;
  tv->tv_sec = __trap34 (SYS_time);
  return 0;
}

static inline int
__setup_argv_for_main (int argc)
{
  char **argv;
  int i = argc;

  argv = __builtin_alloca ((1 + argc) * sizeof (*argv));

  argv[i] = NULL;
  while (i--) {
    argv[i] = __builtin_alloca (1 + __trap34 (SYS_argnlen, i));
    __trap34 (SYS_argn, i, argv[i]);
  }

  return main (argc, argv);
}

int
__setup_argv_and_call_main ()
{
  int argc = __trap34 (SYS_argc);

  if (argc <= 0)
    return main (argc, NULL);
  else
    return __setup_argv_for_main (argc);
}

#endif
