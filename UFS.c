#include "UFS.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "disque.h"


// Quelques fonctions qui pourraient vous être utiles
int NumberofDirEntry(int Size) {
	return Size/sizeof(DirEntry);
}

int min(int a, int b) {
	return a<b ? a : b;
}

int max(int a, int b) {
	return a>b ? a : b;
}


/* Cette fonction va extraire le repertoire d'une chemin d'acces complet, et le copier
   dans pDir.  Par exemple, si le chemin fourni pPath="/doc/tmp/a.txt", cette fonction va
   copier dans pDir le string "/doc/tmp" . Si le chemin fourni est pPath="/a.txt", la fonction
   va retourner pDir="/". Si le string fourni est pPath="/", cette fonction va retourner pDir="/".
   Cette fonction est calquée sur dirname, que je ne conseille pas d'utiliser car elle fait appel
   à des variables statiques/modifie le string entrant. */
int GetDirFromPath(const char *pPath, char *pDir) {
	strcpy(pDir,pPath);
	int len = strlen(pDir); // length, EXCLUDING null
	int index;

	// On va a reculons, de la fin au debut
	while (pDir[len]!='/') {
		len--;
		if (len <0) {
			// Il n'y avait pas de slash dans le pathname
			return 0;
		}
	}
	if (len==0) {
		// Le fichier se trouve dans le root!
		pDir[0] = '/';
		pDir[1] = 0;
	}
	else {
		// On remplace le slash par une fin de chaine de caractere
		pDir[len] = '\0';
	}
	return 1;
}

/* Cette fonction va extraire le nom de fichier d'une chemin d'acces complet.
   Par exemple, si le chemin fourni pPath="/doc/tmp/a.txt", cette fonction va
   copier dans pFilename le string "a.txt" . La fonction retourne 1 si elle
   a trouvée le nom de fichier avec succes, et 0 autrement. */
int GetFilenameFromPathWithSlash(const char *pPath, char *pFilename) {
	// Pour extraire le nom de fichier d'un path complet
	char *pStrippedFilename = strrchr(pPath,'/');
	if (pStrippedFilename!=NULL) {
		//++pStrippedFilename; // On avance pour passer le slash, on veut garder le slash puisque notre code traite deja le slash
		if ((*pStrippedFilename) != '\0') {
			// On copie le nom de fichier trouve
			strcpy(pFilename, pStrippedFilename);
			return 1;
		}
	}
	return 0;
}

int GetFilenameFromPath(const char *pPath, char *pFilename) {
	// Pour extraire le nom de fichier d'un path complet
	char *pStrippedFilename = strrchr(pPath,'/');
	if (pStrippedFilename!=NULL) {
		++pStrippedFilename; // On avance pour passer le slash
		if ((*pStrippedFilename) != '\0') {
			// On copie le nom de fichier trouve
			strcpy(pFilename, pStrippedFilename);
			return 1;
		}
	}
	return 0;
}


/* Cette fonction sert à afficher à l'écran le contenu d'une structure d'i-node */
void printiNode(iNodeEntry iNode) {
	printf("\t\t========= inode %d ===========\n",iNode.iNodeStat.st_ino);
	printf("\t\t  blocks:%d\n",iNode.iNodeStat.st_blocks);
	printf("\t\t  size:%d\n",iNode.iNodeStat.st_size);
	printf("\t\t  mode:0x%x\n",iNode.iNodeStat.st_mode);
	int index = 0;
	for (index =0; index < N_BLOCK_PER_INODE; index++) {
		printf("\t\t      Block[%d]=%d\n",index,iNode.Block[index]);
	}
}


/* ----------------------------------------------------------------------------------------
					            à vous de jouer, maintenant!
   ---------------------------------------------------------------------------------------- */

/*fonction utilitaires*/

