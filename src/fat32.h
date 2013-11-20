#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <iconv.h>
#include <locale.h>
#include <langinfo.h>
#include <errno.h>
#include <ctype.h>

typedef struct __attribute__((__packed__)) fatdino_bootsector {
  uint8_t	BS_jmpBoot[3];
  uint8_t	BS_OEMName[8];
  uint16_t	BPB_BytsPerSec;
  uint8_t	BPB_SecPerClus;
  uint16_t	BPB_RsvdSecCnt;
  uint8_t	BPB_NumFATs;
  uint16_t	BPB_RootEntCnt;
  uint16_t	BPB_TotSec16;
  uint8_t	BPB_Media;
  uint16_t	BPB_FATSz16;
  uint16_t	BPB_SecPerTrk;
  uint16_t	BPB_NumHeads;
  uint32_t	BPB_HiddSec;
  uint32_t	BPB_TotSec32;
  uint32_t	BPB_FATSz32;
  uint16_t	BPB_ExtFlags;
  uint16_t	BPB_FSVer;
  uint32_t	BPB_RootClus;
  uint16_t	BPB_FSInfo;
  uint16_t	BPB_BkBootSec;
  uint8_t	BPB_Reserved[12];
  uint8_t	BS_DrvNum;
  uint8_t	BS_Reserved1;
  uint8_t	BS_BootSig;
  uint32_t	BS_VolID;
  uint8_t	BS_VolLab[11];
  uint8_t	BS_FilSysType[8];
  uint8_t	bootcode[420];
  uint16_t	signature;
} fatdino_BPB;


#define ATTR_READ_ONLY	0x01
#define ATTR_HIDDEN	0x02
#define ATTR_SYSTEM	0x04
#define ATTR_VOLUME_ID	0x08
#define ATTR_DIRECTORY	0x10
#define ATTR_ARCHIVE	0x20
#define ATTR_LONG_NAME	0x0F
			/* ATTR_READ_ONLY |
			 * ATTR_HIDDEN |
			 * ATTR_SYSTEM |
			 * ATTR_VOLUME_ID
			 */
#define NTRES_FNLOWER	0x08
#define NTRES_EXTLOWER	0x10

typedef struct __attribute__((__packed__)) fatdino_direntry {
  uint8_t	DIR_Name[11];
  uint8_t	DIR_Attr;
  uint8_t	DIR_NTRes;	/* 0x00 -> lfn, uppercase;
				 * 0x08 -> name lowercase;
				 * 0x10 -> ext lowercase;
				 * NOTE: there is no option for 3rd bit to 
				 * NOT be equal to 4th
				 * bits 0-2 & 5-7 - FAT+ file size part,
				 * do not touch
				 */
  uint8_t	DIR_CrtTimeTenth;
  uint16_t	DIR_CrtTime;
  uint16_t	DIR_CrtDate;
  uint16_t	DIR_LstAcc;
  uint16_t	DIR_FstClusHI;
  uint16_t	DIR_WrtTime;
  uint16_t	DIR_WrtDate;
  uint16_t	DIR_FstClusLO;
  uint32_t	DIR_FileSize;
} fatdino_DIR;

#define LAST_LONG_ENTRY	0x40

typedef struct __attribute__((__packed__)) fatdino_lfn {
  uint8_t	LDIR_Ord;
  uint8_t	LDIR_Name1[10];
  uint8_t	LDIR_Attr;
  uint8_t	LDIR_Type;
  uint8_t	LDIR_Chksum;
  uint8_t	LDIR_Name2[12];
  uint16_t	LDIR_FstClusLO;
  uint8_t	LDIR_Name3[4];
} fatdino_LDIR;

typedef struct __attribute__((__packed__)) fatdino_fsinfo {
  uint32_t	FSI_LeadSig;
  uint8_t	FSI_Reserved1[480];
  uint32_t	FSI_StrucSig;
  uint32_t	FSI_Free_Count;
  uint32_t	FSI_Nxt_Free;
  uint8_t	FSI_Reserved2[12];
  uint32_t	FSI_TrailSig;
} fatdino_FSINFO;

/*
  function that returns bytes count string in human-readable form
*/
char *fatdino_bytesToHuman(unsigned int bytes);

/*
  function that converts fatdino_DIR's date format into human-readable form,
  given string must be allocated and must be at least 20 bytes long when no ns given
  and at least 24 bytes when ns>0, 
  date format: 'dd-mm-yyyy hh:mm:ss.n',
  return 0 on succes, -1 on fail
*/
int fatdino_dateToHuman(char *string, uint16_t Date, uint16_t Time, uint8_t TimeTenth);
/*
  function that fills buffer with a BPB/BS structure of given FAT32 device,
  returns 0 on success and -1 on fail
*/
int fatdino_getBPB(char *device, fatdino_BPB *buffer);

/*
  function to check if given device is FAT32-formatted,
  if so returns 1, else 0, on error: -1
*/
int fatdino_checkDev(char *device, fatdino_BPB *bpb);

