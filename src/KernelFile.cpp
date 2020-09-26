#include "KernelFile.h"
#include "KernelFS.h"
#include "file.h"


class KernelFS;

KernelFile::KernelFile(char* name, int* location, char mode) {
	position = 0;
	this->mode = mode;
	this->name = name;
	this->Location = new int[2];
	this->Location = location;
	this->Location = KernelFS::findFile(name);
	char* buffer5 = new char[2048];
	KernelFS::myPartition->readCluster(Location[0], buffer5);
	size = (buffer5[3 + Location[1] + 16] << 24) | ((buffer5[2 + Location[1] + 16] & 0xFF) << 16) | ((buffer5[1 + Location[1] + 16] & 0xFF) << 8) | (buffer5[0 + Location[1] + 16] & 0xFF);
	first = (buffer5[3 + Location[1] + 22] << 24) | ((buffer5[2 + Location[1] + 22] & 0xFF) << 16) | ((buffer5[1 + Location[1] + 22] & 0xFF) << 8) | (buffer5[0 + Location[1] + 22] & 0xFF);
	second = (buffer5[3 + Location[1] + 26] << 24) | ((buffer5[2 + Location[1] + 26] & 0xFF) << 16) | ((buffer5[1 + Location[1] + 26] & 0xFF) << 8) | (buffer5[0 + Location[1] + 26] & 0xFF);

	delete buffer5;
}
KernelFile::~KernelFile() {

	char* buffer5= new char[2048];
	KernelFS::myPartition->readCluster(Location[0], buffer5);
	buffer5[Location[1] + 20] = 1;
	buffer5 = KernelFS::oneToFourBytes(buffer5, Location[1], 16, size);
	buffer5 = KernelFS::oneToFourBytes(buffer5, Location[1], 22, first);
	buffer5 = KernelFS::oneToFourBytes(buffer5, Location[1], 26, second);
	KernelFS::myPartition->writeCluster(Location[0], buffer5);
	KernelFS::numOfOpenedFiles--;
	KernelFS::signalSem();
	delete buffer5;

}
char KernelFile::write(BytesCnt cnt, char* bufferInput) {

	if (size == 512 * 512 * 2048) return 0;
	int inputPosition = 0;
	char* buffer= new char[2048];
	KernelFS::myPartition->readCluster(Location[0], buffer);
	int fileClusterFirstLevel = (buffer[3 + Location[1] + 12] << 24) | ((buffer[2 + Location[1] + 12] & 0xFF) << 16) | ((buffer[1 + Location[1] + 12] & 0xFF) << 8) | (buffer[0 + Location[1] + 12] & 0xFF);
	if(fileClusterFirstLevel == -1) { // FILE IS EMPTY - ALLOCATE FIRST LEVEL
		KernelFile::size = cnt;


		int freeCluster = KernelFS::findFreeCluster();
		KernelFS::takeCluster(freeCluster);
		currentPosition[0] = freeCluster;
		
		buffer = KernelFS::oneToFourBytes(buffer, Location[1], 12, freeCluster);		// ZAUZET KLASTER ZA PRVI NIVO
		KernelFS::myPartition->writeCluster(Location[0], buffer);
		char* bufferFirstLevel = new char[2048];
		memset(bufferFirstLevel, -1, 2048 * sizeof(char));		
		int numOfNeededClusters = 1 + ((cnt - 1) / 2048); 
		int i = 0;
		while (numOfNeededClusters > 0) {
			int secondLevelCluster = KernelFS::findFreeCluster();
			KernelFS::takeCluster(secondLevelCluster);
			currentPosition[1] = secondLevelCluster;
			bufferFirstLevel = KernelFS::oneToFourBytes(bufferFirstLevel, i, 0, secondLevelCluster);		// ZAUZET KLASTER ZA DRUGI NIVO
			char* secondLevelBuffer = new char[2048];
			memset(secondLevelBuffer, -1, 2048 * sizeof(char));
			int j = 0;
			while (numOfNeededClusters > 0 && j < 2048) {
				char* dataClusterBuffer = new char[2048];
				memset(dataClusterBuffer, -1, 2048 * sizeof(char));
				int dataCluster = KernelFS::findFreeCluster();
				KernelFS::takeCluster(dataCluster);
				currentPosition[2] = dataCluster;
				secondLevelBuffer = KernelFS::oneToFourBytes(secondLevelBuffer, j, 0, dataCluster);		// ZAUZET KLASTER ZA PODATKE
				int k = 0;
				while (k < 2048 && inputPosition < cnt) {
					currPos = k;
					dataClusterBuffer[k++] = bufferInput[inputPosition++];
					currentPosition[3] = k % 2048;

				} 
				KernelFS::myPartition->writeCluster(dataCluster, dataClusterBuffer);
				numOfNeededClusters--;
				j = j + 4;
				second = j % 2048;
				delete dataClusterBuffer;
			}
			KernelFS::myPartition->writeCluster(secondLevelCluster, secondLevelBuffer);
			i += 4;
			first = i;
			delete secondLevelBuffer;
		}
		
		KernelFS::myPartition->writeCluster(freeCluster, bufferFirstLevel);
		if (currentPosition[3] == 0) allocateNewClusters(&first, &second);

		position = cnt;
		delete buffer;
		return 1;
	}
	else {
		if (!seekW) {
			seekW = true;
			seek(position);
		}
		

		//******
		if (!dataBufferWriteIn) {
			KernelFS::myPartition->readCluster(currentPosition[2], dataBufferWrite);
			dataBufferWriteIn = true;
		}
		int numOfNeededClusters = 1 + ((cnt - 1) / 2048);

		char* firstBuffer = new char[2048];
		char* secondBuffer = new char[2048];
		char* dataBuffer = new char[2048];

		while (numOfNeededClusters > 0) {
			int firstCluster = currentPosition[0];
			int secondCluster = currentPosition[1];
			int dataCluster = currentPosition[2];
			int byte = currentPosition[3];

			//KernelFS::myPartition->readCluster(firstCluster, firstBuffer);
			//KernelFS::myPartition->readCluster(secondCluster, secondBuffer);
			//KernelFS::myPartition->readCluster(dataCluster, dataBuffer);

			while (byte < 2048 && inputPosition < cnt) {
				dataBufferWrite[byte++] = bufferInput[inputPosition++];
			}
			KernelFS::myPartition->writeCluster(dataCluster, dataBufferWrite);
			byte = byte % 2048;
			currentPosition[3] = byte;
			if (currentPosition[3] == 0) {
				allocateNewClusters(&first, &second);
				KernelFS::myPartition->readCluster(currentPosition[2], dataBufferWrite);
			}
			numOfNeededClusters--;
		}
		KernelFile::size = KernelFile::size + cnt;

		delete firstBuffer;
		delete dataBuffer;
		delete secondBuffer;
		delete buffer;
		position +=cnt;
	}

	return 1;
}
BytesCnt KernelFile::read(BytesCnt cnt, char* buffer) {


	if (!seekR) {
		seek(position);
		seekW = true;
	}
	int firstCluster = currentPosition[0];
	int secondCluster = currentPosition[1];
	int dataCluster = currentPosition[2];
	int byte = currentPosition[3];  // slobodan ulaz      **** pozicija unutar data clustera ***** 
	if (this->eof()) return 0;
	int readedChars = 0;

	char* firstBuffer = new char[2048];
	char* secondBuffer = new char[2048];
	char* dataBuffer = new char[2048];
	KernelFS::myPartition->readCluster(firstCluster, firstBuffer);
	KernelFS::myPartition->readCluster(secondCluster, secondBuffer);
	KernelFS::myPartition->readCluster(dataCluster, dataBuffer);

	int takenClusters;


	//CURRENT POS
	takenClusters = position / 2048 + 1;
	dataClussIndex = (takenClusters - 1) % 512;
	secondClussIndex = (takenClusters - 1) / 512;
	char c;
	if (byte != 0) {
		while (byte < 2048 && readedChars < cnt && !eof()) {
			buffer[readedChars++] = dataBuffer[byte++];
		}
		byte = byte % 2048;
		currentPosition[3] = byte; // OK 
		if (!byte) changeClsuters();
	}
	//KernelFS::myPartition->readCluster(currentPosition[1], secondBuffer);
	//KernelFS::myPartition->readCluster(currentPosition[2], dataBuffer);
	while ((readedChars < cnt) && !eof()) {
		int index1 = (firstBuffer[3 + 4*secondClussIndex] << 24) | ((firstBuffer[2 + 4*secondClussIndex] & 0xFF) << 16) | ((firstBuffer[1 + 4*secondClussIndex] & 0xFF) << 8) | (firstBuffer[0 + 4*secondClussIndex] & 0xFF);
		KernelFS::myPartition->readCluster(index1, secondBuffer);
		while (readedChars < cnt && dataClussIndex < 512 && !eof()) {
			int index2 = (secondBuffer[3 + 4*dataClussIndex] << 24) | ((secondBuffer[2 + 4*dataClussIndex] & 0xFF) << 16) | ((secondBuffer[1 + 4*dataClussIndex] & 0xFF) << 8) | (secondBuffer[0 + 4*dataClussIndex] & 0xFF);
			KernelFS::myPartition->readCluster(index2, dataBuffer);
			while (byte < 2048 && readedChars < cnt) {
				buffer[readedChars++] = dataBuffer[byte++];
			}
			byte = byte % 2048;
			currentPosition[3] = byte;
			if (!byte) { 
				changeClsuters();
				KernelFS::myPartition->readCluster(currentPosition[1], secondBuffer);
				KernelFS::myPartition->readCluster(currentPosition[2], dataBuffer);
			}
		}
	}
	delete firstBuffer;
	delete secondBuffer;
	delete dataBuffer;
	position += readedChars;
	return readedChars;
}
char KernelFile::seek(BytesCnt cnt) { // *** PROMENI FIRST I SECOND *****
	position = cnt;
	int takenClusters = position / 2048 + 1;
	currentPosition[3] = position % 2048;
	int dataClussInd = (takenClusters - 1) % 512 ;
	int secondClussInd = (takenClusters - 1) / 512;
	char buffer[2048];
	char* dataLevelBuffer = new char[2048];
	char* secondLevelBuffer = new char[2048];


	KernelFS::myPartition->readCluster(Location[0], buffer);
	int fileClusterFirstLevel = (buffer[3 + Location[1] + 12] << 24) | ((buffer[2 + Location[1] + 12] & 0xFF) << 16) | ((buffer[1 + Location[1] + 12] & 0xFF) << 8) | (buffer[0 + Location[1] + 12] & 0xFF);

	currentPosition[0] = fileClusterFirstLevel;

	KernelFS::myPartition->readCluster(currentPosition[0], buffer);
	int index1 = (buffer[3 + 4*secondClussInd] << 24) | ((buffer[2 + 4*secondClussInd] & 0xFF) << 16) | ((buffer[1 + 4*secondClussInd] & 0xFF) << 8) | (buffer[0 + 4*secondClussInd] & 0xFF);
	currentPosition[1] = index1;
	KernelFS::myPartition->readCluster(index1, secondLevelBuffer);
	int index2 = (secondLevelBuffer[3 + 4*dataClussInd] << 24) | ((secondLevelBuffer[2 + 4 * dataClussInd] & 0xFF) << 16) | ((secondLevelBuffer[1 + 4 * dataClussInd] & 0xFF) << 8) | (secondLevelBuffer[0 + 4 * dataClussInd] & 0xFF);
	currentPosition[2] = index2;
	delete dataLevelBuffer;
	delete secondLevelBuffer;
	return 1;
}
BytesCnt KernelFile::filePos() {
	return position;
}
char KernelFile::eof() {
	if(position == KernelFile::size) return 2;
	else return 0;
	return 1;
}
BytesCnt KernelFile::getFileSize() {
	return size;
}
char KernelFile::truncate() {
	return 1;
}

