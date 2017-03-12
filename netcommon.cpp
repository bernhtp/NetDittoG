#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#ifndef _CONSOLE
#	include "common.hpp"
#	include "err.hpp"
#else
#	include "err.hpp"
#	include "common.hpp"
#endif
#include "NetCommon.hpp"

// returns server name component of UNC
WCHAR *                                    // ret-serverName
   ServerNameGet(
      WCHAR                * serverName   ,// out-server name with leading "\\\\"
      WCHAR const          * path          // in -path to extract server name from
   )
{
   WCHAR const             * s;
   WCHAR                   * t;

   wcscpy(serverName, L"\\\\");
   if ( path[1] == L'\\' )   // if UNC
   {
      // target copy to next backslash or string terminator
      for ( s = path+2, t = serverName+2;  *s  &&  *s != L'\\';  s++, t++ )
         *t = *s;
      *t = L'\0';
   }
   else if ( path[1] == L':' )   // if drive
   {
      WCHAR                  drive[3],
                             unc[40];
      DWORD                  rc;
      DWORD                  len = DIM(unc);

      drive[0] = path[0];
      drive[1] = path[1];
      drive[2] = L'\0';

      switch ( rc = WNetGetConnection(drive, unc, &len) )
      {
         case NO_ERROR:
            break;
         case ERROR_NOT_CONNECTED:
            return NULL;
         default:
            errCommon.SysMsgWrite(ErrS, rc, L"WNetGetConnection(%s)=%d, ", drive, rc);
            return NULL;
      }
      for ( s = unc+2, t = serverName+2;  *s  &&  *s != L'\\';  s++, t++ )
         *t = *s;
      *t = L'\0';
   }
   else
   {
      errCommon.MsgWrite(ErrS, L"Invalid path '%s'", path);
      return NULL;
   }
   return serverName;
}
