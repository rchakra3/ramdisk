#define DATASIZE 736


typedef struct Cluster{

	char name[255];
	//long i_no;
	long sameLevelNext;
	long nextLevel;
	long nextFileBlockNum;
	char type;//d for dir, f for file, n-next File block(continued file block), c for clear,empty,NULL
	mode_t mode;//still unclear
	char data[DATASIZE];//NULL for directories
}Cluster;

Cluster *head;
long lastEmptySpace;
long numClusters;
char root[2];
char cwd[1000];


Cluster* getClusterAtPos(long position);
int insertNode(char *path,mode_t mode,char ClusterType);
long getNextEmpty(void);
int handle_mkdir(char *path, mode_t mode);
char *stripPath(char *path);
char *removeConst(const char *);
void initializeMem(void);
void handle_ReadDir(char *path, void **buf,fuse_fill_dir_t filler);
int handle_rmdir(char *path);
void writeToDisk(char *fileName);
void readFromDisk(char *fileName);
char *stripName(char *path);
int allowedPathName(char *path);
long getClusterNumFromPath(char *og_path);
size_t getSizeOfFile(long clusterNum);
long handleLargeFileBlock(long clusterNum,const char *buf, size_t size,off_t offset,long buf_pos_written);
Cluster* getSubDirHead(char *path);
int handle_rmdir_errors(char *path);
int handle_rename(char *srcPath,char *destPath);
void initializeGlobal(void);
int handle_truncate(char *path,int size);

