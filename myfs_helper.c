#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fuse.h>
#include "myfs.h"

void initializeGlobal(void)
{
	head=NULL;
	lastEmptySpace=0;
	numClusters=0;
	strcpy(root,"/");
}


int handle_truncate(char *path,int size)
{
	Cluster *fileBlock=NULL;
	Cluster *finalFileBlock=NULL;
	long blockIndex=numClusters+1;
	int numBlocks=0;
	int currentBlock=0;

	blockIndex=getClusterNumFromPath(path);
	fileBlock=getClusterAtPos(blockIndex);

	numBlocks=size/(DATASIZE-1);
	
	while(currentBlock<numBlocks)
	{
		fileBlock=getClusterAtPos(fileBlock->nextFileBlockNum);
		if(fileBlock==NULL)
			return 0;

		currentBlock++;
	}

	fileBlock->data[size%(DATASIZE-1)]='\0';
	finalFileBlock=fileBlock;
	fileBlock=getClusterAtPos(fileBlock->nextFileBlockNum);
	finalFileBlock->nextFileBlockNum=numClusters+1;
	while(NULL!=fileBlock)
	{
		fileBlock->type='c';
		fileBlock=getClusterAtPos(fileBlock->nextFileBlockNum);
	}

	return 0;

}


int handle_rename(char *srcPath,char *destPath)
{
	long srcParentPos=numClusters+1;
	long srcPos=numClusters+1;
	long destParentPos=numClusters+1;
	Cluster *srcParent=NULL;
	Cluster *srcCluster=NULL;
	Cluster *traverser=NULL;
	Cluster *previous=NULL;
	Cluster *destParent=NULL;

	//find parent of srcPath
	srcParentPos=getClusterNumFromPath(stripPath(srcPath));
	srcParent=getClusterAtPos(srcParentPos);
	//traverse through parent till you get ref to srcpath
	//change ref to srcPath->sameLevelNext
	traverser=getClusterAtPos(srcParent->nextLevel);
	srcPos=srcParent->nextLevel;
	if(strcmp(traverser->name,stripName(srcPath))==0)
	{
		srcCluster=traverser;
		
		srcParent->nextLevel=srcCluster->sameLevelNext;
	}
	else
	{
		previous=traverser;
		traverser=getClusterAtPos(previous->sameLevelNext);
		while(traverser!=NULL)
		{
			if(strcmp(traverser->name,stripName(srcPath))==0)
			{
				srcCluster=traverser;
				srcPos=previous->sameLevelNext;
				previous->sameLevelNext=traverser->sameLevelNext;
				break;
			}

			previous=traverser;
			traverser=getClusterAtPos(previous->sameLevelNext);
		}

		if(srcPos==srcParent->nextLevel||srcPos>numClusters)
		{
			//THIS SHOULD NEVER HAPPEN
			return -ENOENT;
		}
	}
	
	//find destPath parent
	destParentPos=getClusterNumFromPath(stripPath(destPath));
	destParent=getClusterAtPos(destParentPos);
	
	srcCluster->sameLevelNext=destParent->nextLevel;
	
	destParent->nextLevel=srcPos;
	strcpy(srcCluster->name,stripName(destPath));

	return 0;
}

int handle_rmdir_errors(char *path)
{
	long position=numClusters+1;
	Cluster *node=NULL;

	position=getClusterNumFromPath(path);

	node=getClusterAtPos(position);

	if(NULL==node)
	{
		return -ENOTDIR;
	}

	if(node->nextLevel<numClusters)
	{
		return -ENOTEMPTY;
	}

	return 0;

}


char *removeConst(const char *path)
{
	char *pathValue=NULL;

	pathValue=(char*)malloc(sizeof(char)*(strlen(path)+1));

	if(NULL!=path)
	{
		strcpy(pathValue,path);
	}
	
	if(NULL!=pathValue)
		return pathValue;

	return NULL;
}

void initializeMem(void)
{
	Cluster* traverser;
	long counter;

	strcpy(head->name,"/");
	head->type='d';
	head->sameLevelNext=numClusters+1;
	head->nextLevel=numClusters+1;
	head->nextFileBlockNum=numClusters+1;
	strcpy(head->data,"");

	for(counter=1;counter<numClusters;counter++)
	{
		traverser=getClusterAtPos(counter);
		traverser->type='c';
		traverser->sameLevelNext=numClusters+1;
		traverser->nextLevel=numClusters+1;
		traverser->nextFileBlockNum=numClusters+1;
		strcpy(traverser->name,"");
		strcpy(traverser->data,"");
	}
}


//gets the directory. Eg for path :/a/b/c will return /a/b
char *stripPath(char *path)
{
	
	char *parentPath=NULL;

	char *onlyFile=NULL;

	int size=0;

	onlyFile=(char*) malloc(sizeof(char)*255);

	//find last occurence of /
	onlyFile=strrchr(path,'/');
	
	if(onlyFile==NULL) //invalid path given
	{
		return NULL;
	}
	
	//Get the size of parentPath
	size=strlen(path)-strlen(onlyFile);
	
	if(size==0)//The parent is root!
	{
		return root;
	}

	parentPath=(char*) malloc((size+1)*sizeof(char));

	strncpy(parentPath,path,size);
	
	parentPath[size]='\0';
	
	return parentPath;
	
}


