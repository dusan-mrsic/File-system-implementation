#pragma once
#include "file.h"
#include "KernelFS.h"
#include <iostream>


class KernelFile {
public:	
	KernelFile(char* name, int* location, char mode);
	~KernelFile();
	char write(BytesCnt, char* buffer);
	BytesCnt read(BytesCnt, char* buffer);
	char seek(BytesCnt);
	BytesCnt filePos();
	char eof();
	BytesCnt getFileSize();
	char truncate();
	int size;
private:

	char mode;
	int changeClsuters();
	int allocateNewClusters(int* first, int* second);

	int currentPosition[4]; // 0 - first level; 1 - second level; 2 - data cluster; 3 - byte in data cluster;
	int first;
	int second;
	int currPos;
	char* name;
	int* Location;
	int position;

	char dataBufferWrite[2048];
	bool dataBufferWriteIn = false;

	bool seekW = false;
	bool seekR = false;
	

	int dataClussIndex;
	int secondClussIndex;
};
