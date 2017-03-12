/*
===============================================================================
Module      -  Err.cpp
System      -  Common
Author      -  Tom Bernhardt, Rich Denham
Created     -  94/08/22
Description -  Implements the TError class that handles basic exception
               handling, message generation, and logging functions.
Updates     -
===============================================================================
*/

#include <windows.h>

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <stdarg.h>
#include <share.h>
#include <time.h>
#include <sys\types.h>
#include <sys\stat.h>

//#include "UString.hpp"
#include "Common.hpp"
#include "Err.hpp"

TError::TError(
      int                    displevel    ,// in -mimimum severity level to display
      int                    loglevel     ,// in -mimimum severity level to log
      wchar_t        const * filename     ,// in -file name of log (NULL if none)
      int                    logmode      ,// in -0=replace, 1=append
      int                    beeplevel     // in -min error level for beeping
   )
{
   lastError = 0;
   maxError = 0;
   logLevel = loglevel;
   dispLevel = displevel;
   logFile = NULL;
   beepLevel = beeplevel;
   LogOpen(filename, logmode, loglevel);
}


TError::~TError()
{
   LogClose();
}

// Closes any existing open logFile and opens a new log file if the fileName is
// not null.  If it is a null string, then a default fileName of "Temp.log" is
// used.
BOOL
   TError::LogOpen(
      wchar_t         const * fileName    ,// in -name of file including any path
      int                     mode        ,// in -0=overwrite, 1=append
      int                     level        // in -minimum level to log
   )
{
   BOOL                       retval=TRUE;

   if ( logFile )
   {
      fclose(logFile);
      logFile = NULL;
   }

   if ( fileName && fileName[0] )
   {
      logFile = _wfsopen( fileName, mode == 0 ? L"w" : L"a", _SH_DENYNO );
      if ( !logFile )
      {
         MsgWrite( 4101, L"Log Open(%s) failed", fileName );
         retval = FALSE;
      }
   }

   logLevel = level;

   return retval;
}


//-----------------------------------------------------------------------------
// Writes formatted message to log file and flushes buffers
//-----------------------------------------------------------------------------
void TError::LogWrite(wchar_t const * msg)
{
   TTime                     now;
   wchar_t                   timeStr[20];

   if ( logFile )
   {
      now.SetNow();
      fwprintf(logFile, L"%s-%s\n", now.YMD_HMS(timeStr), msg);
      fflush(logFile);
   }
}

//-----------------------------------------------------------------------------
// Error message with format and arguments
//-----------------------------------------------------------------------------
void __cdecl
   TError::MsgWrite(
      int                    num          ,// in -error number/level code
      wchar_t        const   msg[]        ,// in -error message to display
      ...                                  // in -printf args to msg pattern
   )
{
   wchar_t                   suffix[350];
   va_list                   argPtr;

   va_start(argPtr, msg);
   _vsnwprintf(suffix, DIM(suffix) - 1, msg, argPtr);
   suffix[DIM(suffix) - 1] = L'\0';
   va_end(argPtr);
   MsgProcess(num, suffix);
}

//-----------------------------------------------------------------------------
// System Error message with format and arguments
//-----------------------------------------------------------------------------
void __cdecl
   TError::SysMsgWrite(
      int                    num          ,// in -error number/level code
      DWORD                  lastRc       ,// in -error return code
      wchar_t        const   msg[]        ,// in -error message/pattern to display
      ...                                  // in -printf args to msg pattern
   )
{
   wchar_t                   suffix[350];
   va_list                   argPtr;
   int                       len;

   // When an error occurs while in a constructor for a global object,
   // the TError object may not yet exist.  In this case, "this" is zero
   // and we gotta get out of here before we generate a protection exception.

   if ( !this )
      return;

   va_start(argPtr, msg);
   len = _vsnwprintf(suffix, DIM(suffix) - 1, msg, argPtr);

   // append the system message for the lastRc at the end.
   if ( len < DIM(suffix) - 1 )
   {
      ErrorCodeToText(lastRc, DIM(suffix) - len - 1, suffix + len);
   }
   suffix[DIM(suffix) - 1] = L'\0';
   va_end(argPtr);
   MsgProcess(num, suffix);
}

//-----------------------------------------------------------------------------
// System Error message with format and arguments
//-----------------------------------------------------------------------------
void __cdecl
   TError::SysMsgWrite(
      int                    num          ,// in -error number/level code
      wchar_t        const   msg[]        ,// in -error message/pattern to display
      ...                                  // in -printf args to msg pattern
   )
{
   wchar_t                   suffix[350];
   va_list                   argPtr;
   int                       len;
   DWORD                     lastRc = GetLastError();

   va_start( argPtr, msg );
   len = _vsnwprintf( suffix, DIM(suffix) - 1, msg, argPtr );

   FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM          // msg type
                     | FORMAT_MESSAGE_MAX_WIDTH_MASK
                     | FORMAT_MESSAGE_IGNORE_INSERTS     // no insert chars
                     | 80,                               // 80 chars/line max
                  NULL,                                 // source
                  lastRc,                               // msg number
                  0,                                    // language
                  suffix + len,                         // buffer location
                  DIM(suffix) - len - 1,
                  NULL);
   suffix[DIM(suffix) - 1] = L'\0';
   va_end(argPtr);
   MsgProcess(num, suffix);
}

//-----------------------------------------------------------------------------
// Error message format, display and exception processing function
//-----------------------------------------------------------------------------
void __stdcall
   TError::MsgProcess(
      int                    num          ,// in -error number/level code
      wchar_t        const * str           // in -error string to display
   )
{
   static wchar_t    const   prefLetter[] = L"TIWESVUXXXXX";
   wchar_t                   fullmsg[350];
   struct
   {
      USHORT                 frequency;    // audio frequency
      USHORT                 duration;     // duration in mSec
   }                         audio[] = {{ 300,  20},{ 500,  50},{ 700, 100},
                                        { 800, 200},{1000, 300},{1500, 400},
                                        {2500, 750},{2500,1000},{2500,1000}};

   if ( num >= 0 )
      level = num / 10000;                 // 10000's position of error number
   else
      level = -1;
   if ( num == 0 )
   {
      wcsncpy(fullmsg, str, DIM(fullmsg));
      fullmsg[DIM(fullmsg) - 1] = L'\0';  // ensure null termination
   }
   else
   {
      if ( num > maxError )
         maxError = num;
      _swprintf(fullmsg, L"%c%05d: %-.245s", prefLetter[level+1], num, str);
   }

   lastError = num;
   StrWrite(level, fullmsg);

   if ( level >= beepLevel )
      Beep(audio[level].frequency, audio[level].duration);

   if ( level >= logLevel )
      LogWrite(fullmsg);

   if ( level > 4 )
   {
      exit(level);
   }

   return;
}
