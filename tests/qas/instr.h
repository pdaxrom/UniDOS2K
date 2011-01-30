#define S_NOTHING	0xffffffff

enum {	L_FLAG = 0x01000000,
	B_FLAG = 0x00400000,
	S_FLAG = 0x00100000,
	P_FLAG = 0x0000f000,
	T_FLAG = 0x00200000  };

enum {	E_FLAG = 0x00400000  };

enum {	EQ = 0x00000000,
	NE = 0x10000000,
	CS = 0x20000000,
	CC = 0x30000000,
	MI = 0x40000000,
	PL = 0x50000000,
	VS = 0x60000000,
	VC = 0x70000000,
	HI = 0x80000000,
	LS = 0x90000000,
	GE = 0xa0000000,
	LT = 0xb0000000,
	GT = 0xc0000000,
	LE = 0xd0000000,
	AL = 0xe0000000,
	NV = 0xf0000000  };

enum {	IA = 0x00800000,
	IB = 0x01800000,
	DA = 0x00000000,
	DB = 0x01000000  };

enum {	EA_S = 0x00800000,	/* IA */
	FA_S = 0x01800000,	/* IB */
	ED_S = 0x00000000,	/* DA */
	FD_S = 0x01000000,	/* DB */
	EA_L = 0x01000000,	/* DB */
	FA_L = 0x00000000,	/* DA */
	ED_L = 0x01800000,	/* IB */
	FD_L = 0x00800000  };	/* IA */

enum {	T_PREC_S = 0x00000000,
	T_PREC_D = 0x00008000,
	T_PREC_E = 0x00400000,
	T_PREC_P = 0x00408000  };

enum {	PREC_S   = 0x00000000,
	PREC_D   = 0x00000080,
	PREC_E   = 0x00080000  };

enum {	RND_N = 0x00000000,
	RND_P = 0x00000020,
	RND_M = 0x00000040,
	RND_Z = 0x00000060  };

enum {	DATAPRO		= 1,
	MULT		= 2,
	DTRANS		= 3,
	MDTRANS_L	= 4,
	MDTRANS_S	= 5,
	BRANCH		= 6,
	SWI		= 7,
	FP_DTRANS	= 8,
	FP_RTRANS	= 9,
	FP_DATAOP	= 10,
	FP_STATUS	= 11  };
