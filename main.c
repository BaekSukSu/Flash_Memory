// debugging 용도

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "hybridmapping.h"

FILE *flashmemoryfp;

void ftl_open();
void ftl_write(int lsn, char *sectorbuf);
void ftl_read(int lsn, char *sectorbuf);
void ftl_print();


int main(int argc, char *argv[])
{

	char *blockbuf;
 	char sectorbuf[SECTOR_SIZE];
	int i;

	flashmemoryfp = fopen("flashmemory", "w+b");
	if(flashmemoryfp == NULL)
	{
		printf("file open error\n");
		exit(1);
	}


    // flash memory의 모든 바이트를 '0xff'로 초기화한다.
	blockbuf = (char *)malloc(BLOCK_SIZE);
	memset(blockbuf, 0xFF, BLOCK_SIZE);

	for(i = 0; i < BLOCKS_PER_DEVICE; i++)
	{
		fwrite(blockbuf, BLOCK_SIZE, 1, flashmemoryfp);
	}

	free(blockbuf);

	ftl_open();

	for(i=DATABLKS_PER_DEVICE*PAGES_PER_BLOCK-1; i>(DATABLKS_PER_DEVICE-1)*PAGES_PER_BLOCK-1; i--) {
		sprintf(sectorbuf, "data %d", i);
		ftl_write(i, sectorbuf);
	}

	printf("===================\n");
	ftl_write(3, "data 3"); // pbn 1번 할당 &  2-> 3 -> ..
	ftl_write(28, "1st update lsn 28"); // pbn 0번 반납, 2번 할당 / lsn 29 27 26 25 이동 0 -> 3 -> ...
	ftl_write(28, "2nd update lsn 28"); // pbn 2번 반납, 0번 할당 / lsn 29 27 26 25 이동 2 -> 3 -> ...
	ftl_write(1, "data 1"); // 1번 사용
	ftl_write(1, "1st update lsn 1"); // 1번 사용
	ftl_write(1, "2nd update lsn 1"); // 1번 사용
	ftl_write(1, "3rd update lsn 1"); // 1번 사용
	ftl_write(4, "data 1"); // pbn 이동(pbn 1번 반납, 2번 할당 / lsn3과 1 이동 -> offset 1) 1 -> 3 -> ...
	ftl_print();
	ftl_read(1, sectorbuf);
	printf("%s\n", sectorbuf);
	printf("===================\n");
	ftl_write(1, "4th update lsn 1"); // pbn 2번 그대로 offset 3
	ftl_write(1, "5th update lsn 1"); // pbn 2번 그대로 offset 4 (다 찼음)
	ftl_write(10, "data 10"); // pbn 1번 할당 & 3 -> 4 -> ...
	ftl_write(10, "1st update data 10"); // 그대로 1번 사용
	ftl_write(10, "2nd update 10"); // 그대로 1번 사용
	ftl_write(10, "3rd update 10"); // 그대로 1번 사용
	ftl_write(10, "4th update 10"); // 그대로 1번 사용
	ftl_write(10, "5th update 10"); // pbn 이동(pbn 1번 반납, 3번 할당 / offset -> 0) 1->4->5->6
	ftl_write(1, "6th update 1"); // pbn 이동(pbn 2번 반납, 1번 할당 / lsn 4, 3만 이동 offset -> 2) 2->4->5->6
	ftl_print();
	ftl_read(1, sectorbuf);
	printf("%s\n", sectorbuf);


	fclose(flashmemoryfp);

	return 0;
}