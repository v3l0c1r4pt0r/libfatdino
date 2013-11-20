#include <iostream>
#include <iomanip>
#include <cstdio>
extern "C" {
  #include "../src/fat32.h"
}

using namespace std;

void print_help(char *fn) {
    cout<<"usage: "<<fn<<" COMMAND [ARGS] DEVICE\n"<<
    "check\t\tchecks if device given is FAT32-formatted\n"<<
    "bpb\t\treads BPB/BS structure and shows most of its values\n"<<
    "chain\t\tprints FAT chain of file/dir given as ARG\n"<<
    "cchain\t\tprints FAT chain that starts at cluster given at ARG\n"<<
    "clist\t\tlists directory at cluster given at ARG\n"<<
    "clistl\t\tlists directory at cluster given at ARG (uses LFN)\n"<<
    "list\t\tlists directory given at ARG (uses LFN, / - root directory)\n"<<
    "lists\t\tlists directory given at ARG (/ - root directory)\n"<<
    "read\t\tread file at given ARG and outputs to stdout\n"<<
    "readx\t\tread file at given ARG and outputs to stdout as hex\n"<<
    "size\t\tgets file size at path given as ARG\n"<<
    "cluster\t\tgets cluster number of file/dir given at ARG\n"<<
    "clusterx\tgets cluster number in hex of file/dir given at ARG\n"<<
    "dir\t\tprints content of directory structure of file/dir at given ARG\n"<<
    "fsinfo\t\tprints FSINFO structure content\n"<<
    "mkdir\t\tcreates directory where ARG is in format: /path/to/dir/dir_name\n"<<
    "help\t\tprints this message\n"<<
    "";  
}

void list_dirclus(char *device, fatdino_BPB *bpb, uint8_t *dir, uint32_t clus, short uselfn) {
  fatdino_DIR * direntry = (fatdino_DIR*)dir;
  while(
    (((uint8_t*)direntry - dir) < (512 * bpb->BPB_SecPerClus)) && 
    (direntry->DIR_Name[0] != '\0')
  ) {
    char *fn;
    unsigned int fnSz = 0;
    int isLFN;
    if(uselfn) {
      fn = new char[256];
      isLFN = fatdino_getLFN(device, fn, &fnSz, bpb, clus, ((uint8_t*)direntry - dir));
    }
    else {
      fn = new char[13];
      isLFN = 0;
    }
    char *buf;
    buf = fatdino_SfnToHuman((char*)direntry->DIR_Name,direntry->DIR_NTRes);
    //sfn in buf, lfn in fn
    if(!isLFN) {
      memcpy(fn,buf,13);
      fnSz = strlen(buf);
    }
    char datewrt[20];
    memset(datewrt,' ',19);
    datewrt[19]='\0';
    fatdino_dateToHuman(datewrt,direntry->DIR_WrtDate, direntry->DIR_WrtTime, 0);
    char datecrt[24];
    memset(datecrt,' ',23);
    datecrt[23]='\0';
    fatdino_dateToHuman(datecrt,direntry->DIR_CrtDate, direntry->DIR_CrtTime, direntry->DIR_CrtTimeTenth);
    if((direntry->DIR_Attr&ATTR_LONG_NAME) != ATTR_LONG_NAME) {
      cout<<
      " [ "							//attrib
        <<((((direntry->DIR_Attr)&ATTR_DIRECTORY)!=0) ? "DIR ":"    ")<<""
        <<(((direntry->DIR_Name[0])==0xE5) ? "DEL ":"    ")<<"    "
        <<((((direntry->DIR_Attr)&ATTR_VOLUME_ID)!=0) ? "VOL ":"    ")<<""
        <<((((direntry->DIR_Attr)&ATTR_HIDDEN)!=0) ? "HID ":"    ")<<""
        <<((((direntry->DIR_Attr)&ATTR_SYSTEM)!=0) ? "SYS ":"    ")<<""
        <<((((direntry->DIR_Attr)&ATTR_READ_ONLY)!=0) ? "R/O ":"    ")<<""
        <<((((direntry->DIR_Attr)&ATTR_ARCHIVE)!=0) ? "ARC ":"    ")<<""
        ;if(uselfn)cout<<((isLFN) ? "LFN ":"    ")<<"";cout
      <<"]"<<							//end attrib
      " "<<setw(10)<<fatdino_bytesToHuman(direntry->DIR_FileSize)<<	//sizeh
      " "<<setw(10)<<direntry->DIR_FileSize<<			//sizeb
      " "<<setw(8)<<hex<<(((direntry->DIR_FstClusHI)<<16)+(direntry->DIR_FstClusLO))<<dec<<	//clus
      " "<<left<<setw(19)<<datewrt<<right<<				//wrttimestamp
      " "<<left<<setw(23)<<datecrt<<right<<				//crttimestamp
      " "<<left<<fn<<right<<						//file name
      "\t"<<"\n";
    }
    direntry++;
  }
}

