/* 
	dcmenu.c 

	(c) 2002 BERO
	under KOS licence

*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<fcntl.h>

#define	_arch_dreamcast
#include	<kos.h>
#include	<dc/pvr.h>

#include	<assert.h>

#define	IMG_WIDTH	320
#define	IMG_HEIGHT	240

#define	RGB565(r,g,b)	( (((r)>>3)<<11) | (((g)>>2)<<5) |((b)>>3) )

#define QUAKE2
#ifdef QUAKE2
static char *snapdir = "/scrnshot";
static char *gamecmd = "+set game";
static char *gamedir = "baseq2";
#elif defined(QUAKE2RJ) /* hexen II */
static char *snapdir = "/shots";
static char *gamecmd = "-game";
static char *gamedir = "data1";
#else
static char *snapdir = "";
static char *gamecmd = "-game";
static char *gamedir = "id1";
#endif

//#define	printf(fmt,...)

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned char	data;			// unbounded
} pcx_t;

int pcx_load (const char *filename,unsigned short *dst,int linewidth)
{
	int		i, j;
	pcx_t	*pcx;
	unsigned char *buffer,*p;
	unsigned short pal[256];
	int width,height;

	int fd,size;

	fd = fs_open(filename,O_RDONLY);
	if (fd==0) return -1;

	printf("pcxload:%s %p %d\n",filename,dst,linewidth);
	size = fs_total(fd);

	buffer = malloc(size);
	fs_read(fd,buffer,size);
	fs_close(fd);

	pcx = (pcx_t*)buffer;
	p = buffer + size - 768;

	if (
		(pcx->manufacturer != 0x0a)	// PCX id
	||	(pcx->version != 5)			// 256 color
 	||	(pcx->encoding != 1)		// uncompressed
	||	(pcx->bits_per_pixel != 8)		// 256 color
//	|| (pcx->xmin != 0)
//	|| (pcx->ymin != 0)
//	|| (pcx->xmax != (WIDTH-1))
//	|| (pcx->xmax != (HEIGHT-1))
	|| (pcx->hres != IMG_WIDTH)
//	|| (pcx->vres != HEIGHT)
	|| (pcx->color_planes != 1)		// chunky image
	|| (pcx->bytes_per_line != IMG_WIDTH)
	|| (pcx->palette_type != 2)		// not a grey scale
	|| (p[-1] != 0xc)	// palette ID byte
	) {
		printf("manufacturer:%x version:%d encoding:%d bist_per_pixel:%d xmin;%d ymin:%d xmax:%d ymax:%d hres:%d vres:%d"
		" color_plane:%d bytes_per_line:%d palette_type:%d paller_id:%x\n",
		pcx->manufacturer,
		pcx->version,
		pcx->encoding,
		pcx->bits_per_pixel,
		pcx->xmin,
		pcx->ymin,
		pcx->xmax,
		pcx->ymax,
		pcx->hres,
		pcx->vres,
		pcx->color_planes,
		pcx->bytes_per_line,
		pcx->palette_type,
		p[-1]
		);
		free(buffer);
		return -1;
	}

	width = pcx->hres;
	height = pcx->vres;
	printf("OK\n");

	for(i=0;i<256;i++) {
		pal[i] = RGB565(p[0],p[1],p[2]);
		p+=3;
	}

	p = &pcx->data;
	
	for (i=0 ; i<height ; i++)
	{
		for (j=0 ; j<width ;)
		{
			int c = *p++;
			if (c>0xc0) {
				int count = c&0x3f;
				do { dst[j++] = pal[*p++]; }while(--count);
			} else {
				dst[j++] = pal[c];
			}
		}
		dst+=linewidth;
	}
	free(buffer);

	if (height<IMG_HEIGHT) {
		sq_set16(dst,RGB565(0,0,0),linewidth*(IMG_HEIGHT-height)*2);
	}

	return 0;
} 

