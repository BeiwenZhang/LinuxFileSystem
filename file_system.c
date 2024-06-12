/*
 * file_system.c
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "file_global.h"
//�ֽڷ�ת����С�ˣ�
#define RevByte(low,high) ((high)<<8|(low))
//�ֵķ�ת���ĸ��ֽڷ�һ�£�
#define RevWord(lowest,lower,higher,highest) ((highest)<<24|(higher)<<16|(lower)<<8|lowest)

/*
 * ��fat�����Ϣ������fatbuf[]��
 */
//lseek ��һ���������ļ��ж�λ�ļ�ָ��λ�õ�ϵͳ���á�lseek �����ƶ��ļ������� fd �������ļ�ָ�뵽ָ����λ�á�
// ������˵��lseek ��ԭ���ǣ�
// off_t lseek(int fd, off_t offset, int whence);
//SEEK_SET���ǳ�ʼλ��
int readFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	//read �����ӵ�ǰ�ļ�ָ��λ�ö�ȡ FAT_SIZE �ֽڵ����ݵ� fatbuf ��������
	if(read(fd,fatbuf,FAT_SIZE)<0)
	{
		perror("read failed");
		return -1;
	}

	return 1;
}

/*
 * ���ı��fat��ֵд��fat��
 */
int writeFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if(write(fd,fatbuf,FAT_SIZE)<0)
	{
		perror("read failed");
		return -1;
	}
	if(lseek(fd,FAT_TWO_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if((write(fd,fatbuf,FAT_SIZE))<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}

/*��ӡ�������¼*/
//ɨ������������Boot Sector��
void scanBootSector()
{
	unsigned char buf[SECTOR_SIZE];
	int ret,i;

	if((ret=read(fd,buf,SECTOR_SIZE))<0)
		perror("read boot sector failed");
	for(i=0;i<8;i++)
		bdptor.Oem_name[i]=buf[i+0x03];
	bdptor.Oem_name[i]='\0';

	// �ⲿ�ִ���ͨ���� buf ��������ȡ��Ӧ���ֽڣ�
	// ��������������Ϣ�洢���ṹ�� bdptor �С����磬
	// bdptor.Oem_name ��һ���ַ����飬���ڴ洢������
	// ���� OEM ���ƣ�ͨ���� buf �е���Ӧ�ֽڸ��Ƶ� Oem_name ����ʵ�֡�
	bdptor.BytesPerSector=RevByte(buf[0x0b],buf[0x0c]);
	bdptor.SectorsPerCluster=buf[0x0d];
	bdptor.ReservedSectors=RevByte(buf[0x0e],buf[0x0f]);
	bdptor.FATs=buf[0x10];
	bdptor.RootDirEntries=RevByte(buf[0x11],buf[0x12]);
	bdptor.LogicSectors=RevByte(buf[0x13],buf[0x14]);
	bdptor.MediaType=buf[0x15];
	bdptor.SectorsPerFAT=RevByte( buf[0x16],buf[0x17]);
	bdptor.SectorsPerTrack=RevByte(buf[0x18],buf[0x19]);
	bdptor.Heads=RevByte(buf[0x1a],buf[0x1b]);
	bdptor.HiddenSectors=RevByte(buf[0x1c],buf[0x1d]);

	printf("Oem_name \t\t%s\n"
		"BytesPerSector \t\t%d\n"
		"SectorsPerCluster \t%d\n"
		"ReservedSector \t\t%d\n"
		"FATs \t\t\t%d\n"
		"RootDirEntries \t\t%d\n"
		"LogicSectors \t\t%d\n"
		"MediaType \t\t%d\n"
		"SectorPerFAT \t\t%d\n"
		"SectorPerTrack \t\t%d\n"
		"Heads \t\t\t%d\n"
		"HiddenSectors \t\t%d\n",
		bdptor.Oem_name,
		bdptor.BytesPerSector,
		bdptor.SectorsPerCluster,
		bdptor.ReservedSectors,
		bdptor.FATs,
		bdptor.RootDirEntries,
		bdptor.LogicSectors,
		bdptor.MediaType,
		bdptor.SectorsPerFAT,
		bdptor.SectorsPerTrack,
		bdptor.Heads,
		bdptor.HiddenSectors);
}

/*
 * ������entryname	���ͣ�char
 * 		pentry		���ͣ�Entry *
 * 		mode		���ͣ�int��mode=1��ΪĿ¼���mode=0��Ϊ�ļ�
 * ����ֵ��ƫ��ֵ����0����ɹ���-1����ʧ��
 * ���ܣ�������ǰĿ¼�������ļ���Ŀ¼��
 */
int scanEntry(char *entryname,Entry *pentry,int mode)
{
	int ret,offset,i;
	int cluster_addr;
	char uppername[80];

	for(i=0;i<strlen(entryname);i++)
		uppername[i]=toupper(entryname[i]);
	uppername[i]='\0';

	if(curdir==NULL)		//ɨ���Ŀ¼
	{
		if((ret=lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror("lseek ROOTDIR_OFFSET failed");
		offset=ROOTDIR_OFFSET;

		while(offset<DATA_OFFSET)
		{
			ret=getEntry(pentry);
			offset+=abs(ret);

			if(pentry->subdir==mode&&strcmp((char *)pentry->short_name,uppername)==0)
				return offset;
		}
	}
	else					//ɨ����Ŀ¼
	{
		cluster_addr=DATA_OFFSET+(curdir->FirstCluster-2)*CLUSTER_SIZE;
		if((ret=lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");
		offset=cluster_addr;

		while(offset<cluster_addr+CLUSTER_SIZE)
		{
			ret=getEntry(pentry);
			offset+=abs(ret);

			if(pentry->subdir==mode&&strcmp((char *)pentry->short_name,uppername)==0)
				return offset;
		}
	}

	return -1;
}

/*
 * ������entry�����ͣ�Entry *
 * ����ֵ���ɹ����򷵻�ƫ��ֵ��ʧ�ܣ����ظ�ֵ
 * ���ܣ��Ӹ�Ŀ¼���ļ����еõ��ļ�����
 */
int getEntry(Entry *pentry)
{
	int ret,i;
	int count=0; // ��¼��ȡ���ֽ���
	unsigned char buf[DIR_ENTRY_SIZE],info[2];

	/*��һ��Ŀ¼�����32�ֽ�*/
	if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)  // ��ȡʧ��ʱ���������Ϣ
		perror("read entry failed");
	count+=ret;  // ��¼��ȡ���ֽ���

	if(buf[0]==0xe5||buf[0]==0x00)
		return -1*count;  // ���Ŀ¼��Ϊ�ջ���ɾ�����򷵻ظ�ֵ
	else
	{
		/* buf�������ļ�������FAT�ļ�ϵͳ�У����Ŀ¼��ĵ�һ���ֽ��� 0x0f�����ʾ����һ�����ļ�������Ŀ��ѭ����ȡ��������Щ���ļ�������Ŀ�����ļ��������Ե� */
		/*���ļ��������Ե�*/
		while(buf[11]==0x0f)
		{
			if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)  //read��ϵͳ���ã�fd���ļ�������
				perror("read root dir failed");
			count+=ret; // ��¼��ȡ���ֽ���
		}

		/*������ʽ����ע���β��'\0'*/
		for (i=0;i<=10;i++)
			pentry->short_name[i]=buf[i];
		pentry->short_name[i]='\0';  // ȷ���ַ�����β��'\0'

		formatFileName(pentry->short_name);  // ��ʽ���ļ���

		info[0]=buf[22];
		info[1]=buf[23];
		findTime(&(pentry->hour),&(pentry->minute),&(pentry->second),info);

		info[0]=buf[24];
		info[1]=buf[25];
		findDate(&(pentry->year),&(pentry->month),&(pentry->day),info);

		// �����ļ�����λ����Entry�ṹ�еı�־
		pentry->FirstCluster=RevByte(buf[26],buf[27]);
		pentry->size=RevWord(buf[28],buf[29],buf[30],buf[31]);

		pentry->readonly=(buf[11]&ATTR_READONLY)?1:0;
		pentry->hidden=(buf[11]&ATTR_HIDDEN)?1:0;
		pentry->system=(buf[11]&ATTR_SYSTEM)?1:0;
		pentry->vlabel=(buf[11]&ATTR_VLABEL)?1:0;
		pentry->subdir=(buf[11]&ATTR_SUBDIR)?1:0;
		pentry->archive=(buf[11]&ATTR_ARCHIVE)?1:0;

		return count;  // ���ض�ȡ���ֽ���
	}
}

/*����*/
void findDate(unsigned short *year,unsigned short *month,unsigned short *day,unsigned char info[2])
{
	int date;
	date=RevByte(info[0],info[1]);

	*year=((date&MASK_YEAR)>>9)+1980;
	*month=((date&MASK_MONTH)>>5);
	*day=(date&MASK_DAY);
}

/*ʱ��*/
//Hour��Сʱ���� �Ӹ�λ��ʼռ��5λ����λ11��15��
// Minute�����ӣ��� �м��6λ����λ5��10��
// Second���룩�� ��λռ��5λ����λ0��4��
//�������޳�����Ҫ�Ĳ��֣�������Ҫ�ģ�����λ
//���ֶ�ͨ����2������Ƚ��д洢��������Ҫ����2����ȡ��ʵ������
void findTime(unsigned short *hour,unsigned short *min,unsigned short *sec,unsigned char info[2])
{
	int time;
	time=RevByte(info[0],info[1]);

	*hour=((time&MASK_HOUR)>>11);
	*min=(time&MASK_MIN)>>5;
	*sec=(time&MASK_SEC)*2;
}

/*�ļ�����ʽ�������ڱȽ�*/
void formatFileName(unsigned char *name)
{
	unsigned char *p=name;
	while(*p!='\0')
		p++;
	p--;
	while(*p==' ')
		p--;
	p++;
	*p='\0';
}

/*
 * ������prev�����ͣ�unsigned char
 * ����ֵ����һ��
 * ���ܣ���fat���л����һ�ص�λ��
 */
unsigned short getFatCluster(unsigned short prev)
{
	unsigned short next;
	int index;

	index=prev*2;
	next=RevByte(fatbuf[index],fatbuf[index+1]);

	return next;
}

/*
 * ������cluster�����ͣ�unsigned short
 * ����ֵ��void
 * ���ܣ����fat���еĴ���Ϣ
 */
void clearFatCluster(unsigned short cluster)
{
	int index;
	index=cluster*2;

	fatbuf[index]=0x00;
	fatbuf[index+1]=0x00;
}

/*
 * ������mode�����ͣ�int
 * ����ֵ��1���ɹ���-1��ʧ��
 * ���ܣ���ʾ��Ҫ�鿴Ŀ¼�µ����ݡ�
 * ˵����modeֵΪ0ʱ���鿴��ǰĿ¼��Ϣ��modeֵΪ1ʱ���鿴���еķǸ�Ŀ¼��Ϣ
 */
int ud_ls(int mode)
{
	int ret,offset,cluster_addr;
	int i;
	Entry entry;
	unsigned char buf[DIR_ENTRY_SIZE];

	if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");

	if(mode==1)
	{
		printf("All_Non-Root_dir\n");
		printf("\tname\t\tdate\t\ttime\t\tcluster\tsize\tattr\n");

		for(i=0;i<100;i++)
		{//�����Ѷ����Ķ���ȫ�����
			cluster_addr=DATA_OFFSET+i*CLUSTER_SIZE; //һ��һ�صĶ�
			if((ret=lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset=cluster_addr;

			/*ֻ��һ�ص�����*/
			while(offset<cluster_addr+CLUSTER_SIZE)
			{
				ret=getEntry(&entry);	//��ȡ�ļ�����浽entry��������ƫ������ʾ�Ѿ���ȡ����һ�ص���Ϣ�Ѿ���ȡ׼������һ����
				offset+=abs(ret);

				if(ret>0)
				{
					printf("%16s\t"
						"%d-%d-%d\t"
						"%d:%d:%d  \t"
						"%d\t"
						"%d\t"
						"%s\n",
						entry.short_name,
						entry.year,entry.month,entry.day,
						entry.hour,entry.minute,entry.second,
						entry.FirstCluster,
						entry.size,
						(entry.subdir)?"dir":"file");
				}
			}
		}
		return 1;
	}

	//ģʽ1���� ��������ģʽ0
	if(curdir==NULL)
		printf("Root_dir\n");//��Ŀ¼
	else
		printf("%s_dir\n",curdir->short_name);//��Ӧ������

	printf("\tname\t\tdate\t\ttime\t\tcluster\tsize\tattr\n");

	if(curdir==NULL)	//��ʾ��Ŀ¼��
	{
		/*��fd��λ����Ŀ¼������ʼ��ַ*/
		if((ret=lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror("lseek ROOTDIR_OFFSET failed");

		offset=ROOTDIR_OFFSET;

		/*�Ӹ�Ŀ¼����ʼ������ֱ����������ʼ��ַ*/
		while(offset<DATA_OFFSET)	//����
		{
			ret=getEntry(&entry);//�ɹ���ȡ
			offset+=abs(ret);

			if(ret>0)
			{
				printf("%16s\t"
					"%d-%d-%d\t"
					"%d:%d:%d  \t"
					"%d\t"
					"%d\t"
					"%s\n",
					entry.short_name,
					entry.year,entry.month,entry.day,
					entry.hour,entry.minute,entry.second,
					entry.FirstCluster,
					entry.size,
					(entry.subdir)?"dir":"file");
			}
		}
	}
	else				//ʾ��ǰĿ¼���ҷ�root��
	{
		cluster_addr=DATA_OFFSET+(curdir->FirstCluster-2)*CLUSTER_SIZE;	//�صı��ͨ����2��ʼ��FAT�ļ�ϵͳ���ô�����ķ�ʽ�ķ�ʽ
		if((ret=lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");

		offset=cluster_addr;

		/*ֻ��һ�ص�����*/
		while(offset<cluster_addr+CLUSTER_SIZE)
		{
			ret=getEntry(&entry);
			offset+=abs(ret);

			if(ret>0)
			{
				printf("%16s\t"
					"%d-%d-%d\t"
					"%d:%d:%d  \t"
					"%d\t"
					"%d\t"
					"%s\n",
					entry.short_name,
					entry.year,entry.month,entry.day,
					entry.hour,entry.minute,entry.second,
					entry.FirstCluster,
					entry.size,
					(entry.subdir)?"dir":"file");
			}
		}
	}

	return 0;
}

/*
 * ������dir,���ͣ�char
 * ����ֵ��1���ɹ���-1��ʧ��
 * ���ܣ��ı�Ŀ¼����Ŀ¼����Ŀ¼
 */
int ud_cd(char *directory)
{
	Entry *pentry;	// ���ڴ洢ɨ�赽��Ŀ¼����Ϣ
	int ret;		// ���ڴ洢�������÷���ֵ

	// ���Ŀ¼��Ϊ��ǰĿ¼��"."����������ı�Ŀ¼��ֱ�ӷ��سɹ�
	if(strcmp(directory,".")==0)
	{
		return 1;
	}

	// ���Ŀ¼��Ϊ��һ��Ŀ¼��".."�����ҵ�ǰĿ¼Ϊ�գ���Ŀ¼����������ı�Ŀ¼��ֱ�ӷ��سɹ�
	if(strcmp(directory,"..")==0&&curdir==NULL)
		return 1;

	 // ���Ŀ¼��Ϊ��һ��Ŀ¼��".."���ҵ�ǰĿ¼��Ϊ�գ�������һ��Ŀ¼
	if(strcmp(directory,"..")==0&&curdir!=NULL)
	{
		curdir=fatherdir[dirno];
		dirno--;
		return 1;
	}
	//������ǽ��뵱ǰ��һ���ļ���
	// �����ڴ��Դ洢Ŀ¼����Ϣ
	pentry=(Entry *)malloc(sizeof(Entry));

	// ɨ��Ŀ¼����ȡĿ¼����Ϣ
	ret=scanEntry(directory,pentry,1);
	if(ret<0)
	{
		// ɨ��ʧ�ܣ������ʾ��Ϣ���ͷ��ڴ棬����ʧ��
		printf("no such directory\n");
		free(pentry);
		return -1;
	}

	// Ŀ¼�ż�һ�����浱ǰĿ¼����Ŀ¼���飬���µ�ǰĿ¼Ϊɨ�赽��Ŀ¼��
	dirno++;
	fatherdir[dirno]=curdir;
	curdir=pentry;
	return 1;	// ���سɹ�
}

/*
 * ������filename		���ͣ�char *�������ļ�������
 * 		size		���ͣ�int���ļ��Ĵ�С
 * ����ֵ��1���ɹ���-1��ʧ��
 * ���ܣ��ڵ�ǰĿ¼�´����ļ�
 */
int ud_cf(char *filename,int size)
{
	Entry *pentry;
	int ret,i=0,cluster_addr,offset;
	unsigned short cluster,clusterno[FAT_SIZE];
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	struct tm *local;
	time_t t;
	int cutime,cudate;
	unsigned char data[size];

	t=time(NULL);
	local=localtime(&t);
	//Hour��Сʱ���� �Ӹ�λ��ʼռ��5λ����λ11��15��
	// Minute�����ӣ��� �м��6λ����λ5��10��
	// Second���룩�� ��λռ��5λ����λ0��4��
	//�������޳�����Ҫ�Ĳ��֣�������Ҫ�ģ�����λ
	//���ֶ�ͨ����2������Ƚ��д洢��������Ҫ����2����ȡ��ʵ������
	//����ļ�ϵͳ��ʱ���
	cutime=local->tm_hour*2048+local->tm_min*32+local->tm_sec/2;
	cudate=(local->tm_year+1900-1980)*512+(local->tm_mon+1)*32+local->tm_mday;

	pentry=(Entry *)malloc(sizeof(Entry));
	ret=scanEntry(filename,pentry,0);			//ɨ���Ŀ¼���Ƿ��Ѵ��ڸ��ļ���
	if(ret<0)	//�����ڸ��ļ���
	{
		printf("enter the file's data:\n");
		scanf("%s",data);
		size=strlen(data);
		clustersize=size/CLUSTER_SIZE;

		if(size%CLUSTER_SIZE!=0)
			clustersize++;

		/*��ѯfat���ҵ��հ״أ�������clusterno[]��*/
		for(cluster=2;cluster<1000;cluster++)
		{
			index=cluster*2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				clusterno[i]=cluster;

				i++;
				if(i==clustersize)
					break;
			}
		}

		/*��fat����д����һ����Ϣ*/
		for(i=0;i<clustersize-1;i++)
		{
			index=clusterno[i]*2;
			fatbuf[index]=(clusterno[i+1]&0x00ff);
			fatbuf[index+1]=((clusterno[i+1]&0xff00)>>8);

			if(lseek(fd,(clusterno[i]-2)*CLUSTER_SIZE+DATA_OFFSET,SEEK_SET)<0)	//��������ļ����ļ�ָ������Ϊ�����ļ���ͷoffset���ֽڴ�
				perror("lseek file data failed");
			if(write(fd,data+i*CLUSTER_SIZE,CLUSTER_SIZE)<0)
				perror("write failed");
		}
		/*���һ��д��0xffff*/
		index=clusterno[i]*2;
		fatbuf[index]=0xff;
		fatbuf[index+1]=0xff;

		if(lseek(fd,(clusterno[i]-2)*CLUSTER_SIZE+DATA_OFFSET,SEEK_SET)<0)
			perror("lseek file data failed");
		if(write(fd,data+i*CLUSTER_SIZE,size-i*CLUSTER_SIZE)<0)
			perror("write failed");

		if(curdir==NULL)		//����Ŀ¼��д�ļ�
		{
			if((ret=lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
				perror("lseek ROOTDIR_OFFSET failed");
			offset=ROOTDIR_OFFSET;
			while(offset<DATA_OFFSET)	//Ŀ¼�����������ֿ�����ֹԽ��
			{
				if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)
					perror("read entry failed");

				offset+=abs(ret);

				if(buf[0]!=0xe5&&buf[0]!=0x00)	//Ox00��ʾĿ¼��Ϊ�գ�0xe5��ʾĿ¼������ʹ�ã�����ɾ��  ������Ҫ����������offset
				{
					while(buf[11]==0x0f)
					{
						if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)
							perror("read root dir failed");
						offset+=abs(ret);
					}
				}
				else			//�ҳ���Ŀ¼�����ɾ����Ŀ¼��
				{
					offset-=abs(ret);	//����һ��Ŀ¼��

					/**��д������Ŀ¼��**/
					for(i=0;i<=strlen(filename);i++)
						c[i]=toupper(filename[i]);	//Сдת��д
					for(;i<=10;i++)
						c[i]=' ';		//��' '����ʽ

					c[0x0b]=0x01;

					/*дʱ�������*/
					c[0x16]=(cutime&0x00ff);
					c[0x17]=((cutime&0xff00)>>8);
					c[0x18]=(cudate&0x00ff);
					c[0x19]=((cudate&0xff00)>>8);

					/*д��һ�ص�ֵ*/
					c[0x1a]=(clusterno[0]&0x00ff);
					c[0x1b]=((clusterno[0]&0xff00)>>8);

					/*д�ļ��Ĵ�С*/
					c[0x1c]=(size&0x000000ff);
					c[0x1d]=((size&0x0000ff00)>>8);
					c[0x1e]=((size&0x00ff0000)>>16);
					c[0x1f]=((size&0xff000000)>>24);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek ud_cf failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");

					free(pentry);
					if(writeFat()<0)
						exit(1);

					return 1;
				}
			}
		}
		else	//��ǰĿ¼��Ϊ�գ��ڵ�ǰĿ¼���½��ļ�
		{
			cluster_addr=(curdir->FirstCluster-2)*CLUSTER_SIZE+DATA_OFFSET;
			if((ret=lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");
			offset=cluster_addr;
			while(offset<cluster_addr+CLUSTER_SIZE)  //���Ʋ������صĴ�С����
			{
				if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)
					perror("read entry failed");

				offset+=abs(ret);

				if(buf[0]!=0xe5&&buf[0]!=0x00)
				{
					while(buf[11]==0x0f)
					{
						if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)
							perror("read root dir failed");
						offset+=abs(ret);
					}
				}
				else
				{
					offset-=abs(ret);
					for(i=0;i<=strlen(filename);i++)
						c[i]=toupper(filename[i]);
					for(;i<=10;i++)
						c[i]=' ';

					c[0x0b]=0x01;

					/*дʱ�������*/
					c[0x16]=(cutime&0x00ff);
					c[0x17]=((cutime&0xff00)>>8);
					c[0x18]=(cudate&0x00ff);
					c[0x19]=((cudate&0xff00)>>8);

					/*д��һ�ص�ֵ*/
					c[0x1a]=(clusterno[0]&0x00ff);
					c[0x1b]=((clusterno[0]&0xff00)>>8);

					/*д�ļ��Ĵ�С*/
					c[0x1c]=(size&0x000000ff);
					c[0x1d]=((size&0x0000ff00)>>8);
					c[0x1e]=((size&0x00ff0000)>>16);
					c[0x1f]=((size&0xff000000)>>24);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek ud_cf failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");

					free(pentry);
					if(writeFat()<0)
						exit(1);

					return 1;
				}
			}
		}
	}
	else
	{
		printf("This filename is exist\n");
		free(pentry);
		return -1;
	}

	return 1;
}
/*
 * ������filename		���ͣ�char *���������ļ�����
 * ����ֵ��1���ɹ���-1��ʧ��
 * ���ܣ���ȡ��ǰĿ¼���ļ�������
 */
int ud_rf(char *filename)
{
	Entry *pentry;	// ���ڴ洢ɨ�赽���ļ�����Ϣ
	int ret,size,page;	// �洢�������÷���ֵ���ļ���С���ļ�ҳ��
	int i=0;			// ѭ������
	unsigned char *buf;		// ���ڴ洢�ļ����ݵĻ�����
	unsigned short seed;	// �洢�ļ��ĵ�һ���غ�

	// �����ڴ��Դ洢�ļ�����Ϣ
	pentry=(Entry *)malloc(sizeof(Entry));

	//ɨ�赱ǰĿ¼�����ļ�
	ret=scanEntry(filename,pentry,0);
	if(ret<0)
	{
		// ɨ��ʧ�ܣ������ʾ��Ϣ���ͷ��ڴ棬����ʧ��
		printf("no such file\n");
		free(pentry);
		return -1;
	}

	seed=pentry->FirstCluster;	// ��ȡ�ļ��ĵ�һ���غ�
	size=pentry->size;			// ��ȡ�ļ���С
	page=size/CLUSTER_SIZE;		// �����ļ�ҳ��
	buf=(unsigned char *)malloc(sizeof(unsigned char)*size);

	// ��ҳ��ȡ�ļ�����
	for(i=0;i<page;i++)
	{
		if((ret=lseek(fd,(seed-2)*CLUSTER_SIZE+DATA_OFFSET,SEEK_SET))<0)
			perror("lseek cluster_addr failed");
		read(fd,buf+i*CLUSTER_SIZE,CLUSTER_SIZE);
		seed=getFatCluster(seed);	// ��ȡ��һ���غ�
	}

	// ��ȡ���һҳ�ļ�����
	if((ret=lseek(fd,(seed-2)*CLUSTER_SIZE+DATA_OFFSET,SEEK_SET))<0)
		perror("lseek cluster_addr failed");
	read(fd,buf+i*CLUSTER_SIZE,size-i*CLUSTER_SIZE);

	// ����ļ����ݵ���׼���
	for(i=0;i<size;i++)
		printf("%c",buf[i]);
	printf("\n");

	return 0;	// ���سɹ�
}

/*
 * ������directory�����ͣ�char *
 * ����ֵ��1���ɹ�����1��ʧ��
 * ���ܣ��ڵ�ǰĿ¼�´���Ŀ¼
 */
int ud_md(char *directory)
{
	Entry *pentry;
	int ret,i=0,cluster_addr,offset;
	unsigned short cluster;
	unsigned char c[DIR_ENTRY_SIZE];
	int index;
	unsigned char buf[DIR_ENTRY_SIZE];
	struct tm *local;
	time_t t;
	int cutime,cudate;

	t=time(NULL);
	local=localtime(&t);
	cutime=local->tm_hour*2048+local->tm_min*32+local->tm_sec/2;
	cudate=(local->tm_year+1900-1980)*512+(local->tm_mon+1)*32+local->tm_mday;

	pentry=(Entry *)malloc(sizeof(Entry));
	ret=scanEntry(directory,pentry,1);			//ɨ���Ŀ¼���Ƿ��Ѵ��ڸ�Ŀ¼��

	if(ret<0)
	{
		/*��ѯfat���ҵ��հ״أ�������clusterno[]��*/
		for(cluster=2;cluster<1000;cluster++)
		{
			index=cluster*2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				/*��fat����д��0xffff*/
				fatbuf[index]=0xff;
				fatbuf[index+1]=0xff;

				break;
			}
		}

		if(curdir==NULL)		//����Ŀ¼��дĿ¼
		{
			if((ret=lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
				perror("lseek ROOTDIR_OFFSET failed");
			offset=ROOTDIR_OFFSET;
			while(offset<DATA_OFFSET)
			{
				if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)
					perror("read entry failed");

				offset+=abs(ret);

				if(buf[0]!=0xe5&&buf[0]!=0x00)
				{
					while(buf[11]==0x0f)
					{
						if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)
							perror("read root dir failed");
						offset+=abs(ret);
					}
				}
				else			//�ҳ���Ŀ¼�����ɾ����Ŀ¼��
				{
					offset-=abs(ret);
					for(i=0;i<=strlen(directory);i++)
						c[i]=toupper(directory[i]);
					for(;i<=10;i++)
						c[i]=' ';

					c[0x0b]=0x10;

					/*дʱ�������*/
					c[0x16]=(cutime&0x00ff);
					c[0x17]=((cutime&0xff00)>>8);
					c[0x18]=(cudate&0x00ff);
					c[0x19]=((cudate&0xff00)>>8);

					/*д��һ�ص�ֵ*/
					c[0x1a]=(cluster&0x00ff);
					c[0x1b]=((cluster&0xff00)>>8);

					/*дĿ¼�Ĵ�С*/
					c[0x1c]=(0x00000000&0x000000ff);
					c[0x1d]=((0x00000000&0x0000ff00)>>8);
					c[0x1e]=((0x00000000&0x00ff0000)>>16);
					c[0x1f]=((0x00000000&0xff000000)>>24);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek ud_md failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");

					free(pentry);
					if(writeFat()<0)
						exit(1);

					return 1;
				}
			}
		}
		else
		{
			cluster_addr=(curdir->FirstCluster-2)*CLUSTER_SIZE+DATA_OFFSET;
			if((ret=lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");
			offset=cluster_addr;
			while(offset<cluster_addr+CLUSTER_SIZE)
			{
				if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)
					perror("read entry failed");

				offset+=abs(ret);

				if(buf[0]!=0xe5&&buf[0]!=0x00)
				{
					while(buf[11]==0x0f)
					{
						if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)
							perror("read root dir failed");
						offset+=abs(ret);
					}
				}
				else
				{
					offset=offset-abs(ret);
					for(i=0;i<=strlen(directory);i++)
						c[i]=toupper(directory[i]);
					for(;i<=10;i++)
						c[i]=' ';

					c[0x0b]=0x10;

					/*дʱ�������*/
					c[0x16]=(cutime&0x00ff);
					c[0x17]=((cutime&0xff00)>>8);
					c[0x18]=(cudate&0x00ff);
					c[0x19]=((cudate&0xff00)>>8);

					/*д��һ�ص�ֵ*/
					c[0x1a]=(cluster&0x00ff);
					c[0x1b]=((cluster&0xff00)>>8);

					/*дĿ¼�Ĵ�С*/
					c[0x1c]=(0x00000000&0x000000ff);
					c[0x1d]=((0x00000000&0x0000ff00)>>8);
					c[0x1e]=((0x00000000&0x00ff0000)>>16);
					c[0x1f]=((0x00000000&0xff000000)>>24);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek ud_md failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");

					free(pentry);
					if(writeFat()<0)
						exit(1);

					return 1;
				}
			}
		}
	}
	else
	{
		printf("This directory is exist\n");
		free(pentry);
		return -1;
	}

	return 1;
}

/*
 * ������filename�����ͣ�char *
 * ����ֵ��1���ɹ���-1��ʧ��
 * ���ܣ�ɾ����ǰĿ¼�µ��ļ�
 */
int ud_df(char *filename)
{
	Entry *pentry;
	int ret;
	unsigned char c;
	unsigned short seed,next;

	pentry=(Entry *)malloc(sizeof(Entry));
	ret=scanEntry(filename,pentry,0);			//ɨ�赱ǰĿ¼�����ļ�

	if(ret<0)
	{
		printf("no such file\n");
		free(pentry);
		return -1;
	}

	/*���fat����*/
	seed=pentry->FirstCluster;	//��ȡ�ļ��ĵ�һ���غ�(�ļ�������FAT�е���ʼλ��)
	while((next=getFatCluster(seed))!=0xffff)	//�����ļ������дأ�ֱ��FAT��Ľ������
	{
		clearFatCluster(seed);
		seed=next;
	}

	clearFatCluster(seed);

	/*���Ŀ¼����*/
	c=0xe5;	//���Ŀ¼������ʹ�ã�����ɾ��

	if(lseek(fd,ret-0x20,SEEK_SET)<0)
		perror("lseek ud_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");

	free(pentry);
	if(writeFat()<0)
		exit(1);
	return 1;
}

/*
 * ������directory�����ͣ�char *
 * ����ֵ��1���ɹ���-1��ʧ��
 * ���ܣ�ɾ����ǰĿ¼��ָ����Ŀ¼
 */
int ud_dd(char *directory)
{
	Entry *pentry;
	int ret;
	unsigned char c;
	char ch;
	unsigned short seed,next;

	pentry=(Entry *)malloc(sizeof(Entry));
	ret=scanEntry(directory,pentry,1);			//ɨ�赱ǰĿ¼����Ŀ¼

	if(ret<0)
	{
		printf("no such directory\n");
		free(pentry);
		return -1;
	}

	/*���fat����*/
	seed=pentry->FirstCluster;
	//if(getFatCluster(seed)!=0xffff)
	//{
		//printf("This directory is not empty, do you want to delete it? (Y/N) ");

		//ch=getchar();
	//	if(ch=='Y'||ch=='y')
		//	;
		//else if(ch=='N'||ch=='n')
			//return -1;
		//while(ch!='\n')
			//ch=getchar();
	//}

	while((next=getFatCluster(seed))!=0xffff)
	{
		clearFatCluster(seed);
		seed=next;
	}

	clearFatCluster(seed);

	/*���Ŀ¼����*/
	c=0xe5;	//���Ŀ¼������ʹ�ã�����ɾ��

	if(lseek(fd,ret-0x20,SEEK_SET)<0)
		perror("lseek ud_dd failed");
	if(write(fd,&c,1)<0)
		perror("write failed");
		
			if(lseek(fd,ret-0x40,SEEK_SET)<0)
		perror("lseek ud_dd failed");
	if(write(fd,&c,1)<0)
		perror("write failed");

	free(pentry);
	if(writeFat()<0)
		exit(1);
	return 1;
}

/*ʹ�÷���*/
void do_usage()
{
	printf("please input a command, including followings:\n");
	printf("\tls <-a|-all>\t\tlist all non-root entries\n"
			"\tls <-c|-cur>\t\tlist all entries under current directory\n"
			"\tcd <dir>\t\tchange direcotry\n"
			"\tcf <filename> <size>\tcreate a file\n"
			"\tmd <directory>\t\tmake a directory\n"
			"\tdf <file>\t\tdelete a file\n"
			"\tdd <directory>\t\tdelete a directory\n"
			"\trf <filename>\t\tread a file\n"
			"\texit\t\t\texit this system\n");
}

int main()
{
	char input[10];
	int size=0;
	int i,j,k,num;
	char name[30],subname[30];

	// ��ָ�����豸�ļ��Խ��ж�д������
	//open ������һ��ϵͳ���ã����ڴ��ļ����ߴ����ļ���DEVNAME ���ļ�����O_RDWR �Ǵ��ļ���ģʽ����ʾ�Կɶ�д�ķ�ʽ���ļ���
	if((fd=open(DEVNAME,O_RDWR))<0)
		perror("open failed");

	// ���ã���ȡ��һ��512�ֽڣ����������洢������洢��Ϣ��
	scanBootSector();

	// ��ȡ�ļ������FAT��
	if(readFat()<0)
		exit(1);

	// ��ʾ�÷���Ϣ����ӡ��
	do_usage();

	// �������ѭ��
	while(1)
	{
		memset(name,0,30);	// name �����ǰ 30 ���ֽڶ�����Ϊ 0������ʾЧ����
		printf(">");
		scanf("%s",input);

		// ����û�����ĸ�������
		if(strcmp(input,"exit")==0)
			break;
		else if(strcmp(input,"\n")==0)
			continue;
		else if(strcmp(input,"ls")==0)
		{
			scanf("%s",name);
			// �ж��û�ѡ��鿴������
			if(strcmp(name,"-a")==0||strcmp(name,"-all")==0) //�鿴���ԵķǸ�Ŀ¼��Ϣ
				ud_ls(1);
			else
				ud_ls(0);
		}
		else if(strcmp(input,"dir")==0)	//�鿴��ǰĿ¼��Ϣ
			ud_ls(0);
		else if(strcmp(input,"cd")==0)
		{
			scanf("%s",name);
			// ��������·������Ŀ¼
			if(strncmp(name,"/media/disk/",12)==0)	// ������ "/media/disk/" ��ͷ�ľ���·��
			{
				if(name[strlen(name)-1] != '/')
					strcat(name, "/");
				curdir=NULL;
				dirno=0;
				for(i=12,num=12;i<30&&name[i]!='\0';i++)
				{
					if(name[i]=='/')
					{	// ��ȡ��Ŀ¼����
						memset(subname,0,30);
						for(j=num,k=0;j<i;j++,k++)
							subname[k]=name[j];
						if(ud_cd(subname)<0)	//������Ŀ��Ŀ¼
							break;
						num=i+1;
					}
				}
			}
			else if(strncmp(name,"./",2)==0)	// ������ "./" ��ͷ�����·��  �������
			{
				if(name[strlen(name)-1] != '/')
					strcat(name, "/");
				ud_cd(".");
				for(i=2,num=2;i<30&&name[i]!='\0';i++)
				{
					if(name[i]=='/')
					{	// ��ȡ��Ŀ¼����
						memset(subname,0,30);
						for(j=num,k=0;j<i;j++,k++)
							subname[k]=name[j];
						if(ud_cd(subname)<0)
							break;
						num=i+1;
					}
				}
			}
			// ������ "../" ��ͷ�����·��
			else if(strncmp(name,"../",3)==0)	//���·��������Ŀ¼
			{
				if(name[strlen(name)-1] != '/')
					strcat(name, "/");
				ud_cd("..");
				for(i=3,num=3;i<30&&name[i]!='\0';i++)
				{
					if(name[i]=='/')
					{	// ��ȡ��Ŀ¼����
						memset(subname,0,30);
						for(j=num,k=0;j<i;j++,k++)
							subname[k]=name[j];
						if(ud_cd(subname)<0)
							break;
						num=i+1;
					}
				}
			}
			// ���������������ǰĿ¼����Ŀ¼��
			else
				ud_cd(name);
		}
		else if(strcmp(input,"cf")==0)
		{
			scanf("%s",name);
			scanf("%s",input);
			size=atoi(input);	//�ַ���תInt
			// ʹ��ָ�������ƺʹ�С�������ļ�
			ud_cf(name,size);
		}
		else if(strcmp(input,"md")==0)
		{
			scanf("%s",name);
			// ʹ��ָ�������ƴ�����Ŀ¼
			ud_md(name);
		}
		else if(strcmp(input,"df")==0)
		{
			scanf("%s",name);
			// ɾ��ָ���ļ�
			ud_df(name);
		}
		else if(strcmp(input,"dd")==0)
		{
			scanf("%s",name);
			// ɾ��ָ����Ŀ¼
			ud_dd(name);
		}
		else if(strcmp(input,"rf")==0)
		{
			scanf("%s",name);
			// ��ȡָ���ļ�����
			ud_rf(name);
		}
		else
			// �����޷�ʶ��������ʾ�÷���Ϣ
			do_usage();
	}

	return 0;
}
