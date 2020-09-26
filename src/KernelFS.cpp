#include "fs.h"
#include "KernelFS.h"
#include "file.h"
#include "part.h"

#include <iostream>
#include <Windows.h>			/*DODAJ SVUDA GRESKU AKO JE NULL PARTICIJA*/

using namespace std;

// 21. bajt je za bitove rwa
// 22. bajt da li fajl postoji, ako je upisano 'E'

class File;

HANDLE mutexMount = CreateSemaphore(NULL, 1, 32, NULL);
HANDLE mutexUnmount = CreateSemaphore(NULL, 1, 32, NULL);
HANDLE mutexFormat = CreateSemaphore(NULL, 1, 32, NULL);
HANDLE mutexOpen = CreateSemaphore(NULL, 0, 32, NULL);

KernelFS::KernelFS() {}

Partition* KernelFS::myPartition;
int KernelFS::waitToFormat;
int KernelFS::waitToMount;
int KernelFS::numOfOpenedFiles;
FileCnt KernelFS::numOfFiles;

char KernelFS::mount(Partition* partition) {
	if (partition == NULL) return 0;
	if (myPartition != NULL) {					// partition is already mounted
		waitToMount++;
		wait(mutexMount);
		waitToMount--;
	}
	myPartition = partition;

	return 1;
}

char KernelFS::unmount() {
	if (myPartition == NULL) return 0;
	if (numOfOpenedFiles != 0) wait(mutexUnmount); //need to wait all files to be closed
	myPartition = NULL;
	if (waitToMount) signal(mutexMount);
	return 1;
}

char KernelFS::format() {
	if (numOfOpenedFiles) {							// if !=0 wait until all files are closed
		if (waitToFormat == 1) return 0;			// alredy one waits to Format partition
		waitToFormat++;
		wait(mutexFormat);
		waitToFormat--;
	}

	char buffer[2048];								// initialize bitVector
	memset(buffer, 255, 2048 * sizeof(char));
	buffer[0] = 252;
	myPartition->writeCluster(0, buffer);

	memset(buffer, -1, 2048 * sizeof(char));		//initialize Root Directory
	myPartition->writeCluster(1, buffer);
	myPartition->writeCluster(2, buffer);

	return 1;
}

FileCnt KernelFS::readRootDir() {
	if (myPartition == NULL) return -1;

	return numOfFiles;
}

char KernelFS::doesExist(char* fname) {


	int* res = new int[2]; 
	res = findFile(fname);
	if (res[0] == -2) return 0;
	return 1;

}

