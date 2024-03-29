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

typedef struct {
	int i_register;
	int file_opened;
	int valido;
} oDir;

typedef struct {
	int i_register;
	int byte_opened;
	int valido;
	char name[MAX_FILE_NAME_SIZE];
} oFile;

// Loading disk.
void load_bootBlock(void);
struct t2fs_4tupla * load_register_at_sector(const int sector);
struct t2fs_4tupla **load_MFTArea(const int MFT_register_amt, const int MFT_start_sector);

struct t2fs_record* readRecordsAtSector(const int sector);
int isValidRecord(struct t2fs_record record);
int get_register_from_file (int index_register, char *filename, struct t2fs_record *final_record, int *found_sector, int *found_index);

// Utils
char* charArrayToString(const char* charArray, const int size);
int blockToFirstSector(const int block);
int sectorToBlock(const int sector);
char** split(char* string, int *size);
int find_register_from_path(char *filename);
struct t2fs_record* search_directory(int index_register, char *filename);



// Debugging.
void print_bootBlock(void);
void print_4tupla(const struct t2fs_4tupla tuple);
void print_record(const struct t2fs_record file_entry);
void spacer(int nivel);
void print_tree (int nivel, char *dir);
void print_record(const struct t2fs_record file_entry);
void print_all();

void debug();

// Globals
struct t2fs_bootBlock bootBlock;
struct t2fs_4tupla **MFT_registers;                                   // Acessar elemento -> MFT_registers[NR_REGISTRO][NR_TUPLA]
int MFT_register_amt;
int open_dir_number = 0;
oDir *open_dirs;
oFile open_files[MAX_FILES_OPEN];

int inited = FALSE;


void init() {

	load_bootBlock();
	//print_bootBlock();

	int MFT_start_sector = bootBlock.blockSize;
	int MFT_size = SECTOR_SIZE * bootBlock.blockSize * bootBlock.MFTBlocksSize; //Tam_setor * setores_por_bloco * nr_de_blocos = 2097152 BYTES
	int MFT_register_size = TUPLES_PER_REGISTER*sizeof(struct t2fs_4tupla);     // Ou seja, o MFT tem 2097152bytes, e sabemos que cada registro (32*tupla_size(16) = 512)
	MFT_register_amt = MFT_size/MFT_register_size;		 	    // Logo, 2097152/512 -> 4096(2^12) registros !! (8192 setores ou 2048 blocos)
                                // Acessar elemento -> MFT_registers[NR_REGISTRO][NR_TUPLA]
	MFT_registers = load_MFTArea(MFT_register_amt, MFT_start_sector); //                                      [   0-4095  ][  0-31  ] -> NR_REGISTER relativo ao MFT (+1 depois do bloco de boot)
	int y;
	for (y = 0; y<MAX_FILES_OPEN; y++) 
		open_files[y].valido = FALSE;

	inited = TRUE;

}
void debug() {
	printf("que\n");
	int j;
	for (j=0; j < 10; j++)
		print_4tupla(MFT_registers[j][0]);
}

int truncate2 (FILE2 handle) {

	if (!inited) {
		init();
	}

	struct t2fs_record final_record;
	int record_sector;
	int record_index;

	int index_register = get_register_from_file(open_files[handle].i_register, open_files[handle].name, &final_record, &record_sector, &record_index);

	int z,k,j;
	int start_sector, start_block, num_block_tupla;
	int startsize = open_files[handle].byte_opened;

	final_record.bytesFileSize = startsize;
	final_record.blocksFileSize = final_record.bytesFileSize / (bootBlock.blockSize * SECTOR_SIZE);

	int auxblockstart = final_record.blocksFileSize;

	struct t2fs_record* records;

	records = readRecordsAtSector(record_sector);

	memcpy(&records[record_index], &final_record, sizeof(struct t2fs_record));

	char buffer[SECTOR_SIZE];
	char outro_buffer[SECTOR_SIZE];

	memcpy((void*) outro_buffer, (void*) records,  SECTOR_SIZE);
 
	write_sector (record_sector, outro_buffer);

	while(MFT_registers[index_register][0].atributeType!=0) {
 
	    for ( z = 0; z < TUPLES_PER_REGISTER && MFT_registers[index_register][z].atributeType > 0; z++) {

		if (MFT_registers[index_register][z].numberOfContiguosBlocks <= auxblockstart){
			auxblockstart -= MFT_registers[index_register][z].numberOfContiguosBlocks;
		}
		else {
			for (j = auxblockstart ; j < MFT_registers[index_register][z].numberOfContiguosBlocks; j++)
				setBitmap2 (MFT_registers[index_register][z].logicalBlockNumber + j, 0);
			MFT_registers[index_register][z].atributeType = -1;
			if (auxblockstart != 0) {
				auxblockstart = 0;
				MFT_registers[index_register][z].atributeType = 0;
			}
			memcpy((void*) buffer, (void*) &MFT_registers[index_register][0], SECTOR_SIZE);	
			write_sector (register_to_sector(index_register), buffer);
	
			memcpy((void*) buffer, (void*) &MFT_registers[index_register][16], SECTOR_SIZE);
			write_sector (register_to_sector(index_register) + 1, buffer);
		}
	    }
	    index_register = MFT_registers[index_register][TUPLES_PER_REGISTER-1].virtualBlockNumber;
            if (index_register < 0)
		    break;	       
	}

	return 0;
}

