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
//字节翻转（大小端）
#define RevByte(low,high) ((high)<<8|(low))
//字的翻转（四个字节反一下）
#define RevWord(lowest,lower,higher,highest) ((highest)<<24|(higher)<<16|(lower)<<8|lowest)

/*
 * 读fat表的信息，存入fatbuf[]中
 */
//lseek 是一个用于在文件中定位文件指针位置的系统调用。lseek 用于移动文件描述符 fd 关联的文件指针到指定的位置。
// 具体来说，lseek 的原型是：
// off_t lseek(int fd, off_t offset, int whence);
//SEEK_SET就是初始位置
int readFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	//read 函数从当前文件指针位置读取 FAT_SIZE 字节的数据到 fatbuf 缓冲区中
	if(read(fd,fatbuf,FAT_SIZE)<0)
	{
		perror("read failed");
		return -1;
	}

	return 1;
}

/*
 * 将改变的fat表值写回fat表
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

/*打印启动项记录*/
//扫描引导扇区（Boot Sector）
void scanBootSector()
{
	unsigned char buf[SECTOR_SIZE];
	int ret,i;

	if((ret=read(fd,buf,SECTOR_SIZE))<0)
		perror("read boot sector failed");
	for(i=0;i<8;i++)
		bdptor.Oem_name[i]=buf[i+0x03];
	bdptor.Oem_name[i]='\0';

	// 这部分代码通过从 buf 数组中提取相应的字节，
	// 将引导扇区的信息存储到结构体 bdptor 中。例如，
	// bdptor.Oem_name 是一个字符数组，用于存储引导扇
	// 区的 OEM 名称，通过将 buf 中的相应字节复制到 Oem_name 中来实现。
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
 * 参数：entryname	类型：char
 * 		pentry		类型：Entry *
 * 		mode		类型：int，mode=1，为目录表项；mode=0，为文件
 * 返回值：偏移值大于0，则成功；-1，则失败
 * 功能：搜索当前目录，查找文件或目录项
 */
int scanEntry(char *entryname,Entry *pentry,int mode)
{
	int ret,offset,i;
	int cluster_addr;
	char uppername[80];

	for(i=0;i<strlen(entryname);i++)
		uppername[i]=toupper(entryname[i]);
	uppername[i]='\0';

	if(curdir==NULL)		//扫描根目录
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
	else					//扫描子目录
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
 * 参数：entry，类型：Entry *
 * 返回值：成功，则返回偏移值；失败：返回负值
 * 功能：从根目录或文件簇中得到文件表项
 */
int getEntry(Entry *pentry)
{
	int ret,i;
	int count=0; // 记录读取的字节数
	unsigned char buf[DIR_ENTRY_SIZE],info[2];

	/*读一个目录表项，即32字节*/
	if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)  // 读取失败时输出错误信息
		perror("read entry failed");
	count+=ret;  // 记录读取的字节数

	if(buf[0]==0xe5||buf[0]==0x00)
		return -1*count;  // 如果目录项为空或已删除，则返回负值
	else
	{
		/* buf读的是文件名，在FAT文件系统中，如果目录项的第一个字节是 0x0f，则表示这是一个长文件名的条目。循环读取并丢弃这些长文件名的条目。长文件名，忽略掉 */
		/*长文件名，忽略掉*/
		while(buf[11]==0x0f)
		{
			if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)  //read是系统调用，fd是文件描述符
				perror("read root dir failed");
			count+=ret; // 记录读取的字节数
		}

		/*命名格式化，注意结尾的'\0'*/
		for (i=0;i<=10;i++)
			pentry->short_name[i]=buf[i];
		pentry->short_name[i]='\0';  // 确保字符串结尾有'\0'

		formatFileName(pentry->short_name);  // 格式化文件名

		info[0]=buf[22];
		info[1]=buf[23];
		findTime(&(pentry->hour),&(pentry->minute),&(pentry->second),info);

		info[0]=buf[24];
		info[1]=buf[25];
		findDate(&(pentry->year),&(pentry->month),&(pentry->day),info);

		// 根据文件属性位设置Entry结构中的标志
		pentry->FirstCluster=RevByte(buf[26],buf[27]);
		pentry->size=RevWord(buf[28],buf[29],buf[30],buf[31]);

		pentry->readonly=(buf[11]&ATTR_READONLY)?1:0;
		pentry->hidden=(buf[11]&ATTR_HIDDEN)?1:0;
		pentry->system=(buf[11]&ATTR_SYSTEM)?1:0;
		pentry->vlabel=(buf[11]&ATTR_VLABEL)?1:0;
		pentry->subdir=(buf[11]&ATTR_SUBDIR)?1:0;
		pentry->archive=(buf[11]&ATTR_ARCHIVE)?1:0;

		return count;  // 返回读取的字节数
	}
}

