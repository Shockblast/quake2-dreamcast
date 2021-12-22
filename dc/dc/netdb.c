/*
	minimum network support for quake
*/

#include <string.h>

#include <lwip/arch.h>
#define	LWIP_COMPAT_SOCKETS
#include <lwip/sockets.h>

#include "netdb.h"

#define	HOSTNAME	"dreamcast"

/* "a.b.c.d" to int */
int	inet_addr(const char *s)
{
	int i,ret = 0;
	for(i=0;i<4;i++) {
		int n,c;
		n = 0;
		while(c=*s++,c>='0' && c<='9') {
			n = n*10 + c - '0';
		}
		if (n>255) return -1;
		ret = (ret<<8) + n;
		if (i!=3) {
			if (c!='.') return -1;
		} else {
			if (c!=0) return -1;
		}
	}
	return ret;
}

static char hostname[64],addr[4];
static char *h_addr_list[2] = {addr,NULL};

#define	INADDRSZ	4

static struct hostent host = {
	hostname,
	NULL,
	AF_INET,
	INADDRSZ,
	h_addr_list
};

//#define	IPADR(a,b,c,d)	( (a)|((b)<<8)|((c)<<16)|((d)<<24) )
#define	IPADR(a,b,c,d)	( ((a)<<24)|((b)<<16)|((c)<<8)|(d) )


/* only support a.b.c.d and localhost */

struct hostent	*gethostbyname (const char *name)
{
	int addr;
	if (strcmp(name,"localhost")==0) {
		addr = IPADR(127,0,0,1);
	} else if (strcmp(name,HOSTNAME)==0) {
		addr = IPADR(192,168,0,4);
	} else if (name[0]>='0' && name[0]<='9' && (addr=inet_addr(name))!=-1) {
	} else {
		return NULL;
	}
	strcpy(hostname,name);
	*(long*)host.h_addr_list[0] = htonl(addr);
	return &host;
}

struct hostent	*gethostbyaddr (const char *name, int len, int type)
{
	if (len!=INADDRSZ || type!=AF_INET) return NULL;
	return gethostbyname(name);
}

int gethostname(char *name, size_t len)
{
	strncpy(name,HOSTNAME,len);
	return 0;
}

char *inet_ntoa(struct in_addr in)
{
	static char buf[16]; /* xxx.xxx.xxx.xxx */
	unsigned int addr = in.s_addr;

	sprintf(buf,"%d.%d.%d.%d",addr>>24,(addr>>16)&255,(addr>>8)&255,addr&255);
	return buf;
}
