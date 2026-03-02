// FILESUM.cpp : Defines the entry point for the console application.
#define _CRT_SECURE_NO_WARNINGS
// 20251107 initial release

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Format Error Code	*/
#define HEADER_NO_ERR           0x0000
#define HEADER_ERR_FILE         0x0001
#define HEADER_ERR_OPENFILE     0x0002
#define HEADER_ERR_READFILE     0x0003
#define HEADER_ERR_WRITEFILE    0x0004
#define HEADER_NOT_NF           0x0005


#define  ERROR_EXIST    10
#define  ERROR_ID       11
#define  ERROR_PASSWORD 12
#define  ERROR_FORMAT   13
#define  ALL_PASS 	14
#define  FINISHED	15
#define  FAILURE        16
#define  ERROR_CHECKSUM 17
#define  ERROR_BODY     18
#define  ERROR_IC        19
#define  ERROR_IC_LENGTH   20
#define  ERROR_PRECODE	21
#define  ERROR_WRITING	22



typedef  unsigned long	 DWORD;
typedef  unsigned int    WORD;
typedef  unsigned int    BOOL;
typedef  unsigned char   BYTE;

#define APROM_SIZE 256*1024

//#define APROM_SIZE_msi_vga 0xea00
unsigned char APROM[APROM_SIZE];
int main(int argc, char* argv[])
{


	FILE  *obj_fp, *sum_fp;
	//DWORD  SUM = 0;
	printf("Version: 11_07_00_00_0_01\n\r");	
	unsigned int file_size_temp, file_checksum, file_totallen;
	printf("\n");
	printf("***********************************************\n");
	printf("*           File CheckSum                     *\n");
	printf("***********************************************\n");

	if (argc != 3)
	{
		return 0;
	}
	if (!(obj_fp = fopen(argv[1], "rb")))
	{
		printf("%s FileOpen Errot!\n", argv[1]);
		return 0;
	}
	fseek(obj_fp, 0, SEEK_END);
	file_totallen = ftell(obj_fp);
	fseek(obj_fp, 0, SEEK_SET);
	printf("fize size:%d\n\r", file_totallen);


	for (int i = 0; i < APROM_SIZE; i++)
	{
		APROM[i] = 0xff;
	}

	file_size_temp = 0;
	while (!feof(obj_fp)) {
		fread(&APROM[file_size_temp], sizeof(char), 1, obj_fp);
		file_size_temp++;
	}
	fclose(obj_fp);
	file_checksum = 0;
	//for (int i = 0; i < file_totallen; i++)
	for (unsigned int i = 0; i < file_totallen; i++)
	{
		file_checksum += APROM[i];
	}
	file_checksum = file_checksum & 0xffff;
	printf("file_checksum=0x%x\n\r", file_checksum);
	

	APROM[(APROM_SIZE)-8] = (file_totallen >> 0) & 0xff;
	APROM[(APROM_SIZE)-7] = (file_totallen >> 8) & 0xff;
	APROM[(APROM_SIZE)-6] = (file_totallen >> 16) & 0xff;
	APROM[(APROM_SIZE)-5] = (file_totallen >> 24) & 0xff;
	APROM[(APROM_SIZE)-4] = (file_checksum >> 0) & 0xff;
	APROM[(APROM_SIZE)-3] = (file_checksum >> 8) & 0xff;
	APROM[(APROM_SIZE)-2] = (file_checksum >> 16) & 0xff;
	APROM[(APROM_SIZE)-1] = (file_checksum >> 24) & 0xff;

		if (!(sum_fp = fopen(argv[2], "wb")))
		{
			printf("%s FileOpen Errot!\n", argv[2]);
			fclose(obj_fp);
			return 0;
		}
		fwrite(&APROM, APROM_SIZE, 1, sum_fp);
		fclose(sum_fp);
#if 0
	i = 0; j = 0;
	file_size = 0;
	while (!feof(obj_fp))
	{
		byte1 = getc(obj_fp) & 0x0FF;
		if (!feof(obj_fp))
		{
			j++;
			file_size++;
			SUM += (byte1 & 0xff) << i;
			i += 8;
			if (i >= 32)
				i = 0;
			putc(byte1, sum_fp);
		}
	}
	printf("Copy ok!\n");
	printf("CheckSum = 0x%08X\n", SUM);
	printf("FILE size =0x%08X\n ", file_size);
	FileAddress = 1024 * 64 - 4;
	fseek(sum_fp, FileAddress, SEEK_SET);
	fwrite(&SUM, 4, 1, sum_fp);
	FileAddress = 1024 * 64 - 8;
	fseek(sum_fp, FileAddress, SEEK_SET);
	fwrite(&file_size, 4, 1, sum_fp);
#endif
	
	

	return 0;
}
