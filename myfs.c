
#define FUSE_USE_VERSION 26
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <fuse.h>
#include "myfs.h"


static int myfs_rename(const char *from, const char *to)
{
	char *fromToPass=NULL;
	char *toToPass=NULL;

	fromToPass=removeConst(from);
	toToPass=removeConst(to);

	if(NULL==fromToPass||NULL==toToPass)
	{
		return -ENOMEM;
	}

	return handle_rename(fromToPass,toToPass);
}


static int myfs_getattr(const char *path, struct stat *stbuf)
{
	int res = -1;
	long clusterNum=-1;
	char *pathToPass=NULL;
	Cluster *node;

	pathToPass=removeConst(path);

	if((strlen(pathToPass)+strlen(cwd))>250)
	{
		printf("Absolute Directory Path too long\n");
		return -ENOENT;
	}

	if(NULL!=pathToPass)
	{
		memset(stbuf, 0, sizeof(struct stat));

		if (strcmp(path, "/") == 0) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			res=0;
		} else if ((clusterNum=getClusterNumFromPath(pathToPass))<numClusters) {

			node=getClusterAtPos(clusterNum);

			if(node->type=='d')
			{
				stbuf->st_mode = S_IFDIR | 0755;
			}
			else if(node->type=='f')
			{
				stbuf->st_mode = S_IFREG | 0755;
			}

			stbuf->st_nlink = 1;
			//stbuf->st_size = strlen(path);
			stbuf->st_size =getSizeOfFile(clusterNum);
			res=0;
			//printf("The path exists!!!!\n\n");
		} else{
			res = -ENOENT;
			//puts(path);
		}
	}

	return res;	
}



static int myfs_mkdir(const char *path, mode_t mode)
{
	int res=-1;
	char *pathToPass=NULL;

	pathToPass=removeConst(path);

	if((strlen(pathToPass)+strlen(cwd))>250)
	{
		printf("Absolute Directory Path too long\n");
		return -ENOENT;
	}

	res = handle_mkdir(pathToPass, mode);
	
	return res;
}

static int myfs_access(const char *path, int mask)
{
	//everyone has access!
	return 0;
}


static int myfs_unlink(const char *path)
{
	long clusterNum=0;
	Cluster *file=NULL;
	Cluster *parentCluster=NULL;
	Cluster *traverser=NULL;
	char *pathToPass=NULL;
	pathToPass=removeConst(path);

	if((strlen(pathToPass)+strlen(cwd))>250)
	{
		printf("Absolute Directory Path too long\n");
		return -ENOENT;
	}


	//first remove it from the parent path.
	parentCluster=getClusterAtPos(getClusterNumFromPath(stripPath(pathToPass)));
	//printf("Got parent position:%u from path |%s|\n\n\n",getClusterNumFromPath(stripPath(pathToPass)),stripPath(pathToPass));
	

	clusterNum=getClusterNumFromPath(pathToPass);
	file=getClusterAtPos(clusterNum);

	if(parentCluster->nextLevel==clusterNum)
	{
		parentCluster->nextLevel=file->sameLevelNext;
	}

	else
	{
		traverser=getClusterAtPos(parentCluster->nextLevel);

		while(traverser->sameLevelNext<numClusters)
		{
			if(traverser->sameLevelNext==clusterNum)
			{
				traverser->sameLevelNext=file->sameLevelNext;
				break;
			}
			traverser=getClusterAtPos(traverser->sameLevelNext);
			if(traverser==NULL)
			{
				printf("This should NEVER HAPPEN!!!!!!!!!!!!\n\n");
				break;
			}
		}
	}

	
	while(file->nextFileBlockNum<numClusters)
	{
		file->type='c';
		clusterNum=file->nextFileBlockNum;
		file->nextFileBlockNum=clusterNum+1;
		file=getClusterAtPos(clusterNum);
	}
	file->type='c';

	return 0;
	
}



static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	char *pathToPass=NULL;
	pathToPass=removeConst(path);

	if((strlen(pathToPass)+strlen(cwd))>250)
	{
		printf("Absolute Directory Path too long\n");
		return -ENOENT;
	}

	handle_ReadDir(pathToPass,&buf,filler);


	return 0;
}



