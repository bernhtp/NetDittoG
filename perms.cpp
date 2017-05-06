/*
===============================================================================
 Module        Perms.c
 System        NetDitto
 Created       02/19/93
 Description   Functions for getting, setting, deleting permissions
 Updates

===============================================================================
*/

#include <string.h>

#define INCL_NETUSER
#define INCL_NETGROUP
#define INCL_NETERRORS
#include "netditto.hpp"
#include <lm.h>

#include "NetCommon.hpp"
#include "Security.hpp"

typedef enum {SecNone, SecErr, SecLM, SecNT} SecType;


// Enables backup priviledge on source and restore priviledge on the target
BOOL                                       // ret-TRUE if successful
   BackupPriviledgeSet()
{
   WCHAR const             * tgtServer,
                           * srcServer;
   WCHAR                     target[UNCLEN+1],
                             source[UNCLEN+1];

   // if local drive, set server to NULL
   if ( gTarget.Path()[1] == L':' )
      tgtServer = NULL;
   else
      tgtServer = ServerNameGet(target, gTarget.Path());

   if ( !wcscmp(gSource.Path(), L"-") )
      srcServer = tgtServer;
   else if ( gSource.Path()[1] == L':' )
      srcServer = NULL;
   else
      srcServer = ServerNameGet(source, gSource.Path());

   // If the souce and target systems are different, we need to give the target
   // both backup priviledge in addition to restore priviledge so it can do
   // promiscuous directory scans.
   if ( srcServer != tgtServer
    ||  ( srcServer && _wcsicmp(srcServer, tgtServer) ) )
   {
      return PriviledgeEnable(3, srcServer, SE_BACKUP_NAME,
                                 tgtServer, SE_RESTORE_NAME,
                                 tgtServer, SE_BACKUP_NAME);
   }

   return PriviledgeEnable(2, srcServer, SE_BACKUP_NAME,
                              tgtServer, SE_RESTORE_NAME);
}


/*
class FileAccess
{
private:
   char                      currResource[MAX_PATH];
   WCHAR                     server[CNLEN+3]; // user for remote LM servers
   SecType                   secType;         // Security type and status
   DWORD                     rc;              // last return code
   void                    * currBuffer;      // curr file access security buffer

   void ServerSet();
   DWORD ACLGet(char const * resource);
public:
   FileAccess(char const * resource, char * buffer);
   ~FileAccess() { if ( currBuffer && secType == SecLM)
                      NetApiBufferFree(currBuffer); };
   SecType GetSecType() { return secType; };
   DWORD   SDGet(char const * resource);
};


FileAccess::FileAccess(char const * resource, char * buffer)
{
   char                      path[MAX_PATH];
   short                     l = wcslen(resource) - 1;
   DWORD                     rc;
   DWORD                     maxFileLen,
                             sysFlags;
   char                      volType[10];
   UINT                      driveType;

   strcpy(path, resource);
   if ( path[l] != '\\' )
      strcpy(path + l, "\\");

   if ( !GetVolumeInformation(path, NULL, 0, NULL, &maxFileLen, &sysFlags,
                              volType, sizeof volType) )
   {
      rc = GetLastError();
      ErrMsgFile(3691, rc, "GetVolumeInformation", path);
      secType = SecErr;
      return;
   }

   driveType = GetDriveType(path);
   if ( driveType < 1 )
   {
      rc = GetLastError();
      ErrMsgFile(3691, rc, "GetDriveType", path);
      secType = SecErr;
      return;
   }

   currBuffer = NULL;
   strcpy(currResource, resource);
   rc = 0;
   if ( !strcmp(volType, "NTFS") )
   {
      secType = SecNT;
      currBuffer = buffer;
   }
   else if ( !strcmp(volType, "HPFS386") )
   {
      if ( driveType == DRIVE_REMOTE )
         ServerSet();
      secType = SecLM;
   }
   else if ( !strcmp(volType, "HPFS") )
      secType = SecNone;
   else if ( !strcmp(volType, "FAT") )
      secType = SecNone;
   else if ( !strcmp(volType, "CDFS") )
      secType = SecNone;
   else
      secType = SecErr;

   if ( secType <= SecNone )
      gOptions.dir.perms = gOptions.file.perms = 0;
   return;
}

// sets the FileAccess::server member
void FileAccess::ServerSet()
{
   char                    * unc,
                             drive[3],
                             uncPath[MAX_PATH],
                           * c;
   DWORD                     lenPath = sizeof uncPath;

   if ( currResource[1] == '\\' )  // if UNC
      unc = currResource;
   else
   {
      rc = WNetGetConnection(drive, uncPath, &lenPath);
      if ( rc != NO_ERROR )
      {
         ErrMsgFile(3692, rc, "WNetGetConnection", drive);
         return;
      }
      unc = uncPath;
   }
   for ( c = unc + 2;  *c  &&  *c != '\\';  c++ );
   MultiByteToWideChar(CP_OEMCP, MB_PRECOMPOSED, unc,  c - unc,
                                               server, DIM(server));
   server[c - unc - 2] = 0;
}

// retrieves LM ACL and sets currBuffer address to it.
DWORD FileAccess::ACLGet(char const * resource)
{
   DWORD                    rc;
   WCHAR                    wResource[MAX_PATH];

   if ( currBuffer )             // free previous buffer allocation
      NetApiBufferFree(currBuffer);

   MultiByteToWideChar(CP_OEMCP, MB_PRECOMPOSED, resource,  -1,
                                                 wResource, DIM(wResource));
   switch ( rc = NetAccessGetInfo((LPTSTR)server, (LPTSTR)wResource,
                                  1, (LPBYTE*)&currBuffer) )
   {
      case 0:
         strcpy(currResource, resource);
         break;
      default:
         ErrMsgFile(3693, rc, "NetAccessGetInfo", resource);
         gOptions.dir.perms = gOptions.file.perms = 0;
         currResource[0] = '\0';
         break;
   }
   return rc;
}


// retrieves LM ACL and sets currBuffer address to it.
DWORD FileAccess::SDGet(char const * resource)
{
   DWORD                    rc,
                            req;

   if ( !GetFileSecurity(resource, DACL_SECURITY_INFORMATION
                             | SACL_SECURITY_INFORMATION
                             | GROUP_SECURITY_INFORMATION,
                         (PSECURITY_DESCRIPTOR)currBuffer,
                         gOptions.sizeBuffer / 2, &req) )
   {
      rc = GetLastError();
      ErrMsgFile(3694, rc, "GetFileSecurity", resource);
      currResource[0] = '\0';
      return rc;
   }

   strcpy(currResource, resource);
   return 0;
}
*/



