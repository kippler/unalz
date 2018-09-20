/*
  UNALZ : read and extract module for ALZ format.

  LICENSE (zlib License)
  Copyright (C) 2004-2009 kippler@gmail.com , http://www.kipple.pe.kr

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  이 소프트웨어는 어떠한 명시적 또는 묵시적 보증도 없이 "있는 그대로" 제공됩니다. 그 
  어떤 경우에도 작성자는 이 소프트웨어의 사용으로 인한 손해에 대해 책임을 지지 않습니다.

  다음 제한 규정을 준수하는 경우에 한하여 상업적인 응용 프로그램을 포함하는 모든 용도로 이 소프트웨어를 
  사용하고 자유롭게 수정 및 재배포할 수 있는 권한이 누구에게나 부여됩니다.

  1. 이 소프트웨어의 출처를 잘못 표시하거나 원래 소프트웨어를 자신이 작성했다고 주장해서는 안 됩니다. 제품에 
     이 소프트웨어를 사용하는 경우 요구 사항은 아니지만 제품 설명서에 인정 조항을 넣어 주시면 감사하겠습니다.
  2. 수정된 소스 버전은 반드시 명확하게 표시되어야 하며 원래 소프트웨어로 오인되도록 잘못 표시해서는 안 됩니다.
  3. 모든 소스 배포 시 이 공지를 삭제하거나 수정할 수 없습니다.

  =============================================================================================================
*/


#ifdef _WIN32
#	pragma warning( disable : 4786 )		// stl warning 없애기
#	pragma warning( disable : 4996 )		// crt secure warning
#endif

#include <stdio.h>
#include <iostream>
#include <time.h>
#include <string>
#include <vector>
#include <locale.h>
#include "UnAlz.h"
#include "UnAlzUtils.h"

#include <sys/timeb.h>


BOOL	g_bPipeMode=FALSE;

void Copyright()
{
//	printf("unalz v0.20 (2004/10/22) \n");
//	printf("unalz v0.22 (2004/10/27) \n");
//	printf("unalz v0.23 (2004/10/30) \n");
//	printf("unalz v0.31 (2004/11/27) \n");
//	printf("unalz v0.4 (2005/06/18) \n");
//	printf("unalz v0.5 (2005/07/09) \n");
//	printf("unalz v0.51 (2005/07/24) \n");
//	printf("unalz v0.52 (2005/07/27) \n");
//	printf("unalz v0.53 (2005/10/15) \n");
//	printf("unalz v0.54 (2005/11/21) \n");
//	printf("unalz v0.55 (2006/03/10) \n");
//	printf("unalz v0.60 (2006/12/31) \n");
//	printf("unalz v0.61 (2007/02/10) \n");
//	printf("unalz v0.62 (2008/04/04) \n");
//	printf("unalz v0.63 (2009/01/09) \n");
//	printf("unalz v0.64 (2009/01/20) \n");
	printf("unalz v0.65 (2009/04/01) \n");
	printf("Copyright(C) 2004-2009 by kippler@gmail.com (kippler.com) \n");
}


