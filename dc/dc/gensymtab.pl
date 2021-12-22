#
#
# gensymtab
#
# generate import library

print <<EOL;
struct sym_t {
	char *name;
	void *addr;
};

#define	DEFSYM(a)	{"_" #a,&a},

EOL
while(<>) {
	chomp;
	if (/^ +U _(.*)$/ && !$syms{$1}) {
		$syms{$1} = 1;
		push @symbol,$1;
		print "extern $1();\n";
	}
	
}

print "struct sym_t symbols[]={\n";
foreach(@symbol) {
	print "\tDEFSYM($_)\n";
}
print "\t{0,0},\n";
print "};\n";