static int myfs_rmdir(const char *path)
{
	char *pathToPass=NULL;
	pathToPass=removeConst(path);
	int retCode=-ENOENT;

	if((strlen(pathToPass)+strlen(cwd))>250)
	{
		printf("Absolute Directory Path too long\n");
		return -ENOENT;
	}

	if((retCode=handle_rmdir_errors(pathToPass))<0)
	{
		return retCode;
	}


	return handle_rmdir(pathToPass);
}


static int myfs_open(const char *path, struct fuse_file_info *fi)
{
	/*if (strcmp(path, myfs_path) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;*/

//	printf("Path:%s\n",path);

	return 0;
}

static int myfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	(void) fi;

	long clusterNum=numClusters;
	Cluster *file;
	char *result;
	char *data;
	size_t alreadyRead=0;
	int i=0;
	int empty_counter_temp=0;

	char *pathToPass=NULL;
	pathToPass=removeConst(path);

	if((strlen(pathToPass)+strlen(cwd))>250)
	{
		printf("Absolute Directory Path too long\n");
		return -ENOENT;
	}
	
	//path-file path
	//buf-Buffer to store data in
	//size-amount of data to read
	//offset-offset from the start from file at PATH

	memset(buf,0,size);
	
	clusterNum=getClusterNumFromPath(pathToPass);

	if(clusterNum>numClusters)
	{
		return -ENOENT;
	}

	//file exists...or so it seems.....
	file=getClusterAtPos(clusterNum);

	if(file==NULL)
	{
		return -ENOENT;
	}

	//File exists at Cluster too? Surprising...

	while(offset>DATASIZE)
	{
		if(file->nextFileBlockNum<numClusters)
		{
			file=getClusterAtPos(file->nextFileBlockNum);
			offset-=DATASIZE;
		}
		else
		{
			//THIS FILE DOESN'T CONTAIN THIS MUCH DATA
			return 0;
		}
	}

	//now we've reached the offset from where to start reading

	//copy upto DATASIZE
	//printf("Offset:%u,Size:%u\n",offset,size);
	if((offset+size)<DATASIZE)
	{
		//data=(char*) malloc(sizeof(char)*(DATASIZE+1-offset));
		//take offset into account
		data=(char*)(file->data+offset);
		result=strncpy(buf,data,size);
		alreadyRead=size;
	}
	else
	{
		//copy data from this fileBlock. then move on.
		data=(char*)(file->data+offset);

		//from data read bytes from offset to DATASIZE
		result=strncpy(buf,data,DATASIZE-offset);
		//buf[DATASIZE-offset]='\0';
		alreadyRead=DATASIZE-offset;
		//now lets traverse
		
		while(alreadyRead<size)
		{
			file=getClusterAtPos(file->nextFileBlockNum);
			
			if(file==NULL)
			{
				//The file is not large enough
				
				return alreadyRead;
			}
			
			if(size-alreadyRead<DATASIZE) //we only need to read one new block
			{

				result=strncat(buf,file->data,size-alreadyRead);
				
				alreadyRead+=(size-alreadyRead);
				
				break;
			}
			else
			{
				result=strncat(buf,file->data,DATASIZE);
				
				alreadyRead+=DATASIZE;
				
			}
		}
	}
	
	
	
	if(result==NULL)
	{
		return -ENOENT;
	}
	for(i=0;i<strlen(buf);i++)
	{
		if(buf[i]=='\0')
		{
			empty_counter_temp++;
		}
	}
	
	return alreadyRead;
}

static int myfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	char *pathToPass=NULL;
	pathToPass=removeConst(path);

	if((strlen(pathToPass)+strlen(cwd))>250)
	{
		printf("Absolute Directory Path too long\n");
		return -ENOENT;
	}

	//printf("Before insertNode\n\n");

	return insertNode(pathToPass,mode,'f');

}