int identify2 (char *name, int size) {
	if (!inited) {
		init();
	}

	memcpy(name, "Arthur Medeiros 261587 - Otavio Jacobi - 261569", size);
	
	return 0;

}

FILE2 create2 (char *filename) {
	
	if (!inited) {
		init();
	}
	//debug();

	if (filename[0] != '/'){
		printf("create2 so funciona com path absoluto\n");
		return -1;
	}
		
	return create_file(filename, 1);

}

FILE2 mkdir2 (char *filename) {

	if (!inited) {
		init();
	}
	//debug();

	return create_file(filename, 2);

}

FILE2 open2 (char *filename) {
	if (!inited) {
		init();
	}
	//debug();	

	int size;
	int z,k,j,i,y;
   	int start_sector, start_block, num_block_tupla;
	char **names = split(filename, &size);
	int index_register = 1;
	struct t2fs_record *record;
	struct t2fs_record* records;

	for (i = 0; i<size-1; i++) {
		record = search_directory(index_register, names[i]);
		if (record == NULL) {
			printf("DEU RUIM2\n");
			return -1;
		}
		index_register = record->MFTNumber;
	}
	while(MFT_registers[index_register][0].atributeType!=0) {
 
	    for ( z = 0; (z < TUPLES_PER_REGISTER && MFT_registers[index_register][z].atributeType > 0); z++) {
	 
	        start_block = MFT_registers[index_register][z].logicalBlockNumber;
	        start_sector = blockToFirstSector(start_block);
	        num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;
	 
	        for ( k = 0; (k < num_block_tupla); k++) {
	 
	            for ( j = 0 ; (j < bootBlock.blockSize) ; j++ ) {
	   
	                records = readRecordsAtSector(start_sector + j + k * bootBlock.blockSize);
	 
	                for ( i = 0; i < SECTOR_SIZE/sizeof(struct t2fs_record); i++ ) {
	                    if ( isValidRecord(records[i]) && !strcmp(records[i].name, names[size-1])) {
	                        for (y = 0; y<MAX_FILES_OPEN; y++) {
					if (open_files[y].valido == FALSE) {
						open_files[y].byte_opened = 0;
						open_files[y].valido = TRUE;
						open_files[y].i_register = index_register;
						strcpy(open_files[y].name, records[i].name);
						return y;
					}
				}
				return -2;
	                    }
	                }
	 
	                free(records);
	            }
	
	        }
	    }
	    index_register = MFT_registers[index_register][TUPLES_PER_REGISTER-1].virtualBlockNumber;
	    if (index_register < 0)
	        break;	       
	}

	return -1;

}

int close2 (FILE2 handle) {

	if (!inited) {
		init();
	}
	//debug();

	open_files[handle].valido = FALSE;
	return 0;
}

int seek2 (FILE2 handle, DWORD offset) {

	if (!inited) {
		init();
	}
	//debug();
	
	struct t2fs_record final_record;
	int record_sector;
	int record_index;
	int end_register;


	int index_register = get_register_from_file(open_files[handle].i_register, open_files[handle].name, &final_record, &record_sector, &record_index);
	
	if (offset > final_record.bytesFileSize)
		offset = final_record.bytesFileSize;

	if (open_files[handle].valido) {
		
		open_files[handle].byte_opened = offset;
	}
	else
		return -1;

	return 0;
}

int read2 (FILE2 handle, char *buffer, int size) {

	if (!inited) {
		init();
	}
	//debug();

// TODO: até acabar o byte size;

	struct t2fs_record final_record;
	int record_sector;
	int record_index;

	int index_register = get_register_from_file(open_files[handle].i_register, open_files[handle].name, &final_record, &record_sector, &record_index);

	int z,k,j;
	int start_sector, start_block, num_block_tupla;
	int auxsize = size;
	int startsize = open_files[handle].byte_opened;
	char auxbuffer[SECTOR_SIZE];

	struct t2fs_record* records;

	if (startsize + auxsize > final_record.bytesFileSize)
		return -1;

	if (size == 0)
		return 0;
	else {
		//printf("Oie %d - %s - %d\n", handle, buffer, size);

		//printf("OIe2 %d %d %d %s\n", open_files[handle].i_register, open_files[handle].byte_opened, open_files[handle].valido, open_files[handle].name);	
		
		
		while(MFT_registers[index_register][0].atributeType!=0) {
	 
		    for ( z = 0; (z < TUPLES_PER_REGISTER && MFT_registers[index_register][z].atributeType > 0); z++) {
		 
		        start_block = MFT_registers[index_register][z].logicalBlockNumber;
		        start_sector = blockToFirstSector(start_block);
		        num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;
		 
		        for ( k = 0; (k < num_block_tupla); k++) {
		            for ( j = 0 ; (j < bootBlock.blockSize) ; j++ ) {
		                read_sector (start_sector + j + k * bootBlock.blockSize , auxbuffer);
				if (startsize > 0) {
					if (startsize >= SECTOR_SIZE) {
						startsize -= SECTOR_SIZE;
					}
					else {
						if (startsize + auxsize <= SECTOR_SIZE) {
							memcpy(buffer + (size-auxsize), auxbuffer+startsize, auxsize);
							open_files[handle].byte_opened += size;	
							
							return size; // should return the number of bytes readed
						}
						else {
							startsize = 0;
							memcpy(buffer + (size-auxsize), auxbuffer+startsize, SECTOR_SIZE-startsize);
							auxsize -= SECTOR_SIZE-startsize;
						}
					}
				}
				else {
					if (auxsize <= SECTOR_SIZE) {			
						memcpy(buffer + (size-auxsize), auxbuffer, auxsize);
						open_files[handle].byte_opened += size;

						return size;  //should return the number of bytes readed
					}
					else {
						memcpy(buffer + (size-auxsize), auxbuffer, SECTOR_SIZE);
						auxsize -= SECTOR_SIZE;
					}
				}
		            }
		
		        }
		    }
		    index_register = MFT_registers[index_register][TUPLES_PER_REGISTER-1].virtualBlockNumber;
		    if (index_register < 0)
		        break;	       
		}
	}
	return -1;
}

