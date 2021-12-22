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
// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "../client/client.h"
#include "../client/snd_loc.h"

#define	PAINTBUFFER_SIZE	512 //2048
portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];

#define	NO_TABLE

#ifndef	NO_TABLE
int		snd_scaletable[32][256];
#endif

int	snd_vol;

void S_WriteLinearBlastStereo16 (void);

#if !(defined __linux__ && defined __i386__)
#if	!id386

#if 1 /* DC */

#define G2_LOCK(OLD) \
	do { \
		if (!irq_inside_int()) \
			OLD = irq_disable(); \
		/* suspend any G2 DMA here... */ \
		while((*(volatile unsigned int *)0xa05f688c) & 0x20) \
			; \
	} while(0)

#define G2_UNLOCK(OLD) \
	do { \
		/* resume any G2 DMA here... */ \
		if (!irq_inside_int()) \
			irq_restore(OLD); \
	} while(0)

void S_WriteLinearBlastStereo16_separate (short *_snd_l,short *_snd_r,int *snd_p,unsigned int nsamples,int snd_vol)
{
	int		i;
	int		val;
	int	old;

	unsigned long *snd_l = (unsigned long*)_snd_l;
	unsigned long *snd_r = (unsigned long*)_snd_r;

	for (i=0 ; i<nsamples/2 ; i++)
	{
		int	lval,rval;
		G2_LOCK(old);
		if (val > 0x7fff)
			val = 0x7fff;
		else if (val < (short)0x8000)
			val = (short)0x8000;

		lval = val;

		val = (*snd_p++ *snd_vol)>>8;
		if (val > 0x7fff)
			val = 0x7fff;
		else if (val < (short)0x8000)
			val = (short)0x8000;

		rval = val;

		val = (*snd_p++ *snd_vol)>>8;
		if (val > 0x7fff)
			val = 0x7fff;
		else if (val < (short)0x8000)
			val = (short)0x8000;

		snd_l[i] = lval | (val<<16);

		val = (*snd_p++ *snd_vol)>>8;
		if (val > 0x7fff)
			val = 0x7fff;
		else if (val < (short)0x8000)
			val = (short)0x8000;

		snd_r[i] = rval | (val<<16);
		G2_UNLOCK(old);
		if ((i&3)==0) g2_fifo_wait();
	}
	g2_fifo_wait();
}

#else
void S_WriteLinearBlastStereo16 (void)
{
	int		i;
	int		val;

	for (i=0 ; i<snd_linear_count ; i+=2)
	{
		val = snd_p[i]>>8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i] = (short)0x8000;
		else
			snd_out[i] = val;

		val = snd_p[i+1]>>8;
		if (val > 0x7fff)
			snd_out[i+1] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i+1] = (short)0x8000;
		else
			snd_out[i+1] = val;
	}
}
#endif

#else
__declspec( naked ) void S_WriteLinearBlastStereo16 (void)
{
	__asm {

 push edi
 push ebx
 mov ecx,ds:dword ptr[snd_linear_count]
 mov ebx,ds:dword ptr[snd_p]
 mov edi,ds:dword ptr[snd_out]
LWLBLoopTop:
 mov eax,ds:dword ptr[-8+ebx+ecx*4]
 sar eax,8
 cmp eax,07FFFh
 jg LClampHigh
 cmp eax,0FFFF8000h
 jnl LClampDone
 mov eax,0FFFF8000h
 jmp LClampDone
LClampHigh:
 mov eax,07FFFh
LClampDone:
 mov edx,ds:dword ptr[-4+ebx+ecx*4]
 sar edx,8
 cmp edx,07FFFh
 jg LClampHigh2
 cmp edx,0FFFF8000h
 jnl LClampDone2
 mov edx,0FFFF8000h
 jmp LClampDone2
LClampHigh2:
 mov edx,07FFFh
LClampDone2:
 shl edx,16
 and eax,0FFFFh
 or edx,eax
 mov ds:dword ptr[-4+edi+ecx*2],edx
 sub ecx,2
 jnz LWLBLoopTop
 pop ebx
 pop edi
 ret
	}
}

#endif
#endif

void S_TransferStereo16 (unsigned long *pbuf, int endtime)
{
	int		lpos;
	int		lpaintedtime;

	int 	*snd_p, snd_linear_count;
	short	*snd_out;
	int	samples	= dma.samples>>1;
	
	snd_p = (int *) paintbuffer;
	lpaintedtime = paintedtime;

	while (lpaintedtime < endtime)
	{
	// handle recirculating buffer issues
		lpos = lpaintedtime & (samples-1);

		snd_out = (short *) pbuf + lpos;

		snd_linear_count = samples - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

	// write a linear blast of samples
		S_WriteLinearBlastStereo16_separate (snd_out,snd_out+samples,snd_p,snd_linear_count,snd_vol);

		snd_p += snd_linear_count*2;
		lpaintedtime += snd_linear_count;
	}
}