static int myfs_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{

	//path-file path
	//buf-Buffer to read data from
	//size-amount of data to write
	//offset-offset from the start from file at PATH
	//printf("Write was called with size :%d and offset:%d\n",size,offset);
	long clusterNum=numClusters;
	Cluster *file;
	int i=offset;
	long count=0;
	int temp_block_counter=0;

	char *pathToPass=NULL;
	pathToPass=removeConst(path);

	if((strlen(pathToPass)+strlen(cwd))>250)
	{
		printf("Absolute Directory Path too long\n");
		return -ENOENT;
	}

	//printf("Need to write data of size:%u\n\n",size);

	clusterNum=getClusterNumFromPath(pathToPass);
	//printf("Going to write to cluster %u\n\n",clusterNum);
	

	if(clusterNum>numClusters)
	{
		return -ENOENT;
	}

	//file exists...or so it seems.....
	file=getClusterAtPos(clusterNum);

	//printf("Value of nextFileBlockNum:%u\n",file->nextFileBlockNum);

	if(file==NULL)
	{
		//error with clusters. exit function.
		printf("Cluster is NULL. WHAT DID YOU DO?");
		return -ENOENT;
	}

	if((size+offset)>DATASIZE-1)
	{
		while(offset>DATASIZE-1)
		{
			if(file->nextFileBlockNum<numClusters)
			{
				clusterNum=file->nextFileBlockNum;
				file=getClusterAtPos(clusterNum);
				offset-=DATASIZE;
				temp_block_counter++;
			}
			else
			{
				//printf("\n\nOffset greater than current file size? Offset:%u Total Blocks traversed:%d\n\n",offset,temp_block_counter);
				//THIS FILE DOESN'T CONTAIN THIS MUCH DATA. Is this allowed? Test edge case.
				return 0;
			}
		}

		return handleLargeFileBlock(clusterNum,buf,size,offset,count);
	}

	else
	{
		
		for(i=offset;i<(offset+size);i++)
		{

			file->data[i]=buf[count];
			
			count++;
		}
		file->data[i]='\0';	
	}

	return count;
}


static int myfs_truncate(const char *path, off_t size)
{
	char *pathToPass=NULL;
	pathToPass=removeConst(path);	
	
	return handle_truncate(pathToPass,size);
	
}


static struct fuse_operations myfs_oper = {
	.getattr	= myfs_getattr,
	.readdir	= myfs_readdir,
	.mkdir		= myfs_mkdir,
	.access     = myfs_access,
	.rmdir		= myfs_rmdir,
	.open  		= myfs_open,
	.read 		= myfs_read,
	.mknod		= myfs_mknod,
	.write		= myfs_write,
	.truncate	= myfs_truncate,
	.unlink 	= myfs_unlink,
	.rename 	= myfs_rename,
};



int main(int argc, char *argv[])
{
	long long mem_size=0;
	char *fileName=NULL;
	char *conversionResult=NULL;
	int initialArgCount=0;

	initialArgCount=argc;

	initializeGlobal();

	if(argc<3||argc>4)
	{
		printf("Invalid input. Please enter input in either of the following formats:\n");
		printf("ramdisk <mount_point> <size>\n");
		printf("ramdisk <mount_point> <size> [<filename>]\n");
		return 0;
	}


	if (getcwd(cwd, sizeof(cwd)) == NULL)
   	{
   		printf("Failed to get Current Working Directory\n");
   		return 0;
   	}

   	conversionResult=(char*) (sizeof(char)*strlen(argv[2]));

	mem_size=strtol(argv[2], &conversionResult, 10);

	if(conversionResult==argv[2])
	{
		printf("Please enter a valid size\n");
		return 0;
	}

	mem_size=mem_size*(1024*1024);

	numClusters=(mem_size)/sizeof(Cluster);

	head=(Cluster*) malloc(numClusters*sizeof(Cluster));

	if(NULL==head)
	{
		printf("Either there is not enough memory or the size given as input was 0. Please restart the program with another size\n");
		return 0;
	}

	initializeMem();


	if(argc==4)
	{
		fileName=(char*) malloc(sizeof(char)*strlen(argv[3]));
		strcpy(fileName,argv[3]);
		argc--;

		readFromDisk(fileName);
	}
	argc--;

	fuse_main(argc, argv, &myfs_oper, NULL);

	if(initialArgCount==4)
	{
		writeToDisk(fileName);
	}

	return 0;
}