File* KernelFS::open(char* fname, char mode) {
	int* Location = new int[2];
	Location = findFile(fname);
	if (mode == 'r') {
		if (!doesExist(fname)) return 0;
		char mask = 1;
		char buffer[2048];
		myPartition->readCluster(Location[0], buffer);
		char RWAbits = buffer[Location[1] + 20];
		if ((RWAbits & (mask << 1)) != 0) wait(mutexOpen); // CEKAJ JER NEKO DRUGI OTVORIO ZA WRITE *****
		else if ((RWAbits & (mask << 2)) != 0) wait(mutexOpen); // CEKAJ JER NEKO OVTORIO ZA A *****
		if ((RWAbits & mask) == 0) {
			RWAbits |= 1;
			myPartition->readCluster(Location[0], buffer);
			buffer[Location[1] + 20] = 1;
			myPartition->writeCluster(Location[0], buffer);
			numOfOpenedFiles++;
		}
		return new File(fname, Location, mode);
	}
	if (mode == 'w') {
		if (doesExist(fname)) {
			char mask = 1;
			int *Location = new int[2];
			Location = findFile(fname);
			char buffer[2048];
			myPartition->readCluster(Location[0], buffer);
			char RWAbits = buffer[Location[1] + 20];
			if ((RWAbits & (mask << 1)) != 0) {
				wait(mutexOpen);
			}// CEKAJ JER NEKO DRUGI OTVORIO ZA WRITE *****
			else if ((RWAbits &(mask << 2)) != 0) wait(mutexOpen); // CEKAJ JER NEKO OVTORIO ZA A *****
			else if ((RWAbits & mask) != 0) wait(mutexOpen); // CEKAJ JER JE NEKO OTVORIO FAJL ZA READ *****
			RWAbits |= 2;
			buffer[Location[1] + 20] = RWAbits;

			if (!deleteFile(fname)) return NULL;

			buffer[Location[1] + 12] = -1;
			buffer[Location[1] + 13] = -1;
			buffer[Location[1] + 14] = -1;
			buffer[Location[1] + 15] = -1;

			myPartition->writeCluster(Location[0], buffer);
			numOfOpenedFiles++;
			return new File(fname, Location, mode);
		}
		else {
			char* bufferRoot = new char[2048];
			int rootIndex = numOfFiles / 65; // U KOM JE ULAZU U ROOT
			myPartition->readCluster(1, bufferRoot);
			int index = (bufferRoot[3 + rootIndex] << 24) | ((bufferRoot[2 + rootIndex] & 0xFF) << 16) | ((bufferRoot[1 + rootIndex] & 0xFF) << 8) | (bufferRoot[0 + rootIndex] & 0xFF);
			numOfFiles++;
			if (index == -1) {
				int freeCluster = findFreeCluster();
				takeCluster(freeCluster);
				char bufferForFiles[2048];
				myPartition->readCluster(freeCluster, bufferForFiles);
				char* pomBuffer = new char[32];
				pomBuffer = writeFileToCluster(fname);
				for (int i = 0; i < 32; i++) {
					bufferForFiles[i] = pomBuffer[i];
				}
				bufferForFiles[20] = 2;
				myPartition->writeCluster(freeCluster, bufferForFiles);
				bufferRoot = oneToFourBytes(bufferRoot, 0, 0, freeCluster);
				myPartition->writeCluster(1, bufferRoot);
				numOfOpenedFiles++;
				return new File(fname, Location, mode); // POKAZIVAC NA OTVORENI FAJL
			}
			else {
				char bufferForFiles[2048];
				myPartition->readCluster(index, bufferForFiles);
				int i = 21;
				int j = 1;
				int cnt = 0;
				while (bufferForFiles[i] == 'E') {
					i = j * 32 + 21;
					cnt++;
					j++;
				}
				char* pomBuffer = new char[32];
				pomBuffer = writeFileToCluster(fname);
				for (int j = 0; j < 32; j++) {
					bufferForFiles[cnt * 32 + j] = pomBuffer[j];
				}
				bufferForFiles[20] = 2;
				myPartition->writeCluster(index, bufferForFiles);
				numOfOpenedFiles++;
				return new File(fname, Location, mode); // POKAZIVA NA OTVORENI FAJL
			}


		}
	}
	if (mode == 'a') {
			if (!doesExist(fname)) return 0;
			int* Location  = new int[2];
			char mask = 1;
			Location = findFile(fname);
			char buffer[2048];
			myPartition->readCluster(Location[0], buffer);
			char RWAbits = buffer[Location[1] + 20];
			//if ((RWAbits & (mask << 1)) != 0) wait(mutexOpen); // CEKAJ JER NEKO DRUGI OTVORIO ZA WRITE *****
			 if ((RWAbits &(mask << 2)) != 0) wait(mutexOpen); // CEKAJ JER NEKO OVTORIO ZA A *****
			else if ((RWAbits & mask) != 0) wait(mutexOpen);     // CEKAJ JER JE OTVOREN FAJL ZA READ ******
			RWAbits |= (mask << 3);
			myPartition->readCluster(Location[0], buffer);
			buffer[Location[1] + 20] = 7;
			myPartition->writeCluster(Location[0], buffer);
			numOfFiles++; // POSTAVI KURSOR NA KRAJ FAJLA
			File* f = new File(fname, Location, mode);
			f->seek(f->getFileSize());
			
			return f;
		}

		return 0;
	}


