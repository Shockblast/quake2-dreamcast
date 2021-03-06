/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
//#include <sys/ipc.h>
//#include <sys/shm.h>
//#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
//#include <sys/wait.h>
//#include <sys/mman.h>
#include <errno.h>
//#include <mntent.h>

#include <dlfcn.h>

#include "../qcommon/qcommon.h"

#include "../linux/rw_linux.h"

cvar_t *nostdout;

unsigned	sys_frame_time;

uid_t saved_euid;
qboolean stdin_active = true;

// =======================================================================
// General routines
// =======================================================================

void Sys_ConsoleOutput (char *string)
{
	if (nostdout && nostdout->value)
		return;

	fputs(string, stdout);
}

void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];
	unsigned char		*p;

	va_start (argptr,fmt);
	vsprintf (text,fmt,argptr);
	va_end (argptr);

	if (strlen(text) > sizeof(text))
		Sys_Error("memory overwrite in Sys_Printf");

    if (nostdout && nostdout->value)
        return;

	for (p = (unsigned char *)text; *p; p++) {
		*p &= 0x7f;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
	}
}

void Sys_Quit (void)
{
	CL_Shutdown ();
	Qcommon_Shutdown ();
//    fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	_exit(0);
}

void Sys_Init(void)
{
#if id386
//	Sys_SetFPCW();
#endif
}

void Sys_Error (char *error, ...)
{ 
    va_list     argptr;
    char        string[1024];

// change stdin to non blocking
//    fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

	CL_Shutdown ();
	Qcommon_Shutdown ();
    
    va_start (argptr,error);
    vsprintf (string,error,argptr);
    va_end (argptr);
	fprintf(stderr, "Error: %s\n", string);

	_exit (1);

} 

void Sys_Warn (char *warning, ...)
{ 
    va_list     argptr;
    char        string[1024];
    
    va_start (argptr,warning);
    vsprintf (string,warning,argptr);
    va_end (argptr);
	fprintf(stderr, "Warning: %s", string);
} 

/*
============
Sys_FileTime

returns -1 if not present
============
*/
int	Sys_FileTime (char *path)
{
#if 1
	int fd;
	if ((fd = _open(path,O_RDONLY))==-1) return -1;
	_close(fd);
	return 1;
#else
	struct	stat	buf;
	
	if (stat (path,&buf) == -1)
		return -1;
	
	return buf.st_mtime;
#endif
}

void floating_point_exception_handler(int whatever)
{
//	Sys_Warn("floating point exception\n");
//	signal(SIGFPE, floating_point_exception_handler);
}

char *Sys_ConsoleInput(void)
{
#if 1
	return NULL;
#else
    static char text[256];
    int     len;
	fd_set	fdset;
    struct timeval timeout;

	if (!dedicated || !dedicated->value)
		return NULL;

	if (!stdin_active)
		return NULL;

	FD_ZERO(&fdset);
	FD_SET(0, &fdset); // stdin
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select (1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &fdset))
		return NULL;

	len = read (0, text, sizeof(text));
	if (len == 0) { // eof!
		stdin_active = false;
		return NULL;
	}

	if (len < 1)
		return NULL;
	text[len-1] = 0;    // rip off the /n and terminate

	return text;
#endif
}

/*****************************************************************************/

#define	GAME_HARD_LINKED

static void *game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
#ifndef GAME_HARD_LINKED
	if (game_library) 
		dlclose (game_library);