int KernelFile::changeClsuters() {

	char* buffer = new char[2048];
	char* buffer1 = new char[2048];

	dataClussIndex = (dataClussIndex + 1) % 512;
	if (dataClussIndex == 0) {
		secondClussIndex++;
		KernelFS::myPartition->readCluster(currentPosition[0], buffer);
		int index1 = (buffer[3 + 4*secondClussIndex] << 24) | ((buffer[2 + 4*secondClussIndex] & 0xFF) << 16) | ((buffer[1 + 4*secondClussIndex] & 0xFF) << 8) | (buffer[0 + 4*secondClussIndex] & 0xFF);
		currentPosition[1] = index1;
		KernelFS::myPartition->readCluster(index1, buffer1);
		int index2 = (buffer1[3] << 24) | ((buffer1[2] & 0xFF) << 16) | ((buffer1[1] & 0xFF) << 8) | (buffer1[0] & 0xFF);
		currentPosition[2] = index2;
		delete buffer;
		delete buffer1;
		return 1;
	}
	else {
		KernelFS::myPartition->readCluster(currentPosition[1], buffer1);
		int index3 = (buffer1[3 + 4 * dataClussIndex] << 24) | ((buffer1[2 + 4 * dataClussIndex] & 0xFF) << 16) | ((buffer1[1 + 4 * dataClussIndex] & 0xFF) << 8) | (buffer1[0 + 4 * dataClussIndex] & 0xFF);
		currentPosition[2] = index3;
	}
	delete buffer;
	delete buffer1;
	return 1;
}

