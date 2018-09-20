#include <stdio.h>
#include <time.h>
#include "UnAlzUtils.h"

#ifdef _WIN32
#	define I64FORM(x) "%"#x"I64d" 
#	define U64FORM(x) "%"#x"I64u"
#else
#	define I64FORM(x) "%"#x"lld"
#	define U64FORM(x) "%"#x"llu"
#endif

#ifdef _WIN32
#	pragma warning( disable : 4996 )		// crt secure warning
#endif


#define LEN_ATTR	6

time_t dosTime2TimeT(UINT32 dostime)   // from INFO-ZIP src
{
	struct tm t;         
	t.tm_isdst = -1;     
	t.tm_sec  = (((int)dostime) <<  1) & 0x3e;
	t.tm_min  = (((int)dostime) >>  5) & 0x3f;
	t.tm_hour = (((int)dostime) >> 11) & 0x1f;
	t.tm_mday = (int)(dostime >> 16) & 0x1f;
	t.tm_mon  = ((int)(dostime >> 21) & 0x0f) - 1;
	t.tm_year = ((int)(dostime >> 25) & 0x7f) + 80;
	return mktime(&t);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///          fileAttribute 를 스트링으로 바꿔준다.
/// @param   buf  - 5byte 이상 
/// @param   fileAttribute  - ALZ_FILE_ATTRIBUTE 참조
/// @return  
/// @date    2005-06-23 오후 10:12:35
////////////////////////////////////////////////////////////////////////////////////////////////////
void FileAttr2Str(char szAttr[LEN_ATTR], BYTE fileAttribute)
{
	szAttr[0] = 0;

	if(fileAttribute&ALZ_FILEATTR_FILE)
		CUnAlz::safe_strcat(szAttr, "A", LEN_ATTR);
	else
		CUnAlz::safe_strcat(szAttr, "_", LEN_ATTR);

	if(fileAttribute&ALZ_FILEATTR_DIRECTORY)
		CUnAlz::safe_strcat(szAttr, "D", LEN_ATTR);
	else
		CUnAlz::safe_strcat(szAttr, "_", LEN_ATTR);

	if(fileAttribute&ALZ_FILEATTR_READONLY)
		CUnAlz::safe_strcat(szAttr, "R", LEN_ATTR);
	else
		CUnAlz::safe_strcat(szAttr, "_", LEN_ATTR);

	if(fileAttribute&ALZ_FILEATTR_HIDDEN)
		CUnAlz::safe_strcat(szAttr, "H", LEN_ATTR);
	else
		CUnAlz::safe_strcat(szAttr, "_", LEN_ATTR);
}


// alz 파일을 리스팅 한다 ( -l 옵션 )
int ListAlz(CUnAlz* pUnAlz, const char* src)
{
	CUnAlz::FileList::iterator	i;
	CUnAlz::FileList*			list;

	list = pUnAlz->GetFileList();

	printf("\nListing archive: %s\n"
		   "\n"
		   "Attr  Uncomp Size    Comp Size Date & Time & File Name\n",
		   src);
	printf("---- ------------ ------------ ------------------------------------------------\n");

//	char szDate[64];
	char szTime[64];
	char szAttr[LEN_ATTR];
	UINT64 totalUnCompressedSize = 0;
	UINT64 totalCompressedSize = 0;
	unsigned fileNum = 0;
	time_t	time;
	struct tm*		filetm;
	for(i=list->begin(); i<list->end(); i++)
	{
		// time
		time = dosTime2TimeT(i->head.fileTimeDate);
		filetm = localtime(&time);
//		strftime(szTime, 64, "%H:%M:%S", filetm);
//		strftime(szDate, 64, "%Y-%m-%d", filetm);
		strftime(szTime, 64, "%x %X", filetm);				// use system locale
		// attributes
		FileAttr2Str(szAttr, i->head.fileAttribute);

		printf("%s " I64FORM(12) " " I64FORM(12) " %s  %s%s\n",
			   szAttr, i->uncompressedSize,
			   i->compressedSize, szTime, i->fileName, 
			   i->head.fileDescriptor & ALZ_FILE_DESCRIPTOR_ENCRYPTED ? "*" : "" );

		++fileNum;
		totalUnCompressedSize += i->uncompressedSize;
		totalCompressedSize += i->compressedSize;
	}

	printf("---- ------------ ------------ ------------------------------------------------\n");
	printf("     " U64FORM(12) " " U64FORM(12) " Total %u file%s\n",
		totalUnCompressedSize, totalCompressedSize, fileNum, fileNum<=1 ? "" : "s");

	return 0;
}
