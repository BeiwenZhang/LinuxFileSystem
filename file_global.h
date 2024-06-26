#ifndef FILE_GLOBAL_H_
#define FILE_GLOBAL_H_

/***********************************************************/
/*全局常量定义*/
#define DEVNAME "/dev/sdb8"								    //要打开的设备文件名，表示当前使用的U盘
#define DIR_ENTRY_SIZE 32									//目录项大小,FAT16文件系统中的目录项通常是32字节
#define SECTOR_SIZE 512										//每个扇区大小 大多数存储设备以512字节的扇区为单位进行数据传输
#define CLUSTER_SIZE 512*4									//每个簇大小,FAT16文件系统中的文件分配是以簇为单位的。设置为4个扇区（2KB）的簇大小是出于权衡性能和磁盘空间的考虑
#define FAT_SIZE 250*512									//每个FAT表大小(250ge)
#define FAT_ONE_OFFSET 512									//第一个FAT表起始地址 常见的引导扇区大小512
#define FAT_TWO_OFFSET 512+250*512							//第二个FAT表起始地址跟踪簇（clusters）分配情况的表格，其中包含了文件系统上所有簇的信息。
//FAT表通常会有两个拷贝，分别称为FAT1和FAT2，以提高文件系统的可靠性
#define ROOTDIR_OFFSET 512+250*512+250*512+512				//根目录区起始地址(?)
#define DATA_OFFSET 512+250*512+250*512+512*32				//数据区起始地址(genmulu 32)

/**属性位掩码**/
#define ATTR_READONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VLABEL 0x08
#define ATTR_SUBDIR 0x10
#define ATTR_ARCHIVE 0x20

/**时间掩码 5：6：5 **/
#define MASK_HOUR 0xf800
#define MASK_MIN 0x07e0
#define MASK_SEC 0x001f

/**日期掩码**/
#define MASK_YEAR 0xfe00
#define MASK_MONTH 0x01e0
#define MASK_DAY 0x001f

/***********************************************************/
/*全局结构体定义*/

/**启动记录结构**/
typedef struct BootDescriptor
{
	unsigned char Oem_name[9];								//0x03-0x0a
	int BytesPerSector;										//0x0b-0x0c
	int SectorsPerCluster;									//0x0d
	int ReservedSectors;									//0x0e-0x0f
	int FATs;												//0x10
	int RootDirEntries;										//0x11-0x12
	int LogicSectors;										//0x13-0x14
	int MediaType;											//0x15
	int SectorsPerFAT;										//0x16-0x17
	int SectorsPerTrack;									//0x18-0x19
	int Heads;												//0x1a-0x1b
	int HiddenSectors;										//0x1c-0x1d
}BootDescriptor;

/**根目录项结构**/
typedef struct Entry
{
	unsigned char short_name[12];							//字节 0-10，11字节的短文件名
	unsigned char long_name[27];							//未使用，26字节的长文件名
	unsigned short year,month,day;							//22-23字节
	unsigned short hour,minute,second;						//24-25字节
	unsigned short FirstCluster;							//26-27字节
	unsigned int size;										//28-31字节
	/*属性位			11字节
	 * 7	6	5	4	3	2	1	0
	 * N	N	A	D	V	S	H	R	N：未使用
	 */

	unsigned char readonly:1;								//R(只读)
	unsigned char hidden:1;									//H(隐藏)
	unsigned char system:1;									//S(系统文件)
	unsigned char vlabel:1;									//V(卷标)
	unsigned char subdir:1;									//D(子目录)
	unsigned char archive:1;								//A(存档)
}Entry;

/***********************************************************/
/*全局变量定义*/

int fd;														//文件描述符
BootDescriptor bdptor;										//启动记录
Entry *curdir=NULL;											//当前目录
int dirno=0;												//代表目录的层数
Entry* fatherdir[10];										//父级目录
unsigned char fatbuf[FAT_SIZE];								//存储FAT表信息

/***********************************************************/
/*全局函数声明*/

int ud_ls(int);												//显示所要查看目录下的内容
int ud_cd(char *);											//改变目录到父目录或子目录
int ud_cf(char *,int);										//在当前目录下创建文件
int ud_md(char *);											//在当前目录下创建目录
int ud_df(char *);											//删除当前目录下的文件
int ud_dd(char *);											//删除当前目录下的目录项
int ud_rf(char *);											//读取当前目录下的文件内容

void findDate(unsigned short *,unsigned short *,unsigned short *,unsigned char []);
void findTime(unsigned short *,unsigned short *,unsigned short *,unsigned char []);

int readFat();												//读fat表的信息
int writeFat();												//将改变的fat表值写回fat表
void scanBootSector();										//浏览启动扇区
void scanRootEntry();										//浏览根目录
int scanEntry(char *,Entry *,int);							//浏览当前目录
int getEntry(Entry *);										//从根目录或文件簇中得到文件表项
void formatFileName(unsigned char *);						//文件名格式化
unsigned short getFatCluster(unsigned short);				//在fat表中获得下一簇的位置
void clearFatCluster(unsigned short);						//清除fat表中的簇信息

/***********************************************************/
#endif /* FILE_GLOBAL_H_ */