int main(int argc, char **argv) {
  if(argc>1) {
    if(strcmp(argv[1],"check")==0) {
      if(argc>2) {
	fatdino_BPB *bpb;
	bpb = new fatdino_BPB;
	int rbpb = fatdino_getBPB(argv[2], bpb);
	if(rbpb == 0) {
	  int res = fatdino_checkDev(argv[2], bpb);
	  if(res==1)
	    cout<<argv[2]<<" jest sformatowane jako FAT32\n";
	  else if(res==0)
	    cout<<argv[2]<<" nie jest sformatowane jako FAT32\n";
	  else
	    cout<<argv[2]<<": wystąpił błąd!\n";
	  return 0;
	}
	else return 1;
      }
    }
    else if(strcmp(argv[1],"bpb")==0) {
      if(argc>2) {
	fatdino_BPB *buffer;
	buffer = new fatdino_BPB;
	int res = fatdino_getBPB(argv[2], buffer);
	if(res==0) {
	  cout<<"Wczytano BPB(BIOS Parameter Block) urządzenia "<<argv[2]<<"\n"
	    <<"OEM name: "<<buffer->BS_OEMName<<"\n"
	    <<"Bytes per sector: "<<buffer->BPB_BytsPerSec<<"\n"
	    <<"Sectors per cluster: "<<(unsigned int)buffer->BPB_SecPerClus<<"\n"
	    <<"Reserved sectors: "<<buffer->BPB_RsvdSecCnt<<"\n"
	    <<"Number of FAT copies: "<<buffer->BPB_NumFATs<<"\n"
	    <<"Media type: 0x"<<hex<<(unsigned int)buffer->BPB_Media<<dec<<"\n"
	    <<"Sectors per track: "<<buffer->BPB_SecPerTrk<<"\n"
	    <<"Number of heads: "<<buffer->BPB_NumHeads<<"\n"
	    <<"Hidden sectors: "<<buffer->BPB_HiddSec<<"\n"
	    <<"Sectors in partition: "<<buffer->BPB_TotSec32<<"\n"
	    <<"Size of a single FAT (in sectors): "<<buffer->BPB_FATSz32<<"\n"
	    <<"Mirroring status: ";
	    {
	      if(!((buffer->BPB_ExtFlags)&0x80))
		cout<<"mirroring at runtime";
	      else
		cout<<"active FAT #"<<((buffer->BPB_ExtFlags)&7);
	    }
	    cout<<"\n"
	    <<"FAT32 version: "<<(buffer->BPB_FSVer>>8)<<"."<<(buffer->BPB_FSVer&0xFF)<<"\n"
	    <<"Root cluster number: "<<buffer->BPB_RootClus<<"\n"
	    <<"Sector that contains FSINFO structure: "<<buffer->BPB_FSInfo<<"\n"
	    <<"Backup of boot sector sector number: "<<buffer->BPB_BkBootSec<<"\n"
	    <<"INT 13h drive number: "<<buffer->BS_DrvNum<<"\n";
	    if(buffer->BS_BootSig==0x29) {
	      cout
	    <<"Volume serial number: "<<hex<<buffer->BS_VolID<<dec<<"\n";
	    char buf[12];
	    strncpy(buf,(char*)&(buffer->BS_VolLab),11);
	    cout
	    <<"Volume label: "<<buf<<"\n";
	    strncpy(buf,(char*)&(buffer->BS_FilSysType),8);
	    cout
	    <<"Filesystem type: "<<buf<<"\n"
	    ;
	    }
	  return 0;
	}
	else {
	  cout<<argv[2]<<"wystąpił błąd podczas odczytywania struktury.\n";
	  return 1;
	}
      }      
    }
    else if(strcmp(argv[1],"chain")==0) {
      if(argc>3) {
	if(strcmp(argv[2],"/")==0) {
	  fatdino_BPB *bpb;
	  bpb = new fatdino_BPB;
	  int res = fatdino_getBPB(argv[3], bpb);
	  if(res==0) {
	    uint32_t clus = bpb->BPB_RootClus;
	    cout<<"cluster chain of '"<<argv[2]<<"':\n"<<hex<<"0x"<<clus<<"";
	    uint32_t cluscnt = 0;
	    while(((clus&0x0FFFFFFF) < 0x0FFFFFF8) && 
	      (clus!=0) && 
	      ((clus&0x0FFFFFFF) != 0x0FFFFFF7)) {
	      clus = fatdino_getNextCluster(argv[3], bpb, clus);
	      cout<<"->\n0x"<<clus<<"";
	      cluscnt++;
	    }
	    if(clus&0x0FFFFFFF==0x0FFFFFF7)
	      cout<<"plik uszkodzony!";
	    cout<<dec<<"\n";
	  cout<<"Razem podany plik zajmuje na dysku "<<cluscnt<<" klastrów, czyli "
	    <<fatdino_bytesToHuman(cluscnt*512*bpb->BPB_SecPerClus)<<" ("
	    <<cluscnt*512*bpb->BPB_SecPerClus<<")\n";
	  }
	  else return 1;
	}
	else {
	  cout<<"jeszcze nie teraz...";
	  return 0xffffffff;
	}
      }
      else {
	cout<<argv[0]<<": brakujący argument";
	return 1;
      }
    }
    else if(strcmp(argv[1],"cchain")==0) {
      if(argc>3) {
	fatdino_BPB *bpb;
	bpb = new fatdino_BPB;
	int res = fatdino_getBPB(argv[3], bpb);
	if(res==0) {
	  uint32_t clus;
	  if(argv[2][0]=='0' && argv[2][1]=='x')
	    clus = strtol(argv[2], NULL, 16);
	  else
	    clus = atol(argv[2]);
	  cout<<"cluster chain: \n"<<hex<<"0x"<<clus<<"";
	  uint32_t cluscnt = 0;
	  while(((clus&0x0FFFFFFF) < 0x0FFFFFF8) && 
	    (clus!=0) && 
	    ((clus&0x0FFFFFFF) != 0x0FFFFFF7)) {
	      clus = fatdino_getNextCluster(argv[3], bpb, clus);
	      cout<<"->\n0x"<<clus<<"";
	      cluscnt++;
	  }
	  if(clus&0x0FFFFFFF==0x0FFFFFF7)
	    cout<<"plik uszkodzony!";
	  cout<<dec<<"\n";
	  cout<<"Razem podany plik zajmuje na dysku "<<cluscnt<<" klastrów, czyli "
	    <<fatdino_bytesToHuman(cluscnt*512*bpb->BPB_SecPerClus)<<" ("
	    <<cluscnt*512*bpb->BPB_SecPerClus<<")\n";
	}
	  else return 1;
      }
      else {
	cout<<argv[0]<<": brakujący argument\n";
	return 1;
      }
    }
    else if(strcmp(argv[1],"clist")==0) {
      if(argc>3) {
	fatdino_BPB *bpb = new fatdino_BPB;
	int rbpb = fatdino_getBPB(argv[3], bpb);
	if(rbpb==0) {
	  uint8_t *dir = new uint8_t[bpb->BPB_SecPerClus * 512];
	  uint32_t clus;
	  if(argv[2][0]=='0' && argv[2][1]=='x')
	    clus = strtol(argv[2], NULL, 16);
	  else
	    clus = atol(argv[2]);
	  int rdir = fatdino_getCluster(argv[3], bpb, dir, clus);
	  if(rdir==1) {
	    list_dirclus(argv[3], bpb, dir, clus, false);
	  }
	  else {
	    cout<<argv[0]<<": wystąpił błąd\n";
	  }
	}
	else {
	  cout<<argv[0]<<": wystąpił błąd\n";
	}
      }
      else {
	cout<<argv[0]<<": brakujący argument\n";
	return 1;
      }
    }

    else if(strcmp(argv[1],"clistl")==0) {
      if(argc>3) {
	fatdino_BPB *bpb = new fatdino_BPB;
	int rbpb = fatdino_getBPB(argv[3], bpb);
	if(rbpb==0) {
	  uint8_t *dir = new uint8_t[bpb->BPB_SecPerClus * 512];
	  uint32_t clus;
	  if(argv[2][0]=='0' && argv[2][1]=='x')
	    clus = strtol(argv[2], NULL, 16);
	  else
	    clus = atol(argv[2]);
	  int rdir = fatdino_getCluster(argv[3], bpb, dir, clus);
	  if(rdir==1) {
	    list_dirclus(argv[3], bpb, dir, clus, true);
	  }
	  else {
	    cout<<argv[0]<<": wystąpił błąd\n";
	  }
	}
	else {
	  cout<<argv[0]<<": wystąpił błąd\n";
	}
      }
      else {
	cout<<argv[0]<<": brakujący argument\n";
	return 1;
      }
    }
    else if(
	 (strcmp(argv[1],"list")==0)
      || (strcmp(argv[1],"lists")==0)
      || (strcmp(argv[1],"read")==0)
      || (strcmp(argv[1],"readx")==0)
    ) {
      if(argc>3) {
	/* argv[2] -> path
	 * argv[3] -> device
	 */
	fatdino_BPB *bpb = new fatdino_BPB;
	int rbpb = fatdino_getBPB(argv[3], bpb);
	if(rbpb==0) {
	  if(strcmp(argv[2],"/")==0) {
	    uint32_t clus = bpb->BPB_RootClus;
	    //uint32_t nxt
	    while(((clus&0x0FFFFFFF) < 0x0FFFFFF7) && (clus!=0)) {
	      uint8_t *dir = new uint8_t[bpb->BPB_SecPerClus * 512];
	      int rdir = fatdino_getCluster(argv[3], bpb, dir, clus);
	      if(rdir!=1) {
		return 1;
	      }
	      if(strcmp(argv[1],"lists")==0)
		list_dirclus(argv[3],bpb,dir,clus,false);
	      else if(strcmp(argv[1],"list")==0)
		list_dirclus(argv[3],bpb,dir,clus,true);
	      else {
		cout<<argv[2]<<": / isn't a file!\n";
		return 1;
	      }
	      clus = fatdino_getNextCluster(argv[3], bpb, clus);
	    }
	    if((clus&0x0FFFFFFF == 0x0FFFFFF7) || (clus&0x0FFFFFFF == 0))
	      cout<<argv[2]<<": katalog uszkodzony!\n";
	  }
	  else {
	    uint32_t clus = fatdino_getClusterNumber(argv[3], bpb, argv[2]);
	    if(clus <= 1)
	      return 1;
	    unsigned int filesize = fatdino_getSize(argv[3], bpb, argv[2]);
	    while(((clus&0x0FFFFFFF) < 0x0FFFFFF7) && (clus!=0)) {
	      uint8_t *dir = new uint8_t[bpb->BPB_SecPerClus * 512];
	      int rdir = fatdino_getCluster(argv[3], bpb, dir, clus);
	      if(rdir!=1) {
		return 1;
	      }
	      if(strcmp(argv[1],"lists")==0)
		list_dirclus(argv[3],bpb,dir,clus,false);
	      else if(strcmp(argv[1],"list")==0)
		list_dirclus(argv[3],bpb,dir,clus,true);
	      else if(strcmp(argv[1],"read")==0) {
		int cond = (filesize>(bpb->BPB_SecPerClus * 512)?(bpb->BPB_SecPerClus * 512):filesize);
		for(int i = 0; i<cond; i++)
		  printf("%c",dir[i]);
		cout<<endl;
	      }
	      else if(strcmp(argv[1],"readx")==0) {
		int cond = (filesize>(bpb->BPB_SecPerClus * 512)?(bpb->BPB_SecPerClus * 512):filesize);
		for(int i = 0; i<cond; i++)
		  printf("%02x ",(unsigned char)dir[i]);
		cout<<endl;
	      }
	      clus = fatdino_getNextCluster(argv[3], bpb, clus);
	      if(filesize>=(bpb->BPB_SecPerClus * 512))
		filesize -= (bpb->BPB_SecPerClus * 512);
	    }
	    if((clus&0x0FFFFFFF == 0x0FFFFFF7) || (clus&0x0FFFFFFF == 0))
	      cout<<argv[2]<<": katalog uszkodzony!\n";
	  }
	}
	else {
	  cout<<argv[0]<<": wystąpił błąd\n";
	}
      }
      else {
	cout<<argv[0]<<": brakujący argument\n";
	return 1;
      }
    }
    else if(strcmp(argv[1],"size")==0) {
      if(argc>3) {
	/* argv[2] -> path
	 * argv[3] -> device
	 */
	fatdino_BPB *bpb = new fatdino_BPB;
	int rbpb = fatdino_getBPB(argv[3], bpb);
	if(rbpb==0) {
	  uint32_t fsize = fatdino_getSize(argv[3], bpb, argv[2]);
	  if(fsize>0)
	    cout<<fsize<<endl;
	  else {
	    cout<<argv[0]<<": "<<argv[2]<<" not found or not a file!\n";
	    return 1;
	  }
	}
      }
    }
    else if((strcmp(argv[1],"cluster")==0) || (strcmp(argv[1],"clusterx")==0)) {
      if(argc>3) {
	/* argv[2] -> path
	 * argv[3] -> device
	 */
	fatdino_BPB *bpb = new fatdino_BPB;
	int rbpb = fatdino_getBPB(argv[3], bpb);
	if(rbpb==0) {
	  uint32_t clus = fatdino_getClusterNumber(argv[3], bpb, argv[2]);
	  if(clus>1) {
	    if(argv[1][7]=='\0')
	      cout<<clus<<endl;
	    else
	      printf("0x%02x\n",clus);
	  }
	  else {
	    cout<<argv[0]<<": "<<argv[2]<<" not found!\n";
	    return 1;
	  }
	}
      }
    }
    else if(strcmp(argv[1],"dir")==0) {
      if(argc>3) {
	/* argv[2] -> path
	 * argv[3] -> device
	 */
	fatdino_BPB *bpb = new fatdino_BPB;
	int rbpb = fatdino_getBPB(argv[3], bpb);
	if(rbpb==0) {
	  fatdino_DIR *entry = new fatdino_DIR;
	  unsigned int cluster = 0;
	  unsigned int offset = 0;
	  int res = fatdino_getDir(argv[3], bpb, argv[2], entry, &cluster, &offset);
	  if(res==0)
	    cout<<"Directory structure of file '"<<argv[2]<<"' is at cluster 0x"<<
	      hex<<cluster<<", offset: 0x"<<offset<<dec<<".\n";
	  else
	    return 1;
	  cout<<
	    "Short file name: \""<<fatdino_SfnToHuman((char*)entry->DIR_Name, entry->DIR_NTRes)<<"\" (raw: \""<<entry->DIR_Name<<"\")\n"<<
	    "";char *fn = new char[256];unsigned int fnSz = 0;int isLFN = fatdino_getLFN(argv[3], fn, &fnSz, bpb, cluster, offset);cout<<
	    "Long file name: ";if(isLFN)cout<<"\""<<fn<<"\"";else cout<<"not present";cout<<"\n"<<
	    "Atrributes: 0x"<<hex<<(int)entry->DIR_Attr<<dec<<"\n"<<
	    "Lowercase short file name indicator: "<<(((entry->DIR_NTRes&0x18)!=0)?"yes":"no")<<" (0x"<<hex<<(int)(entry->DIR_NTRes)<<dec<<")\n"<<
	    "";char crt[24];fatdino_dateToHuman(crt,entry->DIR_CrtDate, entry->DIR_CrtTime, entry->DIR_CrtTimeTenth);cout<<
	    "Creation time: "<<crt<<"; Raw values:\n"<<
	      "\tTime (granularity: 10 ms; from previous even second): 0x"<<hex<<(int)entry->DIR_CrtTimeTenth<<dec<<"\n"<<
	      "\tTime (granularity: 2 s): 0x"<<hex<<(int)entry->DIR_CrtTime<<dec<<"\n"<<
	      "\tDate: 0x"<<hex<<(int)entry->DIR_CrtDate<<dec<<"\n"<<
	    "Last access: "<<hex<<(int)entry->DIR_LstAcc<<dec<<"\n"<<
	    "";char wrt[24];fatdino_dateToHuman(wrt,entry->DIR_WrtDate, entry->DIR_WrtTime, 0);cout<<
	    "Write time: "<<wrt<<"; Raw values:\n"<<
	      "\tTime (granularity: 2 s): 0x"<<hex<<(int)entry->DIR_WrtTime<<dec<<"\n"<<
	      "\tDate: 0x"<<hex<<(int)entry->DIR_WrtDate<<dec<<"\n"<<
	    "First cluster number: 0x"<<hex<<((entry->DIR_FstClusHI<<16)+entry->DIR_FstClusLO)<<dec<<"\n"<<
	    "File size: "<<fatdino_bytesToHuman(entry->DIR_FileSize)<<" ("<<entry->DIR_FileSize<<" B)\n"<<
	    "";
	}
      }
    }
    else if(strcmp(argv[1],"fsinfo")==0) {
      if(argc>2) {
	fatdino_BPB *bpb = new fatdino_BPB;
	int rbpb = fatdino_getBPB(argv[2], bpb);
	if(rbpb==0) {
	  fatdino_FSINFO *fsinfo = new fatdino_FSINFO;
	  int fsir = fatdino_getFSINFO(argv[2], bpb, fsinfo);
	  if(fsir == -1) {
	    cout<<"error reading fsinfo!\n";
	    return 1;
	  }
	  if(fsir == 1) {
	    cout<<"FSINFO structure damaged!\n";
	    return 2;
	  }
	  cout<<"Content of FSINFO structure:\n"<<
	  "\tFree cluster count:\t"<<fsinfo->FSI_Free_Count<<"\n"<<
	  "\tNext free cluster:\t0x"<<hex<<fsinfo->FSI_Nxt_Free<<dec<<"\n"<<
	  "";
	}
      }
    }
    else if(strcmp(argv[1],"mkdir")==0) {
      if(argc>3) {
	/* argv[2] -> path
	 * argv[3] -> device
	 */
	fatdino_BPB *bpb = new fatdino_BPB;
	int rbpb = fatdino_getBPB(argv[3], bpb);
	if(rbpb==0) {
	  fatdino_FSINFO *fsinfo = new fatdino_FSINFO;
	  int fsir = fatdino_getFSINFO(argv[3], bpb, fsinfo);
	  if(fsir == -1) {
	    cout<<"error reading fsinfo!\n";
	    return 1;
	  }
	  if(fsir == 1) {
	    cout<<"FSINFO structure damaged!\n";
	    return 2;
	  }
	  uint32_t firstfree;
	  if(fsinfo->FSI_Nxt_Free != 0xFFFFFFFF)
	    firstfree = fatdino_findNextFree(argv[3], bpb, fsinfo->FSI_Nxt_Free);
	  else
	    firstfree = fatdino_findNextFree(argv[3], bpb, 2);
	  cout<<"Pierwszy wolny sektor: "<<firstfree<<"\n";
	  if(firstfree>1)
	  {
	    cout<<" too fast!\n";
	    char test[54] = {'R',0,'E',0,'A',0,'D',0,'M',0,'E',0,'.',0,'d',0,'i',0,'s',0,'k',0,'d',0,'e',0,'f',0,'i',0,'n',0,'e',0,'s',0,0,0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	    char *out = new char[256];
	    //UTF16->UTF8
	    fatdino_FromUTF16((char*)&test, out);
	    //print out
	    cout<<"README~1DIS: "<<out<<"\n";
	    //test SFN and LFN generation
	    char *sfn = new char[13];
	    char *lfn = new char[256];
	    int res = fatdino_nameToSfnAndLfn(test, sfn, lfn);
	    if(res==1)
	    {
	      //lfn+sfn
	      cout<<"SFN: "<<sfn<<"\n";
	      char *ulfn = new char[256];
	      fatdino_FromUTF16(lfn, ulfn);
	      cout<<"LFN: "<<ulfn<<"\n";
	    }
	    else if(res==0)
	    {
	      //sfn
	      cout<<"SFN: "<<sfn<<"\n";
	    }
	    else
	    {
	      cout<<argv[0]<<": error converting to LFN/SFN\n";
	    }
	    //UTF16<-UTF8
	    char *iconvout = new char[256];
	    fatdino_ToUTF16(out, iconvout);
	  }
	  else
	    cout<<"Could not find first free cluster. Is there a one?\n";
	}
      }
    }
    else if(strcmp(argv[1],"help")==0) {
      print_help(argv[0]);
      return 0;
    }
    else {
      cout<<"usage: "<<argv[0]<<" COMMAND [ARGS] DEVICE\n";
      return 1;
    }
  } 
  else {
    print_help(argv[0]);
    return 1;
  }
  return 0;
}