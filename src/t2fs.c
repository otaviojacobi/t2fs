#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/apidisk.h"
#include "../include/bitmap2.h"
#include "../include/t2fs.h"

// Constantes retiradas da definição do trabalho
#define SECTOR_SIZE 256
#define TUPLES_PER_REGISTER 32
#define MAX_FILES_OPEN 20

// Outras constantes
#define TRUE 1
#define FALSE 0


// Loading disk.
void load_bootBlock(void);
struct t2fs_4tupla * load_register_at_sector(const int sector);
struct t2fs_4tupla **load_MFTArea(const int MFT_register_amt, const int MFT_start_sector);

struct t2fs_record* readRecordsAtSector(const int sector);
int isValidRecord(struct t2fs_record record);

// Utils
char* charArrayToString(const char* charArray, const int size);
int blockToFirstSector(const int block);
int sectorToBlock(const int sector);

// Debugging.
void print_bootBlock(void);
void print_4tupla(const struct t2fs_4tupla tuple);
void print_record(const struct t2fs_record file_entry);

// Globals
struct t2fs_bootBlock bootBlock;
struct t2fs_4tupla **MFT_registers;                                   // Acessar elemento -> MFT_registers[NR_REGISTRO][NR_TUPLA]
int MFT_register_amt;

int main() {

	load_bootBlock();
	//print_bootBlock();

	int MFT_start_sector = bootBlock.blockSize;
	int MFT_size = SECTOR_SIZE * bootBlock.blockSize * bootBlock.MFTBlocksSize; //Tam_setor * setores_por_bloco * nr_de_blocos = 2097152 BYTES
	int MFT_register_size = TUPLES_PER_REGISTER*sizeof(struct t2fs_4tupla);     // Ou seja, o MFT tem 2097152bytes, e sabemos que cada registro (32*tupla_size(16) = 512)
	MFT_register_amt = MFT_size/MFT_register_size;		 	    // Logo, 2097152/512 -> 4096(2^12) registros !! (8192 setores ou 2048 blocos)
                                // Acessar elemento -> MFT_registers[NR_REGISTRO][NR_TUPLA]
	MFT_registers = load_MFTArea(MFT_register_amt, MFT_start_sector); //                                      [   0-4095  ][  0-31  ] -> NR_REGISTER relativo ao MFT (+1 depois do bloco de boot)

	int homedir_start_sector = blockToFirstSector(MFT_registers[1][0].logicalBlockNumber); // This is no way completed. We need to extend.


	char file_buffer[SECTOR_SIZE];
	
	create2("FILEEz");

	struct t2fs_record* records = readRecordsAtSector(homedir_start_sector);
	
	int i;
	int current_file_sector;
	char* current_file_content;

	for ( i = 0; i < SECTOR_SIZE/sizeof(struct t2fs_record); i++ ) {
		if ( isValidRecord(records[i]) ) {

			current_file_sector = blockToFirstSector( MFT_registers[records[i].MFTNumber][0].logicalBlockNumber ); // This '0' needs to go out -> we must search all the tuple registers until
			read_sector(current_file_sector, file_buffer);							       // We find the opcode to stop.

			current_file_content = charArrayToString(file_buffer, records[i].bytesFileSize); //Most simple read() ever

			printf("\nThe content of %s is: \n", records[i].name);
			printf("%s", current_file_content);
		}
	}

	//print_record(home_dir_file);print_record(home_dir_file2);

	//printf("%d\n", searchBitmap2(0));

	
	return 0;
}

FILE2 create2 (char *filename) {
	// "/dir1/dir2/nome" -> hard mkdir (?)
        // "/nome" -> criar MFT, criar record
	struct t2fs_record new_record;

	int homedir_start_sector = blockToFirstSector(MFT_registers[1][0].logicalBlockNumber);

	struct t2fs_record* records = readRecordsAtSector(homedir_start_sector);

	char file_buffer[SECTOR_SIZE];

	int i;
	int current_file_sector;
	char* current_file_content;

	for ( i = 0; i < SECTOR_SIZE/sizeof(struct t2fs_record); i++ ) {
		if ( !(isValidRecord(records[i])) ) {
			break;
		}
	}
	
	create_new_record(&records[i], filename);

	unsigned char *buffer;
	
	buffer = (unsigned char *)malloc(sizeof(records));

	memcpy((void*) &(buffer), (void*) &records,  sizeof(records) );

	write_sector (homedir_start_sector, buffer);

	create_new_register(records[i].MFTNumber);

	free(buffer);
	buffer = (unsigned char *)malloc(sizeof(MFT_registers[records[i].MFTNumber])/2);

	memcpy((void*) &(buffer), (void*) &(MFT_registers[records[i].MFTNumber]), sizeof(MFT_registers[records[i].MFTNumber])/2);

	write_sector ( (4 + ((records[i].MFTNumber)*2)), buffer);
}

