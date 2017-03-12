/*
===============================================================================

  Program    - Security
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 10/07/94
  Description- Security support functions


  Updates -

===============================================================================
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

// #include "netditto.hpp"
#ifndef _CONSOLE
#	include <time.h>
#	include "common.hpp"
#endif  
#include "err.hpp"
#include "util32.hpp"
#include "security.hpp"


BOOL 
   PriviledgeEnable(
      int                    nPriv        ,// in -number of priviledges in array
      ...                                  // in -nPriv number of server/priv pairs
   )
{
   HANDLE                    hToken;           
   BOOL                      rc;
   int                       n;
   struct
   {
      TOKEN_PRIVILEGES       tkp;        // token structures 
      LUID_AND_ATTRIBUTES    x[10];      // room for several
   }                         token;
   va_list                   currArg;
   WCHAR const             * server,
                           * priv;

   va_start(currArg, nPriv);
   // Get the current process token handle so we can get backup privilege.
   if ( !OpenProcessToken(GetCurrentProcess(), 
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, 
                          &hToken) )
   {
      errCommon.MsgWrite(ErrS, L"OpenProcessToken failed=%d", GetLastError());
      return FALSE;
   }

   for ( n = 0;  n < nPriv;  n++ )
   {
      server = va_arg(currArg, WCHAR const *);
      priv   = va_arg(currArg, WCHAR const *);

      // Get the LUID for backup privilege. 
      if ( !LookupPrivilegeValue(server,
                                 priv,
                                 &token.tkp.Privileges[n].Luid) )
      {
         rc = GetLastError();
         errCommon.SysMsgWrite(ErrS, rc, L"LookupPrivilegeValue(%s,%s)=%ld, ", 
                               server, priv, rc);
         return FALSE;
      }
      token.tkp.Privileges[n].Attributes = SE_PRIVILEGE_ENABLED;
   }

   token.tkp.PrivilegeCount = nPriv;  

   if ( !AdjustTokenPrivileges(hToken, 
                               FALSE, 
                               &token.tkp, 
                               0,
                               (PTOKEN_PRIVILEGES) NULL, 
                               0) )
   {
      errCommon.SysMsgWrite(ErrS, L"AdjustTokenPrivileges(1)=%ld failed, ", GetLastError());
   }
   // Cannot test the return value of AdjustTokenPrivileges
   if ( GetLastError() != ERROR_SUCCESS )
   {
      errCommon.SysMsgWrite(ErrS, L"AdjustTokenPrivileges(%d)=%ld failed, ", 
                                  nPriv, GetLastError());
      rc = FALSE;
   }
   else
      rc = TRUE;
   
   CloseHandle(hToken);
   
   va_end(currArg);
   
   return rc;
}

/*

class TProcessToken
{
public:
   HANDLE                    hToken;

            TProcessToken()  { };
};

class TProcess
{
public:
   HANDLE                    hProcess;
   TProcessToken             token;   
   format         TProcess()       { hProcess = GetCurrentProcess(); };
};

class CSecurity
{
private:
   HANDLE                    hProcess;     // process handle
   HANDLE                    hToken;       // process token handle
public:

};


BOOL
   TakeOwnership(
      char const           * path          // in -path of dir/file to own
   )
{
   SECURITY_DESCRIPTOR       sid;
   HANDLE                    hToken;
   HANDLE                    hProcess;
   DWORD                     rc;


   hProcess = OpenProcess(PROCESS_QUERY_INFORMATION,
                           FALSE,
                           GetCurrentProcessId());
   if ( hProcess == NULL ) 
      return FALSE;

   if ( !OpenProcessToken(hProcess,
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          TokenHandle) )
      return FALSE;
   
   
   if ( !GetTokenHandle(&hToken) )
   {
      // this really should not happen -- we must be able to get the process token handle
      rc = GetLastError();
      errCommon.SysMsgWrite(ErrV, rc, "GetTokenHandle()=%ld, ", rc);
      return FALSE;
   }
   
   InitializeSecurityDescriptor(&sid, SECURITY_DESCRIPTOR_REVISION);
   if ( !SetSecurityDescriptorOwner(&sid, AliasAdminsSid, FALSE)
                 );
   SetFileSecurity(path, OWNER_SECURITY_INFORMATION, &sid); 
}
*/
