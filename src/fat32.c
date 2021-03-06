#include "fat32.h"

char *fatdino_bytesToHuman(unsigned int bytes) {
  int unit = 0;
  char *result = malloc(10);
  double res = bytes;
  while(res>1024) {
    res = res / 1024;
    unit++;
  }
  const char units[9] = {'\0','K','M','G','T','P','E','Z','Y'};
  char cunit = units[unit];
  if(cunit!='\0')
    sprintf(result,"%.2f %cB",res,cunit);
  else
    sprintf(result,"%.0f B",res,cunit);
  return result;
}

int fatdino_dateToHuman(char *string, uint16_t Date, uint16_t Time, uint8_t TimeTenth) {
#define mDay	0x1f
#define mMonth	0x1e0
#define mYear	0xfe00
  int day = Date & mDay;
  int month = (Date & mMonth) >> 5;
  int year = 1980 + ((Date & mYear) >> 9);
#define mSec	0x1f
#define mMin	0x7e0
#define mHour	0xf800
  int sec = 2 * (Time & mSec);
  int min = (Time & mMin) >> 5;
  int hour = (Time & mHour) >> 11;
  if(day > 0 && day < 32 && month > 0 && month < 13 && year > 1979 && year < 2108 && 
    sec >=0 && sec < 59 && min >= 0 && min < 60 && hour >= 0 && hour < 24) {
    if(TimeTenth>0 && TimeTenth<200) {
      sec += TimeTenth/100;
      sprintf(string,"%02d-%02d-%d %02d:%02d:%02d.%d",day, month, year, hour, min, sec, (TimeTenth%100)*10);
    }
    else
      sprintf(string, "%02d-%02d-%d %02d:%02d:%02d",day, month, year, hour, min, sec);
    return 0;
  }
  else
    return -1;
}

int fatdino_getBPB(char *device, fatdino_BPB *buffer) {
  //open device and read first sector
  int fd = open(device, O_RDONLY);
  unsigned char *buff;
  if(fd > 0) {
    buff = malloc(512);
    if(read(fd, buff, 512) == -1) {
      close(fd);
      return -1;
    }
    close(fd);
  } else return -1;
  
  memcpy(buffer,buff,sizeof(fatdino_BPB));
  
  return 0;
}

int fatdino_checkDev(char *device, fatdino_BPB *bpb) {
  unsigned int ClusCnt = 
    ((unsigned int)bpb->BPB_TotSec32 - 
      ((unsigned int)bpb->BPB_RsvdSecCnt + 
      (unsigned int)bpb->BPB_NumFATs * (unsigned int)bpb->BPB_FATSz32)
    ) / (unsigned int)bpb->BPB_SecPerClus;
  if(
    bpb->BPB_BytsPerSec == 512 &&
    bpb->BPB_NumFATs == 2 &&
    bpb->signature == 0xaa55 &&
    bpb->BPB_RootEntCnt == 0 &&
    1//ClusCnt >= 65525
  ) {
    return 1;
  } else return 0;
}

uint32_t fatdino_getFATSector(fatdino_BPB *bpb, uint32_t cluster) {
  uint32_t FATstart = bpb->BPB_RsvdSecCnt;
  return FATstart + (cluster / 128);
}

uint32_t fatdino_getNextCluster(char *device, fatdino_BPB *bpb, uint32_t cluster) {
  unsigned int offset = fatdino_getFATSector(bpb, cluster) * 512;
  int fd = open(device, O_RDONLY);
  uint32_t next;
  uint32_t *buff = malloc(512);
  if(fd > 0 && lseek(fd,offset,SEEK_SET)==offset) {
    if(read(fd, buff, 512) == -1) {
      close(fd);
      return 0;
    }
    next = buff[cluster%128];
    free(buff);
    close(fd);
    return next;
  } else return 0;
}