/*日期*/
void findDate(unsigned short *year,unsigned short *month,unsigned short *day,unsigned char info[2])
{
	int date;
	date=RevByte(info[0],info[1]);

	*year=((date&MASK_YEAR)>>9)+1980;
	*month=((date&MASK_MONTH)>>5);
	*day=(date&MASK_DAY);
}

/*时间*/
//Hour（小时）： 从高位开始占用5位，即位11到15。
// Minute（分钟）： 中间的6位，即位5到10。
// Second（秒）： 低位占用5位，即位0到4。
//用掩码剔除不需要的部分，保留需要的，再移位
//秒字段通常以2秒的粒度进行存储，所以需要乘以2来获取真实的秒数
void findTime(unsigned short *hour,unsigned short *min,unsigned short *sec,unsigned char info[2])
{
	int time;
	time=RevByte(info[0],info[1]);

	*hour=((time&MASK_HOUR)>>11);
	*min=(time&MASK_MIN)>>5;
	*sec=(time&MASK_SEC)*2;
}

/*文件名格式化，便于比较*/
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
 * 参数：prev，类型：unsigned char
 * 返回值：下一簇
 * 功能：在fat表中获得下一簇的位置
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
 * 参数：cluster，类型：unsigned short
 * 返回值：void
 * 功能：清除fat表中的簇信息
 */
void clearFatCluster(unsigned short cluster)
{
	int index;
	index=cluster*2;

	fatbuf[index]=0x00;
	fatbuf[index+1]=0x00;
}

