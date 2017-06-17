#include<stdio.h>
#include <stdlib.h>

#include "../include/apidisk.h"
#include "../include/apidisk.h"
#include "../include/t2fs.h"

struct t2fs_bootBlock bootBlock;

int main() {
	FILE* bootDesc;

	bootDesc = fopen("./t2fs_disk.dat", "r");

	fread(&bootBlock, sizeof(struct t2fs_bootBlock), 1, bootDesc);

	//printf("%c%c%c%c\n", bootBlock.id[0], bootBlock.id[1], bootBlock.id[2], bootBlock.id[3]);
	//printf("0x%x\n", bootBlock.version);
	//printf("0x%x\n", bootBlock.blockSize);
	//printf("0x%x\n", bootBlock.MFTBlocksSize);
	//printf("0x%x\n", bootBlock.diskSectorSize);

	return 0;
}

