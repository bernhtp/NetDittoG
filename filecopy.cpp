/*
===============================================================================

  Program    - FileCopy
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 02/17/93
  Description- Functions to replicate file contents and set some attributes.

  Updates -

===============================================================================
*/
#include "netditto.hpp"

// Converts binary attribute mask to string
WCHAR * _stdcall
	AttrStr(
		DWORD				attr			,// in -file/dir attribute
		WCHAR			  * retStr			 // out-return attribute string
	)
{
	WCHAR const			  * i;
	WCHAR				  * o;

	for ( i = L"RHS3DA6NT9aCcdef", o = retStr;  *i;  i++, attr >>= 1 )
		if ( attr & 1 )			// if current attr bit on
			*o++ = *i;			//    set output string to corresponding char
	*o = L'\0';					// terminate output retStr
	return retStr;
}


/// context info for FileCopy2 callback function
struct CopyCallbackContext
{
	int					nmessage;
	HRESULT				hr;
};

/// Callback function for FileCopy2
static COPYFILE2_MESSAGE_ACTION CALLBACK CopyFile2Callback(
	COPYFILE2_MESSAGE const	  * p_message	,// in -message to process
	void					  * p_context	)// i/o-message context
{
	CopyCallbackContext		  * context = (CopyCallbackContext *)p_context;
	context->nmessage++;
	if ( p_message->Type == COPYFILE2_CALLBACK_CHUNK_FINISHED )	// update the bytes written stat when chunk complete
		gOptions.bWritten += p_message->Info.ChunkFinished.uliChunkSize.QuadPart;
	else if ( p_message->Type == COPYFILE2_CALLBACK_ERROR )
	{
		context->hr = p_message->Info.Error.hrFailure;
		return COPYFILE2_PROGRESS_STOP;
	}
	return COPYFILE2_PROGRESS_CONTINUE;
}


/// Use CopyFile2 to implement file contents copy
bool FileCopy2Contents(wchar_t const * p_ifile, wchar_t const * p_ofile)
{
	COPYFILE2_EXTENDED_PARAMETERS	cfp;
	CopyCallbackContext					context;

	memset(&context, 0, sizeof context);
	cfp.dwSize = sizeof cfp;
	cfp.dwCopyFlags = COPY_FILE_ALLOW_DECRYPTED_DESTINATION | COPY_FILE_NO_BUFFERING;
	cfp.pfCancel = false;
	cfp.pProgressRoutine = CopyFile2Callback;
	cfp.pvCallbackContext = &context;
	return CopyFile2(p_ifile, p_ofile, &cfp);
}



// Copies file contents
DWORD _stdcall
   FileCopy(
		DirEntry const    * p_srcentry	,// in -source directory entry
		DirEntry const	  * p_tgtentry	 // in -target directory entry
   )
{
   HANDLE					hSrc = INVALID_HANDLE_VALUE,
							hTgt = INVALID_HANDLE_VALUE;
   DWORD					rc = 0;
   BOOL						compressChange;

   // if file attrib R/O and write R/O option, change to R/W
   if ( p_tgtentry )
   {
		if ( p_tgtentry->attrFile & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN) )
		{
			if ( (p_tgtentry->attrFile & FILE_ATTRIBUTE_READONLY  &&  gOptions.global & OPT_GlobalReadOnly)
			  || (p_tgtentry->attrFile & FILE_ATTRIBUTE_HIDDEN    &&  gOptions.global & OPT_GlobalHidden  ) )
			{
				if ( !SetFileAttributes(gTarget.ApiPath(), FILE_ATTRIBUTE_NORMAL) )
				{
					rc = GetLastError();
					err.SysMsgWrite(20103, rc, L"SetFileAttributes(%s,N)=%ld, ",
												gTarget.Path(), rc);
					return rc;
				}
			}
			else
			{
				// The open will fail because we have not given permission to change attributes, so let's quit it
				return 0;
			}
		}
	}

	// if the source and target compression attribute is different and significant
	if ( p_tgtentry )
		if ( (p_srcentry->attrFile ^ p_tgtentry->attrFile) & FILE_ATTRIBUTE_COMPRESSED & gOptions.attrSignif )
			compressChange = TRUE;
		else
			compressChange = FALSE;
	else
		if ( p_srcentry->attrFile & FILE_ATTRIBUTE_COMPRESSED & gOptions.attrSignif )
			compressChange = TRUE;
		else
			compressChange = FALSE;
	if ( compressChange )
	{
		CompressionSet(hSrc, hTgt, p_srcentry->attrFile);
	}

	rc = FileCopy2Contents(gSource.ApiPath(), gTarget.Path());
	if ( rc )
		err.SysMsgWrite(104, rc, L"FileCopy2(%s), ", gTarget.Path());

	if ( gOptions.file.attr & OPT_PropActionUpdate )
		if ( !SetFileAttributes(gTarget.ApiPath(), p_srcentry->attrFile) )
		{
			rc = GetLastError();
			err.SysMsgWrite(20109, rc, L"SetFileAttributes(%s)=%d ",
										gTarget.Path(), rc);
			return rc;
		}

	return rc;
}