//-----------------------------------------------------------------------------
// Gets the ACL for either source or target and puts it into the copyBuffer.
// The buffer is split in two with the first half being for the source and
// the second half for the target.
//-----------------------------------------------------------------------------
static
short                                     // ret-0=OK
   AclGet(
      char                 * resource    ,// in -source or target path
      StatBoth             * stats        // i/o-common get statistics
   )
{
   return 0;
}


//-----------------------------------------------------------------------------
// Creates an ACL for the target resource using the source resource as a
// base.
//-----------------------------------------------------------------------------
static
short _stdcall                            // ret-NetAccessAdd return code
   AclCreate(
   )
{
   return 0;
}


//-----------------------------------------------------------------------------
// Replaces the ACL for the target resource using the source resource as a
// base.
//-----------------------------------------------------------------------------
static
short _stdcall                            // ret-NetAccessSetInfo return code
   AclReplace(
   )
{
   return 0;
}


//-----------------------------------------------------------------------------
// Deletes the ACL for the argument resource name.  There is no acceptable
// return code other than 0.  All others result in a message and fatal
// termination.
//-----------------------------------------------------------------------------
static
short _stdcall                            // ret-NetAccessDel return code
   AclDelete(
   )
{
   return 0;
}


//-----------------------------------------------------------------------------
// Compares the ACLs already in the source and target buffers and returns a
// 0 if they are equal.
//-----------------------------------------------------------------------------
static
short _stdcall                            // ret-0=perms equal
   AclCompare(
   )
{
   return 0;
}


//-----------------------------------------------------------------------------
// Replicates ACLs from source to target based upon the options
//-----------------------------------------------------------------------------
short _stdcall                            // ret-0=perms equal
   PermReplicate(
      BOOL                   isDir       ,// in -0=file, 1=dir
      LogAction            * logAction    // out-log action taken
   )
{
   return 0;
}


//-----------------------------------------------------------------------------
// Virtual Delete of target ACL
//-----------------------------------------------------------------------------
short _stdcall                            // ret-0=perms equal
   PermDelete(
      BOOL                   isDir       ,// in -0=file, 1=dir
      LogAction            * logAction    // out-log action taken
   )
{
   return 0;
}


//-----------------------------------------------------------------------------
// Virtual Create of target ACL (target guaranteed not to exist)
//-----------------------------------------------------------------------------
short _stdcall                            // ret-0=perms equal
   PermCreate(
      BOOL                   isDir       ,// in -0=file, 1=dir
      LogAction            * logAction    // out-log action taken
   )
{
   return 0;
}