int tga_load (const char *filename,unsigned short *dst,int linewidth) 
{
	unsigned	char	*buffer,*p;
	int fd,size;
	int i,j;
	int	width,height;

	fd = fs_open(filename,O_RDONLY);
	if (fd==0) return -1;

	size = fs_total(fd);

	buffer = malloc(size);
	fs_read(fd,buffer,size);
	fs_close(fd);

	width = *(short*)&buffer[12];
	height = *(short*)&buffer[14];

	if (buffer[2]!=2 /* uncompressed type */ || buffer[16]!=24 /* depth*/ || width!=IMG_WIDTH /*|| height!=HEIGHT*/) {
		free(buffer);
		return -1;
	}

	// tga is bgr
	p = buffer+18;
	for (i=0 ; i<height ; i++) {
		for(j=0;j<width;j++) {
			dst[j] = RGB565(p[2],p[1],p[0]);
			p+=3;
		}
		dst+=linewidth;
	}

	if (height<IMG_HEIGHT) {
		sq_set16(dst,RGB565(0,0,0),linewidth*(IMG_HEIGHT-height)*2);
	}

	free (buffer);

	return 0;
} 

#if 0
#include <GL/gl.h>

int tga_save (const char *filename) 
{
	int fd;
	int width=640,height=480;
	int	i,temp,c;
	char	*buffer;
	extern int gl_prim_type;

	gl_prim_type = -1;
	buffer = malloc(width*height*3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	*(short*)&buffer[12] = width;
	*(short*)&buffer[14] = height;
	buffer[16] = 24;	// pixel size

	glReadPixels (0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer+18 ); 

	// swap rgb to bgr
	c = 18+width*height*3;
	for (i=18 ; i<c ; i+=3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}

	fd = fs_open(filename,O_WRONLY);
	fs_write(fd, buffer, width*height*3 + 18 );
	fs_close(fd);

	free (buffer);
} 
#endif

int img_load(const char *filename,unsigned short *dst,int linewidth)
{
	const char *ext = strrchr(filename,'.');
	printf("imgload:%s\n",filename);
	if (ext==NULL) return -1;
	else if (stricmp(ext+1,"pcx")==0) return pcx_load(filename,dst,linewidth);
	else if (stricmp(ext+1,"tga")==0) return tga_load(filename,dst,linewidth);
	else return -1;
}

/* textview */

struct _textptr {
	char *s;
	int len;
};

typedef struct {
	struct _textptr *text;
	char *textbuf;
	int	lines;
} textbuf_t;

#define	iskanji(c)	((c>0x80 && c<0xa0) || (c>0xe0))

textbuf_t* txt_load(const char *filename,int maxwidth)
{
	file_t fd;
	size_t size;
	unsigned char *s;
	textbuf_t *t;
	int line;

	printf("txtload:%s %d\n",filename,maxwidth);

	fd = fs_open(filename,O_RDONLY);
	if (fd==0)  return NULL;
	size = fs_total(fd);

	t = malloc(sizeof(textbuf_t));

	t->textbuf = malloc(size+1);
	t->textbuf[size] = 0;
	fs_read(fd,t->textbuf,size);
	fs_close(fd);

	/* pass 1 */
	s = t->textbuf;
	line = 0;
	while(*s) {
		int len;
		int x = 0;
		int c;
		for(len=0;x<maxwidth;len++) {
			c = s[len];
			if (c=='\t') { x += 8-(x&7); continue;}
			else if (c==0 || c=='\n' || c=='\r') break;
			x++;
		}
		if (iskanji(c)) len--;
		line++;
		if (c==0) break;
		while(s[len]=='\r') len++;
		s = s + len + 1;
	}

	/* pass 2 */
	t->text = malloc(sizeof(struct _textptr)*(line+1));

	s = t->textbuf;
	line = 0;
	while(*s) {
		int len;
		int x = 0;
		int c;
		for(len=0;x<maxwidth;len++) {
			c = s[len];
			if (c=='\t') { x += 8-(x&7); continue;}
			else if (c==0 || c=='\n' || c=='\r') break;
			x++;
		}
		if (iskanji(c)) len--;
		t->text[line].s = s;
		t->text[line].len = len;
		line++;
		if (c==0) break;
		while(s[len]=='\r') len++;
		s = s + len + 1;
	}
	t->lines = line;
	return t;
}

void txt_free(textbuf_t *t)
{
	if (t) {
		free(t->text);
		free(t->textbuf);
		free(t);
	}
}

static void *pvr_commit_init(void)
{
	QACR0 = ((((uint32)PVR_TA_INPUT) >> 26) << 2) & 0x1c;
	QACR1 = ((((uint32)PVR_TA_INPUT) >> 26) << 2) & 0x1c;
	return (void*)0xe0000000;
}

static void v_copy(pvr_vertex_t *vert,const pvr_vertex_t *vbuf)
{
	*vert = *vbuf;
}

#define	pvr_commit(vert,vbuf)	v_copy(vert,vbuf); pvr_dr_commit(vert); (uint32)vert^=32

#define	FONT_W	12
#define	FONT_H	24

static void font_init(unsigned short *vram)
{
	int x,y;
	unsigned int ch;
	for(ch=' ';ch<0x80;ch++) {
		x = ((ch-' ')%16)*FONT_W;
		y = ((ch-' ')/16)*FONT_H;
		bfont_draw(vram+x+y*256,256,1,ch);
	}
}

static void draw_rectangle(float x0,float y0,float z0,float w,float h,uint32 color,float u0,float v0,float u1,float v1);

static void draw_poly_char(float x0,float y0,float z0,uint32 color,unsigned int ch)
{
	float u0 = ((ch-' ')%16)*(FONT_W/256.0f);
	float v0 = ((ch-' ')/16)*(FONT_H/256.0f);
	draw_rectangle(x0,y0,z0,FONT_W,FONT_H,color,u0,v0,u0+(FONT_W/256.0f),v0+(FONT_H/256.0f));
}

static void draw_rectangle(float x0,float y0,float z0,float w,float h,uint32 color,float u0,float v0,float u1,float v1)
{
	pvr_vertex_t v,*vert;
	float x1 = x0 + w;
	float y1 = y0 + h;
	/*  0 1
	    2 3 */

	vert = pvr_commit_init();

	v.flags = PVR_CMD_VERTEX;
	v.x = x0;
	v.y = y0;
	v.z = z0;
	v.u = u0;
	v.v = v0;
	v.argb = color;
	v.oargb = 0xff000000;

	pvr_commit(vert,&v);

	v.x = x1;
	v.u = u1;

	pvr_commit(vert,&v);

	v.x = x0;
	v.y = y1;
	v.u = u0;
	v.v = v1;

	pvr_commit(vert,&v);

	v.flags = PVR_CMD_VERTEX_EOL;
	v.x = x1;
	v.u = u1;

	pvr_commit(vert,&v);
}

static void draw_poly_strn(float x0,float y0,float z0,uint32 color,const char *str,int len)
{
	const unsigned char *s;
	int x;
	s = str;
	for(x=0;len;len--) {
		int c=*s++;
		if (c=='\t') x+=8-(x&7);
		else {
			if (c!=' ' && c<0x80) {
				draw_poly_char(x0+x*FONT_W,y0,z0,color,c);
			}
			x++;
		}
	}
}

static void draw_poly_str(float x0,float y0,float z0,uint32 color,const char *str)
{
	draw_poly_strn(x0,y0,z0,color,str,strlen(str));
}

#include <stdarg.h>
static void draw_poly_printf(float x0,float y0,float z0,uint32 color,const char *fmt, ...)
{
	char str[256];
	int len;
	va_list args;

	va_start(args, fmt);
	len = vsprintf(str, fmt, args);
	va_end(args);

	draw_poly_strn(x0,y0,z0,color,str,len);
}

static void draw_text(float x,float y,float z,uint32 color,textbuf_t *t,int line,int n)
{
	int i;
	for(i=0;i<n && line<t->lines;i++,line++,y+=FONT_H) {
		draw_poly_strn(x,y,z,color,t->text[line].s,t->text[line].len);
	}
}

#define	ISDIR(ent)	((ent)->size<0)

typedef struct {
	char *gamename;
	char *dirname;
	char *cmdline;
	char *snapfile;
	char *txtfile;
} game_t;

typedef struct{
	game_t *games;
	int n_games;
	void *buf;
} gamelist_t;

void free_gamelist(gamelist_t *gamelist)
{
	if (gamelist==NULL) return;
	free(gamelist->buf);
	free(gamelist->games);
	free(gamelist);
}

gamelist_t * get_gamelist(const char *basedir)
{
	char path[256];
	size_t size;
	char *buf,*s;
	int	fd;
	int	n_games,i;
	gamelist_t *gamelist;
	game_t *g;

	sprintf(path,"%s/%s",basedir,"games.lst");
	fd = fs_open(path,O_RDONLY);
	if (fd==0) return NULL;
	size = fs_total(fd);
	buf = malloc(size+2);
	fs_read(fd,buf,size);
	if (buf[size-1]=='\n')
		buf[size]=0;
	else {
		buf[size]='\n';
		buf[size+1]=0;
	}

	/* pass 1 */
	s = buf;
	n_games = 0;
	while(*s) {
		if (*s=='#') {
			while(*s++!='\n');
		}
		while(*s++!='\n');
		n_games++;
	}

	gamelist = malloc(sizeof(gamelist_t));
	gamelist->games = malloc(sizeof(game_t)*n_games);
	gamelist->n_games = n_games;
	gamelist->buf = buf;

	/* pass 2 */
	s = buf;
	for(g=gamelist->games;*s;g++) {
		if (*s=='#') {
			while(*s++!='\n');
		}
		g->gamename = s;
		while(*s!=',') s++;
		*s++ = 0;
		g->dirname = s;
		while(*s!=',') s++;
		*s++ = 0;
		g->cmdline = s;
		while(*s!=',') s++;
		*s++ = 0;
		g->snapfile = s;
		while(*s!=',') s++;
		*s++ = 0;
		g->txtfile = s;
		while(*s!='\n') s++;
		if (s[-1]=='\r') s[-1]=0;
		*s++ = 0;

		if (g->gamename[0]==0) g->gamename = NULL;
		if (g->dirname[0]==0) g->dirname = NULL;
		if (g->cmdline[0]==0) g->cmdline = NULL;
		if (g->snapfile[0]==0) g->snapfile = NULL;
		if (g->txtfile[0]==0) g->txtfile = NULL;
	}

	return gamelist;
}

gamelist_t * get_gamelist_fromdir(const char *basedir)
{
	int	dir;
	dirent_t *ent;
	int	i,n_games;
	game_t *g;
	gamelist_t *gamelist;
	char dirpath[256],*dirname;
	char *namebuf,*p,*p2;


	dir = fs_open(basedir,O_RDONLY|O_DIR);
	if (dir==0) return NULL;

	gamelist = malloc(sizeof(gamelist_t));
//	printf("base=%s\n",basedir);

	strcpy(dirpath,basedir);
	dirname = dirpath+strlen(dirpath);
	*dirname++= '/';

	/* dcload_readdir can only one handle */

	namebuf = malloc(1024*8);
	n_games = 0;
	p = namebuf;
	while((ent = fs_readdir(dir))!=NULL) {
		if (!ISDIR(ent) || ent->name[0]=='.' || stricmp(ent->name,"docs")==0 || stricmp(ent->name,"help")==0) continue;
		strcpy(p,ent->name);
		p += strlen(p)+1;
		n_games++;
	}
	fs_close(dir);

	gamelist->games = malloc(sizeof(game_t)*n_games);
	gamelist->n_games = n_games;
	gamelist->buf = namebuf;

	p2 = p;
	p = namebuf;
	for(i=0,g=gamelist->games;i<n_games;i++,g++,p+=strlen(p)+1) {
		g->dirname = p;
		g->gamename = p;
		g->snapfile = NULL;
		g->txtfile = NULL;
		g->cmdline = NULL;

		/* search snapshot and text file */
		strcpy(dirname,p);
		printf("path=%s\n",dirpath);
		dir = fs_open(dirpath,O_RDONLY|O_DIR);
		while((ent = fs_readdir(dir))!=NULL) {
			char *ext = strrchr(ent->name,'.');
			if (ext==NULL) continue;
			ext++;
			if (snapdir[0]==0 && g->snapfile==NULL && (stricmp(ext,"tga")==0 || stricmp(ext,"pcx")==0)) {
				g->snapfile = p2;
				strcpy(p2,ent->name);
				p2 += strlen(p2)+1;
			} else 
			if (g->txtfile==NULL && (stricmp(ext,"txt")==0 || stricmp(ext,"doc")==0)) {
				g->txtfile = p2;
				strcpy(p2,ent->name);
				p2 += strlen(p2)+1;
			}
		}
		fs_close(dir);
	    if (snapdir[0]) {
		strcat(dirname,snapdir);
		dir = fs_open(dirpath,O_RDONLY|O_DIR);
		if (dir){
		while((ent = fs_readdir(dir))!=NULL) {
			char *ext = strrchr(ent->name,'.');
			if (ext==NULL) continue;
			ext++;
			if (g->snapfile==NULL && (stricmp(ext,"tga")==0 || stricmp(ext,"pcx")==0)) {
				g->snapfile = p2;
				strcat(p2,ent->name);
				p2 += strlen(p2)+1;
			}
		}
		fs_close(dir);
		}
	    }
//		printf("%s %s %s %s\n",games[n_dir].dirname,games[n_dir].gamename,games[n_dir].snapfile,games[n_dir].txtfile);
		assert(p2<namebuf+1024*8);
	}
	return gamelist;
}

/* 
	| select menu |  |  snap shot  |
	|             |  |             |

	| description                  |
        |                              |
*/


#define	TOP_MERGIN	24
#define	BOTTOM_MERGIN	24
#define	LEFT_MERGIN	24
#define	RIGHT_MERGIN	24
#define	TEXT_WIDTH	((640-LEFT_MERGIN-RIGHT_MERGIN)/FONT_W)
#define	TEXT_HEIGHT	((480-240-TOP_MERGIN-BOTTOM_MERGIN)/FONT_H)

static char cmdline[256];

char* menu(int *argc,char **argv,char **basedirs)
{
	char *basedir;
	gamelist_t *gamelist = NULL;
	uint8 mcont;
	char path[256];

	pvr_poly_cxt_t	pvr_cxt;
	pvr_poly_hdr_t	font_polyhdr,snap_polyhdr;
	pvr_ptr_t	font_txr,snap_txr;

	int	index;
	int	line;
	int	i;

	int prev_buttons = 0;
#define	Z	0.5f
#define	WHITE	0xffffffff
#define	GRAY	0xff808080

	textbuf_t *txtbuf = NULL;

	/* init */
	vid_set_mode(DM_640x480, PM_RGB565);
	pvr_init_defaults();
	font_txr = pvr_mem_malloc(256*256*2);
	snap_txr = pvr_mem_malloc(512*256*2);

	font_init(font_txr);

	pvr_poly_cxt_txr(&pvr_cxt,PVR_LIST_OP_POLY,PVR_TXRFMT_RGB565|PVR_TXRFMT_NONTWIDDLED,256,256,font_txr,PVR_FILTER_NONE);
	pvr_cxt.gen.shading = PVR_SHADE_FLAT;
	pvr_poly_compile(&font_polyhdr,&pvr_cxt);
	pvr_poly_cxt_txr(&pvr_cxt,PVR_LIST_OP_POLY,PVR_TXRFMT_RGB565|PVR_TXRFMT_NONTWIDDLED,512,512,snap_txr,PVR_FILTER_NONE);
	pvr_cxt.gen.shading = PVR_SHADE_FLAT;
	pvr_poly_compile(&snap_polyhdr,&pvr_cxt);

retry:
	if (txtbuf) {
		txt_free(txtbuf);
		txtbuf = NULL;
	}
	if (gamelist) free_gamelist(gamelist);
	iso_ioctl(0,0,0);

	for(i=0;(basedir = basedirs[i])!=NULL;i++) {
		char path[256];
		int fd;
		sprintf(path,"%s/%s",basedir,gamedir);
		fd  = fs_open(path,O_DIR);
		if (fd!=0) {
			fs_close(fd);
			break;
		}
	}
	if (basedir==NULL) {
	} else {

		gamelist = get_gamelist(basedir);
		if (gamelist==NULL) gamelist = get_gamelist_fromdir(basedir);
	}

	index = 0;
	txtbuf = NULL;
	goto start;

	/* main loop */
	while(1) {
		int previndex = index;
		int i,offset;
		cont_cond_t	cond;

		mcont = maple_first_controller();
		if (mcont && cont_get_cond(mcont, &cond)==0) {
			int buttons,pressed,changed;

			buttons = cond.buttons^0xffff;
			changed = buttons ^ prev_buttons;
			pressed = buttons & changed;
			prev_buttons = buttons;
			if (pressed & (CONT_START|CONT_A)) {
				if (gamelist==NULL) goto retry;
				/* start */
				break;
			}
			if (pressed & CONT_Y) {
				goto retry;
			}

			if ((pressed & CONT_DPAD_UP) && index>0) index--;
			if ((pressed & CONT_DPAD_DOWN) && gamelist && index<gamelist->n_games-1) index++;
			if (txtbuf) {
				if (cond.ltrig && line>0) line--;
				if (cond.rtrig && line<txtbuf->lines-1) line++;
			}

		//	if ((pressed & CONT_Y)) tga_save("/pc/quake/menu.tga");

		}
		if (previndex!=index) {
			int havesnap;
start:
			if (txtbuf) {
				txt_free(txtbuf);
				txtbuf = NULL;
			}
		    if (gamelist==NULL) {
		    } else {
			if (gamelist->games[index].txtfile) {
				if (gamelist->games[index].dirname)
					sprintf(path,"%s/%s/%s",basedir,gamelist->games[index].dirname,gamelist->games[index].txtfile);
				else
					sprintf(path,"%s/%s",basedir,gamelist->games[index].txtfile);
				txtbuf = txt_load(path,TEXT_WIDTH);
			}
			havesnap = 0;
			if (gamelist->games[index].snapfile) {
				if (gamelist->games[index].dirname)
					sprintf(path,"%s/%s%s/%s",basedir,gamelist->games[index].dirname,snapdir,gamelist->games[index].snapfile);
				else
					sprintf(path,"%s/%s",basedir,gamelist->games[index].snapfile);
				havesnap = (img_load(path,(unsigned short*)snap_txr,512)==0);
			}
			if (!havesnap) {
				sq_set16(snap_txr,RGB565(0,0,255),512*IMG_HEIGHT*2);
				bfont_draw_str((uint16*)snap_txr + (IMG_WIDTH-13*FONT_W)/2 + (IMG_HEIGHT-FONT_H)/2*512,512,0,"NO SCREENSHOT");
			}
                    }
			line = 0;
		}
		
		/* draw start */
		pvr_wait_ready();
		pvr_scene_begin();
		pvr_list_begin(PVR_LIST_OP_POLY);

		/* draw screnshot */
		pvr_prim(&snap_polyhdr,sizeof(snap_polyhdr));
		draw_rectangle(640-320-LEFT_MERGIN,TOP_MERGIN,Z,320,240,WHITE,0,0,320/512.0f,240/512.0f);

		pvr_prim(&font_polyhdr,sizeof(font_polyhdr));
	    if (gamelist==NULL) {
		draw_poly_str(LEFT_MERGIN,TOP_MERGIN+240,Z,WHITE,"Please insert game disk and hit Y button");
	    } else {
		/* draw select menu */
		if (index>9) offset=index-9; else offset=0;
		for(i=0;i<10 && (i+offset<gamelist->n_games);i++) {
			draw_poly_str(LEFT_MERGIN,TOP_MERGIN+i*FONT_H,Z,(i+offset)==index?WHITE:GRAY,gamelist->games[i+offset].gamename);
		}
		/* draw description */
		if (txtbuf) {
			draw_text(LEFT_MERGIN,TOP_MERGIN+240,Z,WHITE,txtbuf,line,TEXT_HEIGHT);
		}
	    }

		pvr_list_finish();
		pvr_list_begin(PVR_LIST_TR_POLY);
		pvr_list_finish();
		pvr_scene_finish();
	}

	pvr_shutdown();

	if (txtbuf) txt_free(txtbuf);

	if (gamelist->games[index].cmdline) {
		strcpy(cmdline,gamelist->games[index].cmdline);
	} else if (gamelist->games[index].dirname && stricmp(gamelist->games[index].dirname,gamedir)!=0) {
		sprintf(cmdline,"%s %s",gamecmd,gamelist->games[index].dirname);
	} else {
		cmdline[0] = 0;
	}
	i = *argc;
	{
		char *s = cmdline;
		while(*s) {
			argv[i++] = s;
			while(*s && *s!=' ') s++;
			if (*s==' ') {
				*s++= 0;
				while(*s==' ') s++;
			}
		}
	}
	argv[i] = NULL;
	*argc = i;

	free_gamelist(gamelist);

	return basedir;
}

#if 0
int main(int argc,char **argv)
{
	char *new_argv[10] = {"quake"};
	menu(&argc,new_argv);
}
#endif