// Compares file contents on a byte by byte basis
DWORD _stdcall
	FileContentsCompare()
{
	HANDLE                  hSrc,
							hTgt;
	DWORD				    rcSrc = 0,
							rcTgt = 0,
							b2 = gOptions.sizeBuffer >> 1, // split buffer
							cmp = 0,
							nSrc,
							nTgt;
	BYTE                  * s,        // source and target for 1's comp compare
						  * t;
	BOOL                    bSrc, bTgt;

	err.MsgWrite(0, L"Fc %s", gTarget.Path());
	hSrc = CreateFile(gSource.ApiPath(),
						GENERIC_READ,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING,
						0);
	if ( hSrc == INVALID_HANDLE_VALUE)
	{
		rcSrc = GetLastError();
		if ( rcSrc == ERROR_SHARING_VIOLATION )
			err.MsgWrite(20101, L"Source file in use %s", gSource.Path());
		else
			err.SysMsgWrite(40101, rcSrc, L"OpenRs(%s)=%d ", gSource.Path(), rcSrc);
		return rcSrc;
	}

	hTgt = CreateFile(gTarget.ApiPath(),
						GENERIC_READ,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING,
						0);
	if ( hTgt == INVALID_HANDLE_VALUE)
	{
		rcTgt = GetLastError();
		if ( rcTgt == ERROR_SHARING_VIOLATION )
			err.MsgWrite(20101, L"Target file in use %s", gTarget.Path());
		else
			err.SysMsgWrite(40101, rcTgt, L"OpenRt(%s)=%d, ", gTarget.Path(), rcTgt);
		return rcTgt;
	}

	while ( (bSrc = ReadFile(hSrc, gOptions.copyBuffer   , b2, &nSrc, NULL))
		 && (bTgt = ReadFile(hTgt, gOptions.copyBuffer+b2, b2, &nTgt, NULL)) )
	{
		if ( nSrc != nTgt )   // this should never occur but check just in case
		{
			cmp = 1;
			break;
		}
		if ( gOptions.global & OPT_GlobalCopyXOR )   // complement contents option
		{
			for ( s = gOptions.copyBuffer, t = gOptions.copyBuffer + b2;
				  s < gOptions.copyBuffer + nSrc;
				  s++, t++ )
			{
			if ( *s != (byte)~*t )
			{
				cmp = 1;
				break;
			}
			}
			if ( cmp )
				break;
		}
		else if ( cmp = memcmp(gOptions.copyBuffer, gOptions.copyBuffer + b2, nSrc) )
			break;

		if ( nSrc < b2 )                 // don't issue read just to get EOF
			break;
	}

	if ( !bSrc )
		rcSrc = GetLastError();
	else
		if ( !bTgt )
			rcTgt = GetLastError();

	CloseHandle(hSrc);
	CloseHandle(hTgt);

	if ( rcSrc = max(rcSrc, rcTgt) )
		err.SysMsgWrite(40104, rcSrc, L"ReadFile(%s)=%d",
			(rcTgt ? gTarget.Path() : gSource.Path()), rcSrc );

	return max(cmp, rcSrc);
}


