/* */

#define	__BEGIN_DECLS
#define	__END_DECLS

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;

typedef char int8;
typedef short int16;
typedef long int32;


#include "elf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

struct sym_t {
	char *name;
	void *addr;
};

struct elf_obj_t {
	void *data;
	struct sym_t *syms;
	char *strtab;
	char name[1];
};

//#define	DEBUG
//#define TEST

static void *find_sym(struct sym_t *syms,const char *name)
{
//	printf("find:%s\n",name);
	while(syms->name && strcmp(syms->name,name)!=0) {
//		printf("%s\n",syms->name);
		syms++;
	}
//	printf(" %p\n",syms->addr);
	return syms->addr;
}

extern struct sym_t symbols[];

static void *find_global(const char *name)
{
	return find_sym(symbols,name);
}

#define	err(arg...)	{ printf(arg); goto errret; }

/*
static void err(const char *str)
{
	puts(str);
	exit(0);
}
*/

static void* malloc_read(int fd,int offset,size_t size)
{
	void *buf = malloc(size);
	lseek(fd,offset,SEEK_SET);
	read(fd,buf,size);
	return buf;
}

#define EM_SH          42   /* Hitachi SH */

struct elf_obj_t *elf_load(const char *name)
{
	int fd;
	char *imgbase=NULL;
	uint32 sz,com;

	struct elf_hdr_t	hdr[1];
	struct elf_shdr_t	*shdrs=NULL;
	struct elf_sym_t	*symtab=NULL;
	struct elf_rela_t	*reltab=NULL;
	int			n_symtab;
	char			*strtab=NULL,*shstrtab=NULL;
	const static char ident[] = {0x7f,'E','L','F',0x01,0x01};

	int	n_syms;

	int i,j,align,sect_bss;

#ifndef O_BINARY
#define	O_BINARY	0
#endif

	fd = open(name,O_RDONLY|O_BINARY);
	if (fd == -1) return NULL;

	read(fd,hdr,sizeof(hdr[0]));

	if (memcmp(hdr->ident, ident, sizeof(ident))!=0 || hdr->machine!=EM_SH) {
		err("invalid header");
	}

	shdrs = malloc_read(fd,hdr->shoff,hdr->shnum*sizeof(*shdrs));

	shstrtab = malloc_read(fd,shdrs[hdr->shstrndx].offset, shdrs[hdr->shstrndx].size);

#ifdef DEBUG
	printf("	entry point	%08x\r\n", hdr->entry);
	printf("	ph offset	%08x\r\n", hdr->phoff);
	printf("	sh offset	%08x\r\n", hdr->shoff);
	printf("	flags		%08x\r\n", hdr->flags);
	printf("	ehsize		%08x\r\n", hdr->ehsize);
	printf("	phentsize	%08x\r\n", hdr->phentsize);
	printf("	phnum		%08x\r\n", hdr->phnum);
	printf("	shentsize	%08x\r\n", hdr->shentsize);
	printf("	shnum		%08x\r\n", hdr->shnum);
	printf("	shstrndx	%08x\r\n", hdr->shstrndx);


#define	print(struc,param)	printf(#param "\t%x\n",struc->param);

	for (i=0; i<hdr->shnum; i++) {
		struct elf_shdr_t *sec = &shdrs[i];
		printf("%d\n",i);
		printf("name %s\n",shstrtab + sec->name);
		print(sec,type);
		print(sec,flags);
		print(sec,addr);
		print(sec,offset);
		print(sec,size);
		print(sec,link);
		print(sec,info);
		print(sec,addralign);
		print(sec,entsize);
		printf("\n");
	}
#endif

	/* search symbol table */
	for(i=0;i<hdr->shnum;i++) {
		if (shdrs[i].type == SHT_SYMTAB) break;
	}
	if (i==hdr->shnum) err("missing symbol table\n"); /* err */

	symtab = malloc_read(fd,shdrs[i].offset,shdrs[i].size);
	n_symtab = shdrs[i].size/sizeof(symtab[0]);

	/* search string table */
	for(i=0;i<hdr->shnum;i++) {
		if (shdrs[i].type == SHT_STRTAB && i!=hdr->shstrndx) break;
	}
	if (i==hdr->shnum) err("missing string table\n"); /* err */
	strtab = malloc_read(fd,shdrs[i].offset,shdrs[i].size);

#ifdef DEBUG