char KernelFS::deleteFile(char* fname) {

	int* Location = new int[2];
	Location = findFile(fname);
	if (Location[0] == -2) return 0; // TREBA DA BUDE GRESKA I AKO JE OTOVOREN FAJL!!! *****
	if (isOpened(fname)) return 0;
	char buffer[2048];
	myPartition->readCluster(Location[0], buffer);
	buffer[Location[1] + 21] = 0; // umesto 'E' stavlja 0
	int fileClusterFirstLevel = (buffer[3 + Location[1] + 12] << 24) | ((buffer[2 + Location[1]+12] & 0xFF) << 16) | ((buffer[1 + Location[1]+12] & 0xFF) << 8) | (buffer[0 + Location[1]+12] & 0xFF);
	char bufferFirstLevel[2048];
	myPartition->readCluster(fileClusterFirstLevel, bufferFirstLevel);
	
	for (int i = 0; i < 2044; i++) {
		if (bufferFirstLevel[i] == -1) break;
		int fileClusterSecondLevel = (bufferFirstLevel[3 + i] << 24) | ((bufferFirstLevel[2 + i] & 0xFF) << 16) | ((bufferFirstLevel[1 + i] & 0xFF) << 8) | (bufferFirstLevel[0 + i] & 0xFF);
		char bufferSecondLevel[2048];
		myPartition->readCluster(fileClusterSecondLevel, bufferSecondLevel);
		for (int j = 0; j < 2044; j++) {
			int fileDataCluster = (bufferSecondLevel[3 + i] << 24) | ((bufferSecondLevel[2 + i] & 0xFF) << 16) | ((bufferSecondLevel[1 + i] & 0xFF) << 8) | (bufferSecondLevel[0 + i] & 0xFF);
			freeCluster(fileDataCluster);
			j += 4;
		}
		freeCluster(fileClusterSecondLevel);
		i += 4;
	}
	freeCluster(fileClusterFirstLevel);
	buffer[12] = -1;// INDEKS PRVOG NIVOA
	buffer[13] = -1;
	buffer[14] = -1;
	buffer[15] = -1;
	buffer[16] = 0;	// VELICINA FAJLA
	buffer[17] = 0;
	buffer[18] = 0;
	buffer[19] = 0;

	myPartition->writeCluster(Location[0], buffer);
	numOfFiles++;
	return 1;
}


int* KernelFS::findFile(char *fname) {
	char bufferRoot[2048];
	char buffer[2048];
	int* fileLocation = new int[2];
	int i = 0;
	myPartition->readCluster(1, bufferRoot);
	for (int i = 0; i < 2045;) {
		int index = (bufferRoot[3 + i] << 24) | ((bufferRoot[2 + i] & 0xFF) << 16) | ((bufferRoot[1 + i] & 0xFF) << 8) | (bufferRoot[0 + i] & 0xFF);
		fileLocation[0] = index;
		if (index != -1) {
			myPartition->readCluster(index, buffer);
			for (int j = 0; j < 2048;) {  //******* DO KAD TREBA IDE OVAJ PETLJA???!?!?!!?!?!?!
				fileLocation[1] = j;
				char* nameOfFile = new char[13];
				int count = 0;
				while (count < 8 && (buffer[count + j] != ' ')) {
					nameOfFile[count] = buffer[count + j];
					count++;
				}
				int count1 = 0;
				int count2 = 8;
				nameOfFile[count++] = '.';
				while (count1 < 3 && (buffer[count2 + j] != ' ')) {
					nameOfFile[count] = buffer[count2 + j];
					count++;
					count1++;
					count2++;
				}
				int length = strlen(fname);
				int k = 0;
				for (k = 0; k < length;) {
					if (nameOfFile[k] == fname[k]) k++;
					else break;
				}
				delete nameOfFile;
				if (k == length) return fileLocation;
				j += 32;
			}
			i = i + 4;
		}
		else {
			fileLocation[0] = -2;
			return fileLocation;
		}
	}
	fileLocation[0] = -2;
	return fileLocation;
}

