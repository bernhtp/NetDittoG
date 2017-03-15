/*
===============================================================================
Module      -  Common.cpp
System      -  Common
Author      -  Tom Bernhardt
Created     -  94/08/22
Description -  Common class implementations.
Updates     -
===============================================================================
*/

//#ifdef USE_STDAFX
//#   include "stdafx.h"
//#   include "rpc.h"
//#else
#include <windows.h>
//#endif

#include <stdio.h>
#include <time.h>
#include <lm.h>


#include "Common.hpp"
#include "Err.hpp"
//#include "UString.hpp"

wchar_t *
   TTime::YMD_HMS(
      WCHAR                * timeStr       // out-YY-MM-DD HH:MM:SS format string
   ) const
{
   struct tm                 tm_time;

   _localtime64_s(&tm_time, &tTime);

   _swprintf(timeStr, L"%02d-%02d-%02d %02d:%02d:%02d",
                tm_time.tm_year % 100, tm_time.tm_mon+1,tm_time.tm_mday,
                tm_time.tm_hour, tm_time.tm_min,  tm_time.tm_sec);
   return timeStr;
}

WCHAR *
   TTime::YYMMDD(
      WCHAR                * dateStr       // out-YYMMDD format string
   ) const
{
   struct tm               * tm_time;

   tm_time = localtime(&tTime);

   _swprintf(dateStr, L"%02d%02d%02d",
                tm_time->tm_year, tm_time->tm_mon+1,tm_time->tm_mday);
   return dateStr;
}


WCHAR * __stdcall                          // ret-3rd parm message string
   ErrorCodeToText(
      DWORD                  code         ,// in -message code
      DWORD                  lenMsg       ,// in -length of message text area
      WCHAR                * msg           // out-returned message text
   )
{
   static HMODULE            hNetMsg = NULL;
   DWORD                     rc;
   WCHAR                   * pMsg;

   msg[0] = L'\0'; // force to null

   if ( code >= NERR_BASE )
   {
      if ( !hNetMsg )
         hNetMsg = LoadLibraryA("netmsg.dll");
      rc = 1;
   }
   else
   {
      rc = DceErrorInqText( code, (RPC_WSTR)msg );
      // Remove any trailing 0x0D or 0x0A
      for ( pMsg = msg + wcslen( msg ) - 1;
            pMsg >= msg;
            pMsg-- )
      {
         if ( (*pMsg == L'\r') || (*pMsg == L'\n') )
            *pMsg = L'\0';
         else
            break;         
      }
   }
   if ( rc )
   {
      if ( code >= NERR_BASE && hNetMsg )
      {
         FormatMessage( FORMAT_MESSAGE_FROM_HMODULE
                      | FORMAT_MESSAGE_MAX_WIDTH_MASK
                      | FORMAT_MESSAGE_IGNORE_INSERTS
                      | 80,
                        hNetMsg,
                        code,
                        0,
                        msg,
                        lenMsg,
                        NULL );
      }
      else
      {
         FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM
                      | FORMAT_MESSAGE_MAX_WIDTH_MASK
                      | FORMAT_MESSAGE_IGNORE_INSERTS
                      | 80,
                        NULL,
                        code,
                        0,
                        msg,
                        lenMsg,
                        NULL );
      }
   }

   // If password too short, append extra text
   if ( code == NERR_PasswordTooShort )
   {  // Add additional text
      pMsg = msg + wcslen(msg);
      wcsncpy( pMsg, L"  This can also be the result of entering a password "
            "that does not conform to your installation's password policy.",
            lenMsg - (pMsg-msg) );
   }

   return msg;
}
