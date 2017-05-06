/*
===============================================================================

  Module     - Unsecure.cpp
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 07/25/95
  Description- 
     This has the function of giving promiscuous delete priviledge to the 
     target file/dir name by creating a persistent WIN32_BACKUP_ID stream
     that is restored to the dir/file using BackupWrite.
  Updates -

===============================================================================
*/
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <direct.h>

#include "netditto.hpp"


// Create a SID for the well-known Everyone group.
static
PSID 
   CreateWorldSid()
{
   SID_IDENTIFIER_AUTHORITY  auth = SECURITY_WORLD_SID_AUTHORITY;
   PSID                      pSid;

   if ( !AllocateAndInitializeSid(
             &auth,
   		    1,
   		    SECURITY_WORLD_RID,
   		    0,
   		    0,
   		    0,
   		    0,
   		    0,
   		    0,
   		    0,
   		    &pSid) )
   {
		return NULL;
   }
   
   return pSid;
}

// Create a SID for the well-known Administrators group.
static
PSID 
   CreateAdminsSid()
{
   SID_IDENTIFIER_AUTHORITY  auth = SECURITY_WORLD_SID_AUTHORITY;
   PSID                      pSid;

   if ( !AllocateAndInitializeSid(
             &auth,
   		    1,
   		    DOMAIN_GROUP_RID_ADMINS,
   		    0,
   		    0,
   		    0,
   		    0,
   		    0,
   		    0,
   		    0,
   		    &pSid) )
   {
		return NULL;
   }
   
   return pSid;
}

// Creates a self-relative form security descriptor that allows all users to 
// delete the object it is attached to and inserts it into a WIN32_STREAM_ID
// structure for a restore operation via BackupWrite.
static
BOOL
   RestoreStreamForDeleteCreate(
      WIN32_STREAM_ID     ** restore      ,// out-backup format restore stream
      DWORD                * lenStream     // out-length of restore stream
   )
{
   PSID                      psidWorld  = CreateWorldSid(),
                             psidAdmins = CreateAdminsSid();
   WIN32_STREAM_ID         * rs;
   DWORD                     lenWorldSid,
                             lenAdminsSid,
                             lenSd = 0;
   struct 
   {
      ACL                       acl;
      ACCESS_ALLOWED_ACE        ace;
      SID                       sid;
      SECURITY_DESCRIPTOR       sd;
   } buffer;

   *restore = NULL;
   lenWorldSid = GetLengthSid(psidWorld);
   lenAdminsSid = GetLengthSid(psidAdmins);

   if ( !psidWorld || ! psidAdmins )
   {
      return NULL;
   }   
   
   if ( !InitializeAcl(
            &buffer.acl,
            sizeof buffer.acl + sizeof buffer.ace + lenWorldSid + lenAdminsSid - sizeof(DWORD),
            ACL_REVISION) )
   {
      return FALSE;
   }

   if ( !AddAccessAllowedAce(&buffer.acl, ACL_REVISION, DELETE, psidWorld) )
   {
      return FALSE;
   }

   if ( !InitializeSecurityDescriptor(&buffer.sd, SECURITY_DESCRIPTOR_REVISION) )
   {
      return FALSE;
   }

   if ( !SetSecurityDescriptorDacl(
            &buffer.sd, 
            TRUE,	         // flag for presence of discretionary ACL 
            &buffer.acl,	// address of discretionary ACL
            FALSE) )
   {
      return FALSE;
   }
   
   // set the owner to the Administrators well-known group
   if ( !SetSecurityDescriptorOwner(
            &buffer.sd, 
            psidAdmins,
            FALSE) )
   {
      return FALSE;
   }

   if ( !SetSecurityDescriptorGroup(
            &buffer.sd, 
            psidAdmins,
            FALSE) )
   {
      return FALSE;
   }

   // This first call to MakeSelfRelativeSD has the sole function of setting
   // lenSd to the correct length used to allocated the size of the stream
   MakeSelfRelativeSD(&buffer.sd, NULL, &lenSd);

   *lenStream = offsetof(WIN32_STREAM_ID, cStreamName) + lenSd;
   rs = (WIN32_STREAM_ID *)malloc(*lenStream);
   if ( !rs )
   {
      return FALSE;
   }
   
   rs->dwStreamId = BACKUP_SECURITY_DATA;   
   rs->dwStreamAttributes = STREAM_CONTAINS_SECURITY;
   rs->dwStreamNameSize = 0;
   rs->Size.QuadPart = lenSd;

   // Put the self-relative SD in the backup stream at the cStreamName point
   // because there is no stream name for security information.
   if ( !MakeSelfRelativeSD(&buffer.sd, (PSECURITY_DESCRIPTOR)rs->cStreamName, &lenSd) )
   {
      return FALSE;
   }

   *restore = rs;

   // No longer need the allocated SIDs since they are serialized in the 
   // self-relative SD created and we can now free them
   FreeSid(psidAdmins);
   FreeSid(psidWorld);

   return TRUE;
}