int write2 (FILE2 handle, char *buffer, int size) {

	if (!inited) {
		init();
	}
	//debug();

	// TODO: COMPARE POINTER WITH END

	struct t2fs_record final_record;
	int record_sector;
	int record_index;
	int end_register;


	int index_register = get_register_from_file(open_files[handle].i_register, open_files[handle].name, &final_record, &record_sector, &record_index);
	int z,k,j;
	int start_sector, start_block, num_block_tupla;
	int auxsize = size;
	int startsize = open_files[handle].byte_opened;
	char auxbuffer[SECTOR_SIZE];
	int new_block_index;

	struct t2fs_record* records;

	//printf("\n%d\n", (size- open_files[handle].byte_opened));

	if (final_record.bytesFileSize < startsize + size) {
		// @TODO MEMCPY
		final_record.bytesFileSize = startsize + size;
		final_record.blocksFileSize = final_record.bytesFileSize / (bootBlock.blockSize * SECTOR_SIZE);
	}

	records = readRecordsAtSector(record_sector);
	memcpy(&records[record_index], &final_record, sizeof(struct t2fs_record));

	char outro_buffer[SECTOR_SIZE];

	memcpy((void*) outro_buffer, (void*) records,  SECTOR_SIZE);
 	
	//printf("Escrevendo no setor: %d + %d- RECORDS \n", start_sector, found_index_sector);
	write_sector (record_sector, outro_buffer);
	

	if (size == 0)
		return 0;
	else {
		
		while(MFT_registers[index_register][0].atributeType!=0) {

		    for ( z = 0; (z < TUPLES_PER_REGISTER && MFT_registers[index_register][z].atributeType > 0); z++) {
		 
		        start_block = MFT_registers[index_register][z].logicalBlockNumber;
		        start_sector = blockToFirstSector(start_block);
		        num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;
		 
		        for ( k = 0; (k < num_block_tupla); k++) { 
		            for ( j = 0 ; (j < bootBlock.blockSize) ; j++ ) {
		                read_sector (start_sector + j + k * bootBlock.blockSize , auxbuffer);
				if (startsize > 0) {
					if (startsize >= SECTOR_SIZE) {
						startsize -= SECTOR_SIZE;
					}
					else {
						if (startsize + auxsize <= SECTOR_SIZE) {
							memcpy(auxbuffer+startsize, buffer + (size-auxsize), auxsize);
							open_files[handle].byte_opened += size;
							write_sector(start_sector + j + k * bootBlock.blockSize , auxbuffer);
							return size;
						}
						else {
							startsize = 0;
							memcpy(auxbuffer+startsize, buffer + (size-auxsize), SECTOR_SIZE-startsize);
							write_sector(start_sector + j + k * bootBlock.blockSize , auxbuffer);	
							auxsize -= SECTOR_SIZE-startsize;
						}
					}
				}
				else {
					if (auxsize <= SECTOR_SIZE) {		
						memcpy(auxbuffer, buffer + (size-auxsize), auxsize);
						open_files[handle].byte_opened += size;
						write_sector(start_sector + j + k * bootBlock.blockSize , auxbuffer);
						return size;
					}
					else {
						memcpy(auxbuffer, buffer + (size-auxsize), SECTOR_SIZE);
						auxsize -= SECTOR_SIZE;
						write_sector(start_sector + j + k * bootBlock.blockSize , auxbuffer);
					}
				}
		            }
		
		        }
		    }
		    if (z < TUPLES_PER_REGISTER) {
			while (auxsize > 0) {

				new_block_index = searchBitmap2(0);
				setBitmap2 (new_block_index, 1);

				if (new_block_index != MFT_registers[index_register][z-1].logicalBlockNumber + MFT_registers[index_register][z-1].numberOfContiguosBlocks) {
					if (z == TUPLES_PER_REGISTER) {
						MFT_registers[index_register][z].atributeType = 2;
				
						end_register = get_new_register();
						create_new_register(end_register);

						MFT_registers[index_register][z].virtualBlockNumber = end_register;

						memcpy((void*) buffer, (void*) &MFT_registers[index_register][0], SECTOR_SIZE);	
						write_sector (register_to_sector(index_register), buffer);

						memcpy((void*) buffer, (void*) &MFT_registers[index_register][16], SECTOR_SIZE);
						write_sector (register_to_sector(index_register) + 1, buffer);	
				
						index_register = end_register;
						z = 0;
					}
					MFT_registers[index_register][z+1].atributeType = 0;
					MFT_registers[index_register][z+1].virtualBlockNumber = 0;
					MFT_registers[index_register][z+1].logicalBlockNumber = 0;
					MFT_registers[index_register][z+1].numberOfContiguosBlocks = 0;

					MFT_registers[index_register][z].atributeType = 1;
					MFT_registers[index_register][z].virtualBlockNumber = MFT_registers[index_register][z-1].virtualBlockNumber + MFT_registers[index_register][z-1].numberOfContiguosBlocks;
					MFT_registers[index_register][z].logicalBlockNumber = new_block_index;
					MFT_registers[index_register][z].numberOfContiguosBlocks = 0;
			
					z += 1;
				}

				MFT_registers[index_register][z-1].numberOfContiguosBlocks++;
				start_block = MFT_registers[index_register][z-1].logicalBlockNumber + MFT_registers[index_register][z-1].numberOfContiguosBlocks - 1;
			       	start_sector = blockToFirstSector(start_block);
				        
				memcpy((void*) buffer, (void*) &MFT_registers[index_register][0], SECTOR_SIZE);	
				write_sector (register_to_sector(index_register), buffer);
	
				memcpy((void*) buffer, (void*) &MFT_registers[index_register][16], SECTOR_SIZE);
				write_sector (register_to_sector(index_register) + 1, buffer);	
	
				for (j = 0; j < bootBlock.blockSize; j++) {
					read_sector (start_sector + j, auxbuffer);
					if (auxsize <= SECTOR_SIZE) {		
						memcpy(auxbuffer, buffer + (size-auxsize), auxsize);
						open_files[handle].byte_opened += size;
						write_sector(start_sector + j, auxbuffer);
						return size;
					}
					else {
						memcpy(auxbuffer, buffer + (size-auxsize), SECTOR_SIZE);
						auxsize -= SECTOR_SIZE;
						write_sector(start_sector + j , auxbuffer);
					}
				}
			}
		    }
		    else {
			    index_register = MFT_registers[index_register][TUPLES_PER_REGISTER-1].virtualBlockNumber;
			    if (index_register < 0)
			        break;	       
		   }
		}
	}
	return -1;
}

