#ifndef _NETDB_H_
#define	_NETDB_H_

#define	MAXHOSTNAMELEN	64

struct	hostent {
	const char	*h_name;	/* official name of host */
	char	**h_aliases;	/* alias list */
	int	h_addrtype;	/* host address type */
	int	h_length;	/* length of address */
	char	**h_addr_list;	/* list of addresses from name server */
#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */
};

struct hostent	*gethostbyaddr (const char *, int, int);
struct hostent	*gethostbyname (const char *);
int gethostname(char *name, size_t len);
int	inet_addr(const char *s);

#endif