/*
===================
S_TransferPaintBuffer

===================
*/
void S_TransferPaintBuffer(int endtime)
{
	int 	out_idx;
	int 	count;
	int 	out_mask;
	int 	*p;
	int 	step;
	int		val;
	unsigned long *pbuf;

	pbuf = (unsigned long *)dma.buffer;

	if (s_testsound->value)
	{
		int		i;
		int		count;

		// write a fixed sine wave
		count = (endtime - paintedtime);
		for (i=0 ; i<count ; i++)
			paintbuffer[i].left = paintbuffer[i].right = sin((paintedtime+i)*0.1)*20000*256;
	}


	if (dma.samplebits == 16 && dma.channels == 2)
	{	// optimized case
		S_TransferStereo16 (pbuf, endtime);
	}
#if 0
	else
	{	// general case
		p = (int *) paintbuffer;
		count = (endtime - paintedtime) * dma.channels;
		out_mask = dma.samples - 1; 
		out_idx = paintedtime * dma.channels & out_mask;
		step = 3 - dma.channels;

		if (dma.samplebits == 16)
		{
			short *out = (short *) pbuf;
			while (count--)
			{
				val = *p >> 8;
				p+= step;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < (short)0x8000)
					val = (short)0x8000;
				out[out_idx] = val;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
		else if (dma.samplebits == 8)
		{
			unsigned char *out = (unsigned char *) pbuf;
			while (count--)
			{
				val = *p >> 8;
				p+= step;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < (short)0x8000)
					val = (short)0x8000;
				out[out_idx] = (val>>8) + 128;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
	}
#endif
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

void S_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int endtime, int offset);
void S_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int endtime, int offset);

void S_PaintChannels(int endtime)
{
	int 	i;
	int 	end;
	channel_t *ch;
	sfxcache_t	*sc;
	int		ltime, count;
	playsound_t	*ps;

	snd_vol = s_volume->value*256;

//Com_Printf ("%i to %i\n", paintedtime, endtime);
	while (paintedtime < endtime)
	{
	// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

		// start any playsounds
		while (1)
		{
			ps = s_pendingplays.next;
			if (ps == &s_pendingplays)
				break;	// no more pending sounds
			if (ps->begin <= paintedtime)
			{
				S_IssuePlaysound (ps);
				continue;
			}

			if (ps->begin < end)
				end = ps->begin;		// stop here
			break;
		}

	// clear the paint buffer
		if (s_rawend < paintedtime)
		{
//			Com_Printf ("clear\n");
			memset(paintbuffer, 0, (end - paintedtime) * sizeof(portable_samplepair_t));
		}
		else
		{	// copy from the streaming sound source
			int		s;
			int		stop;

			stop = (end < s_rawend) ? end : s_rawend;

			for (i=paintedtime ; i<stop ; i++)
			{
				s = i&(MAX_RAW_SAMPLES-1);
				paintbuffer[i-paintedtime] = s_rawsamples[s];
			}
//		if (i != end)
//			Com_Printf ("partial stream\n");
//		else
//			Com_Printf ("full stream\n");
			for ( ; i<end ; i++)
			{
				paintbuffer[i-paintedtime].left =
				paintbuffer[i-paintedtime].right = 0;
			}
		}


	// paint in the channels.
		ch = channels;
		for (i=0; i<MAX_CHANNELS ; i++, ch++)
		{
			ltime = paintedtime;
		
			while (ltime < end)
			{
				if (!ch->sfx || (!ch->leftvol && !ch->rightvol) )
					break;

				// max painting is to the end of the buffer
				count = end - ltime;

				// might be stopped by running out of data
				if (ch->end - ltime < count)
					count = ch->end - ltime;
		
				sc = S_LoadSound (ch->sfx);
				if (!sc)
					break;

				if (count > 0 && ch->sfx)
				{	
					if (sc->width == 1)// FIXME; 8 bit asm is wrong now
						S_PaintChannelFrom8(ch, sc, count,  ltime - paintedtime);
					else
						S_PaintChannelFrom16(ch, sc, count, ltime - paintedtime);
	
					ltime += count;
				}

			// if at end of loop, restart
				if (ltime >= ch->end)
				{
					if (ch->autosound)
					{	// autolooping sounds always go back to start
						ch->pos = 0;
						ch->end = ltime + sc->length;
					}
					else if (sc->loopstart >= 0)
					{
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					}
					else				
					{	// channel just stopped
						ch->sfx = NULL;
					}
				}
			}
															  
		}

	// transfer out according to DMA format
		S_TransferPaintBuffer(end);
		paintedtime = end;
	}
}

void S_InitScaletable (void)
{
	int		i, j;
	int		scale;

	s_volume->modified = false;
#ifndef NO_TABLE
	for (i=0 ; i<32 ; i++)
	{
		scale = i * 8 * 256 * s_volume->value;
		for (j=0 ; j<256 ; j++)
			snd_scaletable[i][j] = ((signed char)j) * scale;
	}
#endif
}


#if !(defined __linux__ && defined __i386__)
#if	!id386

void S_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count, int offset)
{
	int 	data;
#ifdef	NO_TABLE
	int leftvol, rightvol;
	signed char	*sfx;
#else
	int		*lscale, *rscale;
	unsigned char	*sfx;
#endif
	int		i;
	portable_samplepair_t	*samp;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;
		
	//ZOID-- >>11 has been changed to >>3, >>11 didn't make much sense
	//as it would always be zero.
#ifdef	NO_TABLE
	leftvol	= ch->leftvol * s_volume->value;
	rightvol = ch->rightvol * s_volume->value;
#else
	lscale = snd_scaletable[ ch->leftvol >> 3];
	rscale = snd_scaletable[ ch->rightvol >> 3];
#endif
	sfx = (signed char *)sc->data + ch->pos;

	samp = &paintbuffer[offset];

	for (i=0 ; i<count ; i++, samp++)
	{
		data = sfx[i];
#ifdef	NO_TABLE
		samp->left += data*leftvol;
		samp->right += data*rightvol;
#else
		samp->left += lscale[data];
		samp->right += rscale[data];
#endif
	}
	
	ch->pos += count;
}