int KernelFile::allocateNewClusters(int* first, int* second) {
	//seek(position);
	char* firstLevel = new char[2048];
	char* secondLevel = new char[2048];
	char* dataBuffer = new char[2048];
	
	//printf("\n primio: %d  %d \n", *second, *first);


	if (*second) {
		int dataCluster = KernelFS::findFreeCluster();
		KernelFS::takeCluster(dataCluster);
		//printf("\n %d %d  %d \n", dataCluster, *second, *first);
		KernelFS::myPartition->readCluster(currentPosition[1], secondLevel);
		secondLevel = KernelFS::oneToFourBytes(secondLevel,*second, 0, dataCluster);
		KernelFS::myPartition->writeCluster(currentPosition[1], secondLevel);
		currentPosition[2] = dataCluster;
		*second = (*second + 4) % 2048;
		//printf("\n %d \n", *second);
	}
	else {
		int secondCluster = KernelFS::findFreeCluster();
		KernelFS::takeCluster(secondCluster);
		//printf("\n %d %d %d \n", secondCluster, *second, *first);
		KernelFS::myPartition->readCluster(currentPosition[0], firstLevel);
		firstLevel = KernelFS::oneToFourBytes(firstLevel, *first, 0, secondCluster);
		KernelFS::myPartition->writeCluster(currentPosition[0], firstLevel);
		currentPosition[1] = secondCluster;
		int dataCluster = KernelFS::findFreeCluster();
		KernelFS::takeCluster(dataCluster);
		//printf("\n %d %d %d \n", dataCluster, *second, *first);
		KernelFS::myPartition->readCluster(currentPosition[1], secondLevel);
		secondLevel = KernelFS::oneToFourBytes(secondLevel, *second, 0, dataCluster);
		KernelFS::myPartition->writeCluster(currentPosition[1], secondLevel);
		currentPosition[2] = dataCluster;
		*first = *first + 4;
		*second = (*second + 4) % 2048;
		//printf("\n %d \n", *second);
	}

	delete firstLevel;
	delete secondLevel;
	delete dataBuffer;

	return 1;
}