char *stripName(char *path)
{
	char *fileName = strrchr(path, '/');

	if(NULL!=fileName)
	{
		if(fileName[0]!='\0')
		{
			return (char*)(fileName+1);
		}
	}
	
	return fileName;
}



int allowedPathName(char *path)
{
	if(NULL!=path)
		return 1;

	return 0;
}


Cluster* getClusterAtPos(long position)
{

	if(position<numClusters)
		return (Cluster*)(head+position);

	return NULL;
}


long getClusterNumFromPath(char *og_path)
{
	Cluster *traverser;
	char *subdirName;
	long clusterNum=0;
	long temp_clusterNum=0;

	subdirName=(char*) malloc(sizeof(char)*256);
	if(NULL==subdirName)
	{
		return numClusters+1;
	}
	char *path=(char*) malloc(sizeof(char)*strlen(og_path));
	if(NULL==path)
	{
		return numClusters+1;
	}
	strcpy(path,og_path);
	
	if(strcmp(path,"/")==0)
		return 0;

	
	traverser=(Cluster*)getClusterAtPos(head->nextLevel);

	if(traverser==NULL)
	{
		return numClusters+1;
	}

	subdirName=strtok(path,"/");
	temp_clusterNum=head->nextLevel;
	do
	{
		
		while(NULL!=traverser)
		{
			
			clusterNum=temp_clusterNum;
			if(strcmp(traverser->name,subdirName)==0)
			{
				temp_clusterNum=traverser->nextLevel;
				
				traverser=getClusterAtPos(traverser->nextLevel);

				break;
			}
			else
			{
				
				temp_clusterNum=traverser->sameLevelNext;
				//printf("Not equal...now going to cluster number %u\n",temp_clusterNum);
				traverser=getClusterAtPos(traverser->sameLevelNext);

				if(traverser==NULL)
				{
					return (numClusters+1);
				}
				
			}

			
		}
		

	}while((NULL!=(subdirName=strtok(NULL,"/")))&&(NULL!=traverser));

	if(NULL!=subdirName)
	{
		
		return (numClusters+1);
	}

	
	return clusterNum;

}


long getNextEmpty(void)
{
	long nextEmptySpace;
	Cluster *traverser=NULL;

	nextEmptySpace=lastEmptySpace;

	traverser=getClusterAtPos(nextEmptySpace);

	while(NULL!=traverser)
	{
		if(traverser->type=='c')
			break;

		nextEmptySpace++;
		nextEmptySpace=nextEmptySpace%(numClusters-1);

		if(nextEmptySpace==lastEmptySpace)
			return numClusters+1;

		traverser=getClusterAtPos(nextEmptySpace);

	}
	lastEmptySpace=nextEmptySpace;


	
	return nextEmptySpace;
}


size_t getSizeOfFile(long clusterNum)
{
	size_t size=0;
	Cluster *fileBlock=NULL;

	fileBlock=getClusterAtPos(clusterNum);

	size+=strlen(fileBlock->data);

	while(fileBlock->nextFileBlockNum<numClusters)
	{
		fileBlock=getClusterAtPos(fileBlock->nextFileBlockNum);

		size+=strlen(fileBlock->data);
	}
	return size;

}

//last parameter is the count of chars from buf already written to my'file'
long handleLargeFileBlock(long clusterNum,const char *buf, size_t size,off_t offset,long buf_pos_written)
{
	Cluster *fileBlock=NULL;
	Cluster *nextFileBlock=NULL;
	size_t i;
	long nextClusterNum;

	if(size<=buf_pos_written)
	{
		fileBlock->nextFileBlockNum=numClusters+1;

		return buf_pos_written;
	}
	
	fileBlock=getClusterAtPos(clusterNum);

	for(i=offset;i<DATASIZE-1;i++)
	{
		fileBlock->data[i]=buf[buf_pos_written];
		buf_pos_written++;
		if(size<=buf_pos_written)
			break;
	}
	fileBlock->data[i]='\0';
	
	if(size>buf_pos_written)
	{
		nextClusterNum=getNextEmpty();

		if(nextClusterNum>numClusters)
		{
			return -ENOMEM;
		}

		fileBlock->nextFileBlockNum=nextClusterNum;
		nextFileBlock=getClusterAtPos(nextClusterNum);
		nextFileBlock->type='n';
		nextFileBlock->nextLevel=numClusters+1;
		nextFileBlock->sameLevelNext=numClusters+1;
		nextFileBlock->nextFileBlockNum=numClusters+1;

		buf_pos_written=handleLargeFileBlock(nextClusterNum,buf,size,0,buf_pos_written);
	}

	return buf_pos_written;

}