int fatdino_getCluster(char *device, fatdino_BPB *bpb, uint8_t *dir, uint32_t cluster) {
  unsigned int offset = (bpb->BPB_RsvdSecCnt + (bpb->BPB_NumFATs * bpb->BPB_FATSz32) +
    (cluster-2) * bpb->BPB_SecPerClus) * 512;
  int fd = open(device, O_RDONLY);
  if(fd > 0 && lseek(fd,offset,SEEK_SET)==offset) {
    if(read(fd, dir, 512 * bpb->BPB_SecPerClus) == -1) {
      close(fd);
      return -1;
    }
    close(fd);
    return 1;
  }
}

int fatdino_getLFN(char *device, char *ustring, unsigned int *ustringSz, fatdino_BPB *bpb, uint32_t cluster, uint16_t offset) {
  fatdino_DIR *dir = malloc(bpb->BPB_SecPerClus * 512);
  int rdir = fatdino_getCluster(device, bpb, (uint8_t*)dir, cluster);
  if(rdir==1) {
    fatdino_LDIR *entry = (fatdino_LDIR*)dir + (offset / sizeof(fatdino_LDIR)) - 1;
    if(((entry->LDIR_Ord) & LAST_LONG_ENTRY - 1) == 1) {
      char *lfn = malloc(256);
      int lfnSz = 0;
      char *tmp = lfn;
      do {
	//do stuff
	lfnSz+=26;
	memcpy(tmp,entry->LDIR_Name1,10);
	tmp += 10;
	memcpy(tmp,entry->LDIR_Name2,12);
	tmp += 12;
	memcpy(tmp,entry->LDIR_Name3,4);
	tmp += 4;
	entry--;
	if(entry<(fatdino_LDIR*)dir) {
	  //we need to load previous cluster to memory
	  offset = (bpb->BPB_SecPerClus * 512) - sizeof(fatdino_LDIR);
	  cluster--;
	  int res = fatdino_getCluster(device, bpb, (uint8_t*)dir, cluster);
	  entry = (fatdino_LDIR*)dir + (offset / sizeof(fatdino_LDIR));
	  continue;
	}
      }
      while((((entry+1)->LDIR_Ord) & LAST_LONG_ENTRY) != 0x40);
      setlocale( LC_ALL, "" );
      iconv_t cd;
      if((cd = iconv_open(nl_langinfo(CODESET),"UTF-16LE")) == (iconv_t) - 1) {
	cd = NULL;
	return -1;
      }
      errno = 0;
      tmp = lfn;
      char * tmpo = ustring;
      int i;
      for(i = 0;i<lfnSz;i+=2) {
	if((lfn[i] == '\0') && (lfn[i+1] == '\0')) {
	  i+=2;
	  break;
	}
      }
      size_t sizei = i;
      size_t sizeo = lfnSz;
      size_t rc = iconv(cd, &tmp, &sizei, &tmpo, &sizeo);
      if(rc == (size_t) - 1 || errno!=0)
	return -1;
      iconv_close(cd);
      *ustringSz = lfnSz - sizeo;
      ustring=realloc(ustring,*ustringSz);
      free(dir);
      return 1;
    }
    else {
      free(dir);
      return 0;
    }
  } else return -1;
  return 0;
}

uint32_t fatdino_findEntry(char *device, fatdino_BPB *bpb, uint32_t *cluster, char *entry, short followChain) {
  char * entryc = malloc(strlen(entry) + 1);
  strcpy(entryc,entry);
  fatdino_upperCase(entryc);
  uint8_t *dir = malloc(bpb->BPB_SecPerClus * 512);
  int rdir = fatdino_getCluster(device, bpb, dir, *cluster);
  if(rdir==-1)
    return (uint32_t)-1;
  int i;
  for(i = 0; i < bpb->BPB_SecPerClus * 512; i+=sizeof(fatdino_DIR)) {
    fatdino_DIR *en = (fatdino_DIR*)(dir + i);
    char *sfn = fatdino_SfnToHuman(en->DIR_Name, en->DIR_NTRes);
    fatdino_upperCase(sfn);
    if(sfn[0]=='\0')
      return (uint32_t)-1;
    int cmpr = strcmp(entryc, sfn);
    char *lfn = malloc(256);
    unsigned int lfnSz = 0;
    int isLFN = fatdino_getLFN(device, lfn, &lfnSz, bpb, *cluster, i);
    fatdino_upperCase(lfn);
    if(isLFN==1) {
      //lfns match
      cmpr = strcmp(lfn, entryc);
      if(cmpr == 0)
      return i;
    }
    //lfns not match
    if((cmpr == 0) && ((en->DIR_Attr&ATTR_LONG_NAME)!=ATTR_LONG_NAME)) {
      //sfns match
      if(isLFN==0)
	return i;
      else return (uint32_t)-1;
    } 
    else {
      //sfns not match
      if(isLFN==-1)
	return (uint32_t)-1;
    }
  }
  return (uint32_t)-1;
}

