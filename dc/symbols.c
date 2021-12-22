struct sym_t {
	char *name;
	void *addr;
};

#define	DEFSYM(a)	{"_" #a,&a},

extern __assert();
extern __fixunssfsi();
extern __movstr_i4_even();
extern __sdivsi3_i4();
extern __udivsi3_i4();
extern atoi();
extern ceil();
extern closedir();
extern fclose();
extern floorf();
extern fopen();
extern free();
extern fwrite();
extern gettimeofday();
extern malloc();
extern memcmp();
extern memcpy();
extern memset();
extern opendir();
extern powf();
extern readdir();
extern sprintf();
extern strcasecmp();
extern strcat();
extern strchr();
extern strcmp();
extern strcpy();
extern strlen();
extern strncpy();
extern strrchr();
extern tolower();
extern vid_set_mode();
extern vram_s();
extern vsprintf();
extern __movstr_i4_odd();
extern atan2f();
extern atof();
extern fprintf();
extern fread();
extern localtime();
extern qsort();
extern rand();
extern sscanf();
extern strdup();
extern strstr();
extern strtok();
extern time();
struct sym_t symbols[]={
	DEFSYM(__assert)
	DEFSYM(__fixunssfsi)
	DEFSYM(__movstr_i4_even)
	DEFSYM(__sdivsi3_i4)
	DEFSYM(__udivsi3_i4)
	DEFSYM(atoi)
	DEFSYM(ceil)
	DEFSYM(closedir)
	DEFSYM(fclose)
	DEFSYM(floorf)
	DEFSYM(fopen)
	DEFSYM(free)
	DEFSYM(fwrite)
	DEFSYM(gettimeofday)
	DEFSYM(malloc)
	DEFSYM(memcmp)
	DEFSYM(memcpy)
	DEFSYM(memset)
	DEFSYM(opendir)
	DEFSYM(powf)
	DEFSYM(readdir)
	DEFSYM(sprintf)
	DEFSYM(strcasecmp)
	DEFSYM(strcat)
	DEFSYM(strchr)
	DEFSYM(strcmp)
	DEFSYM(strcpy)
	DEFSYM(strlen)
	DEFSYM(strncpy)
	DEFSYM(strrchr)
	DEFSYM(tolower)
	DEFSYM(vid_set_mode)
	DEFSYM(vram_s)
	DEFSYM(vsprintf)
	DEFSYM(__movstr_i4_odd)
	DEFSYM(atan2f)
	DEFSYM(atof)
	DEFSYM(fprintf)
	DEFSYM(fread)
	DEFSYM(localtime)
	DEFSYM(qsort)
	DEFSYM(rand)
	DEFSYM(sscanf)
	DEFSYM(strdup)
	DEFSYM(strstr)
	DEFSYM(strtok)
	DEFSYM(time)
	{0,0},
};