int insertNode(char *path,mode_t mode,char ClusterType)
{
	
	Cluster *dirToInsertIn;
	Cluster *traverser;
	Cluster *newNode;
	long positionToInsertIn=0;
	long parentPosition=0;
	
	parentPosition=getClusterNumFromPath(stripPath(path));


	if(parentPosition>numClusters)
	{
		return -ENOTDIR;
	}

	
	positionToInsertIn=getNextEmpty();

	if(positionToInsertIn>numClusters)
	{
			return -ENOMEM;
	}

	newNode=getClusterAtPos(positionToInsertIn);

	
	if(allowedPathName(path)==1)//returns 1 if allowed
	{
		strcpy(newNode->name,stripName(path));
	}
	else
	{
		return -ENOENT;
	}

	newNode->mode=mode;

	newNode->type=ClusterType;

	newNode->nextLevel=numClusters+1;
	newNode->sameLevelNext=numClusters+1;
	newNode->nextFileBlockNum=numClusters+1;

	dirToInsertIn=getClusterAtPos(parentPosition);

	if(dirToInsertIn==NULL)
	{
		return -ENOTDIR;
	}	

	if(dirToInsertIn->nextLevel==(numClusters+1))
	{
		dirToInsertIn->nextLevel=positionToInsertIn;
		
		return 0;
	}

	//need to traverse the children of the parent
	traverser=getClusterAtPos(dirToInsertIn->nextLevel);

	while(traverser->sameLevelNext<numClusters)
	{
		traverser=getClusterAtPos(traverser->sameLevelNext);
	}

	traverser->sameLevelNext=positionToInsertIn;
	

	return 0;
}



int handle_mkdir(char *path, mode_t mode)
{

	return insertNode(path,mode,'d');
}


Cluster* getSubDirHead(char *path)
{
	Cluster *traverser;

	traverser=getClusterAtPos(getClusterNumFromPath(path));

	if(traverser!=NULL)
	{
		traverser=getClusterAtPos(traverser->nextLevel);
		
	}	

	return traverser;
}


void handle_ReadDir(char *path, void **buf,fuse_fill_dir_t filler)
{

	Cluster *traverser=NULL;

	traverser=getSubDirHead(path);

	while(traverser!=NULL)
	{
		
		filler(*buf,traverser->name,NULL,0);

		traverser=getClusterAtPos(traverser->sameLevelNext);
	}


}


int handle_rmdir(char *path)
{
	long parentPosition=0;
	Cluster *traverser=NULL;
	Cluster *next=NULL;
	Cluster *parent=NULL;
	long tempPos=0;
	
	/*if(!isEmpty(path))
	{
		return -ENOTEMPTY;
	}*/

	parentPosition=getClusterNumFromPath(stripPath(path));

	parent=getClusterAtPos(parentPosition);

	//printf("Parent:%s\n",parent->name);

	traverser=getClusterAtPos(parent->nextLevel);

	//if the first cluster of the parent's next level is the one we want to delete
	if(strcmp(traverser->name,stripName(path))==0)
	{
		//if there are more clusters in the next level, this will not be null. else it will be. either way its cool
		tempPos=traverser->sameLevelNext;

		traverser->type='c';
		traverser->sameLevelNext=numClusters+1;
		traverser->nextLevel=numClusters+1;
		strcpy(traverser->name,"");
		strcpy(traverser->data,"");

		//there was only a single child
		if(tempPos>numClusters)
		{
			parent->nextLevel=numClusters+1;
		}
		else
		{
			parent->nextLevel=tempPos;
		}

		return 0;
	}
	else
	{
		next=getClusterAtPos(traverser->sameLevelNext);
		while(next!=NULL)
		{
			if(strcmp(next->name,stripName(path))==0)
			{
				//found it!
				traverser->sameLevelNext=next->sameLevelNext;

				next->type='c';
				next->sameLevelNext=numClusters+1;
				next->nextLevel=numClusters+1;
				next->nextFileBlockNum=numClusters+1;
				strcpy(next->name,"");
				strcpy(next->data,"");
				break;
			}

			traverser=next;
			next=getClusterAtPos(traverser->sameLevelNext);
		}
		//this should never happen since fuse already checks if directory exists before rmdir
		if(next==NULL)
		{
			return -ENOENT;
		}
		return 0;
	}
	
}


void writeToDisk(char *fileName)
{

	FILE * file= fopen(fileName, "wb");

	if (file != NULL) {

		fwrite(head, sizeof(Cluster), numClusters, file);
    	fclose(file);
    	return;
	}

		//printf("Could not open file to write%s\n",fileName);
	
}


void readFromDisk(char *fileName)
{

	FILE * file= fopen(fileName, "rb");

	if (file != NULL) {

		fread(head, sizeof(Cluster), numClusters, file);
    	fclose(file);
    	return;
	}

	//printf("Could not open file to read%s\n",fileName);

}