BOOL
   UnsecureForDelete(
      DirEntry const       * tgtEntry
   )
{
   void                    * backupContext = NULL;
   DWORD                     lenStream,
                             cbWrite,
                             rc;
   HANDLE                    hTgt;

   if ( !gOptions.unsecure )
   {
      if ( !RestoreStreamForDeleteCreate(&gOptions.unsecure, &lenStream) )
      {
         err.SysMsgWrite(ErrS, L"Create restore stream=%ld ", GetLastError() );
         return FALSE;
      }
   }
   else
      lenStream = (DWORD)_msize(gOptions.unsecure);

   
   cbWrite = GetSecurityDescriptorLength((SECURITY_DESCRIPTOR *)gOptions.unsecure->cStreamName);
/*
   if ( !SetFileSecurity(gTarget.ApiPath(), 
                         DACL_SECURITY_INFORMATION, 
                         (SECURITY_DESCRIPTOR *)gOptions.unsecure->cStreamName) )
   {
      rc = GetLastError();
      if ( rc != ERROR_ACCESS_DENIED )
         err.SysMsgWrite(23807, rc, "SetFileSecurity(%s)=%ld ", 
                                    gTarget.Path(), rc);
   }
   else
      return TRUE;
*/
   
   // open the file/dir for restore.
   hTgt = CreateFile(gTarget.ApiPath(), 
                     GENERIC_WRITE | WRITE_OWNER | WRITE_DAC,
                     FILE_SHARE_READ,
                     NULL, 
                     OPEN_EXISTING, 
                     FILE_FLAG_BACKUP_SEMANTICS 
                     | (tgtEntry->attrFile & FILE_ATTRIBUTE_DIRECTORY 
                       ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL),
                     0);
   if ( hTgt == INVALID_HANDLE_VALUE )
   {
      rc = GetLastError();
      switch ( rc )
      {
         case ERROR_SHARING_VIOLATION:
            err.MsgWrite(20161, L"Target file in use - bypassed - %s", 
                                gTarget.Path());
            break;
         case ERROR_ACCESS_DENIED:
         default:
            err.SysMsgWrite(30177, rc, L"DelOpenWb(%s,%lx)=%ld ", 
                                   gTarget.Path(),
                                   tgtEntry->attrFile,
                                   rc);
      }
      return FALSE;
   }

   // write the restore stream to the target
   if ( !BackupWrite(hTgt, 
                     (PBYTE)gOptions.unsecure, 
                     lenStream,
                     &cbWrite,
                     FALSE,
                     TRUE,
                     &backupContext) )
   {
      rc = GetLastError();
      err.SysMsgWrite(34877, rc, L"BackupWriteD(%s,%ld)=%ld ",
                             gTarget.Path(),
                             IsValidSecurityDescriptor((SECURITY_DESCRIPTOR *)gOptions.unsecure->cStreamName), 
                             rc); 
      
      CloseHandle(hTgt);
      return FALSE;
   }

   CloseHandle(hTgt);

   return TRUE;
}