/* realease d'un bloque sur le bitmap bloque */ 
int release_free_block(UINT16 block_no){
	char free_block_bitmap[BLOCK_SIZE];
	ReadBlock(FREE_BLOCK_BITMAP, free_block_bitmap);
	free_block_bitmap[block_no] = 1;
	printf("GLOFS: relache bloque: %d\n", block_no);
	WriteBlock(FREE_BLOCK_BITMAP, free_block_bitmap);
	return -1;
}

/* allocation d'un bloque sur le bitmap bloque */
int get_free_block(){
	char free_block_bitmap[BLOCK_SIZE];
	ReadBlock(FREE_BLOCK_BITMAP, free_block_bitmap);
	int i = BASE_BLOCK_INODE + (N_INODE_ON_DISK / NUM_INODE_PER_BLOCK);
	while (i < N_BLOCK_ON_DISK && free_block_bitmap[i] == 0){
		i++;
	}
	if (i > N_BLOCK_ON_DISK){
		return -1;
	}
	free_block_bitmap[i] = 0;
	printf("GlOFS saisie bloque: %d\n", i);
	WriteBlock(FREE_BLOCK_BITMAP, free_block_bitmap);
	return i;

}

/* release d'un inode sur le bitmap */
int release_free_inode(UINT16 inode_no){
	char free_inode_bitmap[BLOCK_SIZE];
	ReadBlock(FREE_INODE_BITMAP, free_inode_bitmap);
	free_inode_bitmap[inode_no] = 1;
	printf("GLOFS: relache du inode: %d\n", inode_no);
	WriteBlock(FREE_INODE_BITMAP, free_inode_bitmap);
	return -1;
}

/* allocation d'un inode sur le inode bitmap */
int get_free_inode(){
	char free_inode_bitmap[BLOCK_SIZE];
	ReadBlock(FREE_INODE_BITMAP, free_inode_bitmap);
	int i = ROOT_INODE;
	while (i < N_INODE_ON_DISK && free_inode_bitmap[i] == 0){
		i++;
	}
	if (i > N_INODE_ON_DISK){
		return -1;
	}
	free_inode_bitmap[i] = 0;
	printf("GlOFS saisie inode: %d\n", i);
	WriteBlock(FREE_INODE_BITMAP, free_inode_bitmap);
	return i;
	
}



/* get du inode entry selon un numero de inode */
int get_inode_entry(ino inode_no, iNodeEntry *p_inode_entry){
	if (inode_no > N_INODE_ON_DISK){
		return -1; //on ne peut avoir un inode plus grand que le nombre sur le disque 
	}

	/* on va chercher le iNodeEntry a partir du numero de inode */
	char block_data[BLOCK_SIZE];
	UINT16 inode_block_no = BASE_BLOCK_INODE + (inode_no / NUM_INODE_PER_BLOCK); //base block plus le numero de inode diviser par le nombre de inode par bloque pour avoir le bloque number
	UINT16 inode_offset = inode_no % NUM_INODE_PER_BLOCK; //offset dans la liste sur le bloque
	ReadBlock(inode_block_no, block_data);
	iNodeEntry *p_inode_array = (iNodeEntry*) block_data;
	*p_inode_entry = p_inode_array[inode_offset];

	return 0;
}

int write_inode_on_block(iNodeEntry *p_entries){
	char data_block[BLOCK_SIZE];
	UINT16 block_no = BASE_BLOCK_INODE + (p_entries->iNodeStat.st_ino / NUM_INODE_PER_BLOCK);
	UINT16 inode_offset = p_entries->iNodeStat.st_ino % NUM_INODE_PER_BLOCK;
	ReadBlock(block_no, data_block);
	iNodeEntry *block_entries = (iNodeEntry*)data_block;
	block_entries[inode_offset] = *p_entries;
	WriteBlock(block_no, data_block);
	return 0;
}