void Usage()
{
	Copyright();

	printf("\n");
	/*
#ifdef _UNALZ_ICONV
		printf("USAGE : unalz [ -utf8 | -cp949 | -euc-kr ] sourcefile.alz [dest path] \n");
#	ifdef _UNALZ_UTF8
		printf("        -utf8  : convert filename's codepage to UTF-8 (default)\n");
		printf("        -cp949 : convert filename's codepage to CP949\n");
		printf("        -euc-kr: convert filename's codepage to EUC-KR\n");
#	else
		printf("        -utf8  : convert filename's codepage to UTF-8\n");
		printf("        -cp949 : convert filename's codepage to CP949 (default)\n");
		printf("        -euc-kr: convert filename's codepage to EUC-KR\n");
#	endif // _UNALZ_UTF8
#else		// no iconv
	printf("USAGE : unalz sourcefile.alz [dest path] \n");
#endif // _UNALZ_ICONV
	*/

	printf("Usage : unalz [<switches>...] archive.alz [<file names to extract>...]\n");

	printf("\n");
	printf("<switches>\n");
#ifdef _UNALZ_ICONV
#	ifdef _UNALZ_UTF8
		printf("  -utf8        : convert filename's codepage to UTF-8 (default)\n");
		printf("  -cp949       : convert filename's codepage to CP949\n");
		printf("  -euc-kr      : convert filename's codepage to EUC-KR\n");
#	else
		printf("  -utf8        : convert filename's codepage to UTF-8\n");
		printf("  -cp949       : convert filename's codepage to CP949 (default)\n");
		printf("  -euc-kr      : convert filename's codepage to EUC-KR\n");
#	endif // _UNALZ_UTF8
#endif // _UNALZ_ICONV
		printf("  -l           : list contents of archive\n");
		printf("  -d directory : set output directory\n");
		printf("  -p           : extract files to pipe, no messages\n");
		printf("  -pwd <pwd>   : set password\n");

}





void UnAlzCallback(const char* szFileName, INT64 nCurrent, INT64 nRange, void* param, BOOL* bHalt)
{
	if(g_bPipeMode) return;		// slient

#define MSG_BUF_LEN	1024
	// progress
	static char  szMessage[MSG_BUF_LEN]={0};
	static INT64 nPrevPercent = -1;
	INT64	percent;

	// 파일명 출력..
	if(szFileName)
	{
		printf("\n");
#ifdef _WIN32
		_snprintf(szMessage, MSG_BUF_LEN, "unalziiiing : %s (%I64dbytes) ", szFileName, nRange);
#else
		snprintf(szMessage, MSG_BUF_LEN, "unalziiiing : %s (%lldbytes) ", szFileName, nRange);
#endif
		printf("%s.", szMessage);
		fflush(stdout);
		nPrevPercent = -1;
		return;
	}

	percent = nCurrent*100/nRange;

	if(nPrevPercent/10==percent/10) return; 	// 너무 잦은 업데이트 방지..
	nPrevPercent = percent;

	printf(".");
	fflush(stdout);

}


