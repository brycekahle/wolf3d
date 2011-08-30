// ID_HEAD.H


#define	EXTENSION	"WLF"
#define WOLF

#define	TEXTGR	0
#define	CGAGR	1
#define	EGAGR	2
#define	VGAGR	3

#define GRMODE	VGAGR

#include "VERSION.H"

#ifdef _MSC_VER

#define asm __asm
#define _CRT_SECURE_NO_WARNINGS 1

#endif

typedef	enum	{False,True}	boolean;
typedef	unsigned	char		byte;
typedef	unsigned	short		word;
typedef	unsigned	long		longword;
typedef	byte *					Ptr;

typedef	struct
		{
			int	x,y;
		} Point;

typedef	struct
		{
			Point	ul,lr;
		} Rect;


void	Quit (char *error);		// defined in user program