//read inode and return the child inode associated with dir
int read_inode_dir(ino parent_inode, char *sub_path){
	char data_block[BLOCK_SIZE];
	UINT16 inode_block_no = BASE_BLOCK_INODE + (parent_inode / NUM_INODE_PER_BLOCK);
	ReadBlock(inode_block_no, data_block);
	iNodeEntry *p_inode_entries = (iNodeEntry*)data_block;
	UINT16 inode_offset = parent_inode % NUM_INODE_PER_BLOCK;
	UINT16 no_dir_entries_in_block = NumberofDirEntry(p_inode_entries[inode_offset].iNodeStat.st_size);
	//lecture du bloque du inode pour apres lire les dir entries
	ReadBlock(p_inode_entries[inode_offset].Block[0], data_block);
	DirEntry *p_dir_entries = (DirEntry*)data_block;	

	for (int i = 0; i < no_dir_entries_in_block; i++){
		//printf("dir: %s\n", p_dir_entries[i].Filename);
		//printf("sub: %s\n", sub_path);
		if (strcmp(sub_path, p_dir_entries[i].Filename) == 0){
			//printf("child read: %d\n", p_dir_entries[i].iNode);
			return p_dir_entries[i].iNode;
		}
	}

	return -1;
}

int get_sub_path(char *p_path, char *sub_path){
	for (int i = 0; i < strlen(p_path); i++){
		if (p_path[i] != '/'){
			sub_path[i] = p_path[i];
		}
		else if (p_path[i] == '/'){
			sub_path[i] = '\0';
			return 0;
		}
	}

	//printf("sub there: %s\n", sub_path);
	return -1;
}


int get_inode_from_filename(const char *p_filename, ino *p_inode_no){
	ino *child_inode = p_inode_no;
	ino parent = *p_inode_no;
	//root case
	if (strcmp("/", p_filename) == 0){
		*p_inode_no = ROOT_INODE;
		return 0;
	}

	//other cases
	else if (strcmp("/", p_filename) != 0){
		char *sub_path = malloc(sizeof(char) * BLOCK_SIZE);
		char *temp_path = malloc(sizeof(char) * FILENAME_SIZE);
		//printf("filename: %s\n", p_filename);
		int counter = 0;
		for (int i = 1; i < strlen(p_filename); i++){
			if (p_filename[i] != '/'){
				temp_path[i-1] = p_filename[i];
			}
			//not las dir to check inode
			if (p_filename[i] == '/'){
				//printf("here \n");
				if (counter > 0){
					char *temp_sub_path = malloc(sizeof(char) * FILENAME_SIZE);
					get_sub_path(sub_path, temp_sub_path);
					//printf("sub here: %s\n", temp_sub_path);
					*p_inode_no = read_inode_dir(*p_inode_no, temp_sub_path);
					if (*p_inode_no == -1){
						return -1;
					}
					free(temp_sub_path); //free here works
					sub_path = p_filename + i + 1;
				}
				else{
					temp_path[i-1] = '\0';
					sub_path = p_filename + i + 1;
					*p_inode_no = read_inode_dir(*p_inode_no, temp_path);
					free(temp_path);
					if (*p_inode_no == -1){
						return -1;
					}
				}
				
				counter++;
			}

			//last dir to check
			else if ((i + 1) == strlen(p_filename)){
				//printf("there \n");
				if (counter > 0){
					*p_inode_no = read_inode_dir(*p_inode_no, sub_path);
					//free(sub_path);
					if (*p_inode_no == -1){
						return -1;
					}
					//printf("child file1: %d\n", *p_inode_no);
				}
				else{
					temp_path[i] = '\0';
					*p_inode_no = read_inode_dir(*p_inode_no, temp_path);
					free(temp_path);
					if (*p_inode_no == -1){
						return -1;
					}
					//printf("child file2: %d\n", *p_inode_no);
				}
				break;
			}

		}

		//memory leak but seg fault when freeing?
		//free(temp_path);
		//free(sub_path);
		return 0;

	}

	return -1;
}