int get_register_from_file (int index_register, char *filename, struct t2fs_record *final_record, int *found_sector, int *found_index) {
	int z,k,j,i;
   	int start_sector, start_block, num_block_tupla;
	struct t2fs_record* records;

	while(MFT_registers[index_register][0].atributeType!=0) {
 
	    for ( z = 0; (z < TUPLES_PER_REGISTER && MFT_registers[index_register][z].atributeType > 0); z++) {
	 
	        start_block = MFT_registers[index_register][z].logicalBlockNumber;
	        start_sector = blockToFirstSector(start_block);
	        num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;
	 
	        for ( k = 0; (k < num_block_tupla); k++) {
	 
	            for ( j = 0 ; (j < bootBlock.blockSize) ; j++ ) {
	   
	                records = readRecordsAtSector(start_sector + j + k * bootBlock.blockSize);
	 
	                for ( i = 0; i < SECTOR_SIZE/sizeof(struct t2fs_record); i++ ) {
	                    if ( isValidRecord(records[i]) && !strcmp(records[i].name, filename)) {
	
				memcpy( final_record, &records[i], sizeof(struct t2fs_record));
				*found_sector = start_sector + j + k * bootBlock.blockSize;   // POCOTÓ POCOTÓ POCOTÓ
				*found_index = i;
	                        return records[i].MFTNumber;
	                    }
	                }
	 
	                free(records);
	            }
	
	        }
	    }
	    index_register = MFT_registers[index_register][TUPLES_PER_REGISTER-1].virtualBlockNumber;
	    if (index_register < 0)
	        break;	       
	}
}


DIR2 opendir2 (char *pathname) {

	if (!inited) {
		init();
	}
	//debug();
//TODO: TRATAR LIXO
	int size, i;
	char **names = split(pathname, &size);
	int index_register = 1;
	struct t2fs_record *record;

	for (i = 0; i<size; i++) {
		record = search_directory(index_register, names[i]);
		if (record == NULL || record->TypeVal != 2) {
			printf("DEU RUIM3\n");
			return -1;
		}
		index_register = record->MFTNumber;
	}

	for (i = 0; i<open_dir_number; i++) {
		if (open_dirs[i].valido == FALSE) {
			open_dirs[i].file_opened = 0;
			open_dirs[i].valido = TRUE;
			open_dirs[i].i_register = index_register;
			return i;
		}
	}

	open_dir_number += 1;
	open_dirs = (oDir*)realloc(open_dirs, sizeof(oDir) * open_dir_number);
	open_dirs[open_dir_number-1].file_opened = 0;
	open_dirs[open_dir_number-1].valido = TRUE;
	
	open_dirs[open_dir_number-1].i_register = index_register;

	return open_dir_number-1;

}