#else

__declspec( naked ) void S_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count, int offset)
{
	__asm {
 push esi
 push edi
 push ebx
 push ebp
 mov ebx,ds:dword ptr[4+16+esp]
 mov esi,ds:dword ptr[8+16+esp]
 mov eax,ds:dword ptr[4+ebx]
 mov edx,ds:dword ptr[8+ebx]
 cmp eax,255
 jna LLeftSet
 mov eax,255
LLeftSet:
 cmp edx,255
 jna LRightSet
 mov edx,255
LRightSet:
 and eax,0F8h
 add esi,20
 and edx,0F8h
 mov edi,ds:dword ptr[16+ebx]
 mov ecx,ds:dword ptr[12+16+esp]
 add esi,edi
 shl eax,7
 add edi,ecx
 shl edx,7
 mov ds:dword ptr[16+ebx],edi
 add eax,offset snd_scaletable
 add edx,offset snd_scaletable
 sub ebx,ebx
 mov bl,ds:byte ptr[-1+esi+ecx*1]
 test ecx,1
 jz LMix8Loop
 mov edi,ds:dword ptr[eax+ebx*4]
 mov ebp,ds:dword ptr[edx+ebx*4]
 add edi,ds:dword ptr[paintbuffer+0-8+ecx*8]
 add ebp,ds:dword ptr[paintbuffer+4-8+ecx*8]
 mov ds:dword ptr[paintbuffer+0-8+ecx*8],edi
 mov ds:dword ptr[paintbuffer+4-8+ecx*8],ebp
 mov bl,ds:byte ptr[-2+esi+ecx*1]
 dec ecx
 jz LDone
LMix8Loop:
 mov edi,ds:dword ptr[eax+ebx*4]
 mov ebp,ds:dword ptr[edx+ebx*4]
 add edi,ds:dword ptr[paintbuffer+0-8+ecx*8]
 add ebp,ds:dword ptr[paintbuffer+4-8+ecx*8]
 mov bl,ds:byte ptr[-2+esi+ecx*1]
 mov ds:dword ptr[paintbuffer+0-8+ecx*8],edi
 mov ds:dword ptr[paintbuffer+4-8+ecx*8],ebp
 mov edi,ds:dword ptr[eax+ebx*4]
 mov ebp,ds:dword ptr[edx+ebx*4]
 mov bl,ds:byte ptr[-3+esi+ecx*1]
 add edi,ds:dword ptr[paintbuffer+0-8*2+ecx*8]
 add ebp,ds:dword ptr[paintbuffer+4-8*2+ecx*8]
 mov ds:dword ptr[paintbuffer+0-8*2+ecx*8],edi
 mov ds:dword ptr[paintbuffer+4-8*2+ecx*8],ebp
 sub ecx,2
 jnz LMix8Loop
LDone:
 pop ebp
 pop ebx
 pop edi
 pop esi
 ret
	}
}

#endif
#endif

void S_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int count, int offset)
{
	int data;
	int left, right;
	int leftvol, rightvol;
	signed short *sfx;
	int	i;
	portable_samplepair_t	*samp;

	leftvol = ch->leftvol*snd_vol;
	rightvol = ch->rightvol*snd_vol;
	sfx = (signed short *)sc->data + ch->pos;

	samp = &paintbuffer[offset];
	for (i=0 ; i<count ; i++, samp++)
	{
		data = sfx[i];
		left = (data * leftvol)>>8;
		right = (data * rightvol)>>8;
		samp->left += left;
		samp->right += right;
	}

	ch->pos += count;
}

