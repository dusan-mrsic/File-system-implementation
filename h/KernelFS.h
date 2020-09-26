#pragma once
#include <iostream>
#include "part.h"
#include "fs.h"
#include <Windows.h>
#define signal(x) ReleaseSemaphore(x,1,NULL)
#define wait(x) WaitForSingleObject(x,INFINITE)


typedef long FileCnt;
typedef unsigned long BytesCnt; 
class Partition;
class File;
class KernelFile;

class KernelFS {
public:
	KernelFS();

	static char mount(Partition* partition);

	static char unmount();

	static char format();

	static FileCnt readRootDir();

	static char doesExist(char* fname);

	static File* open(char* fname, char mode);

	static char deleteFile(char* fname);

	static int isOpened(char* fname);

	static int size(char* fname);
protected:
	static void signalSem();
	static int* findFile(char* fname);
	static int findFreeCluster();
	static void takeCluster(int cluster);
	static char* writeFileToCluster(char *fname);
	static void freeCluster(int num);
	
	static char* oneToFourBytes(char* buffer, int location, int position, int value);

private:
	friend class KernelFile;

	static Partition* myPartition;

	static int numOfOpenedFiles;
	static int waitToMount;
	static int waitToFormat;
	
	
	static FileCnt numOfFiles;

	int openForRead = 1 << 160;
	int openForWrite = 1 << 161;
	int openForAll = 1 << 162;


};