// copies the contents of the source file to the target given open file handles
static DWORD _stdcall
	FileBackupContents(
		HANDLE					p_hSrc			,// in -input file handle
		HANDLE					p_hTgt			 // in -output file handle
	)
{
	DWORD						rc = 0,
								nSrc,
								nTgt;
	BOOL						b;
	void					  * r = NULL,    // required by the BackupRead/Write APIs
							  * w = NULL;

	while ( b = BackupRead(p_hSrc,
							gOptions.copyBuffer,
							gOptions.sizeBuffer,
							&nSrc,
							FALSE,
							TRUE,
							&r) )
	{
		if ( nSrc == 0 )                    // if end-of-file, break while loop
			break;

/*
		// need to fix so only XOR data stream
		if ( gOptions.global & OPT_GlobalCopyXOR )   // complement contents option
			for ( i = (int *)gOptions.copyBuffer;  (BYTE *)i < gOptions.copyBuffer + nSrc;  i++ )
			*i = ~*i;                              // one's complement buffer
*/
		if ( !BackupWrite(p_hTgt, gOptions.copyBuffer, nSrc, &nTgt, FALSE, TRUE, &w) )
		{
			rc = GetLastError();
			// The following code attempts to recover from an error where the owner SID
			// in the scurity descriptor that is being written is not valid on the target
			// system.  This happens when replicating from a workstation to a server where
			// local accounts, not known by the server, are owners, e.g., the local admin.
			// It does this by skipping over the security stream and continuing the restore.
			// ~~Eventually we want to enhace this to modify the owner SID to something like
			// the Administrators well-known group to preserve the rest of the SD.
			if ( rc == ERROR_INVALID_OWNER )
			{
				WIN32_STREAM_ID * s = (WIN32_STREAM_ID *)gOptions.copyBuffer;
				int               n;

				if ( s->dwStreamId == BACKUP_SECURITY_DATA )
				{
					n = sizeof *s + s->Size.LowPart + s->dwStreamNameSize - 4;
					if ( !BackupWrite(p_hTgt, gOptions.copyBuffer + n, nSrc-n, &nTgt, FALSE, TRUE, &w) )
						rc = GetLastError();
					else
						rc = 0;
				}
			}
			if ( rc )
			{
				err.SysMsgWrite(30103, rc, L"BackupWrite(%d)=%ld, ", nSrc, rc);
				return rc;
			}
		}
		gOptions.bWritten += nTgt;
	}
	if ( !b )
		if ( rc = GetLastError() )
			err.SysMsgWrite(40104, rc, L"BackupRead(%s)=%ld ", gSource.Path(), rc);

	return rc;
}