char *fatdino_SfnToHuman(char *shortname, uint8_t NTRes) {
  char *buf = malloc(13);
  memset(buf,0,13);
  strncpy(buf,shortname,8);
  if((NTRes&NTRES_FNLOWER) != 0) {
    char *casebuf = buf;
    while(*casebuf != '\0') {
      *casebuf = tolower(*casebuf);
      casebuf++;
    }
  }
  char *tmpbuf = buf+7;
  while(tmpbuf[0]==' ') {
    tmpbuf--;
  }
  if(shortname[8]!=' ') {
    tmpbuf[1]='.';
    strncpy(tmpbuf+2,(char*)(shortname+8),3);
    if((NTRes&NTRES_EXTLOWER) != 0) {
      char *casebuf = tmpbuf+2;
      while(*casebuf != '\0') {
	*casebuf = tolower(*casebuf);
	casebuf++;
      }
    }
    tmpbuf+=4;
    while(tmpbuf[0]==' ') {	
      tmpbuf--;
    }
    tmpbuf[1]='\0';
  } else memset(tmpbuf+1,'\0',13 - (tmpbuf - buf));
  return buf;
}

unsigned int fatdino_getSize(char *device, fatdino_BPB *bpb, char *fpath) {
  uint32_t clus = bpb->BPB_RootClus;
  char *path = malloc(strlen(fpath)+2);
  strcpy(path,fpath);
  char *tmp = path;
  char *end = NULL;
  char *entry = NULL;
  if(path[strlen(path)-1] == '/')
  path[strlen(path)-1] = '\0';
  tmp++;
  while(end = strchr(tmp,'/')) {
    uint8_t *dir = malloc(bpb->BPB_SecPerClus * 512);
    entry = (char*)realloc((void*)entry, end + 1 - tmp);
    strncpy(entry, tmp, end - tmp);
    entry[end - tmp] = '\0';
    uint32_t offset = fatdino_findEntry(device, bpb, &clus, entry, 1);
    if(offset == (uint32_t)-1) {
      //not found
      return 0;
    }
    int rdir = fatdino_getCluster(device, bpb, dir, clus);
    if(rdir==-1) {
      //error reading dir entry
      return 0;
    }
    fatdino_DIR *en = (fatdino_DIR*)(dir + offset);
    if((en->DIR_Attr&ATTR_DIRECTORY)!=ATTR_DIRECTORY) {
      //not a directory!
      return 0;
    }
    clus = (en->DIR_FstClusHI<<16) + en->DIR_FstClusLO;
    tmp = end + 1;
  }
  uint32_t offset = fatdino_findEntry(device, bpb, &clus, tmp, 1);
  if(offset == (uint32_t)-1) {
    //not found
    return 0;
  }
  uint8_t *dir = malloc(bpb->BPB_SecPerClus * 512);
  int rdir = fatdino_getCluster(device, bpb, dir, clus);
  if(rdir!=1) {
    //error gettig cluster
    return 0;
  }
  fatdino_DIR *en = (fatdino_DIR*)dir + (offset / sizeof(fatdino_DIR));
  clus = (en->DIR_FstClusHI<<16) + en->DIR_FstClusLO;
  unsigned int filesize = en->DIR_FileSize;
  return filesize;
}

