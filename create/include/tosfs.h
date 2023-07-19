#include <stdint.h>

typedef uint8_t sector_t[512];
typedef uint8_t block_t[4096];

typedef struct{
	uint8_t DAttribs; // bit 7 set = active or bootable
	uint8_t CHSPStart[3];
	uint8_t PType;
	uint8_t CHSAOLPSector[3];
	uint32_t LBAOPStart;
	uint32_t NOSIPartition;
} __attribute((packed)) _MBRPartTableEntry_t;

typedef struct{
	uint32_t UDId;
	uint16_t Rsvd0;
	_MBRPartTableEntry_t PTEntries[4];
} __attribute((packed)) MasterBootRecord_t;

typedef struct{
	uint8_t Sig[8]; // set to "TOSFS\x00\x00\x00"
	uint64_t LFAEBlock;
	uint64_t BARSLba;
	uint64_t BSLba;
	uint64_t PSILba;
	uint64_t LBAOBOPart;
	uint8_t PName[11];
	uint16_t RSCount;
	uint64_t BARSSec; // can be optimized (calculation) (complete) as: 1+(((((PSILba-RSCount)*512)>>14)+1)>>12)
	uint16_t VNumber;
	uint64_t RDSLba;
	uint8_t Res[425];
} __attribute((packed)) TOSFS10Header_t;

enum TOSFS10Perms{
	CRead = (1 << 0),
	CWrite = (1 << 1),
	CCreate = (1 << 2),
	CDelete = (1 << 3),
	CExecute = (1 << 4),
	Overwrite = (1 << 5),
};

typedef struct{
	uint64_t Perms;
	uint8_t NAGOGroup[23];
	uint8_t Nullterm;
} __attribute((packed)) _TOSFS10PermEntry_t;

typedef struct{
	uint64_t MSPerms;
	uint64_t GUPerms;
	_TOSFS10PermEntry_t PEntries[127];
	uint8_t reserved[16];
} __attribute((packed)) TOSFS10FilePermsDataBlock_t;

enum TOSFS10BlockAllocType{
	Unused = 0,
	FODAFull = 1,
	FODANFull = 2,
	FODData = 3,
};

typedef struct{
	uint8_t _0 : 2;
	uint8_t _1 : 2;
	uint8_t _2 : 2;
	uint8_t _3 : 2;
} __attribute((packed)) TOSFS10BlockAllocRegionEntryGroup_t;

enum TOSFS10FileAttributes{
	FAEUsed = (1 << 0),
	IFHidden = (1 << 1),
	IFSpan = (1 << 2),
	IFDir = (1 << 3),
	IFSLink = (1 << 4),
};

typedef struct{
	uint8_t FAttributes; // to be used with enum TOSFS10FileAttributes
	uint32_t FLMTime;
	uint32_t FLRTime;
	uint32_t FCTime;
	uint64_t FSIBytes;
	uint64_t FPSBlock;
	uint64_t FSBlock;
	uint8_t FName[466];
	uint8_t NTerminator; // set to 0
} __attribute((packed)) _TOSFS10FileAllocEntry_t;

typedef struct{
	_TOSFS10FileAllocEntry_t FAEntries[4];
} __attribute((packed)) TOSFS10FileAllocEntryBlock_t;

typedef struct{
	uint8_t FRFData[4088];
	uint64_t NFRBlock;
} __attribute((packed)) TOSFS10FileDataBlock_Dynamic_t;

typedef struct{
	uint8_t FRFData[4096];
} __attribute((packed)) TOSFS10FileDataBlock_Span_t;

typedef struct{
	uint8_t EUsed; // 0: unused; 1: used
	uint64_t FODNHash; // sum of all characters as a uint64_t including null bytes excluding preallocated null terminator divided by 128 as a double
	uint64_t FODELba;
} __attribute((packed)) _TOSFS10DirectoryDataEntry_t;

typedef struct{
	_TOSFS10DirectoryDataEntry_t DRDData[240];
	uint64_t Rsvd;
	uint64_t NDRBlock;
} __attribute((packed)) TOSFS10DirectoryDataBlock_t;
