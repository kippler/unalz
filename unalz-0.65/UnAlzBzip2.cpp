////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// bzip2 관련 file i/o 함수들 - 원래 UnAlzbzlib.c (bzlib.c) 에 있던 함수들은 분할 압축 처리를 위해서
/// 약간의 수정을 가한 소스들이다..
/// FILE* handle 관련 부분들이 수정되었다.
/// 
/// @author   kippler@gmail.com
/// @date     2004-10-03 오후 3:09:11
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////

//#include "stdafx.h"
//#include "zlib/zlib.h"

#ifdef _WIN32
# include "bzip2/bzlib.h"
#else
# include <bzlib.h>
#endif

#include "bzip2/bzlib_private.h"


#include "UnAlz.h"


typedef struct {
  CUnAlz*    handle;
  Char      buf[BZ_MAX_UNUSED];
  Int32     bufN;
  Bool      writing;
  bz_stream strm;
  Int32     lastErr;
  Bool      initialisedOk;
}
MybzFile;

#define BZ_SETERR(eee){if (bzerror != NULL) *bzerror = eee;if (bzf != NULL) bzf->lastErr = eee;}



////////////////////////////////////////////////////////////////////////////////////////////////////
///          BZIP2 파일 처리 함수 
/// @param   bzerror  
/// @param   f  
/// @param   verbosity  
/// @param   _small  
/// @param   unused  
/// @param   nUnused  
/// @return  
/// @date    2004-10-03 오전 3:16:45
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnAlz::MYBZFILE* CUnAlz::BZ2_bzReadOpen(int* bzerror, CUnAlz* f, int verbosity, int _small,void* unused, int   nUnused)
{
   MybzFile* bzf = NULL;
   int     ret;

   BZ_SETERR(BZ_OK);

   if (f == NULL || 
       (_small != 0 && _small != 1) ||
       (verbosity < 0 || verbosity > 4) ||
       (unused == NULL && nUnused != 0) ||
       (unused != NULL && (nUnused < 0 || nUnused > BZ_MAX_UNUSED)))
      { BZ_SETERR(BZ_PARAM_ERROR); return NULL; };

//   if (f->FEof())
//      { BZ_SETERR(BZ_IO_ERROR); return NULL; };

   bzf = (MybzFile*)malloc ( sizeof(MybzFile) );
   if (bzf == NULL) 
      { BZ_SETERR(BZ_MEM_ERROR); return NULL; };

   BZ_SETERR(BZ_OK);

   bzf->initialisedOk = False;
   bzf->handle        = f;
   bzf->bufN          = 0;
   bzf->writing       = False;
   bzf->strm.bzalloc  = NULL;
   bzf->strm.bzfree   = NULL;
   bzf->strm.opaque   = NULL;
   
   while (nUnused > 0) {
      bzf->buf[bzf->bufN] = *((UChar*)(unused)); bzf->bufN++;
      unused = ((void*)( 1 + ((UChar*)(unused))  ));
      nUnused--;
   }

   ret = BZ2_bzDecompressInit ( &(bzf->strm), verbosity, _small );
   if (ret != BZ_OK)
      { BZ_SETERR(ret); free(bzf); return NULL; };

   bzf->strm.avail_in = bzf->bufN;
   bzf->strm.next_in  = bzf->buf;

   bzf->initialisedOk = True;
   return bzf;   
}


int CUnAlz::BZ2_bzread(MYBZFILE* b, void* buf, int len )
{
   int bzerr, nread;
   if (((MybzFile*)b)->lastErr == BZ_STREAM_END) return 0;
   nread = BZ2_bzRead(&bzerr,b,buf,len);
   if (bzerr == BZ_OK || bzerr == BZ_STREAM_END) {
      return nread;
   } else {
      return -1;
   }
}


int CUnAlz::BZ2_bzRead(int* bzerror, MYBZFILE* b, void* buf, int len)
{
   Int32   n, ret;
   MybzFile* bzf = (MybzFile*)b;
   BOOL	bIsEncrypted = 	bzf->handle->IsEncryptedFile();		// 암호걸린 파일인가?

   BZ_SETERR(BZ_OK);

   if (bzf == NULL || buf == NULL || len < 0)
      { BZ_SETERR(BZ_PARAM_ERROR); return 0; };

   if (bzf->writing)
      { BZ_SETERR(BZ_SEQUENCE_ERROR); return 0; };

   if (len == 0)
      { BZ_SETERR(BZ_OK); return 0; };

   bzf->strm.avail_out = len;
   bzf->strm.next_out = (char*)buf;

   while (True) {

//      if (ferror(bzf->handle)) 
//         { BZ_SETERR(BZ_IO_ERROR); return 0; };

      if (bzf->strm.avail_in == 0 && !bzf->handle->FEof()) {
         bzf->handle->FRead(bzf->buf, sizeof(UChar)*BZ_MAX_UNUSED, &n);

		 if(bIsEncrypted)
			bzf->handle->DecryptingData(n, (BYTE *)bzf->buf); // xf86 NOT tested -> tested

         if(n==0)
            { BZ_SETERR(BZ_IO_ERROR); return 0; };
         bzf->bufN = n;
         bzf->strm.avail_in = bzf->bufN;
         bzf->strm.next_in = bzf->buf;
      }

      ret = BZ2_bzDecompress ( &(bzf->strm) );

      if (ret != BZ_OK && ret != BZ_STREAM_END)
         { BZ_SETERR(ret); return 0; };

      if (ret == BZ_OK && bzf->handle->FEof() && 
          bzf->strm.avail_in == 0 && bzf->strm.avail_out > 0)
         { BZ_SETERR(BZ_UNEXPECTED_EOF); return 0; };

      if (ret == BZ_STREAM_END)
         { BZ_SETERR(BZ_STREAM_END);
           return len - bzf->strm.avail_out; };
      if (bzf->strm.avail_out == 0)
         { BZ_SETERR(BZ_OK); return len; };
      
   }

   return 0; /*not reached*/
}

void CUnAlz::BZ2_bzReadClose( int *bzerror, MYBZFILE *b )
{
   MybzFile* bzf = (MybzFile*)b;

   BZ_SETERR(BZ_OK);
   if (bzf == NULL)
      { BZ_SETERR(BZ_OK); return; };

   if (bzf->writing)
      { BZ_SETERR(BZ_SEQUENCE_ERROR); return; };

   if (bzf->initialisedOk)
      (void)BZ2_bzDecompressEnd ( &(bzf->strm) );
   free ( bzf );
}