int main(int argc, char* argv[])
{
	setlocale(LC_ALL, "");

	if(argc<2)
	{
		Usage();
		return 0;
	}

	CUnAlz	unalz;
	char* source=NULL;
	const char* destpath=".";
	const char* destcodepage=NULL;
	char* password=NULL;
	int   count;
	BOOL  listMode = FALSE;
	vector<string>	filelist;

	/*	old method
	for (count=1 ; count < argc && argv[count][0] == '-'; ++count)
	{
#ifdef _UNALZ_ICONV
		// utf8 옵션 처리
		if(strcmp(argv[count], "-utf8")==0)
		{
			destcodepage = "UTF-8";				// utf-8 support
		}
		else if(strcmp(argv[count], "-cp949")==0)
		{
			destcodepage = "CP949";				// cp949 
		}
		else if(strcmp(argv[count], "-euc-kr")==0)
		{
			destcodepage = "EUC-KR";			// EUC-KR
		}
		else
#endif
		if(strcmp(argv[count], "-l")==0 || strcmp(argv[count], "-list")==0)
		{
			listMode = TRUE;
		}
	}

#ifdef _UNALZ_ICONV
	if(count>=argc)	{Usage();return 0;}		// 옵션만 쓰면 어쩌라고..
	if(destcodepage) unalz.SetDestCodepage(destcodepage);
#endif

	// 소스 파일
	source=argv[count];						
	count++;

	// 대상 경로
	if(count<argc)							
	{
		destpath = argv[count];
		count++;
	}
	*/

	for (count=1 ; count < argc; count++)
	{
#ifdef _UNALZ_ICONV
		// utf8 옵션 처리
		if(strcmp(argv[count], "-utf8")==0)
		{
			destcodepage = "UTF-8";				// utf-8 support
		}
		else if(strcmp(argv[count], "-cp949")==0)
		{
			destcodepage = "CP949";				// cp949 
		}
		else if(strcmp(argv[count], "-euc-kr")==0)
		{
			destcodepage = "EUC-KR";			// EUC-KR
		}
		else
#endif
		if(strcmp(argv[count], "-l")==0 || strcmp(argv[count], "-list")==0)
		{
			listMode = TRUE;
		}
		else if(strcmp(argv[count], "-p")==0)
		{
			g_bPipeMode = TRUE;
		}
		else if(strcmp(argv[count], "-d")==0)		// dest dir
		{
			count++;
			if(count>=argc)	{Usage();return 0;}	// dest dir 이 정상 지정되지 않았다..
			destpath = argv[count];
		}
		else if(strcmp(argv[count], "-pwd")==0)		// pwd
		{
			count++;
			if(count>=argc)	{Usage();return 0;}	// dest dir 이 정상 지정되지 않았다..
			password = argv[count];
		}
		else									// 옵션이 아닌 경우
		{
			if(source==NULL)					// 소스 파일 경로
			{
				source=argv[count];							
			}
			else								// 압축풀 파일 
			{
				filelist.push_back(argv[count]);
			}
		}
	}

	if(source==NULL) {Usage();return 0;}		// 옵션만 쓰면 어쩌라고..

#ifdef _UNALZ_ICONV
	if(destcodepage) unalz.SetDestCodepage(destcodepage);
#endif


	if(g_bPipeMode==FALSE)
		Copyright();						// copyright 표시

	// pipe mode setting
	unalz.SetPipeMode(g_bPipeMode);

	// 파일 열기
	if(unalz.Open(source)==FALSE)
	{
		if(unalz.GetLastErr()==CUnAlz::ERR_CORRUPTED_FILE)
		{
			printf("It's corrupted file.\n");		// 그냥 계속 풀기..
		}
		else
		{
			printf("file open error : %s\n", source);
			printf("err code(%d) (%s)\n", unalz.GetLastErr(), unalz.GetLastErrStr());
			return 0;
		}
	}

	if (listMode)
	{
		return ListAlz(&unalz, source);
	}
	else
	{
		if(unalz.IsEncrypted())
		{
			if(password)						// command line 으로 암호가 지정되었을 경우.
			{
				unalz.SetPassword(password);
			}
			else
			{
				char temp[256];
				char pwd[256];
				printf("Enter Password : ");
				fgets(temp,256,stdin);
				sscanf(temp, "%s\n", pwd);		// remove \n
				unalz.SetPassword(pwd);
			}
		}

		if(g_bPipeMode==FALSE)
			printf("\nExtract %s to %s\n", source, destpath);

		// callback 함수 세팅
		unalz.SetCallback(UnAlzCallback, (void*)NULL);

		if (filelist.empty()==false)		// 파일 지정하기.
		{
			vector<string>::iterator i;
			for(i=filelist.begin();i<filelist.end();i++)
			{
				if(unalz.SetCurrentFile(i->c_str())==FALSE)
				{
					if(g_bPipeMode==FALSE)
						printf("filename not matched : %s\n", i->c_str());
				}
				else
					unalz.ExtractCurrentFile(destpath);
			}
		}
		else								// 모든 파일 다풀기.
		{
			if(unalz.ExtractAll(destpath)==FALSE)
			{
				if(g_bPipeMode==FALSE)
				{
					printf("\n");
					printf("extract %s to %s failed.\n", source, destpath);
					printf("err code(%d) (%s)\n", unalz.GetLastErr(),
						   unalz.GetLastErrStr());
				}
			}
		}
		if(g_bPipeMode==FALSE)
			printf("\ndone.\n");
	}

	return 0;
}