int readdir2 (DIR2 handle, DIRENT2 *dentry) {

	if (!inited) {
		init();
	}
	//debug();	

	int z,k,j,i;
	int start_sector, start_block, num_block_tupla;
	int count = open_dirs[handle].file_opened;
	int current_file_sector;
	char* current_file_content;
	int index_register = open_dirs[handle].i_register;
	struct t2fs_record* records;

	while(MFT_registers[index_register][0].atributeType!= 0) {

		for ( z = 0; (z < TUPLES_PER_REGISTER && MFT_registers[index_register][z].atributeType > 0); z++) {

			start_block = MFT_registers[index_register][z].logicalBlockNumber;
			start_sector = blockToFirstSector(start_block);
			num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;

			for ( k = 0; (k < num_block_tupla); k++) {

				for ( j = 0 ; (j < bootBlock.blockSize) ; j++ ) {
	
					records = readRecordsAtSector(start_sector + j + k * bootBlock.blockSize);

					for ( i = 0; i < SECTOR_SIZE/sizeof(struct t2fs_record); i++ ) {
						if ( isValidRecord(records[i])) {
							if (count != 0)
								count--;
							else {
								strcpy(dentry->name, records[i].name);
								dentry->fileType = records[i].TypeVal;                   /* Tipo do arquivo: regular (0x01) ou diretório (0x02) */
								dentry->fileSize = records[i].bytesFileSize;
								open_dirs[handle].file_opened++;
								return 0;
							}
						}
					}

					free(records);
				}

			}
		}
			index_register = MFT_registers[index_register][TUPLES_PER_REGISTER-1].virtualBlockNumber;
			if (index_register < 0)
				return -1;
		
	}
	return -1;
}

int closedir2 (DIR2 handle)	{

	if (!inited) {
		init();
	}
	//debug();

	open_dirs[handle].valido = FALSE;

	return 0;

}

int delete2(char* filename) {

    if (!inited) {
	init();
    }
	//debug();
 
    int index_register = get_delete_register(filename, 1);
 
    int z,k,j,i;
    int start_sector, start_block, num_block_tupla;
 
    if(index_register == -1) {
        printf("Nao pode ser deletado: path invalido\n");
        return -1;
    }
    else if (index_register == -2) {
        printf("Nao pode ser deletado: nao delete diretorios com essa funcao\n");
        return -1;
    }
 
 
    while(MFT_registers[index_register][0].atributeType!=0) {
 
        for ( z = 0; (z < TUPLES_PER_REGISTER && MFT_registers[index_register][z].atributeType > 0); z++) {
 
            start_block = MFT_registers[index_register][z].logicalBlockNumber;
            start_sector = blockToFirstSector(start_block);
            num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;
 
            MFT_registers[index_register][z].atributeType = -1;
 
            for ( k = 0; (k < num_block_tupla); k++) {
                setBitmap2(start_block + k, 0);
            }
        }
        index_register = MFT_registers[index_register][TUPLES_PER_REGISTER-1].virtualBlockNumber;
        if (index_register < 0)
            break;
       
    }
    printf("Apagou o arquivo\n");
    return 0;
}
 
int rmdir2 (char *pathname) {

	if (!inited) {
		init();
	}
	//debug();
 
    int index_register = get_delete_register(pathname, 2);
   
 
 
    int z,k,j,i;
    int start_sector, start_block, num_block_tupla;
 
    if(index_register == -1) {
        printf("Nao pode ser deletado: path invalido\n");
        return -1;
    }
    else if (index_register == -2) {
        printf("Nao pode ser deletado: nao delete arquivos com essa funcao\n");
        return -1;
    }
    else if (index_register == -3) {
        printf("Nao pode apagar dir que nao ta vazio!\n");
        return -1;
    }
 
    while(MFT_registers[index_register][0].atributeType!=0) {
 
        for ( z = 0; (z < TUPLES_PER_REGISTER && MFT_registers[index_register][z].atributeType > 0); z++) {
 
            start_block = MFT_registers[index_register][z].logicalBlockNumber;
            start_sector = blockToFirstSector(start_block);
            num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;
 
            MFT_registers[index_register][z].atributeType = -1;
 
            for ( k = 0; (k < num_block_tupla); k++) {
                setBitmap2(start_block + k, 0);
            }
        }
        index_register = MFT_registers[index_register][TUPLES_PER_REGISTER-1].virtualBlockNumber;
        if (index_register < 0)
            break;
       
    }

    printf("Apagou o dir\n");
    return 0;
   
}
 
int isDirEmpty(int index_register) {
   
    int z,k,j,i;
    int start_sector, start_block, num_block_tupla;
 
    struct t2fs_record* records;
 
    char buffer[SECTOR_SIZE];
 
    while(MFT_registers[index_register][0].atributeType!=0) {
 
        for ( z = 0; (z < TUPLES_PER_REGISTER && MFT_registers[index_register][z].atributeType > 0); z++) {
 
            start_block = MFT_registers[index_register][z].logicalBlockNumber;
            start_sector = blockToFirstSector(start_block);
            num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;
 
            for ( k = 0; (k < num_block_tupla); k++) {
 
                for ( j = 0 ; (j < bootBlock.blockSize) ; j++ ) {
   
                    records = readRecordsAtSector(start_sector + j + k * bootBlock.blockSize);
 
                    for ( i = 0; i < SECTOR_SIZE/sizeof(struct t2fs_record); i++ ) {
                        if ( isValidRecord(records[i])) {
                            return FALSE;
                        }
                    }
 
                    free(records);
                }
 
            }
        }
        index_register = MFT_registers[index_register][TUPLES_PER_REGISTER-1].virtualBlockNumber;
        if (index_register < 0)
            break;
       
    }
 
    return TRUE;
 
}
 
