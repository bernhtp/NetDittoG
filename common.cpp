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

#include <windows.h>

#include <stdio.h>
#include <time.h>
#include <lm.h>

#include "Common.hpp"
#include "Err.hpp"


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


wchar_t * _stdcall						 // ret-DDDdHH:MM:SS elapsed time str
ElapsedTimeStr(
	long				secs			,// in -number of seconds
	wchar_t				str[]			 // out-return buffer string
)
{
	int					h, m, s;

	s = secs % 60;                // ss = number of SS seconds
	secs /= 60;                            // secs = number of minutes

	m = secs % 60;                // mm = number of MM minutes
	secs /= 60;                            // secs = number of hours

	h = secs % 24;                // hh = number of HH hourcs
	secs /= 24;                            // secs = number of days

	if (secs == 0)                       // if no days
		_swprintf(str, L"    %2d:%02d:%02d", h, m, s);
	else
		_swprintf(str, L"%3ldd%2d:%02d:%02d", secs, h, m, s);

	return str;
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


int _stdcall
CompareFILETIME(
	FILETIME               f1			,// in -file time 1
	FILETIME               f2            // in -file time 2
)
{
	if (f1.dwHighDateTime == f2.dwHighDateTime)
		return f1.dwLowDateTime - f2.dwLowDateTime;
	return f1.dwHighDateTime - f2.dwHighDateTime;
}


int _stdcall
CompareFILETIME(
	FILETIME               f1			,// in -file time 1
	FILETIME               f2            // in -file time 2
);

WCHAR * _stdcall                           // ret-result buffer conversion of str
CommaStr(
	WCHAR                  result[]		,// out-result buffer
	WCHAR          const   str[]         // in -string to be converted
);