/*
 * 参数：mode，类型：int
 * 返回值：1，成功；-1，失败
 * 功能：显示所要查看目录下的内容。
 * 说明：mode值为0时，查看当前目录信息；mode值为1时，查看所有的非根目录信息
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
		{//遍历把读到的东西全打出来
			cluster_addr=DATA_OFFSET+i*CLUSTER_SIZE; //一簇一簇的读
			if((ret=lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset=cluster_addr;

			/*只读一簇的内容*/
			while(offset<cluster_addr+CLUSTER_SIZE)
			{
				ret=getEntry(&entry);	//读取文件表项存到entry，并返回偏移量表示已经读取，这一簇的信息已经获取准备给下一簇用
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

	//模式1结束 ，下面是模式0
	if(curdir==NULL)
		printf("Root_dir\n");//根目录
	else
		printf("%s_dir\n",curdir->short_name);//对应的名字

	printf("\tname\t\tdate\t\ttime\t\tcluster\tsize\tattr\n");

	if(curdir==NULL)	//显示根目录区
	{
		/*将fd定位到根目录区的起始地址*/
		if((ret=lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror("lseek ROOTDIR_OFFSET failed");

		offset=ROOTDIR_OFFSET;

		/*从根目录区开始遍历，直到数据区起始地址*/
		while(offset<DATA_OFFSET)	//读完
		{
			ret=getEntry(&entry);//成功读取
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
	else				//示当前目录（且非root）
	{
		cluster_addr=DATA_OFFSET+(curdir->FirstCluster-2)*CLUSTER_SIZE;	//簇的编号通常从2开始（FAT文件系统采用簇链表的方式的方式
		if((ret=lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");

		offset=cluster_addr;

		/*只读一簇的内容*/
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
 * 参数：dir,类型：char
 * 返回值：1，成功；-1，失败
 * 功能：改变目录到父目录或子目录
 */
int ud_cd(char *directory)
{
	Entry *pentry;	// 用于存储扫描到的目录项信息
	int ret;		// 用于存储函数调用返回值

	// 如果目录名为当前目录（"."），则无需改变目录，直接返回成功
	if(strcmp(directory,".")==0)
	{
		return 1;
	}

	// 如果目录名为上一级目录（".."），且当前目录为空（根目录），则无需改变目录，直接返回成功
	if(strcmp(directory,"..")==0&&curdir==NULL)
		return 1;

	 // 如果目录名为上一级目录（".."）且当前目录不为空，返回上一级目录
	if(strcmp(directory,"..")==0&&curdir!=NULL)
	{
		curdir=fatherdir[dirno];
		dirno--;
		return 1;
	}
	//否则就是进入当前的一个文件夹
	// 分配内存以存储目录项信息
	pentry=(Entry *)malloc(sizeof(Entry));

	// 扫描目录，获取目录项信息
	ret=scanEntry(directory,pentry,1);
	if(ret<0)
	{
		// 扫描失败，输出提示信息，释放内存，返回失败
		printf("no such directory\n");
		free(pentry);
		return -1;
	}

	// 目录号加一，保存当前目录到父目录数组，更新当前目录为扫描到的目录项
	dirno++;
	fatherdir[dirno]=curdir;
	curdir=pentry;
	return 1;	// 返回成功
}

/*
 * 参数：filename		类型：char *，创建文件的名称
 * 		size		类型：int，文件的大小
 * 返回值：1，成功；-1，失败
 * 功能：在当前目录下创建文件
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
	//Hour（小时）： 从高位开始占用5位，即位11到15。
	// Minute（分钟）： 中间的6位，即位5到10。
	// Second（秒）： 低位占用5位，即位0到4。
	//用掩码剔除不需要的部分，保留需要的，再移位
	//秒字段通常以2秒的粒度进行存储，所以需要乘以2来获取真实的秒数
	//获得文件系统的时间戳
	cutime=local->tm_hour*2048+local->tm_min*32+local->tm_sec/2;
	cudate=(local->tm_year+1900-1980)*512+(local->tm_mon+1)*32+local->tm_mday;

	pentry=(Entry *)malloc(sizeof(Entry));
	ret=scanEntry(filename,pentry,0);			//扫描根目录，是否已存在该文件名
	if(ret<0)	//不存在该文件名
	{
		printf("enter the file's data:\n");
		scanf("%s",data);
		size=strlen(data);
		clustersize=size/CLUSTER_SIZE;

		if(size%CLUSTER_SIZE!=0)
			clustersize++;

		/*查询fat表，找到空白簇，保存在clusterno[]中*/
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

		/*在fat表中写入下一簇信息*/
		for(i=0;i<clustersize-1;i++)
		{
			index=clusterno[i]*2;
			fatbuf[index]=(clusterno[i+1]&0x00ff);
			fatbuf[index+1]=((clusterno[i+1]&0xff00)>>8);

			if(lseek(fd,(clusterno[i]-2)*CLUSTER_SIZE+DATA_OFFSET,SEEK_SET)<0)	//随机访问文件，文件指针设置为距离文件开头offset个字节处
				perror("lseek file data failed");
			if(write(fd,data+i*CLUSTER_SIZE,CLUSTER_SIZE)<0)
				perror("write failed");
		}
		/*最后一簇写入0xffff*/
		index=clusterno[i]*2;
		fatbuf[index]=0xff;
		fatbuf[index+1]=0xff;

		if(lseek(fd,(clusterno[i]-2)*CLUSTER_SIZE+DATA_OFFSET,SEEK_SET)<0)
			perror("lseek file data failed");
		if(write(fd,data+i*CLUSTER_SIZE,size-i*CLUSTER_SIZE)<0)
			perror("write failed");

		if(curdir==NULL)		//往根目录下写文件
		{
			if((ret=lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
				perror("lseek ROOTDIR_OFFSET failed");
			offset=ROOTDIR_OFFSET;
			while(offset<DATA_OFFSET)	//目录区和数据区分开，防止越界
			{
				if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)
					perror("read entry failed");

				offset+=abs(ret);

				if(buf[0]!=0xe5&&buf[0]!=0x00)	//Ox00表示目录项为空，0xe5表示目录项曾被使用，现已删除  不能用要跳过，更新offset
				{
					while(buf[11]==0x0f)
					{
						if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)
							perror("read root dir failed");
						offset+=abs(ret);
					}
				}
				else			//找出空目录项或已删除的目录项
				{
					offset-=abs(ret);	//回退一个目录项

					/**编写创建的目录项**/
					for(i=0;i<=strlen(filename);i++)
						c[i]=toupper(filename[i]);	//小写转大写
					for(;i<=10;i++)
						c[i]=' ';		//补' '：格式

					c[0x0b]=0x01;

					/*写时间和日期*/
					c[0x16]=(cutime&0x00ff);
					c[0x17]=((cutime&0xff00)>>8);
					c[0x18]=(cudate&0x00ff);
					c[0x19]=((cudate&0xff00)>>8);

					/*写第一簇的值*/
					c[0x1a]=(clusterno[0]&0x00ff);
					c[0x1b]=((clusterno[0]&0xff00)>>8);

					/*写文件的大小*/
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
		else	//当前目录不为空，在当前目录下新建文件
		{
			cluster_addr=(curdir->FirstCluster-2)*CLUSTER_SIZE+DATA_OFFSET;
			if((ret=lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");
			offset=cluster_addr;
			while(offset<cluster_addr+CLUSTER_SIZE)  //控制不超过簇的大小限制
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

					/*写时间和日期*/
					c[0x16]=(cutime&0x00ff);
					c[0x17]=((cutime&0xff00)>>8);
					c[0x18]=(cudate&0x00ff);
					c[0x19]=((cudate&0xff00)>>8);

					/*写第一簇的值*/
					c[0x1a]=(clusterno[0]&0x00ff);
					c[0x1b]=((clusterno[0]&0xff00)>>8);

					/*写文件的大小*/
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
 * 参数：filename		类型：char *，待查找文件名称
 * 返回值：1，成功；-1，失败
 * 功能：读取当前目录下文件的内容
 */
int ud_rf(char *filename)
{
	Entry *pentry;	// 用于存储扫描到的文件项信息
	int ret,size,page;	// 存储函数调用返回值、文件大小和文件页数
	int i=0;			// 循环变量
	unsigned char *buf;		// 用于存储文件内容的缓冲区
	unsigned short seed;	// 存储文件的第一个簇号

	// 分配内存以存储文件项信息
	pentry=(Entry *)malloc(sizeof(Entry));

	//扫描当前目录查找文件
	ret=scanEntry(filename,pentry,0);
	if(ret<0)
	{
		// 扫描失败，输出提示信息，释放内存，返回失败
		printf("no such file\n");
		free(pentry);
		return -1;
	}

	seed=pentry->FirstCluster;	// 获取文件的第一个簇号
	size=pentry->size;			// 获取文件大小
	page=size/CLUSTER_SIZE;		// 计算文件页数
	buf=(unsigned char *)malloc(sizeof(unsigned char)*size);

	// 逐页读取文件内容
	for(i=0;i<page;i++)
	{
		if((ret=lseek(fd,(seed-2)*CLUSTER_SIZE+DATA_OFFSET,SEEK_SET))<0)
			perror("lseek cluster_addr failed");
		read(fd,buf+i*CLUSTER_SIZE,CLUSTER_SIZE);
		seed=getFatCluster(seed);	// 获取下一个簇号
	}

	// 读取最后一页文件内容
	if((ret=lseek(fd,(seed-2)*CLUSTER_SIZE+DATA_OFFSET,SEEK_SET))<0)
		perror("lseek cluster_addr failed");
	read(fd,buf+i*CLUSTER_SIZE,size-i*CLUSTER_SIZE);

	// 输出文件内容到标准输出
	for(i=0;i<size;i++)
		printf("%c",buf[i]);
	printf("\n");

	return 0;	// 返回成功
}

/*
 * 参数：directory，类型：char *
 * 返回值：1，成功；－1，失败
 * 功能：在当前目录下创建目录
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
	ret=scanEntry(directory,pentry,1);			//扫描根目录，是否已存在该目录名

	if(ret<0)
	{
		/*查询fat表，找到空白簇，保存在clusterno[]中*/
		for(cluster=2;cluster<1000;cluster++)
		{
			index=cluster*2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				/*在fat表中写入0xffff*/
				fatbuf[index]=0xff;
				fatbuf[index+1]=0xff;

				break;
			}
		}

		if(curdir==NULL)		//往根目录下写目录
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
				else			//找出空目录项或已删除的目录项
				{
					offset-=abs(ret);
					for(i=0;i<=strlen(directory);i++)
						c[i]=toupper(directory[i]);
					for(;i<=10;i++)
						c[i]=' ';

					c[0x0b]=0x10;

					/*写时间和日期*/
					c[0x16]=(cutime&0x00ff);
					c[0x17]=((cutime&0xff00)>>8);
					c[0x18]=(cudate&0x00ff);
					c[0x19]=((cudate&0xff00)>>8);

					/*写第一簇的值*/
					c[0x1a]=(cluster&0x00ff);
					c[0x1b]=((cluster&0xff00)>>8);

					/*写目录的大小*/
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

					/*写时间和日期*/
					c[0x16]=(cutime&0x00ff);
					c[0x17]=((cutime&0xff00)>>8);
					c[0x18]=(cudate&0x00ff);
					c[0x19]=((cudate&0xff00)>>8);

					/*写第一簇的值*/
					c[0x1a]=(cluster&0x00ff);
					c[0x1b]=((cluster&0xff00)>>8);

					/*写目录的大小*/
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
 * 参数：filename，类型：char *
 * 返回值：1，成功；-1，失败
 * 功能：删除当前目录下的文件
 */
int ud_df(char *filename)
{
	Entry *pentry;
	int ret;
	unsigned char c;
	unsigned short seed,next;

	pentry=(Entry *)malloc(sizeof(Entry));
	ret=scanEntry(filename,pentry,0);			//扫描当前目录查找文件

	if(ret<0)
	{
		printf("no such file\n");
		free(pentry);
		return -1;
	}

	/*清除fat表项*/
	seed=pentry->FirstCluster;	//获取文件的第一个簇号(文件数据在FAT中的起始位置)
	while((next=getFatCluster(seed))!=0xffff)	//遍历文件的所有簇，直到FAT表的结束标记
	{
		clearFatCluster(seed);
		seed=next;
	}

	clearFatCluster(seed);

	/*清除目录表项*/
	c=0xe5;	//标记目录项曾被使用，现已删除

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
 * 参数：directory，类型：char *
 * 返回值：1，成功；-1，失败
 * 功能：删除当前目录下指定的目录
 */
int ud_dd(char *directory)
{
	Entry *pentry;
	int ret;
	unsigned char c;
	char ch;
	unsigned short seed,next;

	pentry=(Entry *)malloc(sizeof(Entry));
	ret=scanEntry(directory,pentry,1);			//扫描当前目录查找目录

	if(ret<0)
	{
		printf("no such directory\n");
		free(pentry);
		return -1;
	}

	/*清除fat表项*/
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

	/*清除目录表项*/
	c=0xe5;	//标记目录项曾被使用，现已删除

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

/*使用方法*/
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

	// 打开指定的设备文件以进行读写操作，
	//open 函数是一个系统调用，用于打开文件或者创建文件。DEVNAME 是文件名，O_RDWR 是打开文件的模式，表示以可读写的方式打开文件。
	if((fd=open(DEVNAME,O_RDWR))<0)
		perror("open failed");

	// 调用，读取第一个512字节（保存扇区存储的整体存储信息）
	scanBootSector();

	// 读取文件分配表（FAT）
	if(readFat()<0)
		exit(1);

	// 显示用法信息（打印）
	do_usage();

	// 主命令处理循环
	while(1)
	{
		memset(name,0,30);	// name 数组的前 30 个字节都设置为 0。（显示效果）
		printf(">");
		scanf("%s",input);

		// 检查用户输入的各种命令
		if(strcmp(input,"exit")==0)
			break;
		else if(strcmp(input,"\n")==0)
			continue;
		else if(strcmp(input,"ls")==0)
		{
			scanf("%s",name);
			// 判断用户选择查看的类型
			if(strcmp(name,"-a")==0||strcmp(name,"-all")==0) //查看所以的非根目录信息
				ud_ls(1);
			else
				ud_ls(0);
		}
		else if(strcmp(input,"dir")==0)	//查看当前目录信息
			ud_ls(0);
		else if(strcmp(input,"cd")==0)
		{
			scanf("%s",name);
			// 根据输入路径更改目录
			if(strncmp(name,"/media/disk/",12)==0)	// 处理以 "/media/disk/" 开头的绝对路径
			{
				if(name[strlen(name)-1] != '/')
					strcat(name, "/");
				curdir=NULL;
				dirno=0;
				for(i=12,num=12;i<30&&name[i]!='\0';i++)
				{
					if(name[i]=='/')
					{	// 提取子目录名称
						memset(subname,0,30);
						for(j=num,k=0;j<i;j++,k++)
							subname[k]=name[j];
						if(ud_cd(subname)<0)	//逐层进入目标目录
							break;
						num=i+1;
					}
				}
			}
			else if(strncmp(name,"./",2)==0)	// 处理以 "./" 开头的相对路径  处理儿子
			{
				if(name[strlen(name)-1] != '/')
					strcat(name, "/");
				ud_cd(".");
				for(i=2,num=2;i<30&&name[i]!='\0';i++)
				{
					if(name[i]=='/')
					{	// 提取子目录名称
						memset(subname,0,30);
						for(j=num,k=0;j<i;j++,k++)
							subname[k]=name[j];
						if(ud_cd(subname)<0)
							break;
						num=i+1;
					}
				}
			}
			// 处理以 "../" 开头的相对路径
			else if(strncmp(name,"../",3)==0)	//相对路径，父级目录
			{
				if(name[strlen(name)-1] != '/')
					strcat(name, "/");
				ud_cd("..");
				for(i=3,num=3;i<30&&name[i]!='\0';i++)
				{
					if(name[i]=='/')
					{	// 提取子目录名称
						memset(subname,0,30);
						for(j=num,k=0;j<i;j++,k++)
							subname[k]=name[j];
						if(ud_cd(subname)<0)
							break;
						num=i+1;
					}
				}
			}
			// 处理其他情况（当前目录的子目录）
			else
				ud_cd(name);
		}
		else if(strcmp(input,"cf")==0)
		{
			scanf("%s",name);
			scanf("%s",input);
			size=atoi(input);	//字符串转Int
			// 使用指定的名称和大小创建新文件
			ud_cf(name,size);
		}
		else if(strcmp(input,"md")==0)
		{
			scanf("%s",name);
			// 使用指定的名称创建新目录
			ud_md(name);
		}
		else if(strcmp(input,"df")==0)
		{
			scanf("%s",name);
			// 删除指定文件
			ud_df(name);
		}
		else if(strcmp(input,"dd")==0)
		{
			scanf("%s",name);
			// 删除指定的目录
			ud_dd(name);
		}
		else if(strcmp(input,"rf")==0)
		{
			scanf("%s",name);
			// 读取指定文件内容
			ud_rf(name);
		}
		else
			// 对于无法识别的命令，显示用法信息
			do_usage();
	}

	return 0;
}