unsigned int fatdino_getClusterNumber(char *device, fatdino_BPB *bpb, char *fpath) {
  uint32_t clus = bpb->BPB_RootClus;
  char *path = malloc(strlen(fpath)+2);
  strcpy(path,fpath);
  char *tmp = path;
  char *end = NULL;
  char *entry = NULL;
  if(path[strlen(path)-1] == '/')
  path[strlen(path)-1] = '\0';
  tmp++;
  while(end = strchr(tmp,'/')) {
    uint8_t *dir = malloc(bpb->BPB_SecPerClus * 512);
    entry = (char*)realloc((void*)entry, end + 1 - tmp);
    strncpy(entry, tmp, end - tmp);
    entry[end - tmp] = '\0';
    uint32_t offset = fatdino_findEntry(device, bpb, &clus, entry, 1);
    if(offset == (uint32_t)-1) {
      //not found
      return 0;
    }
    int rdir = fatdino_getCluster(device, bpb, dir, clus);
    if(rdir==-1) {
      //error reading dir entry
      return 0;
    }
    fatdino_DIR *en = (fatdino_DIR*)(dir + offset);
    if((en->DIR_Attr&ATTR_DIRECTORY)!=ATTR_DIRECTORY) {
      //not a directory!
      return 0;
    }
    clus = (en->DIR_FstClusHI<<16) + en->DIR_FstClusLO;
    tmp = end + 1;
  }
  uint32_t offset = fatdino_findEntry(device, bpb, &clus, tmp, 1);
  if(offset == (uint32_t)-1) {
    //not found
    return 0;
  }
  uint8_t *dir = malloc(bpb->BPB_SecPerClus * 512);
  int rdir = fatdino_getCluster(device, bpb, dir, clus);
  if(rdir!=1) {
    //error gettig cluster
    return 0;
  }
  fatdino_DIR *en = (fatdino_DIR*)dir + (offset / sizeof(fatdino_DIR));
  clus = (en->DIR_FstClusHI<<16) + en->DIR_FstClusLO;
  return clus;
}

int fatdino_getDir(char *device, fatdino_BPB *bpb, char *fpath, fatdino_DIR *dirstruct, unsigned int *cluster, unsigned int *offset) {
  uint32_t clus = bpb->BPB_RootClus;
  char *path = malloc(strlen(fpath)+2);
  strcpy(path,fpath);
  char *tmp = path;
  char *end = NULL;
  char *entry = NULL;
  if(path[strlen(path)-1] == '/')
  path[strlen(path)-1] = '\0';
  tmp++;
  while(end = strchr(tmp,'/')) {
    uint8_t *dir = malloc(bpb->BPB_SecPerClus * 512);
    entry = (char*)realloc((void*)entry, end + 1 - tmp);
    strncpy(entry, tmp, end - tmp);
    entry[end - tmp] = '\0';
    uint32_t off = fatdino_findEntry(device, bpb, &clus, entry, 1);
    if(off == (uint32_t)-1) {
      //not found
      return 1;
    }
    int rdir = fatdino_getCluster(device, bpb, dir, clus);
    if(rdir==-1) {
      //error reading dir entry
      return 1;
    }
    fatdino_DIR *en = (fatdino_DIR*)(dir + off);
    if((en->DIR_Attr&ATTR_DIRECTORY)!=ATTR_DIRECTORY) {
      //not a directory!
      return 1;
    }
    clus = (en->DIR_FstClusHI<<16) + en->DIR_FstClusLO;
    tmp = end + 1;
  }
  uint32_t off = fatdino_findEntry(device, bpb, &clus, tmp, 1);
  if(off == (uint32_t)-1) {
    //not found
    return 1;
  }
  uint8_t *dir = malloc(bpb->BPB_SecPerClus * 512);
  int rdir = fatdino_getCluster(device, bpb, dir, clus);
  if(rdir!=1) {
    //error gettig cluster
    return 1;
  }
  fatdino_DIR *en = (fatdino_DIR*)dir + (off / sizeof(fatdino_DIR));
  if(dirstruct!=NULL)
    memcpy(dirstruct,en,sizeof(fatdino_DIR));	//return dir struct
  *offset = off;				//return offset in cluster
  *cluster = clus;				//return cluster number
  return 0;
}

void fatdino_upperCase(char *string) {
  char *tmp = string;
  while(*tmp!='\0') {
    *tmp = toupper(*tmp);
    tmp++;
  }
}

/*

*/