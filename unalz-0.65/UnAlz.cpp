
#ifdef _WIN32
#	include "zlib/zlib.h"
#	include "bzip2/bzlib.h"
#else
#	include <zlib.h>
#	include <bzlib.h>
#endif

#include "UnAlz.h"


#ifdef _WIN32
#	pragma warning( disable : 4996 )		// crt secure warning
#endif

// utime 함수 처리
#if defined(_WIN32) || defined(__CYGWIN__)
#	include <time.h>
#	include <sys/utime.h>
#endif
#ifdef __GNUC__
#	include <time.h>
#	include <utime.h>
#endif

// mkdir
#ifdef _WIN32				
#	include <direct.h>		
#else
#	include <sys/stat.h>
#endif

#ifdef _UNALZ_ICONV			// code page support
#	include <iconv.h>
#endif

#if defined(__linux__) || defined(__GLIBC__) || defined(__GNU__) || defined(__APPLE__)
#	include <errno.h>
#endif

#if defined(__NetBSD__)
#	include <sys/param.h>	// __NetBSD_Version__
#	include <errno.h>		// iconv.h 때문에 필요 
#endif

#ifdef _WIN32				// safe string 처리
#	include <strsafe.h>
#endif


// ENDIAN 처리
#ifdef _WIN32	// (L)
#	define swapint64(a)	(UINT64) ( (((a)&0x00000000000000FFL) << 56) | (((a)&0x000000000000FF00L) << 40) | (((a)&0x0000000000FF0000L) << 24) | (((a)&0x00000000FF000000L) << 8)  | (((a)&0x000000FF00000000L) >> 8)  | (((a)&0x0000FF0000000000L) >> 24) | (((a)&0x00FF000000000000L) >> 40) | (((a)&0xFF00000000000000L) >> 56) )
#else			// (LL)
#	define swapint64(a)	(UINT64) ( (((a)&0x00000000000000FFLL) << 56) | (((a)&0x000000000000FF00LL) << 40) | (((a)&0x0000000000FF0000LL) << 24) | (((a)&0x00000000FF000000LL) << 8)  | (((a)&0x000000FF00000000LL) >> 8)  | (((a)&0x0000FF0000000000LL) >> 24) | (((a)&0x00FF000000000000LL) >> 40) | (((a)&0xFF00000000000000LL) >> 56) )
#endif
#define swapint32(a)    ((((a)&0xff)<<24)+(((a>>8)&0xff)<<16)+(((a>>16)&0xff)<<8)+(((a>>24)&0xff)))
#define swapint16(a)    (((a)&0xff)<<8)+(((a>>8)&0xff))

typedef UINT16	(*_unalz_le16toh)(UINT16 a);
typedef UINT32	(*_unalz_le32toh)(UINT32 a);
typedef UINT64	(*_unalz_le64toh)(UINT64 a);

static _unalz_le16toh unalz_le16toh=NULL;
static _unalz_le32toh unalz_le32toh=NULL;
static _unalz_le64toh unalz_le64toh=NULL;

static UINT16	le16tole(UINT16 a){return a;}
static UINT32	le32tole(UINT32 a){return a;}
static UINT64	le64tole(UINT64 a){return a;}
static UINT16	le16tobe(UINT16 a){return swapint16(a);}
static UINT32	le32tobe(UINT32 a){return swapint32(a);}
static UINT64	le64tobe(UINT64 a){return swapint64(a);}


#ifndef MAX_PATH
#	define MAX_PATH 260*6			// 그냥 .. 충분히..
#endif

#ifdef _WIN32
#	define PATHSEP "\\"
#	define PATHSEPC '\\'
#else
#	define PATHSEP "/"
#	define PATHSEPC '/'
#endif

static time_t dosTime2TimeT(UINT32 dostime)   // from INFO-ZIP src
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

static BOOL IsBigEndian(void) 
{
   union {
        short a;
        char  b[2];
   } endian;
   
   endian.a = 0x0102;
   if(endian.b[0] == 0x02) return FALSE;
   return TRUE;
}


#ifdef _WIN32
#	define safe_sprintf	StringCbPrintfA
#else
#	define safe_sprintf	snprintf
#endif


// 64bit file handling support
#if (_FILE_OFFSET_BITS==64)
#	define unalz_fseek	fseeko
#	define unalz_ftell	ftello
#else
#	define unalz_fseek	fseek
#	define unalz_ftell	ftell
#endif


// error string table <- CUnAlz::ERR 의 번역
static const char* errorstrtable[]=
{
	"no error",										// ERR_NOERR
	"general error",								// ERR_GENERAL
	"can't open archive file",						// ERR_CANT_OPEN_FILE
	"can't open dest file or path",					// ERR_CANT_OPEN_DEST_FILE
//	"can't create dest path",						// ERR_CANT_CREATE_DEST_PATH
	"corrupted file",								// ERR_CORRUPTED_FILE
	"not alz file",									// ERR_NOT_ALZ_FILE
	"can't read signature",							// ERR_CANT_READ_SIG
	"can't read file",								// ERR_CANT_READ_FILE
	"error at read header",							// ERR_AT_READ_HEADER
	"invalid filename length",						// ERR_INVALID_FILENAME_LENGTH
	"invalid extrafield length",					// ERR_INVALID_EXTRAFIELD_LENGTH,
	"can't read central directory structure head",	// ERR_CANT_READ_CENTRAL_DIRECTORY_STRUCTURE_HEAD, 
	"invalid filename size",						// ERR_INVALID_FILENAME_SIZE,
	"invalid extrafield size",						// ERR_INVALID_EXTRAFIELD_SIZE,
	"invalid filecomment size",						// ERR_INVALID_FILECOMMENT_SIZE,
	"cant' read header",							// ERR_CANT_READ_HEADER,
	"memory allocation failed",						// ERR_MEM_ALLOC_FAILED,
	"file read error",								// ERR_FILE_READ_ERROR,
	"inflate failed",								// ERR_INFLATE_FAILED,
	"bzip2 decompress failed",						// ERR_BZIP2_FAILED,
	"invalid file CRC",								// ERR_INVALID_FILE_CRC
	"unknown compression method",					// ERR_UNKNOWN_COMPRESSION_METHOD
									
	"iconv-can't open iconv",						// ERR_ICONV_CANT_OPEN,
	"iconv-invalid multisequence of characters",	// ERR_ICONV_INVALID_MULTISEQUENCE_OF_CHARACTERS,
	"iconv-incomplete multibyte sequence",			// ERR_ICONV_INCOMPLETE_MULTIBYTE_SEQUENCE,
	"iconv-not enough space of buffer to convert",	// ERR_ICONV_NOT_ENOUGH_SPACE_OF_BUFFER_TO_CONVERT,
	"iconv-etc",									// ERR_ICONV_ETC,
	
	"password was not set",							// ERR_PASSWD_NOT_SET,
	"invalid password",								// ERR_INVALID_PASSWD,
	"user aborted",					
};