int add_filename_in_directory(char *filename, iNodeEntry* p_inode_entry, ino inode_no){
	char data_block[BLOCK_SIZE];
	p_inode_entry->iNodeStat.st_size += sizeof(DirEntry);
	write_inode_on_block(p_inode_entry);
	UINT16 size = p_inode_entry->iNodeStat.st_size;
	int no_of_entries = NumberofDirEntry(size);
	UINT16 block_no = p_inode_entry->Block[0];
	//printf("block no: %d\n", p_inode_entry->Block[0]);
	ReadBlock(block_no, data_block);
	DirEntry *p_dir_entries = (DirEntry*)data_block;
	p_dir_entries[no_of_entries - 1].iNode = inode_no;
	strcpy(p_dir_entries[no_of_entries - 1].Filename, filename);
	
	
	//printf("dir entry %s\n", p_dir_entries->Filename);
	WriteBlock(block_no, data_block);

	return 0;
}

int remove_filename_in_directory(iNodeEntry *p_inode_dir, ino inode_no_delete){
	p_inode_dir->iNodeStat.st_size -= BLOCK_SIZE / sizeof(DirEntry);
	write_inode_on_block(p_inode_dir);

	char data_block[BLOCK_SIZE];
	ReadBlock(p_inode_dir->Block[0], data_block);
	DirEntry *p_dir_entry = (DirEntry*)data_block;
	int size = NumberofDirEntry(p_inode_dir->iNodeStat.st_size);
	int my_switch = 0;
	for (int i = 0; i < size; i++){
		if (p_dir_entry[i].iNode == inode_no_delete){
			my_switch = 1;
		}
		if (my_switch == 1){
			p_dir_entry[i] = p_dir_entry[i + 1];
		}
	}
	WriteBlock(p_inode_dir->Block[0], data_block);

	return 0;
}


/*fonction a implementees*/

int bd_countfreeblocks(void) {
	char temp_block[BLOCK_SIZE];
	int free_block_count = 0;
	ReadBlock(FREE_BLOCK_BITMAP, temp_block);
	for (int i = 0; i < N_BLOCK_ON_DISK; i++){
		if(temp_block[i] != 0){
			free_block_count += 1;
		}
	}
	return free_block_count;
}

int bd_stat(const char *pFilename, gstat *pStat) {
	ino inode = ROOT_INODE;
	iNodeEntry p_inode_entry;
	if (get_inode_from_filename(pFilename, &inode) != 0){
		return -1;
	}
	if (get_inode_entry(inode, &p_inode_entry) != 0){
		return -1;
	}

	*pStat = p_inode_entry.iNodeStat;


	return 0;
}

int bd_create(const char *pFilename) {
	char *dir = malloc(sizeof(char)*BLOCK_SIZE);
	char *filename = malloc(sizeof(char)*FILENAME_SIZE);
	ino inode_dir = ROOT_INODE;
	ino last_inode_found = ROOT_INODE;

	GetDirFromPath(pFilename, dir);
	GetFilenameFromPathWithSlash(pFilename, filename);
	get_inode_from_filename(dir, &inode_dir); //get inode of dir
	last_inode_found = inode_dir; //next step erase inode_dir we need a backup
	if (inode_dir == -1){
		free(dir);
		free(filename);
		return -1;
	}
	get_inode_from_filename(filename, &inode_dir); //get inode of filename in dir
	if (inode_dir != -1){
		free(dir);
		free(filename);
		return -2;
	} 

	char inode_data[BLOCK_SIZE];
	UINT16 inode_block_no = BASE_BLOCK_INODE + (last_inode_found / NUM_INODE_PER_BLOCK);
	UINT16 inode_offset = last_inode_found % NUM_INODE_PER_BLOCK;
	ReadBlock(inode_block_no, inode_data);
	iNodeEntry *p_inode_entries = (iNodeEntry*)inode_data;
	if (p_inode_entries[inode_offset].iNodeStat.st_size >= BLOCK_SIZE){
		free(dir);
		free(filename);
		return -4;
	}

	GetFilenameFromPath(pFilename, filename);
	inode_dir = get_free_inode();
	iNodeEntry inode_entries;
	get_inode_entry(inode_dir, &inode_entries);
	inode_entries.iNodeStat.st_size = 0;
	inode_entries.iNodeStat.st_nlink = 1;
	inode_entries.iNodeStat.st_blocks = 0;
	inode_entries.iNodeStat.st_ino = inode_dir;
	inode_entries.iNodeStat.st_mode = G_IFREG;
	inode_entries.iNodeStat.st_mode = inode_entries.iNodeStat.st_mode | G_IRUSR | G_IWUSR | G_IRGRP | G_IWGRP;
	write_inode_on_block(&inode_entries);
	iNodeEntry inode_dir_entries;
	get_inode_entry(last_inode_found, &inode_dir_entries);
	add_filename_in_directory(filename, &inode_dir_entries, inode_dir);

	free(dir);
	free(filename);

	return 0;
}