int get_delete_register (char *filename, int type) {
 
    int z,k,j,i;
    int start_sector, start_block, num_block_tupla;
 
    int size;
    char **names = split(filename, &size);
    int index_register = 1;
   
    struct t2fs_record* records;
    struct t2fs_record* record;
 
    char buffer[SECTOR_SIZE];
 
    if (filename[0] != '/') {
        printf("Passe o Path absoluto !!\n");
        return -1;
    }
       
 
    for (i = 0; i<size-1; i++) {
        record = search_directory(index_register, names[i]);
        if (record == NULL) {
            printf("DEU RUIM 4\n");
            return -1;
        }
        index_register = record->MFTNumber;
    }
 
    //printf("MFT_REGISTER %d\n",index_register);
 
    while(MFT_registers[index_register][0].atributeType!=0) {
 
        for ( z = 0; (z < TUPLES_PER_REGISTER && MFT_registers[index_register][z].atributeType > 0); z++) {
 
            start_block = MFT_registers[index_register][z].logicalBlockNumber;
            start_sector = blockToFirstSector(start_block);
            num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;
 
            for ( k = 0; (k < num_block_tupla); k++) {
 
                for ( j = 0 ; (j < bootBlock.blockSize) ; j++ ) {
   
                    records = readRecordsAtSector(start_sector + j + k * bootBlock.blockSize);
 
                    for ( i = 0; i < SECTOR_SIZE/sizeof(struct t2fs_record); i++ ) {
                        if ( isValidRecord(records[i]) && !strcmp(records[i].name, names[size-1])  ) {
                            if (records[i].TypeVal == type && records[i].TypeVal == 1) {
                                records[i].TypeVal = 0;
                           
                                memcpy((void*) buffer, (void*) records,  SECTOR_SIZE);
                                write_sector (start_sector + j + k * bootBlock.blockSize, buffer);
                           
                                return records[i].MFTNumber;
                            }
                            else if (records[i].TypeVal == type && records[i].TypeVal == 2) {
                                if ( !isDirEmpty(records[i].MFTNumber)) {
                                    return -3;
                                }
                                records[i].TypeVal = 0;
                           
                                memcpy((void*) buffer, (void*) records,  SECTOR_SIZE);
                                write_sector (start_sector + j + k * bootBlock.blockSize, buffer);
                           
                                return records[i].MFTNumber;
 
                            }
                            else {
                                return -2;
                            }
                        }
                    }
 
                    free(records);
                }
 
            }
        }
        index_register = MFT_registers[index_register][TUPLES_PER_REGISTER-1].virtualBlockNumber;
        if (index_register < 0)
            break;
       
    }
 
    return -1;
}

