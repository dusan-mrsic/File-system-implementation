#pragma once
#include "fs.h"
#include "KernelFS.h"
#include <iostream>

class KernelFile; 
class File {
public:   
	~File();                                 //zatvaranje fajla   
	char write (BytesCnt, char* buffer);    
	BytesCnt read (BytesCnt, char* buffer);   
	char seek (BytesCnt);   
	BytesCnt filePos();   
	char eof ();   
	BytesCnt getFileSize ();   
	char truncate (); 
private:   
	friend class FS;   
	friend class KernelFS;   
	File(char* name, int* location, char mode);  //objekat fajla se može kreirati samo otvaranjem   
	KernelFile *myImpl; 
};