/*
  function that reads given cluster and returns at address pointed by buf,
  returns 1 on success and -1 on fail
*/
int fatdino_getCluster(char *device, fatdino_BPB *bpb, uint8_t *buf, uint32_t cluster);

/*
  function that returns number of FAT sector with given cluster chain element,
  returns sector number if valid bpb given, otherwise result is undefined
*/
uint32_t fatdino_getFATSector(fatdino_BPB *bpb, uint32_t cluster);

/*
  functions that finds next cluster in cluster chain of given cluster number,
  returns 0 on fail, cluster number on success
*/
uint32_t fatdino_getNextCluster(char *device, fatdino_BPB *bpb, uint32_t cluster);

/*
  function that searches for entry in given directory cluster,
  modifies cluster if entry found in another cluster
  returns offset to matching directory entry 
  or (uint32_t)-1 when not found or error
*/
uint32_t fatdino_findEntry(char *device, fatdino_BPB *bpb, uint32_t *cluster, char *entry, short followChain);

/*
  function that gets unicode long file name of entry in dir struct array
  pointed by offset, returns 1 if LDIR found, 0 if not and -1 on fail
*/
int fatdino_getLFN(char *device, char *ustring, unsigned int *ustringSz, fatdino_BPB *bpb, uint32_t cluster, uint16_t offset);

/*
  function that converts 8.3 filename to readable form
*/
char *fatdino_SfnToHuman(char *shortname, uint8_t NTRes);

/*
  function that returns size of file/dir at given path or 0 at error
*/
unsigned int fatdino_getSize(char *device, fatdino_BPB *bpb, char *fpath);

/*
  function that returns cluster number of file/dir at given path,
  0 on not found or 1 on another error
*/
unsigned int fatdino_getClusterNumber(char *device, fatdino_BPB *bpb, char *fpath);

/*
  function that gets DIR structure (if pointer is not NULL),
  number of containing cluster and struct offset,
  on error returns 1, on success - 0
*/
int fatdino_getDir(char *device, fatdino_BPB *bpb, char *fpath, fatdino_DIR *dirstruct, unsigned int *cluster, unsigned int *offset);
/*
  function that transforms string to uppercase
*/
void fatdino_upperCase(char *string);

/*
 * here is the beggining of functions that are needed only for writing
 */

/*
 * function that converts string from default system encoding to UTF-16,
 * src must be double zero terminated
 * returns 1 on success and -1 on fail
 */
int fatdino_ToUTF16(char *src, char *dst);


/*
 * function that converts string to default system encoding from UTF-16,
 * src must be double zero terminated
 * returns 1 on success and -1 on fail
 */
int fatdino_FromUTF16(char *src, char *dst);

/*
 * function that converts string from one encoding to another using iconv
 */
int fatdino_iconvImplementation(char *src, char *src_enc, char *dst, char *dst_enc);

/*
 * function that computes cluster number basing on FAT sector and offset in that sector
 */
//TODO:
uint32_t fatdino_FatToCluster(fatdino_BPB *bpb, uint32_t fat, uint16_t offset);

/*
  function that decides if name given as UTF-16 could be written as SFN only
  and returns SFN and if neccessary LFN created from given name,
  returns 0 if only SFN created, 1 if LFN created too and -1 on fail
*/
//TODO:
int fatdino_nameToSfnAndLfn(char *name, char *sfn, char *lfn);

/*
  function that finds next free cluster, starting on cluster start, returns cluster number or 0 on error
*/
//TODO: find clusters freed after file/dir deletion (maybe in other function?)
uint32_t fatdino_findNextFree(char *device, fatdino_BPB *bpb, uint32_t start);

/*
  function that gets FSINFO structure sector from disk,
  returns 0 on success, -1 on fail and 1 on fsinfo damaged
*/
int fatdino_getFSINFO(char *device, fatdino_BPB *bpb, fatdino_FSINFO *fsinfo);

/*
  function that creates empty directory at given cluster
  and zeroes rest of the cluster, returns 0 on success and 1 on fail
*/
//TODO:
int fatdino_createDir(char *device, fatdino_BPB *bpb, uint32_t cluster);

/*
  function that adds entry to the existing directory,
  if neccessary follow cluster chain or adds new cluster to it
  returns 0 on success and 1 on fail
*/
//TODO:
int fatdino_addDirEntry(char *device, fatdino_BPB *bpb, uint32_t cluster, char *targetname, uint32_t targetclus, uint8_t targetattr, uint8_t targetsize);

/*
  function that creates LDIR structure chain for lfn given,
  results num of structures created
*/
//TODO:
uint8_t fatdino_createLDIR(char *lfn, char *buffer);

/*
  function that creates DIR structure and fills 32-byte buffer
*/
//TODO:
void fatdino_createDIR(char *sfn, char *buffer, uint32_t cluster, uint8_t attr, uint32_t filesize);