int bd_read(const char *pFilename, char *buffer, int offset, int numbytes) {
	ino inode_found = ROOT_INODE;
	char *dir = malloc(sizeof(char)*BLOCK_SIZE);
	GetDirFromPath(pFilename, dir);

	get_inode_from_filename(pFilename, &inode_found);
	if (inode_found == -1){
		free(dir);
		return -1;
	}

	iNodeEntry p_inode_entry;
	get_inode_entry(inode_found, &p_inode_entry);

	if (p_inode_entry.iNodeStat.st_mode & G_IFDIR){
		free(dir);
		return -2;
	}

	if (offset >= p_inode_entry.iNodeStat.st_size){
		free(dir);
		return 0;
	}

	char file_data[BLOCK_SIZE];
	ReadBlock(p_inode_entry.Block[0], file_data);

	int bytes = 0;
	for (int i = offset; i < (offset + numbytes) && i < p_inode_entry.iNodeStat.st_size; i++){
		buffer[bytes] = file_data[i];
		bytes++;
	}

	free(dir);
	return bytes;;
}

int bd_mkdir(const char *pDirName) {
	char *last_dir = malloc(sizeof(char)*FILENAME_SIZE);
	char *path = malloc(sizeof(char)*BLOCK_SIZE);
	ino inode_found = ROOT_INODE;
	GetDirFromPath(pDirName, path);
	GetFilenameFromPath(pDirName, last_dir);

	if (strcmp(last_dir, "") == 0){
		free(last_dir);
		free(path);
		return -3;
	}

	get_inode_from_filename(pDirName, &inode_found);
	if (inode_found != -1){
		free(last_dir);
		free(path);
		return -2;
	}

	inode_found = ROOT_INODE;
	get_inode_from_filename(path, &inode_found);
	if (inode_found == -1){
		free(last_dir);
		free(path);
		return -1;
	}

	iNodeEntry parent_inode_entry;
	get_inode_entry(inode_found, &parent_inode_entry);

	if (parent_inode_entry.iNodeStat.st_size > BLOCK_SIZE){
		free(last_dir);
		free(path);
		return -4;
	}

	ino child_inode_no = get_free_inode();
	int child_inode_block_no = get_free_block();
	
	parent_inode_entry.iNodeStat.st_nlink += 1;
	write_inode_on_block(&parent_inode_entry);
	add_filename_in_directory(last_dir, &parent_inode_entry, child_inode_no);

	iNodeEntry child_inode_entry;
	get_inode_entry(child_inode_no, &child_inode_entry);
	get_inode_entry(child_inode_no, &child_inode_entry);
	child_inode_entry.Block[0] = child_inode_block_no;
	child_inode_entry.iNodeStat.st_ino = child_inode_no;
	child_inode_entry.iNodeStat.st_nlink = 2;
	child_inode_entry.iNodeStat.st_blocks = 1;
	child_inode_entry.iNodeStat.st_size = 2 * sizeof(DirEntry);
	child_inode_entry.iNodeStat.st_mode = G_IFDIR | G_IRWXU | G_IRWXG;
	write_inode_on_block(&child_inode_entry);


	char data_block[BLOCK_SIZE];
	ReadBlock(child_inode_block_no, data_block);
	DirEntry *p_child_entries = (DirEntry*)data_block;
	strcpy(p_child_entries[0].Filename, ".");
	p_child_entries[0].iNode = child_inode_no;
	strcpy(p_child_entries[1].Filename, "..");
	p_child_entries[1].iNode = inode_found;
	WriteBlock(child_inode_block_no, data_block);


	free(last_dir);
	free(path);
	return 0;
}