#endif
	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (void *parms)
{
#ifndef GAME_HARD_LINKED
	void	*(*GetGameAPI) (void *);
#endif

	char	name[MAX_OSPATH];
	char	curpath[MAX_OSPATH];
	char	*path;
#ifdef __i386__
	const char *gamename = "gamei386.so";
#elif defined __alpha__
	const char *gamename = "gameaxp.so";
#elif defined __sh__
	const char *gamename = "gamesh.so";
#else
#error Unknown arch
#endif

//	setreuid(getuid(), getuid());
//	setegid(getgid());

	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

//	getcwd(curpath, sizeof(curpath));
//	strcpy(curpath,"/pc");

#ifndef GAME_HARD_LINKED
	Com_Printf("------- Loading %s -------\n", gamename);

	// now run through the search paths
	path = NULL;
	while (1)
	{
		path = FS_NextPath (path);
		if (!path)
			return NULL;		// couldn't find one anywhere
		sprintf (name, "%s/%s", path, gamename);
		printf("load:%s\n",name);
		game_library = dlopen (name, RTLD_LAZY );
		if (game_library)
		{
			Com_Printf ("LoadLibrary (%s)\n",name);
			break;
		}
	}

	GetGameAPI = (void *)dlsym (game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame ();		
		return NULL;
	}
#endif

	return GetGameAPI (parms);
}

/*****************************************************************************/

void Sys_AppActivate (void)
{
}

void Sys_SendKeyEvents (void)
{
#ifndef DEDICATED_ONLY
	KBD_Update();
#endif

	// grab frame time 
	sys_frame_time = Sys_Milliseconds();
}

/*****************************************************************************/

char *Sys_GetClipboardData(void)
{
	return NULL;
}



#define	_arch_dreamcast
#include <kos.h>

const char *FS_Tmpdir()
{
	return "/mem";
}

const char *FS_Savedir()
{
	static char savedir[]="/vmu/a1";

	int port,unit;
	uint8 addr;
	addr = maple_first_vmu();
	if (addr==0) return "dmy";
	maple_raddr(addr,&port,&unit);
	savedir[5] = 'a' + port;
	savedir[6] = '0' + unit;

	return savedir;
}

int qmain (int argc, char **argv)
{
	int 	time, oldtime, newtime;

	const static char *basedirs[]={
	"/cd/Quake2", /* installed */
	"/cd/Q2Demo", /* demo version */
	"/cd/install/data", /* official CD-ROM */
	"/pc/quake2", /* debug */
	NULL
	};

	char *basedir;
#if 0
	int i;
	
	for(i=0;(basedir = basedirs[i])!=NULL;i++) {
		int fd  = fs_open(basedir,O_DIR);
		if (fd!=0) {
			fs_close(fd);
			break;
		}
	}
	if (basedir==NULL)
		Sys_Error("can't find quake dir");
#else
	{
	static char *args[10] = {"quake2",NULL};
	argc = 1;
	argv = args;
	basedir = menu(&argc,argv,basedirs);
	}
#endif
#if 0
	struct ip_addr ipaddr, gw, netmask;

	/* Change these for your network */
	IP4_ADDR(&ipaddr, 192,168,0,4);
	IP4_ADDR(&netmask, 255,255,255,0);
	IP4_ADDR(&gw, 192,168,0,1);
	lwip_init_static(&ipaddr, &netmask, &gw);
#endif

	fs_mem_init();

//	static char *args[] = {"quake2","-basedir","/pc/quake2",NULL,NULL};
//	argv = args;
//	argc = 4;
//	extern cvar_t	*fs_basedir;

	// go back to real user for config loads
//	saved_euid = geteuid();
//	seteuid(getuid());
	Cvar_Set("basedir",basedir);

	Qcommon_Init(argc, argv);

//	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	nostdout = Cvar_Get("nostdout", "0", 0);
	if (!nostdout->value) {
//		fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
//		printf ("Linux Quake -- Version %0.3f\n", LINUX_VERSION);
	}

//	printf("basedir=%s\n",fs_basedir->string);

    oldtime = Sys_Milliseconds ();
    while (1)
    {
// find time spent rendering last frame
		do {
			newtime = Sys_Milliseconds ();
			time = newtime - oldtime;
		} while (time < 1);
        Qcommon_Frame (time);
		oldtime = newtime;
    }

}

void Sys_CopyProtect(void)
{
}

int main(int argc,char **argv)
{
//	char *args[] = {"quake2","+set","game","lunar","+map","drain",NULL};
//	argc  = 6;
	return qmain(argc,argv);
}
