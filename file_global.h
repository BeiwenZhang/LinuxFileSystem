#ifndef FILE_GLOBAL_H_
#define FILE_GLOBAL_H_

/***********************************************************/
/*ȫ�ֳ�������*/
#define DEVNAME "/dev/sdb8"								    //Ҫ�򿪵��豸�ļ�������ʾ��ǰʹ�õ�U��
#define DIR_ENTRY_SIZE 32									//Ŀ¼���С,FAT16�ļ�ϵͳ�е�Ŀ¼��ͨ����32�ֽ�
#define SECTOR_SIZE 512										//ÿ��������С ������洢�豸��512�ֽڵ�����Ϊ��λ�������ݴ���
#define CLUSTER_SIZE 512*4									//ÿ���ش�С,FAT16�ļ�ϵͳ�е��ļ��������Դ�Ϊ��λ�ġ�����Ϊ4��������2KB���Ĵش�С�ǳ���Ȩ�����ܺʹ��̿ռ�Ŀ���
#define FAT_SIZE 250*512									//ÿ��FAT����С(250ge)
#define FAT_ONE_OFFSET 512									//��һ��FAT����ʼ��ַ ����������������С512
#define FAT_TWO_OFFSET 512+250*512							//�ڶ���FAT����ʼ��ַ���ٴأ�clusters����������ı������а������ļ�ϵͳ�����дص���Ϣ��
//FAT��ͨ�����������������ֱ��ΪFAT1��FAT2��������ļ�ϵͳ�Ŀɿ���
#define ROOTDIR_OFFSET 512+250*512+250*512+512				//��Ŀ¼����ʼ��ַ(?)
#define DATA_OFFSET 512+250*512+250*512+512*32				//��������ʼ��ַ(genmulu 32)

/**����λ����**/
#define ATTR_READONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VLABEL 0x08
#define ATTR_SUBDIR 0x10
#define ATTR_ARCHIVE 0x20

/**ʱ������ 5��6��5 **/
#define MASK_HOUR 0xf800
#define MASK_MIN 0x07e0
#define MASK_SEC 0x001f

/**��������**/
#define MASK_YEAR 0xfe00
#define MASK_MONTH 0x01e0
#define MASK_DAY 0x001f

/***********************************************************/
/*ȫ�ֽṹ�嶨��*/

/**������¼�ṹ**/
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

/**��Ŀ¼��ṹ**/
typedef struct Entry
{
	unsigned char short_name[12];							//�ֽ� 0-10��11�ֽڵĶ��ļ���
	unsigned char long_name[27];							//δʹ�ã�26�ֽڵĳ��ļ���
	unsigned short year,month,day;							//22-23�ֽ�
	unsigned short hour,minute,second;						//24-25�ֽ�
	unsigned short FirstCluster;							//26-27�ֽ�
	unsigned int size;										//28-31�ֽ�
	/*����λ			11�ֽ�
	 * 7	6	5	4	3	2	1	0
	 * N	N	A	D	V	S	H	R	N��δʹ��
	 */

	unsigned char readonly:1;								//R(ֻ��)
	unsigned char hidden:1;									//H(����)
	unsigned char system:1;									//S(ϵͳ�ļ�)
	unsigned char vlabel:1;									//V(����)
	unsigned char subdir:1;									//D(��Ŀ¼)
	unsigned char archive:1;								//A(�浵)
}Entry;

/***********************************************************/
/*ȫ�ֱ�������*/

int fd;														//�ļ�������
BootDescriptor bdptor;										//������¼
Entry *curdir=NULL;											//��ǰĿ¼
int dirno=0;												//����Ŀ¼�Ĳ���
Entry* fatherdir[10];										//����Ŀ¼
unsigned char fatbuf[FAT_SIZE];								//�洢FAT����Ϣ

/***********************************************************/
/*ȫ�ֺ�������*/

int ud_ls(int);												//��ʾ��Ҫ�鿴Ŀ¼�µ�����
int ud_cd(char *);											//�ı�Ŀ¼����Ŀ¼����Ŀ¼
int ud_cf(char *,int);										//�ڵ�ǰĿ¼�´����ļ�
int ud_md(char *);											//�ڵ�ǰĿ¼�´���Ŀ¼
int ud_df(char *);											//ɾ����ǰĿ¼�µ��ļ�
int ud_dd(char *);											//ɾ����ǰĿ¼�µ�Ŀ¼��
int ud_rf(char *);											//��ȡ��ǰĿ¼�µ��ļ�����

void findDate(unsigned short *,unsigned short *,unsigned short *,unsigned char []);
void findTime(unsigned short *,unsigned short *,unsigned short *,unsigned char []);

int readFat();												//��fat������Ϣ
int writeFat();												//���ı��fat��ֵд��fat��
void scanBootSector();										//�����������
void scanRootEntry();										//�����Ŀ¼
int scanEntry(char *,Entry *,int);							//�����ǰĿ¼
int getEntry(Entry *);										//�Ӹ�Ŀ¼���ļ����еõ��ļ�����
void formatFileName(unsigned char *);						//�ļ�����ʽ��
unsigned short getFatCluster(unsigned short);				//��fat���л����һ�ص�λ��
void clearFatCluster(unsigned short);						//���fat���еĴ���Ϣ

/***********************************************************/
#endif /* FILE_GLOBAL_H_ */