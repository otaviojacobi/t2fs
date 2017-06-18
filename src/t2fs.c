#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/apidisk.h"
#include "../include/apidisk.h"
#include "../include/t2fs.h"

// Constantes retiradas da definição do trabalho
#define SECTOR_SIZE 256
#define TUPLES_PER_REGISTER 32

// Outras constantes
#define TRUE 1
#define FALSE 0


// Loading disk.
void load_bootBlock(void);
struct t2fs_4tupla * load_register_at_sector(const int sector);
struct t2fs_4tupla **load_MFTArea(const int MFT_register_amt, const int MFT_beginning_sector);

// Debugging.
void print_bootBlock(void);
void print_4tupla(const struct t2fs_4tupla tuple);

struct t2fs_bootBlock bootBlock;

int main() {

	load_bootBlock();	
	//print_bootBlock();

	int MFT_beginning_sector = bootBlock.blockSize;
	int MFT_size = SECTOR_SIZE * bootBlock.blockSize * bootBlock.MFTBlocksSize; //Tam_setor * setores_por_bloco * nr_de_blocos = 2097152			    
	int MFT_register_size = TUPLES_PER_REGISTER*sizeof(struct t2fs_4tupla);     // Ou seja, o MFT tem 2097152bytes, e sabemos que cada registro (32*tupla_size(16) = 512)
	int MFT_register_amt = MFT_size/MFT_register_size;		 	    // Logo, 2097152/512 -> 4096(2^12) registros !! (8192 setores ou 2048 blocos)
		
	struct t2fs_4tupla **MFT_registers;                                   // Acessar elemento -> MFT_registers[NR_REGISTRO][NR_TUPLA]
	MFT_registers = load_MFTArea(MFT_register_amt, MFT_beginning_sector); //                                  [   0-4095  ][  0-31  ] -> NR_REGISTER relativo ao MFT (+1 em relação ao geral)
					          				                      	
	print_4tupla(MFT_registers[0][0]);print_4tupla(MFT_registers[1][0]);

	int bitMap_starter_sector = bootBlock.blockSize * MFT_registers[0][0].logicalBlockNumber;

	//printf("%d\n", bitMap_starter_sector);

	char file_buffer[256];
	read_sector(8200, file_buffer);

	struct t2fs_record bitmap;

	memcpy((void*) &(bitmap.TypeVal), 	 (void*) &file_buffer[0],  1  );
	memcpy((void*) &(bitmap.name), 		 (void*) &file_buffer[1],  51 );
	memcpy((void*) &(bitmap.blocksFileSize), (void*) &file_buffer[52], 4  );
	memcpy((void*) &(bitmap.bytesFileSize),  (void*) &file_buffer[56], 4  );
	memcpy((void*) &(bitmap.MFTNumber), 	 (void*) &file_buffer[60], 4  );

	
	//printf("%d\n", searchBitmap2(0));

	printf("%d\n", bitmap.TypeVal);
	printf("%s\n", bitmap.name);
	printf("%d\n", bitmap.blocksFileSize);
	printf("%d\n", bitmap.bytesFileSize);
	printf("%d\n", bitmap.MFTNumber);
	

	return 0;
}


void load_bootBlock(void) {

	char buffer[SECTOR_SIZE];
	read_sector(0,buffer);

	memcpy((void*) &(bootBlock.id),             (void*) &buffer[0],  4 );
	memcpy((void*) &(bootBlock.version),        (void*) &buffer[4],  2 );
	memcpy((void*) &(bootBlock.blockSize),      (void*) &buffer[6],  2 );
	memcpy((void*) &(bootBlock.MFTBlocksSize),  (void*) &buffer[8],  2 );
	memcpy((void*) &(bootBlock.diskSectorSize), (void*) &buffer[10], 4 );
}

struct t2fs_4tupla* load_register_at_sector(const int sector) {
	
	char sector_buffer[SECTOR_SIZE];
	
	struct t2fs_4tupla *mft_register = (struct t2fs_4tupla*)malloc(TUPLES_PER_REGISTER * sizeof(struct t2fs_4tupla));

	read_sector(sector, sector_buffer);
	memcpy((void*) &mft_register[0], (void*) &sector_buffer[0],  SECTOR_SIZE);

	read_sector(sector+1, sector_buffer);				    	    //Cada registro ocupa 512 bytes, e cada setor tem 256bytes logo, sempre lemos 2 setores para termos um
	memcpy((void*) &mft_register[16], (void*) &sector_buffer[0],  SECTOR_SIZE); //registro

	return mft_register;
}

struct t2fs_4tupla** load_MFTArea(const int MFT_register_amt, const int MFT_beginning_sector) {

	struct t2fs_4tupla **MFT_registers = (struct t2fs_4tupla**)malloc(MFT_register_amt * sizeof(struct t2fs_4tupla*));
	
	int MFT_current_sector = MFT_beginning_sector;
	int i;
	for(i=0;i<MFT_register_amt;i++) {

		MFT_registers[i] = load_register_at_sector(MFT_current_sector);
		MFT_current_sector += 2;
	}

	return MFT_registers;
}


///From here only print/debugging functions.
void print_bootBlock(void) {

	printf("System: %c%c%c%c\n", bootBlock.id[0], bootBlock.id[1], bootBlock.id[2], bootBlock.id[3]); //'T' '2' 'F' 'S' -> array of chars, not a string.
	printf("Version: 0x%x\n", bootBlock.version);							  //0x07E11
	printf("Sectors per Block: 0x%x\n", bootBlock.blockSize);					  //0x0004
	printf("Blocks in MFT: 0x%x\n", bootBlock.MFTBlocksSize);					  //0x0800
	printf("Total sectors in disk: 0x%x\n", bootBlock.diskSectorSize);				  //0x8000
}

void print_4tupla(const struct t2fs_4tupla tuple) {
	printf("Tipo: %d\n", tuple.atributeType);
	printf("VBN: %d\n", tuple.virtualBlockNumber);
	printf("LBN: %d\n", tuple.logicalBlockNumber);
	printf("Contiguous Blocks: %d\n", tuple.numberOfContiguosBlocks);
}

void print_register(const struct t2fs_4tupla *mft_register) {

}