FILE2 create_file(char *filename, int type) {

	struct t2fs_record new_record;


	struct t2fs_record* records; 

	char file_buffer[SECTOR_SIZE];

	int i,j,k,z;
	int found = FALSE;
	int end_found = FALSE;
	int end_register = -1;
	int found_index_record = -1;
	int found_index_sector = -1;
	int current_file_sector;
	char* current_file_content;


	int start_block;
	int start_sector;
	int num_block_tupla;

	char buffer[SECTOR_SIZE];

	int index_register = find_register_from_path(filename);
	int size;
	char **names = split(filename, &size);

	if (index_register == -1) {
		printf("Erro. Tem certeza que digitou o path correto ?\n");
		return -1;
	}

	if (search_directory(index_register, names[size-1]) != NULL) {
		printf("Ja existe arquivo com mesmo nome %s.\n", filename);
		return -1;
	}

	while (!found && !end_found) {
		for ( z = 0; (z < TUPLES_PER_REGISTER-1 && !found && MFT_registers[index_register][z].atributeType > 0); z++) {
			
			//printf("INDEX REGISTER: %d\n", index_register);

			start_block = MFT_registers[index_register][z].logicalBlockNumber;
			start_sector = blockToFirstSector(start_block);
			num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;
	
			for ( k = 0; (k < num_block_tupla && !found); k++) {
	
				for ( j = 0 ; (j < bootBlock.blockSize && !found) ; j++ ) {
			
					//printf("start sector: %d - j: %d - k : %d\n", start_sector, j, k);
			
					records = readRecordsAtSector(start_sector + j + k * bootBlock.blockSize);
		
					for ( i = 0; (i < SECTOR_SIZE/sizeof(struct t2fs_record) && !found); i++ ) {
						if ( !(isValidRecord(records[i])) ) {
							found = TRUE;

							found_index_record = i;
							//printf("found_index_record: %d\n", found_index_record);
							found_index_sector = j + k*bootBlock.blockSize;
							//printf("found_index_sector: %d\n", found_index_sector);
						}
					}
					if (!found)
						free(records);
				}
			}
		}
	
		if (z == TUPLES_PER_REGISTER -1 && !found) {

			//printf("FODEU\n");
		
			if (MFT_registers[index_register][z].atributeType == 2) {
				index_register = MFT_registers[index_register][z].virtualBlockNumber;
			}
			else if (MFT_registers[index_register][z].atributeType == 0) {

				MFT_registers[index_register][z].atributeType = 2;
				
				end_register = get_new_register();
				create_new_register(end_register);

				MFT_registers[index_register][z].virtualBlockNumber = end_register;
							// ULTRA SUPER CAVALICE-EX //-2 ARCADE EDITION: 2017/1
				MFT_registers[end_register][0].atributeType = 0;

				memcpy((void*) buffer, (void*) &MFT_registers[index_register][0], SECTOR_SIZE);	
				write_sector (register_to_sector(index_register), buffer);

				memcpy((void*) buffer, (void*) &MFT_registers[index_register][16], SECTOR_SIZE);
				write_sector (register_to_sector(index_register) + 1, buffer);	
				
				index_register = end_register;
			}
		}
		else if (MFT_registers[index_register][z].atributeType == 0) {
			end_found = TRUE;
		}
	}

	int new_block_index;
	
	if (!found) {

		if(z < TUPLES_PER_REGISTER) {


			new_block_index = searchBitmap2(0);
			setBitmap2 (new_block_index, 1);

			MFT_registers[index_register][z].atributeType = 1;
			MFT_registers[index_register][z].virtualBlockNumber = MFT_registers[index_register][z-1].virtualBlockNumber + MFT_registers[index_register][z-1].numberOfContiguosBlocks;
			MFT_registers[index_register][z].logicalBlockNumber = new_block_index;
			MFT_registers[index_register][z].numberOfContiguosBlocks = 1;
		
			MFT_registers[index_register][z+1].atributeType = 0;
			MFT_registers[index_register][z+1].virtualBlockNumber = 0;
			MFT_registers[index_register][z+1].logicalBlockNumber = 0;
			MFT_registers[index_register][z+1].numberOfContiguosBlocks = 0;

			//printf("OI ???\n");

			memcpy((void*) buffer, (void*) &MFT_registers[index_register][0], SECTOR_SIZE);	
			write_sector (register_to_sector(index_register), buffer);

			memcpy((void*) buffer, (void*) &MFT_registers[index_register][16], SECTOR_SIZE);
			write_sector (register_to_sector(index_register) + 1, buffer);	

			records = readRecordsAtSector(new_block_index);
			start_block = new_block_index;
			start_sector = blockToFirstSector(start_block);
			num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;
			found = TRUE;
			found_index_record = 0;
			found_index_sector = 0;
		}
		else {
			

		}
		
	}
	

	create_new_record(&records[found_index_record], names[size-1], type);

	memcpy((void*) buffer, (void*) records,  SECTOR_SIZE);
 	
	//printf("Escrevendo no setor: %d + %d- RECORDS \n", start_sector, found_index_sector);
	write_sector (start_sector + found_index_sector, buffer);

	create_new_register(records[found_index_record].MFTNumber);

	memcpy((void*) buffer, (void*) & MFT_registers[records[found_index_record].MFTNumber][0], SECTOR_SIZE);
	
	//printf("Escrevendo no setor: %d- REGISTERS \n: Esse>", register_to_sector(records[found_index_record].MFTNumber));
	//print_4tupla(MFT_registers[records[found_index_record].MFTNumber][0]);

	write_sector ( register_to_sector(records[found_index_record].MFTNumber), buffer);		// ISSO HOLY YES

	memcpy((void*) buffer, (void*) & MFT_registers[records[found_index_record].MFTNumber][16], SECTOR_SIZE);
	write_sector ( register_to_sector(records[found_index_record].MFTNumber)+1, buffer);

	return (start_sector + found_index_sector); // Setor que o record foi escrito

}

char** split(char* string, int *size) {


	char *dup= (char*)malloc(strlen(string) + 1);

	strcpy(dup, string);

	int i;
	int path_size = 0; 

	if (strcmp(string, "/") == 0) {
		*size = 0;
		return NULL;
	}

	for (i=0; i<strlen(string); i++) {
		if (string[i] == '/') 
			path_size++;
		
	}
	*size = path_size;


	char** result = (char**)malloc(path_size*sizeof(char*));


	char* tken = strtok(dup, "/");
 
	i=0;
	while (tken != NULL) {
		result[i] = (char*)malloc(strlen(tken)*sizeof(char));
		strcpy( result[i], tken); 
		i++;
		tken = strtok(NULL, "/");
	}

	return result;
}

int find_register_from_path(char *filename) {
	
	int size, i;
	char **names = split(filename, &size);
	int index_register = 1;
		
	struct t2fs_record *record;

	for (i = 0; i<size-1; i++) {
		record = search_directory(index_register, names[i]);
		if (record == NULL) {
			printf("DEU RUIM1\n");
			return -1;
		}
		index_register = record->MFTNumber;
	}

	return index_register;

}