int bd_write(const char *pFilename, const char *buffer, int offset, int numbytes) { 

	if (offset > MAX_FILE_SIZE){
		return -4;
	}

	ino inode_found = ROOT_INODE;
	get_inode_from_filename(pFilename, &inode_found);
	if (inode_found == -1){
		return -1;
	}

	iNodeEntry p_inode_entry;
	get_inode_entry(inode_found, &p_inode_entry);
	if (p_inode_entry.iNodeStat.st_mode & G_IFDIR){
		return -2;
	}
	if (p_inode_entry.iNodeStat.st_size < offset){
		return -3;
	}

	char file_data[BLOCK_SIZE];
	ReadBlock(p_inode_entry.Block[0], file_data);

	char temp[BLOCK_SIZE];
	for (int i = 0; i < p_inode_entry.iNodeStat.st_size; i++){
		temp[i] = file_data[i];
	}

	int bytes = 0;
	int counter = 0;
	for (int i = offset; i <= BLOCK_SIZE && i < (offset + numbytes) && counter <= numbytes; i++){
		if(temp[i] != buffer[counter]){
			temp[i] = buffer[bytes];
			bytes++;
		}
		counter++;
		
	}

	WriteBlock(p_inode_entry.Block[0], temp);
	if (offset + bytes > p_inode_entry.iNodeStat.st_size){
		p_inode_entry.iNodeStat.st_size = offset + bytes;
		printf("%d\n", p_inode_entry.iNodeStat.st_size);
	}

	write_inode_on_block(&p_inode_entry);


	return bytes;
}

int bd_hardlink(const char *pPathExistant, const char *pPathNouveauLien) {
	ino inode_exist = ROOT_INODE;
	ino inode_new = ROOT_INODE;
	ino temp_test_if_new_exist = ROOT_INODE;
	char last_dir_new[BLOCK_SIZE];
	GetDirFromPath(pPathNouveauLien, last_dir_new);
	get_inode_from_filename(pPathExistant, &inode_exist);
	get_inode_from_filename(last_dir_new, &inode_new);
	get_inode_from_filename(pPathNouveauLien, & temp_test_if_new_exist);

	if (inode_exist == -1){
		return -1;
	}
	if (temp_test_if_new_exist != -1){
		return -2;
	}

	iNodeEntry inode_entry_new;
	iNodeEntry inode_entry_exist;
	get_inode_entry(inode_new, &inode_entry_new);
	get_inode_entry(inode_exist, &inode_entry_exist);

	if (inode_entry_exist.iNodeStat.st_mode & G_IFDIR){
		return -3;
	}
	if (inode_entry_new.iNodeStat.st_size >= BLOCK_SIZE){
		return -4;
	}

	char data_block[BLOCK_SIZE];
	char filename_new[FILENAME_SIZE];
	GetFilenameFromPath(pPathNouveauLien, filename_new);

	ReadBlock(inode_entry_new.Block[0], data_block);
	DirEntry *p_entry_new = (DirEntry*)data_block;
	UINT16 no_of_entries = NumberofDirEntry(inode_entry_new.iNodeStat.st_size);

	p_entry_new[no_of_entries].iNode = inode_entry_exist.iNodeStat.st_ino;
	strcpy(p_entry_new[no_of_entries].Filename, filename_new);

	inode_entry_new.iNodeStat.st_size += sizeof(DirEntry);
	inode_entry_exist.iNodeStat.st_nlink += 1;
	WriteBlock(inode_entry_new.Block[0], data_block);

	write_inode_on_block(&inode_entry_new);
	write_inode_on_block(&inode_entry_exist);

	return 0;
}

