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
	for (int i = 0; i < N_BLOCK_ON_DISK; i++){
		if (free_block_bitmap[i] != 0){
			free_block_bitmap[i] = 0;
			printf("GLOFS: saisie du bloque: %d\n", i);
			WriteBlock(FREE_BLOCK_BITMAP, free_block_bitmap);
			return i;
		}
	}

	return -1;
}

/* release d'un inode sur le bitmap */
int release_free_inode(UINT16 inode_no){
	char free_inode_bitmap[BLOCK_SIZE];
	ReadBlock(FREE_INODE_BITMAP, free_inode_bitmap);
	free_inode_bitmap[inode_no] = 0;
	printf("GLOFS: relache du inode: %d\n", inode_no);
	WriteBlock(FREE_INODE_BITMAP, free_inode_bitmap);
	return -1;
}

/* allocation d'un inode sur le inode bitmap */
int get_free_inode(){
	char free_inode_bitmap[BLOCK_SIZE];
	ReadBlock(FREE_INODE_BITMAP, free_inode_bitmap);
	for (int i = 0; i < N_INODE_ON_DISK; i ++){
		if (free_inode_bitmap[i] != 0){
			free_inode_bitmap[i] = 0;
			printf("GLOFS: saisie du inode: %d\n", i);
			WriteBlock(FREE_INODE_BITMAP, free_inode_bitmap);
			return i;
		}
	}
	return -1;
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
		printf("dir: %s\n", p_dir_entries[i].Filename);
		printf("sub: %s\n", sub_path);
		if (strcmp(sub_path, p_dir_entries[i].Filename) == 0){
			printf("child read: %d\n", p_dir_entries[i].iNode);
			return p_dir_entries[i].iNode;
		}
	}

	return -1;
}


int get_inode_from_filename(char *p_filename, ino *p_inode_no){
	ino *child_inode = p_inode_no;
	ino parent = *p_inode_no;
	//root case
	if (strcmp("/", p_filename) == 0){
		*p_inode_no = ROOT_INODE;
		return 0;
	}

	//other cases
	else if (strcmp("/", p_filename) != 0){
		char *sub_path;
		char temp_path[FILENAME_SIZE];
		printf("filename: %s\n", p_filename);
		int counter = 0;
		for (int i = 1; i < strlen(p_filename); i++){
			if (p_filename[i] != '/'){
				temp_path[i-1] = p_filename[i];
			}
			//not las dir to check inode
			if (p_filename[i] == '/'){
				printf("here \n");
				sub_path = p_filename + i + 1;
				if (counter > 0){
					
				}
				else{
					*p_inode_no = read_inode_dir(*p_inode_no, temp_path);
				}
				
				counter++;
			}

			//last dir to check
			else if ((i + 1) == strlen(p_filename)){
				printf("there \n");
				if (counter > 0){
					*p_inode_no = read_inode_dir(*p_inode_no, sub_path);
					printf("child file1: %d\n", *p_inode_no);
				}
				else{
					*p_inode_no = read_inode_dir(*p_inode_no, temp_path);
					printf("child file2: %d\n", *p_inode_no);
				}
				
				return 0;
			}

			printf("temp: %s\n", temp_path);
			printf("i: %d\n", i);
		}

	}

	return -1;
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
	return -1;
}

int bd_read(const char *pFilename, char *buffer, int offset, int numbytes) {
	return -1;
}

int bd_mkdir(const char *pDirName) {
	return -1;
}

int bd_write(const char *pFilename, const char *buffer, int offset, int numbytes) { 
	return -1;
}

int bd_hardlink(const char *pPathExistant, const char *pPathNouveauLien) {
	return -1;
}

int bd_unlink(const char *pFilename) {
	return -1;
}

int bd_truncate(const char *pFilename, int NewSize) {
	return -1;
}

int bd_rmdir(const char *pFilename) {
	return -1;
}

int bd_rename(const char *pFilename, const char *pDestFilename) {
	return -1;
}

int bd_readdir(const char *pDirLocation, DirEntry **ppListeFichiers) {
	return -1;
}

int bd_symlink(const char *pPathExistant, const char *pPathNouveauLien) {
    return -1;
}

int bd_readlink(const char *pPathLien, char *pBuffer, int sizeBuffer) {
    return -1;
}

