/*
	CCI hardware ID header file, here we start from
	EVT0 and end with PVT2.
*/

enum {
	EVT0,
	EVT1,
	EVT2,
	DVT1,
	DVT2,
	PVT1,
	PVT2
};

enum {
	NO_BAND9,
	BAND9
};

extern int cci_hw_id;
extern int cci_band_id;