int bd_unlink(const char *pFilename) {
	ino inode_found = ROOT_INODE;
	get_inode_from_filename(pFilename, &inode_found);
	if (inode_found == -1){
		return -1;
	}

	iNodeEntry inode_entry;
	get_inode_entry(inode_found, &inode_entry);
	if (inode_entry.iNodeStat.st_mode & G_IFDIR){ //on utilise G_IFDIR puisque le flag pour les sym link sont different que ceux pour les reg file
		return -2;
	}

	ino inode_dir = ROOT_INODE;
	char dir[BLOCK_SIZE];
	char filename[FILENAME_SIZE];
	iNodeEntry inode_dir_entry;
	GetDirFromPath(pFilename, dir);
	GetFilenameFromPath(pFilename, filename);
	get_inode_from_filename(dir, &inode_dir);
	get_inode_entry(inode_dir, &inode_dir_entry);

	char data_block[BLOCK_SIZE];
	ReadBlock(inode_dir_entry.Block[0], data_block);
	DirEntry *p_dir_entries = (DirEntry*)data_block;
	UINT16 no_of_entries = NumberofDirEntry(inode_dir_entry.iNodeStat.st_size);
	int i;
	int j;
	for (int i = 0; i < no_of_entries; i++){
		if (strcmp(p_dir_entries[i].Filename, filename) == 0){
			if (i != no_of_entries - 1){
				for(j = 1; j < no_of_entries - i; j++){
					p_dir_entries[i + j - 1] = p_dir_entries[i + j];
				}
				break;
			}
		}
	}

	WriteBlock(inode_dir_entry.Block[0], data_block);
	inode_dir_entry.iNodeStat.st_size -= sizeof(DirEntry);
	write_inode_on_block(&inode_dir_entry);

	inode_entry.iNodeStat.st_nlink -= 1;
	if (inode_entry.iNodeStat.st_nlink == 0){
		if (inode_entry.iNodeStat.st_blocks > 0){
			release_free_block(inode_entry.Block[0]);
		}
		release_free_inode(inode_found);
	}
	else{
		write_inode_on_block(&inode_entry);
	}

	return 0;
}

int bd_truncate(const char *pFilename, int NewSize) {
	if (NewSize > MAX_FILE_SIZE){
		NewSize = MAX_FILE_SIZE;
	}
	ino inode_found = ROOT_INODE;
	get_inode_from_filename(pFilename, &inode_found);
	if (inode_found == -1){
		return -1;
	}

	iNodeEntry inode_entry;
	get_inode_entry(inode_found, &inode_entry);
	if (inode_entry.iNodeStat.st_mode & G_IFDIR){
		return -2;
	}

	if (NewSize == 0){
		release_free_block(inode_entry.Block[0]);
	}
	inode_entry.iNodeStat.st_size = NewSize;
	write_inode_on_block(&inode_entry);

	return inode_entry.iNodeStat.st_size;
}

int bd_rmdir(const char *pFilename) {
	ino ino_found = ROOT_INODE;
	get_inode_from_filename(pFilename, &ino_found);

	if (ino_found == -1){
		return -1;
	}

	iNodeEntry inode_entry;
	get_inode_entry(ino_found, &inode_entry);

	if (!(inode_entry.iNodeStat.st_mode & G_IFDIR)){
		return -2;
	}

	if(NumberofDirEntry(inode_entry.iNodeStat.st_size) > 2){
		return -3;
	}

	ino dir_inode = ROOT_INODE;
	char dir[BLOCK_SIZE];
	GetDirFromPath(pFilename, dir);
	get_inode_from_filename(dir, &dir_inode);
	iNodeEntry dir_entry;
	get_inode_entry(dir_inode, &dir_entry);

	remove_filename_in_directory(&dir_entry, ino_found);
	dir_entry.iNodeStat.st_nlink -= 1;
	write_inode_on_block(&dir_entry);
	release_free_block(inode_entry.Block[0]);
	release_free_inode(ino_found);


	return 0;
}

