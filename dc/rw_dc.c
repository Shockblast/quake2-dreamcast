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
#include "../ref_soft/r_local.h"
#include "../client/keys.h"
#include "../linux/rw_linux.h"

#include <dc/sq.h>

static short palette[256];
#define	RGB565(r,g,b)	( (((r)>>3)<<11) |(((g)>>2)<<5)|((b)>>3) )

static void sq_update(unsigned short *dest,unsigned char *s,unsigned short *pal,int n)
{
	unsigned int *d = (unsigned int *)(void *)
		(0xe0000000 | (((unsigned long)dest) & 0x03ffffe0));
    
	/* Set store queue memory area as desired */
	QACR0 = ((((unsigned int)dest)>>26)<<2)&0x1c;
	QACR1 = ((((unsigned int)dest)>>26)<<2)&0x1c;
    
	/* fill/write queues as many times necessary */
	n>>=5;
	while(n--) {
		int v;
#if 1
		v = pal[*s++];
		v|= pal[*s++]<<16;
		d[0] = v;
		v = pal[*s++];
		v|= pal[*s++]<<16;
		d[1] = v;
		v = pal[*s++];
		v|= pal[*s++]<<16;
		d[2] = v;
		v = pal[*s++];
		v|= pal[*s++]<<16;
		d[3] = v;
		v = pal[*s++];
		v|= pal[*s++]<<16;
		d[4] = v;
		v = pal[*s++];
		v|= pal[*s++]<<16;
		d[5] = v;
		v = pal[*s++];
		v|= pal[*s++]<<16;
		d[6] = v;
		v = pal[*s++];
		v|= pal[*s++]<<16;
		d[7] = v;
#else
		v = *s++;
		d[0] = pal[v&255] | (pal[v>>8]<<16);
		v = *s++;
		d[1] = pal[v&255] | (pal[v>>8]<<16);
		v = *s++;
		d[2] = pal[v&255] | (pal[v>>8]<<16);
		v = *s++;
		d[3] = pal[v&255] | (pal[v>>8]<<16);
		v = *s++;
		d[4] = pal[v&255] | (pal[v>>8]<<16);
		v = *s++;
		d[5] = pal[v&255] | (pal[v>>8]<<16);
		v = *s++;
		d[6] = pal[v&255] | (pal[v>>8]<<16);
		v = *s++;
		d[7] = pal[v&255] | (pal[v>>8]<<16);
#endif
		asm("pref @%0" : : "r" (d));
		d += 8;
	}
	/* Wait for both store queues to complete */
	d = (unsigned int *)0xe0000000;
	d[0] = d[8] = 0;
}

#define PM_RGB555	0
#define PM_RGB565	1
#define PM_RGB888	3
#define DM_320x240	1

static qboolean SWimp_InitGraphics( qboolean fullscreen )
{
	SWimp_Shutdown();

	if (vid.width!=320 || vid.height!=240) {
		ri.Con_Printf (PRINT_ALL, "Mode %d %d not found\n", vid.width, vid.height);
		return false; // mode not found
	}

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow (vid.width, vid.height);

//	ri.Con_Printf (PRINT_ALL, "Setting VGAMode: %d\n", current_mode );

//	Cvar_SetValue ("vid_mode", (float)modenum);
	

// get goin'

	vid_set_mode(DM_320x240, PM_RGB565);

	vid.rowbytes = vid.width;
	vid.buffer = malloc(vid.width*vid.height);
	if (!vid.buffer)
		Sys_Error("Unabled to alloc vid.buffer!\n");

	return true;
}

void		SWimp_BeginFrame( float camera_separation )
{
}

extern unsigned short *vram_s;

void		SWimp_EndFrame (void)
{
	if (vid.buffer)
		sq_update(vram_s,vid.buffer,palette,vid.rowbytes*vid.height*2);
}

int			SWimp_Init( void *hInstance, void *wndProc )
{
	return	true;
}

void		SWimp_SetPalette( const unsigned char *pal)
{
	int i;
	for(i=0;i<256;i++) {
		palette[i] = RGB565(pal[0],pal[1],pal[2]);
		pal+=4;
	}
}

void		SWimp_Shutdown( void )
{
	if (vid.buffer) {
		free(vid.buffer);
		vid.buffer = NULL;
	}
}

rserr_t		SWimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	ri.Con_Printf (PRINT_ALL, "setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( pwidth, pheight, mode ) )
	{
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	ri.Con_Printf( PRINT_ALL, " %d %d\n", *pwidth, *pheight);

	if ( !SWimp_InitGraphics( false ) ) {
		// failed to set a valid mode in windowed mode
		return rserr_invalid_mode;
	}

	R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_8to24table );

	return rserr_ok;
}

void		SWimp_AppActivate( qboolean active )
{
}