	printf("before\n");
	for(i=0;i<n_symtab;i++) {
		struct elf_sym_t *s = &symtab[i];
		printf("%08x %08x %x %x %x %s\n",s->value,s->size,s->info,s->other,s->shndx,strtab + s->name);
	}
#endif

	/* address */
	sz = 0;
	align = 1;
	for(i=0;i<hdr->shnum;i++) {
		if (shdrs[i].flags & SHF_ALLOC) {
			shdrs[i].addr = (sz + shdrs[i].addralign -1)&~(shdrs[i].addralign -1);
			if (align<shdrs[i].addralign) align = shdrs[i].addralign;
			// if (shdrs[i].type == SHT_NOBITS) sect_bss = i;
			sz += shdrs[i].size;
			printf("%08x %08x %s\n",shdrs[i].addr,shdrs[i].size,shstrtab + shdrs[i].name);
		}
	}

#define	UND	0
#define	ABS	0xfff1
#define	COM	0xfff2

	/* fix com & undefined */
	com = sz; /* end of bss */
	n_syms = 0;

	for(i=0;i<n_symtab;i++) {
		switch(symtab[i].shndx) {
		case UND: {
			void *value;
			char *name = strtab + symtab[i].name;
			if (name[0]==0) break;
			value = find_global(name);
			if (value==NULL) {
				err("unresolve %s\n",name);
			}
			symtab[i].value = (uint32)value;
		//	printf("UND:%p %s\n",symtab[i].value,strtab + symtab[i].name);
			break;
		}
		case ABS:
			break;
		case COM:
			/* i thinks com.value means align (always 4) */
			com = (com+symtab[i].value -1)&~(symtab[i].value-1);
			if (align<symtab[i].value) align = symtab[i].value;
			symtab[i].value = com;
			com+=symtab[i].size;
#ifdef DEBUG
			printf("COM:%p %d %s\n",symtab[i].value,symtab[i].size,strtab + symtab[i].name);
#endif
		default:
			if ((symtab[i].info & 0xf0)==STB_GLOBAL) {
				n_syms++;
			}
			break;
		}
	}
	printf("%08x %08x .common\n",sz,com-sz);
	printf("total = %d(%x) align=%d\n",com,com,align);

	/* load code */
	imgbase = memalign(com,align);
	for(i=0;i<hdr->shnum;i++) {
		if (shdrs[i].flags & SHF_ALLOC) {
			shdrs[i].addr += (uint32)imgbase;
			switch(shdrs[i].type) {
			case SHT_NOBITS: /* bss */
				memset((void*)shdrs[i].addr, 0, shdrs[i].size);
				break;
			default:
				lseek(fd,shdrs[i].offset,SEEK_SET);
				read(fd,(void*)shdrs[i].addr, shdrs[i].size);
				break;
			}
			printf("%p %s\n",shdrs[i].addr,shstrtab + shdrs[i].name);
		}
	}

	/* initialize com */
	memset(imgbase + sz,0,com-sz);

	/* resolve all symbole */

	for(i=0;i<n_symtab;i++) {
		switch(symtab[i].shndx) {
		case ABS:
		case UND:
			break;
		case COM:
			symtab[i].value += (uint32)imgbase;
			break;
		default:
			symtab[i].value += shdrs[symtab[i].shndx].addr;
			break;
		}
	}

#ifdef DEBUG
	printf("after\n");
	for(i=0;i<n_symtab;i++) {
		struct elf_sym_t *s = &symtab[i];
		printf("%08x %08x %x %x %x %s\n",s->value,s->size,s->info,s->other,s->shndx,strtab + s->name);
	}
#endif