int bd_rename(const char *pFilename, const char *pDestFilename) {
	if (strcmp(pFilename, pDestFilename) == 0){
		return 0;
	}

	int ret = bd_hardlink(pFilename, pDestFilename);
	if (ret == -1 || ret == -2){
		return -1;
	}
	else if(ret == 0){
		bd_unlink(pFilename);
		return 0;
	}
	else{
		char dest_dir[BLOCK_SIZE];
		GetDirFromPath(pDestFilename, dest_dir);

		char new_file[FILENAME_SIZE];
		GetFilenameFromPath(pDestFilename, new_file);

		char source_dir[BLOCK_SIZE];
		GetDirFromPath(pFilename, source_dir);

		ino old_inode = ROOT_INODE;
		ino dest_inode = ROOT_INODE;
		ino source_inode = ROOT_INODE;
		ino temp = ROOT_INODE;
		get_inode_from_filename(dest_dir, &dest_inode);
		get_inode_from_filename(pFilename, &old_inode);
		get_inode_from_filename(source_dir, &source_inode);
		get_inode_from_filename(pDestFilename, &temp);

		if (temp != -1){
			return -1;
		}


		if (source_inode == -1 || old_inode == -1 || dest_inode == -1){
			return -1;
		}

		iNodeEntry source_inode_entry;
		iNodeEntry dest_inode_entry;
		iNodeEntry full_source_path_entry;

		get_inode_entry(source_inode, &source_inode_entry);
		remove_filename_in_directory(&source_inode_entry, old_inode);
		source_inode_entry.iNodeStat.st_nlink -= 1;
		write_inode_on_block(&source_inode_entry);

		get_inode_entry(dest_inode, &dest_inode_entry);
		add_filename_in_directory(new_file, &dest_inode_entry, old_inode);
		dest_inode_entry.iNodeStat.st_nlink += 1;
		write_inode_on_block(&dest_inode_entry);

		get_inode_entry(old_inode, &full_source_path_entry);
		char data_block[BLOCK_SIZE];
		ReadBlock(full_source_path_entry.Block[0], data_block);
		DirEntry *p_dir_entry = (DirEntry*)data_block;
		p_dir_entry++;
		p_dir_entry->iNode = dest_inode;
		WriteBlock(full_source_path_entry.Block[0], data_block);


		return 0;



	}

	return 0;
}

int bd_readdir(const char *pDirLocation, DirEntry **ppListeFichiers) {
	ino inode_found = ROOT_INODE;
	get_inode_from_filename(pDirLocation, &inode_found);
	if (inode_found == -1){
		return -1;
	}

	iNodeEntry inode_entry;
	get_inode_entry(inode_found, &inode_entry);
	if (!(inode_entry.iNodeStat.st_mode & G_IFDIR)){
		return -2;
	}

	char data_block[BLOCK_SIZE];
	ReadBlock(inode_entry.Block[0], data_block);
	*ppListeFichiers = (DirEntry*)malloc(inode_entry.iNodeStat.st_size);
	memcpy(*ppListeFichiers, data_block, inode_entry.iNodeStat.st_size);

	int count = NumberofDirEntry(inode_entry.iNodeStat.st_size);

	return count;
}

int bd_symlink(const char *pPathExistant, const char *pPathNouveauLien) {
    return -1;
}

int bd_readlink(const char *pPathLien, char *pBuffer, int sizeBuffer) {
    return -1;
}