////////////////////////////////////////////////////////////////////////////////////////////////////
///          ctor
/// @date    2004-03-06 오후 11:19:49
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnAlz::CUnAlz()
{
	memset(m_files, 0, sizeof(m_files));
	m_nErr = ERR_NOERR;
	m_posCur = m_fileList.end();//(FileList::iterator)NULL;
	m_pFuncCallBack = NULL;
	m_pCallbackParam = NULL;
	m_bHalt = FALSE;
	m_nFileCount = 0;
	m_nCurFile = -1;
	m_nVirtualFilePos = 0;
	m_nCurFilePos = 0;
	m_bIsEOF = FALSE;
	m_bIsEncrypted = FALSE;
	m_bIsDataDescr = FALSE;
	m_bPipeMode = FALSE;

#ifdef _UNALZ_ICONV

#ifdef _UNALZ_UTF8
	safe_strcpy(m_szToCodepage, "UTF-8",UNALZ_LEN_CODEPAGE) ;		// 기본적으로 utf-8
#else 
	safe_strcpy(m_szToCodepage, "CP949",UNALZ_LEN_CODEPAGE) ;		// 기본적으로 CP949
#endif // _UNALZ_UTF8

	safe_strcpy(m_szFromCodepage, "CP949",UNALZ_LEN_CODEPAGE);		// alz 는 949 만 지원
#endif // _UNALZ_ICONV

	// check endian
	if(unalz_le16toh==NULL)
	{
		if(IsBigEndian())
		{
			unalz_le16toh = le16tobe;
			unalz_le32toh = le32tobe;
			unalz_le64toh = le64tobe;
		}
		else
		{
			unalz_le16toh = le16tole;
			unalz_le32toh = le32tole;
			unalz_le64toh = le64tole;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          dtor
/// @date    2004-03-06 오후 11:19:52
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnAlz::~CUnAlz()
{
	Close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          progress callback func setting
/// @date    2004-03-01 오전 6:02:05
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnAlz::SetCallback(_UnAlzCallback* pFunc, void* param)
{
	m_pFuncCallBack = pFunc;
	m_pCallbackParam = param;
}

#ifdef _WIN32
#if !defined(__GNUWIN32__) && !defined(__GNUC__)
////////////////////////////////////////////////////////////////////////////////////////////////////
///          파일 열기
/// @param   szPathName  
/// @return  
/// @date    2004-03-06 오후 11:03:59
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::Open(LPCWSTR szPathName)
{
	char szPathNameA[MAX_PATH];
	::WideCharToMultiByte(CP_ACP, 0, szPathName, -1, szPathNameA, MAX_PATH, NULL, NULL);
	return Open(szPathNameA);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
///          대상 파일 세팅하기.
/// @param   szFileName  
/// @return  
/// @date    2004-03-06 오후 11:06:20
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::SetCurrentFile(LPCWSTR szFileName)
{
	char szFileNameA[MAX_PATH];
	::WideCharToMultiByte(CP_ACP, 0, szFileName, -1, szFileNameA, MAX_PATH, NULL, NULL);
	return SetCurrentFile(szFileNameA);
}
BOOL CUnAlz::IsFolder(LPCWSTR szPathName)
{
	UINT32 dwRet;
	dwRet = GetFileAttributesW(szPathName);
	if(dwRet==0xffffffff) return FALSE;
	if(dwRet & FILE_ATTRIBUTE_DIRECTORY) return TRUE;
	return FALSE;
}
#endif // __GNUWIN32__
#endif // _WIN32

BOOL CUnAlz::Open(const char* szPathName)
{
	if(FOpen(szPathName)==FALSE) 
	{
		m_nErr = ERR_CANT_OPEN_FILE;
		return FALSE;
	}

	BOOL	bValidAlzHeader = FALSE;

	// file 분석시작..
	for(;;)
	{
		SIGNATURE	sig;
		BOOL		ret;

		if(FEof()) break;
		//int pos = unalz_ftell(m_fp);
		sig = ReadSignature();
		if(sig==SIG_EOF)
		{
			break;
		}
		if(sig==SIG_ERROR)
		{
			if(bValidAlzHeader)
				m_nErr = ERR_CORRUPTED_FILE;	// 손상된 파일
			else
				m_nErr = ERR_NOT_ALZ_FILE;	// alz 파일이 아니다.
			return FALSE;						// 깨진 파일..
		}

		if(sig==SIG_ALZ_FILE_HEADER) 
		{
			ret = ReadAlzFileHeader();
			bValidAlzHeader = TRUE;					// alz 파일은 맞다.
		}
		else if(sig==SIG_LOCAL_FILE_HEADER) ret = ReadLocalFileheader();
		else if(sig==SIG_CENTRAL_DIRECTORY_STRUCTURE) ret = ReadCentralDirectoryStructure();
		else if(sig==SIG_ENDOF_CENTRAL_DIRECTORY_RECORD) ret = ReadEndofCentralDirectoryRecord();
		else
		{
			// 미구현된 signature ? 깨진 파일 ?
			ASSERT(0);
			m_nErr = ERR_CORRUPTED_FILE;
			return FALSE;
		}
		
		if(ret==FALSE) 
		{
			return FALSE;
		}

		if(FEof()) break;
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          파일 닫기..
/// @return  
/// @date    2004-03-06 오후 11:04:21
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnAlz::Close()
{
	FClose();

	// 목록 날리기..
	FileList::iterator	i;

	for(i=m_fileList.begin(); i<m_fileList.end(); i++)
	{
		i->Clear();
	}

	m_posCur = m_fileList.end();//(FileList::iterator)NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          FILE 내의 SIGNATURE 읽기
/// @return  
/// @date    2004-03-06 오후 11:04:47
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnAlz::SIGNATURE CUnAlz::ReadSignature()
{
	UINT32	dwSig;
	if(FRead(&dwSig, sizeof(dwSig))==FALSE)
	{
		if(FEof())
			return SIG_EOF;
		m_nErr = ERR_CANT_READ_SIG;
		return SIG_ERROR;
	}

	return (SIGNATURE)unalz_le32toh(dwSig);		// little to host;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          ALZ HEADER SIGNATURE 읽기
/// @return  
/// @date    2004-03-06 오후 11:05:11
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::ReadAlzFileHeader()
{
	SAlzHeader	header;
	if(FRead(&header, sizeof(header))==FALSE)
	{
		ASSERT(0);
		m_nErr = ERR_CANT_READ_FILE;
		return FALSE;
	}
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          각각의 파일 헤더 읽기
/// @return  
/// @date    2004-03-06 오후 11:05:18
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::ReadLocalFileheader()
{
	SAlzLocalFileHeader zipHeader;
	int ret;

	ret = FRead(&(zipHeader.head), sizeof(zipHeader.head));
	if(ret==FALSE) 
	{
		m_nErr = ERR_AT_READ_HEADER;
		return FALSE;
	}

	// ALZ 확장..
	if( (zipHeader.head.fileDescriptor & (SHORT)1) != 0){
		m_bIsEncrypted = TRUE;									// 하나라도 암호 걸렸으면 세팅한다.
	}
	if( (zipHeader.head.fileDescriptor & (SHORT)8) != 0){
		m_bIsDataDescr = TRUE;
	}
	
	int byteLen = zipHeader.head.fileDescriptor/0x10;

	if(byteLen)
	{
		FRead(&(zipHeader.compressionMethod),	sizeof(zipHeader.compressionMethod));
		FRead(&(zipHeader.unknown),				sizeof(zipHeader.unknown));
		FRead(&(zipHeader.fileCRC),				sizeof(zipHeader.fileCRC));

		FRead(&(zipHeader.compressedSize), byteLen);
		FRead(&(zipHeader.uncompressedSize),  byteLen);			// 압축 사이즈가 없다.
	}

	// little to system 
    zipHeader.fileCRC				=   unalz_le32toh(zipHeader.fileCRC);
    zipHeader.head.fileNameLength   =   unalz_le16toh(zipHeader.head.fileNameLength);
    zipHeader.compressedSize        =   unalz_le64toh(zipHeader.compressedSize);
    zipHeader.uncompressedSize      =   unalz_le64toh(zipHeader.uncompressedSize); 

	// FILE NAME
	zipHeader.fileName = (char*)malloc(zipHeader.head.fileNameLength+sizeof(char));
	if(zipHeader.fileName==NULL)
	{
		m_nErr = ERR_INVALID_FILENAME_LENGTH;
		return FALSE;
	}
	FRead(zipHeader.fileName, zipHeader.head.fileNameLength);
	if(zipHeader.head.fileNameLength > MAX_PATH - 5)
		zipHeader.head.fileNameLength = MAX_PATH - 5;
	zipHeader.fileName[zipHeader.head.fileNameLength] = (CHAR)NULL;


#ifdef _UNALZ_ICONV		// codepage convert

	if(strlen(m_szToCodepage))
	{

	#define ICONV_BUF_SIZE	(260*6)			// utf8 은 최대 6byte 
	size_t ileft, oleft;
	iconv_t cd;
	size_t iconv_result;
	size_t size;
	char inbuf[ICONV_BUF_SIZE];
	char outbuf[ICONV_BUF_SIZE];
#if defined(__FreeBSD__) || defined(__CYGWIN__) ||  defined(__NetBSD__)
	const char *inptr = inbuf;
#else
	char *inptr = inbuf;
#endif
	char *outptr = outbuf;
	
	size = strlen(zipHeader.fileName)+1;
	strncpy(inbuf, zipHeader.fileName, size);
	ileft = size;
	oleft = sizeof(outbuf);
	
	cd = iconv_open(m_szToCodepage, m_szFromCodepage);		// 보통 "CP949" 에서 "UTF-8" 로 
	iconv(cd, NULL, NULL, NULL, NULL);
	if( cd == (iconv_t)(-1)) 
	{
		m_nErr = ERR_ICONV_CANT_OPEN;		// printf("Converting Error : Cannot open iconv");
		return FALSE;
	}
	else
	{
		iconv_result = iconv(cd, &inptr, &ileft, &outptr, &oleft);
		
		if(iconv_result== (size_t)(-1))		// iconv 실패..
		{
			if (errno == EILSEQ) 
				m_nErr = ERR_ICONV_INVALID_MULTISEQUENCE_OF_CHARACTERS; // printf("Invalid Multibyte Sequence of Characters");
			else if (errno == EINVAL) 
				m_nErr = ERR_ICONV_INCOMPLETE_MULTIBYTE_SEQUENCE; //printf("Incomplete  multibyte sequence");
			else if (errno != E2BIG) 
				m_nErr = ERR_ICONV_NOT_ENOUGH_SPACE_OF_BUFFER_TO_CONVERT;	// printf("Not enough space of buffer to convert");
			else 
				m_nErr = ERR_ICONV_ETC;
			iconv_close(cd);
			return FALSE;
		} 
		else 
		{
			outbuf[ICONV_BUF_SIZE-oleft] = 0;
			if(zipHeader.fileName) free(zipHeader.fileName);
			zipHeader.fileName = strdup(outbuf);
			if (zipHeader.fileName == NULL)
			{
				m_nErr = ERR_ICONV_ETC;
				iconv_close(cd);
				return FALSE;
			}
			// printf("\n  Converted File Name : %s", outbuf);
		}
		
		iconv_close(cd);
    }

	}
#endif

	/*
	// EXTRA FIELD LENGTH
	if(zipHeader.head.extraFieldLength)
	{
		zipHeader.extraField = (BYTE*)malloc(zipHeader.head.extraFieldLength);
		if(zipHeader.extraField==NULL)
		{
			m_nErr = ERR_INVALID_EXTRAFIELD_LENGTH;
			return FALSE;
		}
		FRead(zipHeader.extraField, 1, zipHeader.head.extraFieldLength);
	}
	*/

	if(IsEncryptedFile(zipHeader.head.fileDescriptor)) 
		FRead(zipHeader.encChk, ALZ_ENCR_HEADER_LEN);  // xf86

	// SKIP FILE DATA
	zipHeader.dwFileDataPos = FTell();						// data 의 위치 저장하기..
	FSeek(FTell()+zipHeader.compressedSize);

	// DATA DESCRIPTOR
	/*
	if(zipHeader.head.generalPurposeBitFlag.bit1)
	{
		FRead(zipHeader.extraField, 1, sizeof(zipHeader.extraField),);
	}
	*/

	/*
#ifdef _DEBUG
	printf("NAME:%s COMPRESSED SIZE:%d UNCOMPRESSED SIZE:%d COMP METHOD:%d\n", 
		zipHeader.fileName, 
		zipHeader.compressedSize, 
		zipHeader.uncompressedSize,
		zipHeader.compressionMethod
		);
#endif
	*/

	// 파일을 목록에 추가한다..
	m_fileList.push_back(zipHeader);

	return TRUE;
}


BOOL CUnAlz::ReadCentralDirectoryStructure()
{
	SCentralDirectoryStructure header;

	if(FRead(&header, sizeof(header.head))==FALSE)
	{
		m_nErr = ERR_CANT_READ_CENTRAL_DIRECTORY_STRUCTURE_HEAD;
		return FALSE;
	}

	/*
	// read file name
	if(header.head.fileNameLength)
	{
		header.fileName = (char*)malloc(header.head.fileNameLength+1);
		if(header.fileName==NULL)
		{
			m_nErr = ERR_INVALID_FILENAME_SIZE;
			return FALSE;
		}
		FRead(header.fileName, 1, header.head.fileNameLength, m_fp);
		header.fileName[header.head.fileNameLength] = NULL;
	}

	// extra field;
	if(header.head.extraFieldLength)
	{
		header.extraField = (BYTE*)malloc(header.head.extraFieldLength);
		if(header.extraField==NULL)
		{
			m_nErr = ERR_INVALID_EXTRAFIELD_SIZE;
			return FALSE;
		}
		FRead(header.extraField, 1, header.head.extraFieldLength, m_fp);
	}

	// file comment;
	if(header.head.fileCommentLength)
	{
		header.fileComment = (char*)malloc(header.head.fileCommentLength+1);
		if(header.fileComment==NULL)
		{
			m_nErr = ERR_INVALID_FILECOMMENT_SIZE;
			return FALSE;
		}
		FRead(header.fileComment, 1, header.head.fileCommentLength, m_fp);
		header.fileComment[header.head.fileCommentLength] = NULL;
	}
	*/

	return TRUE;
}


BOOL CUnAlz::ReadEndofCentralDirectoryRecord()
{
	/*
	SEndOfCentralDirectoryRecord	header;

	if(FRead(&header, sizeof(header.head), 1, m_fp)!=1)
	{
		m_nErr = ERR_CANT_READ_HEADER;
		return FALSE;
	}

	if(header.head.zipFileCommentLength)
	{
		header.fileComment = (char*)malloc(header.head.zipFileCommentLength+1);
		if(header.fileComment==NULL)
		{
			m_nErr = ERR_INVALID_FILECOMMENT_SIZE;
			return FALSE;
		}
		FRead(header.fileComment, 1, header.head.zipFileCommentLength, m_fp);
		header.fileComment[header.head.zipFileCommentLength] = NULL;
	}
	*/

	return TRUE;
}


BOOL CUnAlz::SetCurrentFile(const char* szFileName)
{
	FileList::iterator	i;

	// 순차적으로 찾는다.
	for(i=m_fileList.begin(); i<m_fileList.end(); i++)
	{
#ifdef _WIN32
		if(stricmp(i->fileName, szFileName)==0)
#else
		if(strcmp(i->fileName, szFileName)==0)
#endif
		{
			m_posCur = i;
			return TRUE;
		}
	}

	m_posCur = m_fileList.end();//(FileList::iterator)NULL;

	return FALSE;
}

void CUnAlz::SetCurrentFile(FileList::iterator newPos)
{
	m_posCur = newPos;
}

#ifndef MAX_WBITS
#  define MAX_WBITS   15 /* 32K LZ77 window */
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
///          버퍼에 압축 풀기. 버퍼는 당근 충분한 크기가 준비되어 있어야 한다.
/// @param   pDestBuf  
/// @return  
/// @date    2004-03-07 오전 12:26:13
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::ExtractCurrentFileToBuf(BYTE* pDestBuf, int nBufSize)
{
	SExtractDest	dest;
	dest.nType = ET_MEM;
	dest.buf = pDestBuf;
	dest.bufpos = 0;
	dest.bufsize = nBufSize;
	return ExtractTo(&dest);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///          현재 파일 (SetCurrentFile로 지)을 대상 경로에 대상 파일로 푼다.
/// @param   szDestPathName  - 대상 경로
/// @param   szDestFileName  - 대상 파일명, NULL 이면 원래 파일명 사용
/// @return  
/// @date    2004-03-06 오후 11:06:59
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::ExtractCurrentFile(const char* szDestPathName, const char* szDestFileName)
{
	if(m_posCur==m_fileList.end()/*(FileList::iterator)NULL*/) {ASSERT(0); return FALSE;}
	BOOL	ret=FALSE;

	SExtractDest	dest;
	char			szDestPathFileName[MAX_PATH];
	
	if(chkValidPassword() == FALSE) 
	{
		return FALSE;
	}

	if( szDestPathName==NULL||
		strlen(szDestPathName) + (szDestFileName?strlen(szDestFileName):strlen(m_posCur->fileName))+1 > MAX_PATH
	   )	// check buffer overflow
	{
		ASSERT(0);
		m_nErr = ERR_GENERAL;
		return FALSE;
	}
	
	// 경로명
	safe_strcpy(szDestPathFileName, szDestPathName, MAX_PATH);
	if(szDestPathFileName[strlen(szDestPathFileName)]!=PATHSEPC)
		safe_strcat(szDestPathFileName, PATHSEP, MAX_PATH);

	// 파일명
	if(szDestFileName) safe_strcat(szDestPathFileName, szDestFileName, MAX_PATH);
	else safe_strcat(szDestPathFileName, m_posCur->fileName, MAX_PATH);

	// ../../ 형식의 보안 버그 확인
	if( strstr(szDestPathFileName, "../")||
		strstr(szDestPathFileName, "..\\"))
	{
		ASSERT(0);
		m_nErr = ERR_GENERAL;
		return FALSE;
	}

#ifndef _WIN32 
	{
		char* p = szDestPathFileName;			// 경로 delimiter 바꾸기 
		while(*p)
		{
			if(*p=='\\') *p='/';
			p++;
		}
	}
#endif

	// 압축풀 대상 ( 파일 )
	dest.nType = ET_FILE;

	if(m_bPipeMode)
		dest.fp = stdout;				// pipe mode 일 경우 stdout 출력
	else
		dest.fp = fopen(szDestPathFileName, "wb");

	// 타입이 폴더일 경우..
	if(m_bPipeMode==FALSE && (m_posCur->head.fileAttribute) & ALZ_FILEATTR_DIRECTORY )
	{
//printf("digpath:%s\n", szDestPathFileName);
		// 경로파기
		DigPath(szDestPathFileName);
		return TRUE;
//		m_nErr = ERR_CANT_CREATE_DEST_PATH;
//		return FALSE;
	}

	// 파일 열기 실패시 - 경로를 파본다
	if(dest.fp==NULL) 
	{
		DigPath(szDestPathFileName);
		dest.fp = fopen(szDestPathFileName, "wb");
	}

	// 그래도 파일열기 실패시.
	if(dest.fp==NULL) 
	{
		// 대상 파일 열기 실패
		m_nErr = ERR_CANT_OPEN_DEST_FILE;
//printf("dest pathfilename:%s\n",szDestPathFileName);
		if(m_pFuncCallBack)
		{
//			CHAR buf[1024];
//			sprintf(buf, "파일 열기 실패 : %s", szDestPathFileName);
//			m_pFuncCallBack(buf, 0,0,m_pCallbackParam, NULL);
		}
		return FALSE;
	}
//#endif

	// CALLBACK 세팅
	if(m_pFuncCallBack) m_pFuncCallBack(m_posCur->fileName, 0,m_posCur->uncompressedSize,m_pCallbackParam, NULL);

	ret = ExtractTo(&dest);
	if(dest.fp!=NULL)
	{
		fclose(dest.fp);
		// file time setting - from unalz_wcx_01i.zip
		utimbuf tmp;
		tmp.actime = 0;													// 마지막 엑세스 타임
		tmp.modtime = dosTime2TimeT(m_posCur->head.fileTimeDate);		// 마지막 수정일자만 변경(만든 날자는 어떻게 바꾸지?)
		utime(m_posCur->fileName, &tmp);
	}
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          대상에 압축 풀기..
/// @param   dest  
/// @return  
/// @date    2004-03-07 오전 12:44:36
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::ExtractTo(SExtractDest* dest)
{
	BOOL ret = FALSE;
	// 압축 방법에 따라서 압축 풀기
	if(m_posCur->compressionMethod==COMP_NOCOMP)
	{
		ret = ExtractRawfile(dest, *m_posCur);
	}
	else if(m_posCur->compressionMethod==COMP_BZIP2)
	{
		ret = ExtractBzip2(dest, *m_posCur);			// bzip2
	}
	else if(m_posCur->compressionMethod==COMP_DEFLATE)
	{
		ret = ExtractDeflate2(dest, *m_posCur);			// deflate
	}
	else	// COMP_UNKNOWN
	{
		// alzip 5.6 부터 추가된 포맷(5.5 에서는 풀지 못한다. 영문 5.51 은 푼다 ) 
		// 하지만 어떤 버전에서 이 포맷을 만들어 내는지 정확히 알 수 없다.
		// 공식으로 릴리즈된 알집은 이 포맷을 만들어내지 않는다. 비공식(베타?)으로 배포된 버전에서만 이 포맷을 만들어낸다.
		m_nErr = ERR_UNKNOWN_COMPRESSION_METHOD;
		ASSERT(0);										
		ret = FALSE;
	}
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          DEFLATE 로 풀기 - 테스트용 함수. 모든 파일을 한꺼번에 읽어서 푼다. 실제 사용 안함.
/// @param   fp  - 대상 파일
/// @param   file  - 소스 파일 정보
/// @return  
/// @date    2004-03-06 오후 11:09:17
////////////////////////////////////////////////////////////////////////////////////////////////////
/*
BOOL CUnAlz::ExtractDeflate(FILE* fp, SAlzLocalFileHeader& file)
{
	z_stream	stream;
	BYTE*		pInBuf=NULL;
	BYTE*		pOutBuf=NULL;
	int			nInBufSize = file.compressedSize;
	int			nOutBufSize = file.uncompressedSize;
	int			err;
	int			flush=Z_SYNC_FLUSH;
	BOOL		ret = FALSE;

	memset(&stream, 0, sizeof(stream));

	pInBuf = (BYTE*)malloc(nInBufSize);
	if(pInBuf==NULL)
	{
		m_nErr = ERR_MEM_ALLOC_FAILED;
		goto END;
	}

	pOutBuf = (BYTE*)malloc(nOutBufSize);
	if(pOutBuf==NULL)
	{
		m_nErr = ERR_MEM_ALLOC_FAILED;
		goto END;
	}

	// 한번에 읽어서
	fseek(m_fp, file.dwFileDataPos, SEEK_SET);
	if(FRead(pInBuf, nInBufSize, 1, m_fp)!=1)
	{
		m_nErr = ERR_FILE_READ_ERROR;
		goto END;
	}

	// 초기화..
	inflateInit2(&stream, -MAX_WBITS);

	stream.next_out = pOutBuf;
	stream.avail_out = nOutBufSize;
	stream.next_in = pInBuf;
	stream.avail_in = nInBufSize;

	err = inflate(&stream, flush);

	if(err!=Z_OK && err!=Z_STREAM_END )
	{
		m_nErr = ERR_INFLATE_FAILED;
		goto END;
	}

	fwrite(pOutBuf, 1, nOutBufSize, fp);

	ret = TRUE;

END :
	inflateEnd(&stream);

	if(pInBuf) free(pInBuf);
	if(pOutBuf) free(pOutBuf);
	return ret;
}
*/

////////////////////////////////////////////////////////////////////////////////////////////////////
///          대상 폴더에 현재 압축파일을 전부 풀기
/// @param   szDestPathName  - 대상 경로
/// @return  
/// @date    2004-03-06 오후 11:09:49
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::ExtractAll(const char* szDestPathName)
{
	FileList::iterator	i;

	for(i=m_fileList.begin(); i<m_fileList.end(); i++)
	{
		m_posCur = i;
		if(ExtractCurrentFile(szDestPathName)==FALSE) return FALSE;
		if(m_bHalt) 
			break;							// 멈추기..
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          대상 경로 파기 - 압축 파일 내에 폴더 정보가 있을 경우, 다중 폴더를 판다(dig)
/// @param   szPathName  
/// @return  
/// @date    2004-03-06 오후 11:10:12
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::DigPath(const char* szPathName)
{
	char*	dup = strdup(szPathName);
	char	seps[]   = "/\\";
	char*	token;
	char	path[MAX_PATH] = {0};
	char*	last;

	// 경로만 뽑기.
	last = dup + strlen(dup);
	while(last!=dup)
	{
		if(*last=='/' || *last=='\\') 
		{
			*last = (char)NULL;
			break;
		}
		last --;
	}

	
	token = strtok( dup, seps );
	while( token != NULL )
	{
		if(strlen(path)==0)
		{
			if(szPathName[0]=='/')			// is absolute path ?
				safe_strcpy(path,"/", MAX_PATH);
			else if(szPathName[0]=='\\' && szPathName[1]=='\\')	// network drive ?
				safe_strcpy(path,"\\\\", MAX_PATH);
			safe_strcat(path, token, MAX_PATH);
		}
		else
		{
			safe_strcat(path, PATHSEP,MAX_PATH);
			safe_strcat(path, token,MAX_PATH);
		}

		if(IsFolder(path)==FALSE)
#ifdef _WIN32
			_mkdir(path);
#else
			mkdir(path,  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif
//printf("path:%s\n", path);
		token = strtok( NULL, seps );
	}

	free(dup);
	if(IsFolder(szPathName)) return TRUE;
	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          제대로된 폴더 인가?
/// @param   szPathName  
/// @return  
/// @date    2004-03-06 오후 11:03:26
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::IsFolder(const CHAR* szPathName)
{
#ifdef _WIN32
	UINT32 dwRet;
	dwRet = GetFileAttributesA(szPathName);
	if(dwRet==0xffffffff) return FALSE;
	if(dwRet & FILE_ATTRIBUTE_DIRECTORY) return TRUE;
	return FALSE;
#else

	struct stat buf;
	int result;

	result = stat(szPathName, &buf);
	if(result!=0) return FALSE;
//printf("isfolder:%s, %d,%d,%d\n", szPathName, buf.st_mode, S_IFDIR, buf.st_mode & S_IFDIR);
	if(buf.st_mode & S_IFDIR) return TRUE;
	return FALSE;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          압축을 풀 대상에 압축을 푼다.
/// @param   dest  - 대상 OBJECT
/// @param   buf  - 풀린 데이타
/// @param   nSize  - 데이타의 크기
/// @return  쓴 바이트수. 에러시 -1 리턴
/// @date    2004-03-07 오전 12:37:41
////////////////////////////////////////////////////////////////////////////////////////////////////
int	CUnAlz::WriteToDest(SExtractDest* dest, BYTE* buf, int nSize)
{
	if(dest->nType==ET_FILE)
	{
		return fwrite(buf,  1, nSize, dest->fp);
	}
	else if(dest->nType==ET_MEM)
	{
		if(dest->buf==NULL) return nSize;			// 대상이 NULL 이다... 압축푸는 시늉만 한다..
		if(dest->bufpos+nSize >dest->bufsize)		// 에러.. 버퍼가 넘쳤다.
		{
			ASSERT(0);
			return -1;
		}
		// memcpy
		memcpy(dest->buf + dest->bufpos, buf, nSize);
		dest->bufpos += nSize;
		return nSize;
	}
	else
	{
		ASSERT(0);
	}
	return -1;
}

/* 실패한 방법.. 고생한게 아까워서 못지움.
#define ALZDLZ_HEADER_SIZE	4		// alz 파일의 bzip2 헤더 크기
#define BZIP2_HEADER_SIZE	10		// bzip 파일의 헤더 크기
#define BZIP2_CRC_SIZE		4		// bzip2 의 crc
#define BZIP2_TAIL_SIZE		10		// 대충 4+5 정도.?
BYTE bzip2Header[BZIP2_HEADER_SIZE] = {0x42, 0x5a, 0x68, 0x39, 0x31, 0x41, 0x59, 0x26, 0x53, 0x59};

BOOL CUnAlz::ExtractBzip2_bak(FILE* fp, SAlzLocalFileHeader& file)
{
	bz_stream	stream;
	BYTE*		pInBuf=NULL;
	BYTE*		pOutBuf=NULL;
	int			nInBufSize = file.compressedSize;
	int			nOutBufSize = file.uncompressedSize;
	//int			err;
	int			flush=Z_SYNC_FLUSH;
	BOOL		ret = FALSE;
	UINT32 crc = 0xffffffff;

	//BYTE		temp[100];
	
	memset(&stream, 0, sizeof(stream));

	pInBuf = (BYTE*)malloc(nInBufSize + BZIP2_HEADER_SIZE + BZIP2_CRC_SIZE - ALZDLZ_HEADER_SIZE + BZIP2_TAIL_SIZE);
	if(pInBuf==NULL)
	{
		m_nErr = ERR_MEM_ALLOC_FAILED;
		goto END;
	}

	pOutBuf = (BYTE*)malloc(nOutBufSize);
	if(pOutBuf==NULL)
	{
		m_nErr = ERR_MEM_ALLOC_FAILED;
		goto END;
	}

	// ALZ 의 BZIP 헤더 ("DLZ.") 스킵하기.
	fseek(m_fp, ALZDLZ_HEADER_SIZE, SEEK_CUR);
	// BZIP2 헤더 삽입
	memcpy(pInBuf, bzip2Header, BZIP2_HEADER_SIZE);
	// BZIP2 CRC
	memcpy(pInBuf+BZIP2_HEADER_SIZE, &(crc), BZIP2_CRC_SIZE);

	// 진짜 압축된 데이타를  한번에 읽어서
	fseek(m_fp, file.dwFileDataPos+ALZDLZ_HEADER_SIZE, SEEK_SET);
	if(FRead(pInBuf+BZIP2_HEADER_SIZE+BZIP2_CRC_SIZE, nInBufSize-ALZDLZ_HEADER_SIZE, 1, m_fp)!=1)
	{
		m_nErr = ERR_FILE_READ_ERROR;
		goto END;
	}


	// 초기화..
	stream.bzalloc = NULL;
	stream.bzfree = NULL;
	stream.opaque = NULL;
	ret = BZ2_bzDecompressInit ( &stream, 3,0 );
	if (ret != BZ_OK) goto END;


	//memcpy(temp, pInBuf, 100);

	stream.next_in = (char*)pInBuf;
	stream.next_out = (char*)pOutBuf;
	stream.avail_in = nInBufSize+BZIP2_HEADER_SIZE+BZIP2_CRC_SIZE+BZIP2_TAIL_SIZE;
	stream.avail_out = nOutBufSize;


	ret = BZ2_bzDecompress ( &stream );

	// BZ_DATA_ERROR 가 리턴될 수 있다..
	//if (ret == BZ_OK) goto END;
	//if (ret != BZ_STREAM_END) goto END;

	BZ2_bzDecompressEnd(&stream);

	fwrite(pOutBuf, 1, nOutBufSize, fp);

	ret = TRUE;

END :

	if(pInBuf) free(pInBuf);
	if(pOutBuf) free(pOutBuf);

	if(ret==FALSE) 	BZ2_bzDecompressEnd(&stream);

	return ret;
}
*/

////////////////////////////////////////////////////////////////////////////////////////////////////
///          RAW 로 압축된 파일 풀기
/// @param   fp  - 대상 파일
/// @param   file  - 소스 파일
/// @return  
/// @date    2004-03-06 오후 11:10:53
////////////////////////////////////////////////////////////////////////////////////////////////////
#define BUF_LEN		(4096*2)
BOOL CUnAlz::ExtractRawfile(SExtractDest* dest, SAlzLocalFileHeader& file)
{
	BOOL		ret = FALSE;
	BYTE		buf[BUF_LEN];
	INT64		read;
	INT64		sizeToRead;
	INT64		bufLen = BUF_LEN;
	INT64		nWritten = 0;
	BOOL		bHalt = FALSE;
	BOOL		bIsEncrypted = 	IsEncryptedFile();		// 암호걸린 파일인가?
	UINT32		dwCRC32= 0;



	// 위치 잡고.
	FSeek(file.dwFileDataPos);

	sizeToRead = file.compressedSize;			// 읽을 크기.

	m_nErr = ERR_NOERR;
	while(sizeToRead)
	{
		read = min(sizeToRead, bufLen);
		if(FRead(buf, (int)read)==FALSE)
		{
			break;
		}

		if(bIsEncrypted)
			DecryptingData((int)read, buf); // xf86

		dwCRC32 = crc32(dwCRC32, buf, (UINT)(read));

		WriteToDest(dest, buf, (int)read);
		//fwrite(buf, read, 1, fp);
		sizeToRead -= read;

		nWritten+=read;

		// progress callback
		if(m_pFuncCallBack)
		{
			m_pFuncCallBack(NULL, nWritten, file.uncompressedSize, m_pCallbackParam, &bHalt);
			if(bHalt) 
			{
				break;
			}
		}
	}

	m_bHalt = bHalt;

	if(m_nErr==ERR_NOERR)			// 성공적으로 압축을 풀었다.. CRC 검사하기..
	{
		if(file.fileCRC==dwCRC32)
		{
			ret = TRUE;
		}
		else
		{
			m_nErr = ERR_INVALID_FILE_CRC;
		}
	}


	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          BZIP2 압축 풀기..
/// @param   fp_w  - 대상 파일
/// @param   file  - 소스 파일 정보
/// @return  
/// @date    2004-03-01 오전 5:47:36
////////////////////////////////////////////////////////////////////////////////////////////////////
#define BZIP2_EXTRACT_BUF_SIZE	0x2000
BOOL CUnAlz::ExtractBzip2(SExtractDest* dest, SAlzLocalFileHeader& file)
{
	BZFILE		*bzfp = NULL;
	int			smallMode = 0;
	int			verbosity = 1;
	int			bzerr;
	INT64		len;
	BYTE		buff[BZIP2_EXTRACT_BUF_SIZE];
	INT64		nWritten = 0;
	BOOL		bHalt = FALSE;
	UINT32		dwCRC32= 0;
	BOOL		ret = FALSE;

	FSeek(file.dwFileDataPos);

	bzfp = BZ2_bzReadOpen(&bzerr,this,verbosity,smallMode,0,0);

	if(bzfp==NULL){ASSERT(0); return FALSE;}

	m_nErr = ERR_NOERR;
	while((len=BZ2_bzread(bzfp,buff,BZIP2_EXTRACT_BUF_SIZE))>0)
	{
		WriteToDest(dest, (BYTE*)buff, (int)len);
		//fwrite(buff,1,len,fp_w);

		dwCRC32 = crc32(dwCRC32,buff, (UINT)(len));


		nWritten+=len;

		// progress callback
		if(m_pFuncCallBack)
		{
			m_pFuncCallBack(NULL, nWritten, file.uncompressedSize, m_pCallbackParam, &bHalt);
			if(bHalt) 
			{
				break;
			}
		}
	}

	if(len<0)			// 에러 상황..
	{
		m_nErr = ERR_INFLATE_FAILED;
	}

	BZ2_bzReadClose( &bzerr, bzfp);

	m_bHalt = bHalt;

	if(m_nErr==ERR_NOERR)			// 성공적으로 압축을 풀었다.. CRC 검사하기..
	{
		if(file.fileCRC==dwCRC32)
		{
			ret = TRUE;
		}
		else
		{
			m_nErr = ERR_INVALID_FILE_CRC;
		}
	}


	/*		
	// FILE* 를 사용할경우 사용하던 코드. - 멀티 볼륨 지원 안함..
	BZFILE	*bzfp = NULL;
	int		smallMode = 0;
	int		verbosity = 1;
	int		bzerr;
	int		len;
	char	buff[BZIP2_EXTRACT_BUF_SIZE];
	INT64	nWritten = 0;
	BOOL	bHalt = FALSE;

	FSeek(file.dwFileDataPos, SEEK_SET);

	bzfp = BZ2_bzReadOpen(&bzerr,m_fp,verbosity,smallMode,0,0);

	while((len=BZ2_bzread(bzfp,buff,BZIP2_EXTRACT_BUF_SIZE))>0)
	{
		WriteToDest(dest, (BYTE*)buff, len);
		//fwrite(buff,1,len,fp_w);

		nWritten+=len;

		// progress callback
		if(m_pFuncCallBack)
		{
			m_pFuncCallBack(NULL, nWritten, file.uncompressedSize, m_pCallbackParam, &bHalt);
			if(bHalt) 
			{
				break;
			}
		}
	}

	BZ2_bzReadClose( &bzerr, bzfp);

	m_bHalt = bHalt;
	*/

	return ret;
}


#ifndef UNZ_BUFSIZE
#define UNZ_BUFSIZE		0x1000 // (16384)
#endif

#define IN_BUF_SIZE		UNZ_BUFSIZE
#define OUT_BUF_SIZE	0x1000 //IN_BUF_SIZE

////////////////////////////////////////////////////////////////////////////////////////////////////
///          deflate 로 압축 풀기. ExtractDeflate() 와 달리 조금씩 읽어서 푼다.
/// @param   fp  
/// @param   file  
/// @return  
/// @date    2004-03-06 오후 11:11:36
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::ExtractDeflate2(SExtractDest* dest, SAlzLocalFileHeader& file)
{
	z_stream	stream;
	BYTE		pInBuf[IN_BUF_SIZE];
	BYTE		pOutBuf[OUT_BUF_SIZE];
	int			nInBufSize = IN_BUF_SIZE;
	int			nOutBufSize = OUT_BUF_SIZE;
	int			err;
	int			flush=Z_SYNC_FLUSH;
	BOOL		ret = FALSE;
	INT64		nRestReadCompressed;
	UINT32		dwCRC32= 0;
	INT64		rest_read_uncompressed;
	UINT		iRead = 0;
	INT64	nWritten = 0;
	BOOL	bHalt = FALSE;
	BOOL	bIsEncrypted = 	IsEncryptedFile();		// 암호걸린 파일인가?

	memset(&stream, 0, sizeof(stream));

	FSeek(file.dwFileDataPos);

	inflateInit2(&stream, -MAX_WBITS);
	nRestReadCompressed = file.compressedSize;
	rest_read_uncompressed = file.uncompressedSize;

	// 출력 부분.
	stream.next_out = pOutBuf;
	stream.avail_out = OUT_BUF_SIZE;

	m_nErr = ERR_NOERR;
	while(stream.avail_out>0)
	{
		if(stream.avail_in==0 && nRestReadCompressed>0)
		{
			UINT uReadThis = UNZ_BUFSIZE;
			if (nRestReadCompressed<(int)uReadThis)
				uReadThis = (UINT)nRestReadCompressed;		// 읽을 크기.

			if (uReadThis == 0)
				break;					// 중지

			if(FRead(pInBuf, uReadThis)==FALSE)
			{
				m_nErr = ERR_CANT_READ_FILE;
				goto END;
			}
			
			if(bIsEncrypted)
				DecryptingData(uReadThis, pInBuf); // xf86

//			dwCRC32 = crc32(dwCRC32,pInBuf, (UINT)(uReadThis));
			
			nRestReadCompressed -= uReadThis;
			stream.next_in = pInBuf;
			stream.avail_in = uReadThis;
		}

        UINT uTotalOutBefore,uTotalOutAfter;
        const BYTE *bufBefore;
        UINT uOutThis;
        int flush=Z_SYNC_FLUSH;

        uTotalOutBefore = stream.total_out;
        bufBefore = stream.next_out;

        err=inflate(&stream,flush);

        uTotalOutAfter = stream.total_out;
        uOutThis = uTotalOutAfter-uTotalOutBefore;
        
        dwCRC32 = crc32(dwCRC32,bufBefore, (UINT)(uOutThis));

        rest_read_uncompressed -= uOutThis;

        iRead += (UINT)(uTotalOutAfter - uTotalOutBefore);
		
		WriteToDest(dest, pOutBuf, uOutThis);
		//fwrite(pOutBuf, uOutThis, 1, fp);		// file 에 쓰기.
		stream.next_out = pOutBuf;
		stream.avail_out = OUT_BUF_SIZE;

		nWritten+=uOutThis;

		// progress callback
		if(m_pFuncCallBack)
		{
			m_pFuncCallBack(NULL, nWritten, file.uncompressedSize, m_pCallbackParam, &bHalt);
			if(bHalt) 
			{
				m_nErr = ERR_USER_ABORTED;
				break;
			}
		}

        if (err==Z_STREAM_END)
			break;
			//if(iRead==0) break; // UNZ_EOF;
        if (err!=Z_OK) 
		{
			m_nErr = ERR_INFLATE_FAILED;
            goto END;
		}
	}
	m_bHalt = bHalt;


	if(m_nErr==ERR_NOERR)			// 성공적으로 압축을 풀었다.. CRC 검사하기..
	{
		if(file.fileCRC==dwCRC32)
		{
			ret = TRUE;
		}
		else
		{
			m_nErr = ERR_INVALID_FILE_CRC;
		}
	}


END :
	inflateEnd(&stream);

	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          파일 열기
/// @param   szPathName  
/// @return  
/// @date    2004-10-02 오후 11:47:14
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::FOpen(const char* szPathName)
{
	char* temp = strdup(szPathName);			// 파일명 복사..
	int	  i;
	int	  nLen = strlen(szPathName);
	UINT64 nFileSizeLow;
	UINT32 dwFileSizeHigh;
	m_nFileCount = 0;
	m_nCurFile = 0;
	m_nVirtualFilePos = 0;
	m_nCurFilePos = 0;
	m_bIsEOF = FALSE;
	for(i=0;i<MAX_FILES;i++)						// aa.alz 파일명을 가지고 aa.a00 aa.a01 aa.a02 .. 만들기
	{
		if(i>0) 
			safe_sprintf(temp+nLen-3, 4, "%c%02d", (i-1)/100+'a', (i-1)%100);

#ifdef _WIN32
		m_files[i].fp = CreateFileA(temp, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if(m_files[i].fp==INVALID_HANDLE_VALUE) break;
		nFileSizeLow = GetFileSize(m_files[i].fp, (DWORD*)&dwFileSizeHigh);
#else
		m_files[i].fp = fopen(temp, "rb");
		if(m_files[i].fp==NULL) break;
		dwFileSizeHigh=0;
		unalz_fseek(m_files[i].fp,0,SEEK_END);		
		nFileSizeLow=unalz_ftell(m_files[i].fp);
		unalz_fseek(m_files[i].fp,0,SEEK_SET);
#endif
		m_nFileCount++;
		m_files[i].nFileSize = ((INT64)nFileSizeLow) + (((INT64)dwFileSizeHigh)<<32);
		if(i==0) m_files[i].nMultivolHeaderSize = 0;
		else m_files[i].nMultivolHeaderSize = MULTIVOL_HEAD_SIZE;
		m_files[i].nMultivolTailSize = MULTIVOL_TAIL_SIZE;
	}
	free(temp);
	if(m_nFileCount==0) return FALSE;
	m_files[m_nFileCount-1].nMultivolTailSize = 0;			// 마지막 파일은 꼴랑지가 없다..
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          파일 닫기
/// @return  
/// @date    2004-10-02 오후 11:48:53
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnAlz::FClose()
{
	int i;
#ifdef _WIN32
	for(i=0;i<m_nFileCount;i++) CloseHandle(m_files[i].fp);
#else
	for(i=0;i<m_nFileCount;i++) fclose(m_files[i].fp);
#endif
	memset(m_files, 0, sizeof(m_files));
	m_nFileCount = 0;
	m_nCurFile = -1;
	m_nVirtualFilePos = 0;
	m_nCurFilePos = 0;
	m_bIsEOF = FALSE;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          파일의 끝인가?
/// @return  
/// @date    2004-10-02 오후 11:48:21
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::FEof()
{
	return m_bIsEOF;
	/*
	if(m_fp==NULL){ASSERT(0); return TRUE;}
	if(feof(m_fp)) return TRUE;
	return FALSE;
	*/
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///          현재 파일 위치
/// @return  
/// @date    2004-10-02 오후 11:50:50
////////////////////////////////////////////////////////////////////////////////////////////////////
INT64 CUnAlz::FTell()
{
	return m_nVirtualFilePos;	//	return ftell(m_fp);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          파일 위치 세팅
/// @param   offset  
/// @param   origin  
/// @return  
/// @date    2004-10-02 오후 11:51:53
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::FSeek(INT64 offset)
{
	m_nVirtualFilePos = offset;
	int		i;
	INT64	remain=offset;
	LONG	remainHigh;

	m_bIsEOF = FALSE;

	for(i=0;i<m_nFileCount;i++)			// 앞에서 루프를 돌면서 위치 선정하기..
	{
		if(remain<=m_files[i].nFileSize-m_files[i].nMultivolHeaderSize-m_files[i].nMultivolTailSize)
		{
			m_nCurFile = i;
			m_nCurFilePos = remain+m_files[i].nMultivolHeaderSize;			// 물리적 위치.
			remainHigh = (LONG)((m_nCurFilePos>>32)&0xffffffff);
#ifdef _WIN32
			SetFilePointer(m_files[i].fp, LONG(m_nCurFilePos), &remainHigh, FILE_BEGIN);
#else
			unalz_fseek(m_files[i].fp, m_nCurFilePos, SEEK_SET);
#endif
			return TRUE;
		}
		remain -= (m_files[i].nFileSize-m_files[i].nMultivolHeaderSize-m_files[i].nMultivolTailSize);
	}

	// 실패..?
	ASSERT(0);
	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          파일 읽기
/// @param   buffer  
/// @param   size  
/// @param   count  
/// @return  
/// @date    2004-10-02 오후 11:44:05
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::FRead(void* buffer, UINT32 nBytesToRead, int* pTotRead )
{
	BOOL ret;
	UINT32 nNumOfBytesRead;
	INT64 dwRemain;
	UINT32 dwRead;
	UINT32 dwTotRead;

	dwRemain = nBytesToRead;
	dwTotRead = 0;
	if(pTotRead) *pTotRead=0;

	while(dwRemain)
	{
		dwRead = (UINT32)min(dwRemain, (m_files[m_nCurFile].nFileSize-m_nCurFilePos-m_files[m_nCurFile].nMultivolTailSize));
		if(dwRead==0) {
			m_bIsEOF = TRUE;return FALSE;
		}
#ifdef _WIN32
		ret = ReadFile(m_files[m_nCurFile].fp, ((BYTE*)buffer)+dwTotRead, dwRead, (DWORD*)&nNumOfBytesRead, NULL);
		if(ret==FALSE && GetLastError()==ERROR_HANDLE_EOF) 
		{
			m_bIsEOF = TRUE;return FALSE;
		}

#else
		nNumOfBytesRead = fread(((BYTE*)buffer)+dwTotRead, 1,dwRead ,m_files[m_nCurFile].fp);
		if(nNumOfBytesRead<=0) 
		{
			m_bIsEOF = TRUE;return FALSE;
		}
		ret=TRUE;
#endif
		if(dwRead!=nNumOfBytesRead)					// 발생하면 안된다..
		{
			ASSERT(0); return FALSE;
		}

		m_nVirtualFilePos += nNumOfBytesRead;	// virtual 파일 위치..

		m_nCurFilePos+=nNumOfBytesRead;					// 물리적 파일 위치.
		dwRemain-=nNumOfBytesRead;
		dwTotRead+=nNumOfBytesRead;
		if(pTotRead) *pTotRead=dwTotRead;

		if(m_nCurFilePos==m_files[m_nCurFile].nFileSize-m_files[m_nCurFile].nMultivolTailSize)	// overflow
		{
			m_nCurFile++;

#ifdef _WIN32
			if(m_files[m_nCurFile].fp==INVALID_HANDLE_VALUE)
#else
			if(m_files[m_nCurFile].fp==NULL)
#endif
			{
				m_bIsEOF = TRUE;
				if(dwRemain==0) return TRUE;						// 완전히 끝까지 읽었다..
				return FALSE;
			}

			m_nCurFilePos = m_files[m_nCurFile].nMultivolHeaderSize;					// header skip
#ifdef _WIN32
			SetFilePointer(m_files[m_nCurFile].fp, (int)m_nCurFilePos, NULL, FILE_BEGIN);
#else
			unalz_fseek(m_files[m_nCurFile].fp, m_nCurFilePos, SEEK_SET);
#endif
		}
		else
			if(m_nCurFilePos>m_files[m_nCurFile].nFileSize-m_files[m_nCurFile].nMultivolTailSize) ASSERT(0);
	}
	
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          error code 를 스트링으로 바꿔 준다.
/// @param   nERR  
/// @return  
/// @date    2004-10-24 오후 3:28:39
////////////////////////////////////////////////////////////////////////////////////////////////////
const char* CUnAlz::LastErrToStr(ERR nERR)
{
	if(nERR>= sizeof(errorstrtable)/sizeof(errorstrtable[0])) {ASSERT(0); return NULL; }
	return errorstrtable[nERR];
}


// by xf86
BOOL	CUnAlz::chkValidPassword()
{
	if(IsEncryptedFile()==FALSE) {return TRUE;}

	if (getPasswordLen() == 0){
		m_nErr = ERR_PASSWD_NOT_SET;
		return FALSE;
	}
	InitCryptKeys(m_szPasswd);
	if(CryptCheck(m_posCur->encChk) == FALSE){
		m_nErr = ERR_INVALID_PASSWD;
		return FALSE;
	}
	return TRUE;
}


/*
////////////////////////////////////////////////////////////////////////////////////////////////////
//	from CZipArchive
//	Copyright (C) 2000 - 2004 Tadeusz Dracz
//
//		http://www.artpol-software.com
//
//	it's under GPL.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnAlz::CryptDecodeBuffer(UINT32 uCount, CHAR *buf)
{
	if (IsEncrypted())
		for (UINT32 i = 0; i < uCount; i++)
			CryptDecode(buf[i]);
}

void CUnAlz::CryptInitKeys()
{
	m_keys[0] = 305419896L;
	m_keys[1] = 591751049L;
	m_keys[2] = 878082192L;
	for (int i = 0; i < strlen(m_szPasswd); i++)
		CryptUpdateKeys(m_szPasswd[i]);
}

void CUnAlz::CryptUpdateKeys(CHAR c)
{
	
	m_keys[0] = CryptCRC32(m_keys[0], c);
	m_keys[1] += m_keys[0] & 0xff;
	m_keys[1] = m_keys[1] * 134775813L + 1;
	c = CHAR(m_keys[1] >> 24);
	m_keys[2] = CryptCRC32(m_keys[2], c);
}

BOOL CUnAlz::CryptCheck(CHAR *buf)
{
	CHAR b = 0;
	for (int i = 0; i < ALZ_ENCR_HEADER_LEN; i++)
	{
		b = buf[i]; 
		CryptDecode((CHAR&)b);
	}

	if (IsDataDescr()) // Data descriptor present
		return CHAR(m_posCur->head.fileTimeDate >> 8) == b;
	else
		return CHAR(m_posCur->maybeCRC >> 24) == b;
}

CHAR CUnAlz::CryptDecryptCHAR()
{
	int temp = (m_keys[2] & 0xffff) | 2;
	return (CHAR)(((temp * (temp ^ 1)) >> 8) & 0xff);
}

void CUnAlz::CryptDecode(CHAR &c)
{
	c ^= CryptDecryptCHAR();
	CryptUpdateKeys(c);
}

UINT32 CUnAlz::CryptCRC32(UINT32 l, CHAR c)
{
	const ULONG *CRC_TABLE = get_crc_table();
	return CRC_TABLE[(l ^ c) & 0xff] ^ (l >> 8);
}
*/

////////////////////////////////////////////////////////////////////////////////////////////////////
///          암호걸린 파일인지 여부
/// @param   fileDescriptor  
/// @return  
/// @date    2004-11-27 오후 11:25:32
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::IsEncryptedFile(BYTE fileDescriptor)
{
	return fileDescriptor&0x01;
}
BOOL CUnAlz::IsEncryptedFile()
{
	return m_posCur->head.fileDescriptor&0x01;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          암호로 키 초기화
/// @param   szPassword  
/// @return  
/// @date    2004-11-27 오후 11:04:01
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnAlz::InitCryptKeys(const CHAR* szPassword)
{
	m_key[0] = 305419896;
	m_key[1] = 591751049;
	m_key[2] = 878082192;

	int i;
	for(i=0;i<(int)strlen(szPassword);i++)
	{
		UpdateKeys(szPassword[i]);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          데이타로 키 업데이트하기
/// @param   c  
/// @return  
/// @date    2004-11-27 오후 11:04:09
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnAlz::UpdateKeys(BYTE c)
{
	m_key[0] = CRC32(m_key[0], c);
	m_key[1] = m_key[1]+(m_key[0]&0x000000ff);
	m_key[1] = m_key[1]*134775813+1;
	m_key[2] = CRC32(m_key[2],m_key[1]>>24);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          암호가 맞는지 헤더 체크하기 
/// @param   buf  
/// @return  
/// @date    2004-11-27 오후 11:04:24
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CUnAlz::CryptCheck(const BYTE* buf)
{
	int i;
	BYTE c;
	BYTE temp[ALZ_ENCR_HEADER_LEN];

	memcpy(temp, buf, ALZ_ENCR_HEADER_LEN);		// 임시 복사.

	for(i=0;i<ALZ_ENCR_HEADER_LEN;i++)
	{
		c = temp[i] ^ DecryptByte();
		UpdateKeys(c);
		temp[i] = c;
	}

	if (IsDataDescr()) // Data descriptor present
		return (m_posCur->head.fileTimeDate >> 8) == c;
	else
		return ( ((m_posCur->fileCRC)>>24) ) == c;		// 파일 crc 의 최상위 byte 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          키에서 데이타 추출
/// @return  
/// @date    2004-11-27 오후 11:05:36
////////////////////////////////////////////////////////////////////////////////////////////////////
BYTE CUnAlz::DecryptByte()
{
	UINT16 temp;
	temp = m_key[2] | 2;
	return (temp * (temp^1))>>8;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          데이타 압축 풀기
/// @param   nSize  
/// @param   data  
/// @return  
/// @date    2004-11-27 오후 11:03:30
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnAlz::DecryptingData(int nSize, BYTE* data)
{
	BYTE* p = data;
	BYTE temp;

	while(nSize)
	{
		temp = *p ^ DecryptByte();
		UpdateKeys(temp);
		*p = temp;
		nSize--;
		p++;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///          CRC 테이블 참조
/// @param   l  
/// @param   c  
/// @return  
/// @date    2004-11-27 오후 11:14:16
////////////////////////////////////////////////////////////////////////////////////////////////////
UINT32 CUnAlz::CRC32(UINT32 l, BYTE c)
{
	const z_crc_t *CRC_TABLE = get_crc_table();
	return CRC_TABLE[(l ^ c) & 0xff] ^ (l >> 8);
}

void CUnAlz::SetPassword(char *passwd) 
{ 
	if(strlen(passwd) == 0) return; 
	safe_strcpy(m_szPasswd, passwd, UNALZ_LEN_PASSWORD); 
}

#ifdef _UNALZ_ICONV
void CUnAlz::SetDestCodepage(const char* szToCodepage)
{
	safe_strcpy(m_szToCodepage, szToCodepage, UNALZ_LEN_CODEPAGE); 
}
#endif



////////////////////////////////////////////////////////////////////////////////////////////////////
///          문자열 처리 함수들
/// @param   l  
/// @param   c  
/// @return  
/// @date    2007-02
////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int CUnAlz::_strlcpy (char *dest, const char *src, unsigned int size)
{
	register unsigned int i = 0;
	if (size > 0) {
	size--;
	for (i=0; size > 0 && src[i] != '\0'; ++i, size--)
		dest[i] = src[i];
	dest[i] = '\0';
	}
	while (src[i++]);
	return i;
}
unsigned int CUnAlz::_strlcat (char *dest, const char *src, unsigned int size)
{
	register char *d = dest;
	for (; size > 0 && *d != '\0'; size--, d++);
	return (d - dest) + _strlcpy(d, src, size);
}

// 안전한 strcpy
void CUnAlz::safe_strcpy(char* dst, const char* src, size_t dst_size)
{
#ifdef _WIN32
	lstrcpynA(dst, src, dst_size);
#else
	_strlcpy(dst, src, dst_size);
#endif
}

void CUnAlz::safe_strcat(char* dst, const char* src, size_t dst_size)
{
#ifdef _WIN32
	StringCchCatExA(dst, dst_size, src, NULL, NULL, STRSAFE_FILL_BEHIND_NULL);
	//lstrcatA(dst, src);			// not safe!!
#else
	_strlcat(dst, src, dst_size);
#endif
}