// Copies file contents via the backup APIs and thus copying security, streams, etc.
DWORD _stdcall
	FileBackupCopy(
		DirEntry const		  * p_srcentry	,// in -source directory entry
		DirEntry const		  * p_tgtentry	 // in -target directory entry
	)
{
	HANDLE						hSrc,
								hTgt;
	DWORD						rc = 0,
								attr;
	WCHAR						temp[2][12];

	// if file R/O and write R/O option, change to R/W
	if ( p_tgtentry  &&  p_tgtentry->attrFile & FILE_ATTRIBUTE_READONLY )
		if ( gOptions.global & OPT_GlobalReadOnly )
			if ( !SetFileAttributes(gTarget.ApiPath(), FILE_ATTRIBUTE_NORMAL) )
			{
				rc = GetLastError();
				err.SysMsgWrite(20103, rc, L"SetFileAttributes(%s)=%ld, ",
											gTarget.Path(), rc);
				return rc;
			}
	attr = (p_srcentry->attrFile & FILE_ATTRIBUTE_DIRECTORY ? FILE_ATTRIBUTE_DIRECTORY	: FILE_ATTRIBUTE_NORMAL) 
		| FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_BACKUP_SEMANTICS;

	hSrc = CreateFile(gSource.ApiPath(),
						GENERIC_READ,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						attr,
						0);
	if ( hSrc == INVALID_HANDLE_VALUE )
	{
		rc = GetLastError();
		if ( rc == ERROR_SHARING_VIOLATION )
			err.MsgWrite(20161, L"Source file in use - bypassed - %s", gSource.Path());
		else
			err.SysMsgWrite(40161, rc, L"OpenR(%s)=%ld, ", gSource.ApiPath(), rc);
		return rc;
	}

	hTgt = CreateFile(gTarget.ApiPath(),
						GENERIC_WRITE | GENERIC_READ | WRITE_OWNER | WRITE_DAC,
						FILE_SHARE_READ,
						NULL,
						OPEN_ALWAYS | (p_tgtentry && !(p_tgtentry->attrFile & FILE_ATTRIBUTE_DIRECTORY) ? TRUNCATE_EXISTING : 0),
						attr,
						0);
	if ( hTgt == INVALID_HANDLE_VALUE )
	{
		rc = GetLastError();
		switch ( rc )
		{
			case ERROR_SHARING_VIOLATION:
			err.MsgWrite(20161, L"Target file in use - bypassed - %s", gTarget.Path());
			break;
			case ERROR_ACCESS_DENIED:
			default:
			err.SysMsgWrite(30162, rc, L"OpenWb(%s,%lx)=%ld (attr=%s->%s,%x/%x), ",
									gTarget.Path(),
									attr,
									rc,
									p_srcentry ? AttrStr(p_srcentry->attrFile, temp[0]) : L"-",
									p_tgtentry ? AttrStr(attr, temp[1]) : L"-",
									p_srcentry ? p_srcentry->attrFile : 0,
									p_tgtentry ? attr : 0);
		}
		CloseHandle(hSrc);
		return rc;
	}

	// if the source and target compression attribute is different and significant
	if ( p_tgtentry )
		attr = FILE_ATTRIBUTE_COMPRESSED & gOptions.attrSignif & (p_srcentry->attrFile ^ p_tgtentry->attrFile);
	else
		attr = FILE_ATTRIBUTE_COMPRESSED & gOptions.attrSignif & p_srcentry->attrFile;
	if ( attr )
	{
		CompressionSet(hSrc, hTgt, p_srcentry->attrFile);
	}

	if ( rc = FileBackupContents(hSrc, hTgt) )
		err.SysMsgWrite(104, rc, L"FileCopyContents(%s), ", gTarget.Path());

	if ( !SetFileTime(hTgt, NULL, NULL, &p_srcentry->ftimeLastWrite) )
	{
		rc = GetLastError();
		err.SysMsgWrite(40110, rc, L"SetFileTime(%s,%02lX)=%ld ", gTarget.Path(),
							p_srcentry->attrFile, rc);
		rc = 0;
	}

	CloseHandle(hSrc);
	CloseHandle(hTgt);

	if ( gOptions.file.attr & OPT_PropActionUpdate )
	{
		if ( p_tgtentry )
			attr = ~FILE_ATTRIBUTE_COMPRESSED
				& gOptions.attrSignif
				& (p_srcentry->attrFile ^ p_tgtentry->attrFile);
		else
			attr = ~FILE_ATTRIBUTE_COMPRESSED
				& gOptions.attrSignif
				& p_srcentry->attrFile;
		if ( attr )
		{
			if ( !SetFileAttributes(gTarget.ApiPath(), p_srcentry->attrFile) )
			{
				rc = GetLastError();
				err.SysMsgWrite(20109, rc, L"SetFileAttributes(%s)=%d ", gTarget.Path(), rc);
				return rc;
			}
		}
	}
	return rc;
}