/*
int search_file_in_register(char *filename, int index_register) {

	int z,k,j,i;
	int start_sector, start_block, num_block_tupla;
	struct t2fs_record *record;

	while(MFT_registers[index_register][0].atributeType!=-1) {

		for ( z = 0; (z < TUPLES_PER_REGISTER && MFT_registers[index_register][z].atributeType != 0); z++) {

			start_block = MFT_registers[index_register][z].logicalBlockNumber;
			start_sector = blockToFirstSector(start_block);
			num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;

			for ( k = 0; (k < num_block_tupla); k++) {

				for ( j = 0 ; (j < bootBlock.blockSize) ; j++ ) {
	
					records = readRecordsAtSector(start_sector + j);

					for ( i = 0; i < SECTOR_SIZE/sizeof(struct t2fs_record); i++ ) {
						if ( isValidRecord(records[i]) && !strcmp(records[i].name, filename)) {
							return &records[i];
						}
					}

					free(records);
				}

			}
		}
			index_register = MFT_registers[index_register][TUPLES_PER_REGISTER-1].virtualBlockNumber;
			if (index_register < 0)
				break;
		
	}

}
*/

struct t2fs_record* search_directory(int index_register, char *filename) {

	int z,k,j,i;
	int start_sector, start_block, num_block_tupla;

	struct t2fs_record* records;


	while(MFT_registers[index_register][0].atributeType!=0) {

		for ( z = 0; (z < TUPLES_PER_REGISTER && MFT_registers[index_register][z].atributeType > 0); z++) {

			start_block = MFT_registers[index_register][z].logicalBlockNumber;
			start_sector = blockToFirstSector(start_block);
			num_block_tupla = MFT_registers[index_register][z].numberOfContiguosBlocks;

			for ( k = 0; (k < num_block_tupla); k++) {

				for ( j = 0 ; (j < bootBlock.blockSize) ; j++ ) {
	
					records = readRecordsAtSector(start_sector + j + k * bootBlock.blockSize);

					for ( i = 0; i < SECTOR_SIZE/sizeof(struct t2fs_record); i++ ) {
						if ( isValidRecord(records[i]) && !strcmp(records[i].name, filename)) {
							return &records[i];
						}
					}

					free(records);
				}

			}
		}
			index_register = MFT_registers[index_register][TUPLES_PER_REGISTER-1].virtualBlockNumber;
			if (index_register < 0)
				break;
		
	}

	return NULL;
}


int register_to_sector(const int register_number) {
	return bootBlock.blockSize + (register_number * (TUPLES_PER_REGISTER*sizeof(struct t2fs_4tupla)) / SECTOR_SIZE); // (4 + register_number*2)
}

int create_new_register(int index) {

	MFT_registers[index][0].atributeType = 1;
	MFT_registers[index][0].virtualBlockNumber = 0;

	MFT_registers[index][0].logicalBlockNumber = searchBitmap2(0);
	setBitmap2 (MFT_registers[index][0].logicalBlockNumber, 1);

	MFT_registers[index][0].numberOfContiguosBlocks = 1;

	MFT_registers[index][1].atributeType = 0;
	MFT_registers[index][1].virtualBlockNumber = 0;
	MFT_registers[index][1].logicalBlockNumber = 0;
	MFT_registers[index][1].numberOfContiguosBlocks = 0;
	
	return 0;

}

int create_new_record(struct t2fs_record *new_record, char *filename, int type) {
	new_record->TypeVal = type; 
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

		memcpy((void*) &(records[i].TypeVal),        (void*) &sector_buffer[ 0 + 64*i], 1  );
		memcpy((void*) &(records[i].name), 	     (void*) &sector_buffer[ 1 + 64*i], 51 );
		memcpy((void*) &(records[i].blocksFileSize), (void*) &sector_buffer[52 + 64*i], 4  );
		memcpy((void*) &(records[i].bytesFileSize),  (void*) &sector_buffer[56 + 64*i], 4  );
		memcpy((void*) &(records[i].MFTNumber),      (void*) &sector_buffer[60 + 64*i], 4  );
		//memcpy((void*) &records[i], (void*) &sector_buffer[size_record*i], size_record);

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
void print_all (void) {
    print_tree(0, "/");
}

void print_tree (int nivel, char *dir) {
   
    //printf("AQUI: %s\n", dir);
    int a = opendir2(dir);
    char name_ant[MAX_FILE_NAME_SIZE];  
    name_ant[0] = ' ';
 
    DIRENT2 dentry;
 
    int k;
 
    char rec_name[MAX_FILE_NAME_SIZE];
 
    while(1) {
 
        readdir2 (a, &dentry);          
       
        if( !strcmp(dentry.name, name_ant) ) {
            break;
        }
 
       
        if ( dentry.fileType == 1) {
            spacer(nivel);
            printf("%s\n", dentry.name);
        }
        else if( dentry.fileType == 2) {
            spacer(nivel);
           
            strcpy(rec_name, "");
 
            strcat(rec_name, dir);
 
            if (nivel != 0) {
                strcat(rec_name, "/");
            }
            strcat(rec_name, dentry.name);
           
            printf("%s\n", dentry.name);           
 
            print_tree(nivel+1, rec_name);
 
        }  
 
        strcpy(name_ant, dentry.name);
    }
}
void spacer(int nivel) {
    int k;
    for (k=0; k <nivel; k++) {  
        printf("  ");      
    }
}