int KernelFS::findFreeCluster() {
	
	char buffer[2048];
	char mask = 1;
	myPartition->readCluster(0, buffer);
	for (int i = 0; i < 2048; i++) {
		for (int j = 0; j < 8; j++) {
			if ((buffer[i] & (mask << j)) != 0)
				return 8 * i + j;
		}
	}
}

void KernelFS::takeCluster(int num) {
	char buffer[2048];
	char mask = 1;
	myPartition->readCluster(0, buffer);
	char index = buffer[num / 8];
	index = index & ~(mask << (num % 8));
	buffer[num / 8] = index;
	myPartition->writeCluster(0, buffer);
}

void KernelFS::freeCluster(int num) {
	char* buffer = new char[2048];
	char mask = 1;
	myPartition->readCluster(0, buffer);
	int pom = num / 8;
	char index = buffer[pom];
	index = index | (mask << (num % 8));
	buffer[pom] = index;
	myPartition->writeCluster(0, buffer);
	delete buffer;
}

char* KernelFS::writeFileToCluster(char* fname) {
	char buffer[32];
	char *tek = fname;
	int i = 0;
	while (*tek != '.') {
		buffer[i++] = *tek;
		tek++;
	}
	tek++;
	if (i < 8) {
		while (i < 8) {
			buffer[i++] = ' ';
		}
	}
	while (*tek != NULL) {
		buffer[i++] = *tek;
		tek++;
	}
	if (i < 11) {
		while (i < 11) {
			buffer[i++] = ' ';
		}
	}
	buffer[i++] = 0;	// NE KORISTI SE
	buffer[i++] = -1;	// INDEKS PRVOG NIVOA
	buffer[i++] = -1;
	buffer[i++] = -1;
	buffer[i++] = -1;
	buffer[i++] = 0;	// VELICINA FAJLA
	buffer[i++] = 0;
	buffer[i++] = 0;
	buffer[i++] = 0;

	buffer[20] = 0; // RWAbits
	buffer[21] = 'E';

	return buffer;
}

int KernelFS::isOpened(char* fname) {
	int* Location = new int[2];
	char mask = 1;
	Location = findFile(fname);
	char buffer[2048];
	myPartition->readCluster(Location[0], buffer);
	char RWAbits = buffer[Location[1] + 20];
	if (RWAbits & (mask << 1) != 0) return 1; // OTVORIO ZA WRITE *****
	else if (RWAbits &(mask << 2) != 0) return 1; // OVTORIO ZA A *****
	else if (RWAbits & mask != 0) return 1;     //OTVORIO ZA READ ******
	delete Location;
	return 0;
}

int KernelFS::size(char* fname) {
	int* Location = new int[2];
	Location = findFile(fname);
	char buffer[2048];
	myPartition->readCluster(Location[0], buffer);
	int size = (buffer[3 + 16 + Location[1]] << 24) | ((buffer[2 + 16 + Location[1]] & 0xFF) << 16) | ((buffer[1 + 16 + Location[1]] & 0xFF) << 8) | (buffer[0 + 16 + Location[1]] & 0xFF);	
	delete Location;
	return size;
}

char* KernelFS::oneToFourBytes(char* buffer, int locationInCluster, int position, int value) {
	buffer[0 + locationInCluster + position] = (value) & 0xFF;     
	buffer[1 + locationInCluster + position] = (value >> 8) & 0xFF;
	buffer[2 + locationInCluster + position] = (value >> 16) & 0xFF;
	buffer[3 + locationInCluster + position] = (value >> 24) & 0xFF;
	return buffer;
}

void KernelFS::signalSem() {
	signal(mutexOpen);
}