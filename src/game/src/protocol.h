#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

inline const char *encode_byte(char ind)
{
	static char a[8];
	*((char *) a) = ind;
	return (a);
}

inline const char *encode_2bytes(sh_int ind)
{
	static char a[8];
	*((sh_int *) a) = ind;
	return (a);
}

inline const char *encode_4bytes(int ind)
{
	static char a[8];
	*((int *) a) = ind;
	return (a);
}

inline BYTE decode_byte(const void * a)
{
	return (*(BYTE *) a);
}

inline WORD decode_2bytes(const void * a)
{
	return (*((WORD *) a));
}

inline INT decode_4bytes(const void *a)
{
	return (*((INT *) a));
}

#endif