	/* relocation */
	for(i=0;i<hdr->shnum;i++) {
		int j,n_reltab;
		uint32 sectbase;

		if (shdrs[i].type!=SHT_RELA || !(shdrs[shdrs[i].info].flags&SHF_ALLOC)) continue;

		sectbase = shdrs[shdrs[i].info].addr;
#ifdef DEBUG
		printf("%s -> %s(%p)\n",shstrtab + shdrs[i].name, shstrtab + shdrs[shdrs[i].info].name,sectbase);
#endif
		reltab = malloc_read(fd,shdrs[i].offset,shdrs[i].size);
		n_reltab = shdrs[i].size / sizeof(struct elf_rela_t);


		for(j=0;j<n_reltab;j++) {
			int sym;
			uint32 value;
			if (ELF32_R_TYPE(reltab[j].info) != R_SH_DIR32) {
				err("unsupport rela type %02x\n",ELF32_R_TYPE(reltab[j].info));
			}
			sym = ELF32_R_SYM(reltab[j].info);
			switch(symtab[sym].info&0xf) {
			case STT_OBJECT:
			case STT_FUNC:
			case STT_SECTION:
				break;
			case STN_UNDEF:
				if (symtab[sym].shndx==UND) break;
			default:
				err("unsupport sym %x\n",symtab[sym].info);
			}
			value = *(uint32*)(sectbase + reltab[j].offset);
#ifdef DEBUG
			printf("%p %x %x -> %x (%s+%d)\n",
				sectbase + reltab[j].offset,
				reltab[j].addend,
				value,
				value + symtab[sym].value + reltab[j].addend,
				((symtab[sym].info&0xf)==STT_SECTION)?(shstrtab + shdrs[symtab[sym].shndx].name) : (strtab + symtab[sym].name),
				value + reltab[j].addend
			);
#endif
			*(uint32*)(sectbase + reltab[j].offset) += symtab[sym].value + reltab[j].addend;
		}

		free(reltab);
	}

	close(fd);

	printf("finish\n");

	
	{
		struct sym_t	*syms; /* glibal symbols */
		struct elf_obj_t	*elfobj;

		free(shdrs);
		free(shstrtab);

		/* build global symbol table */
		syms = malloc(sizeof(syms[0])*(n_syms+1));

		j = 0;
		for(i=0;i<n_symtab;i++) {
			if ((symtab[i].info & 0xf0)==STB_GLOBAL && symtab[i].info!= (STN_UNDEF|STB_GLOBAL)) {
				syms[j].name = strtab + symtab[i].name;
				syms[j].addr = (void*)symtab[i].value;
			//	printf("%p %s\n",syms[j].addr,syms[j].name);
				j++;
			}
		}
		syms[n_syms].name = NULL;
		syms[n_syms].addr = NULL;
	//	printf("%d %d\n",j,n_syms);

		free(symtab);

		elfobj = malloc(sizeof(*elfobj)+strlen(name));

		elfobj->data = imgbase;
		elfobj->syms = syms;
		elfobj->strtab = strtab;
		strcpy(elfobj->name,name);
		return elfobj;
	}

errret:
	close(fd);
	if (shdrs) free(shdrs);
	if (symtab) free(symtab);
	if (strtab) free(strtab);
	if (shstrtab) free(shstrtab);
	if (imgbase) free(imgbase);

	return NULL;
}

void *elf_sym(struct elf_obj_t *elfobj,const char *name)
{
	return find_sym(elfobj->syms,name);
}

int elf_free(struct elf_obj_t *elfobj)
{
	free(elfobj->data);
	free(elfobj->syms);
	free(elfobj->strtab);
	free(elfobj);
	return 0;
}

#include <dlfcn.h>

void *dlopen (const char *dlname, int mode)
{
	return elf_load(dlname);
}

void *dlsym (void *dl, const char *name)
{
	void *ret;
	char buf[256]; //= alloca(strlen(name+2));
	buf[0] = '_';
	strcpy(buf+1,name);
	printf("dlsym:%s->%s\n",name,buf);
	ret = elf_sym(dl,buf);
	if (ret) printf("%p\n",ret);
	return ret;
}

int dlclose (void *dl)
{
	return elf_free(dl);
}

char *dlerror (void)
{
	return "error\n";
}

#ifdef TEST
int main(int argc,char **argv)
{
	void *p,*func;
	p = elf_load(argv[1]);
	func = elf_sym(p,"_WriteClient");
	if (!func) {
		printf("can't find %s\n","_WriteClient");
	}
	func = elf_sym(p,"_abcd");
	if (!func) {
		printf("can't find %s\n","abcd");
	}
	elf_free(p);
	return 0;
}
#endif
