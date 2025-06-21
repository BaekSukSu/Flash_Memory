#include <stdio.h>
#include <string.h>
#include "hybridmapping.h"

extern FILE *flashmemoryfp;

int fdd_read(int ppn, char *pagebuf)
{
	int ret;

	fseek(flashmemoryfp, PAGE_SIZE*ppn, SEEK_SET);
	ret = fread((void *)pagebuf, PAGE_SIZE, 1, flashmemoryfp);
	if(ret == 1) {
		return 1;
	}
	else {
		return -1;
	}
}

int fdd_write(int ppn, char *pagebuf)
{
	int ret;

	fseek(flashmemoryfp, PAGE_SIZE*ppn, SEEK_SET);
	ret = fwrite((void *)pagebuf, PAGE_SIZE, 1, flashmemoryfp);
	if(ret == 1) {
		return 1;
	}
	else {
		return -1;
	}
}

int fdd_erase(int pbn)
{
	char blockbuf[BLOCK_SIZE];
	int ret;

	memset((void*)blockbuf, (char)0xFF, BLOCK_SIZE);
	
	fseek(flashmemoryfp, BLOCK_SIZE*pbn, SEEK_SET);
	
	ret = fwrite((void *)blockbuf, BLOCK_SIZE, 1, flashmemoryfp);
	
	if(ret == 1) { 
		return 1;
	}
	else {
		return -1;
	}
}
