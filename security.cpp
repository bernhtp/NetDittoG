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

#include "netditto.hpp"
#include <aclapi.h>

#include "security.hpp"


// For each "on" bit in the bitmap, appends the corresponding char in
// mapStr to retStr, thus forming a recognizable form of the bit string.
static
int _stdcall                             // ret-legngth of string written
BitMapStr(
	DWORD					p_bitmap	,// in -bits to map
	WCHAR const           * p_mapstr	,// in -map character array string
	WCHAR				  * p_retstr     // out-return selected map char string
)
{
	WCHAR const           * m;
	WCHAR				  * r = p_retstr;

	for ( m = p_mapstr;  *m;  m++, p_bitmap >>= 1 )
		if ( p_bitmap & 1 )				// if current permission on
			*r++ = *m;					//    set output string to corresponding char
	*r = L'\0';

	return (int)(r - p_retstr);
}


BOOL PriviledgeEnable(
		int                 p_nPriv			,// in -number of priviledges in array
		...                                  // in -nPriv number of server/priv pairs
	)
{
	HANDLE					hToken;           
	BOOL					rc;
	int						n;
	struct
	{
		TOKEN_PRIVILEGES		tkp;        // token structures 
		LUID_AND_ATTRIBUTES		x[20];      // room for several
	}						token;
	va_list                 currArg;
	WCHAR const           * server,
						  * priv;

	va_start(currArg, p_nPriv);
	// Get the current process token handle so we can get backup privilege.
	if ( !OpenProcessToken(GetCurrentProcess(), 
							TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, 
							&hToken) )
	{
		errCommon.MsgWrite(ErrS, L"OpenProcessToken failed=%d", GetLastError());
		return FALSE;
	}

	for ( n = 0;  n < p_nPriv;  n++ )
	{
		server = va_arg(currArg, WCHAR const *);
		priv   = va_arg(currArg, WCHAR const *);

		// Get the LUID for backup privilege. 
		if ( !LookupPrivilegeValue(server, priv, &token.tkp.Privileges[n].Luid) )
		{
			rc = GetLastError();
			errCommon.SysMsgWrite(ErrS, rc, L"LookupPrivilegeValue(%s,%s)=%ld, ", 
								server, priv, rc);
			return FALSE;
		}
		token.tkp.Privileges[n].Attributes = SE_PRIVILEGE_ENABLED;
	}

	token.tkp.PrivilegeCount = p_nPriv;  

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
									p_nPriv, GetLastError());
		rc = FALSE;
	}
	else
		rc = TRUE;
   
	CloseHandle(hToken);
   
	va_end(currArg);
   
	return rc;
}


bool TFileSD::ReadSD()
{
	DWORD rc = GetSecurityInfo(
		m_handle,
		SE_FILE_OBJECT,
		DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
		&m_owner,
		&m_group,
		&m_dacl,
		&m_sacl,
		&m_sd);
	return rc == ERROR_SUCCESS;
}

bool TFileSD::WriteSD(PSID p_owner, PSID p_group, PACL p_dacl, PACL p_sacl)
{
	DWORD rc = SetSecurityInfo(
		m_handle,
		SE_FILE_OBJECT,
		DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
		p_owner,
		p_group,
		p_dacl,
		p_sacl);
	return rc == ERROR_SUCCESS;
}

bool TFileSD::PrintSD() const
{

}

WCHAR * 
TFileSD::PermStr(
	DWORD					p_access		,// in -access mask
	WCHAR				  * p_permstr        // out-return permissions string
	)
{
	// static char const    fileSpecific[] = "R W WaErEwX . ArAw";
	// static char const    dirSpecific[]  = "L C M ErEwT D ArAw";
	static WCHAR const		specific[] = L"RWbeEXDaA.......",
							standard[] = L"DpPOs...",
							generic[] = L"SM..AXWR";
	WCHAR				  * o = p_permstr;

	if ( (p_access & FILE_ALL_ACCESS) == FILE_ALL_ACCESS )
		*o++ = '*';
	else
		o += BitMapStr(p_access, specific, o);

	p_access >>= 16;
	*o++ = '-';
	if ( (p_access & (STANDARD_RIGHTS_ALL >> 16)) == (STANDARD_RIGHTS_ALL >> 16) )
		*o++ = '*';
	else
		o += BitMapStr(p_access, standard, o);

	p_access >>= 8;
	if ( p_access )
	{
		*o++ = '-';
		o += BitMapStr(p_access, generic, o);
	}
	*o = L'\0';                // null terminate string

	return p_permstr;
}