int create_new_register(int index) {
	MFT_registers[index][0].atributeType = 1;
	MFT_registers[index][0].virtualBlockNumber = 0;

	MFT_registers[index][0].logicalBlockNumber = searchBitmap2(0);
	setBitmap2 (MFT_registers[index][0].logicalBlockNumber, 1);

	MFT_registers[index][0].numberOfContiguosBlocks = 0;

	MFT_registers[index][1].atributeType = 1;
	MFT_registers[index][1].virtualBlockNumber = 0;
	MFT_registers[index][1].logicalBlockNumber = 0;
	MFT_registers[index][1].numberOfContiguosBlocks = 0;

}

int create_new_record(struct t2fs_record *new_record, char *filename) {
	new_record->TypeVal = 1; 
    strcpy(new_record->name, filename); 	/* Nome do arquivo. : string com caracteres ASCII (0x21 até 0x7A), case sensitive.             */
    new_record->blocksFileSize = 0; 		/* Tamanho do arquivo, expresso em número de blocos de dados */
    new_record->bytesFileSize = 0;  		/* Tamanho do arquivo. Expresso em número de bytes.          */
    new_record->MFTNumber = get_new_register();

	return 0;
}

int get_new_register(void) {

	int i;
	for (i=4; i < MFT_register_amt ; i++)
		if (MFT_registers[i][0].atributeType == -1) {
			return i;
		}
}

void load_bootBlock(void) {

	char buffer[SECTOR_SIZE];
	read_sector(0,buffer);

	//memcpy((void*) &(bootBlock.id),             (void*) &buffer[0],  4 );
	//memcpy((void*) &(bootBlock.version),        (void*) &buffer[4],  2 );
	//memcpy((void*) &(bootBlock.blockSize),      (void*) &buffer[6],  2 );
	//memcpy((void*) &(bootBlock.MFTBlocksSize),  (void*) &buffer[8],  2 );
	//memcpy((void*) &(bootBlock.diskSectorSize), (void*) &buffer[10], 4 ); //explicito eh melhor que implicito mas eficiencia
	memcpy((void*) &bootBlock, (void*) buffer, sizeof(struct t2fs_bootBlock));
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

struct t2fs_4tupla** load_MFTArea(const int MFT_register_amt, const int MFT_start_sector) {

	struct t2fs_4tupla **MFT_registers = (struct t2fs_4tupla**)malloc(MFT_register_amt * sizeof(struct t2fs_4tupla*));

	int MFT_current_sector = MFT_start_sector;
	int i;
	for(i=0;i<MFT_register_amt;i++) {

		MFT_registers[i] = load_register_at_sector(MFT_current_sector);
		MFT_current_sector += 2;
	}

	return MFT_registers;
}

// each record has 64bytes -> 4 records per sector: @return ALL entries (even the invalid ones)
struct t2fs_record* readRecordsAtSector(const int sector) {

	char sector_buffer[SECTOR_SIZE];
	read_sector(sector, sector_buffer);

	struct t2fs_record* records = (struct t2fs_record*)malloc(SECTOR_SIZE); // For otimization reasons -> SECTOR_SIZE(256)/record_size(64) * record_size(64) -> SECTOR_SIZE !!!
	int size_record = sizeof(struct t2fs_record);

	int i;
	for ( i = 0; i < SECTOR_SIZE/size_record; i++ ) { // In case of the SECTOR_SIZE changes or the t2fs_record changes, this won't break.

		//memcpy((void*) &(records[i].TypeVal),        (void*) &sector_buffer[ 0 + 64*i], 1  );
		//memcpy((void*) &(records[i].name), 	     (void*) &sector_buffer[ 1 + 64*i], 51 );
		//memcpy((void*) &(records[i].blocksFileSize), (void*) &sector_buffer[52 + 64*i], 4  );
		//memcpy((void*) &(records[i].bytesFileSize),  (void*) &sector_buffer[56 + 64*i], 4  );
		//memcpy((void*) &(records[i].MFTNumber),      (void*) &sector_buffer[60 + 64*i], 4  );
		memcpy((void*) &records[i], (void*) &sector_buffer[size_record*i], size_record);

	}

	return records;
}

int isValidRecord(struct t2fs_record record) {
	if ( record.TypeVal == 1 || record.TypeVal == 2 )
		return record.TypeVal;
	else
		return FALSE;
}

// UTILS
int blockToFirstSector(const int block) {
	return bootBlock.blockSize * block;
}


int sectorToBlock(const int sector) {
	return (int) sector/bootBlock.blockSize;
}

char* charArrayToString(const char* charArray, const int size) {

	char* string = (char*)malloc(sizeof(char)*size); //Explicit is better than implicit
	memcpy((void*) string, (void*) charArray, size);
	string[size] = '\0';
	return string;
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

void print_record(const struct t2fs_record file_entry) {

	printf("%d\n", file_entry.TypeVal);
	printf("%s\n", file_entry.name);
	printf("%d\n", file_entry.blocksFileSize);
	printf("%d\n", file_entry.bytesFileSize);
	printf("%d\n", file_entry.MFTNumber);
}
