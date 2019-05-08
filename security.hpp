BOOL PriviledgeEnable(
		int                 nPriv        ,// in -number of priviledges in array
		...                               // in -nPriv number of server/priv pairs
   );


/// TFileSD - security descriptor access for file handle
class TFileSD : public THandle
{
	PSID					m_owner;
	PSID					m_group;
	PACL					m_dacl;
	PACL					m_sacl;
	PSECURITY_DESCRIPTOR	m_sd;
protected:
	static	WCHAR * PermStr(DWORD p_access, WCHAR * p_permstr);
public:
	TFileSD(HANDLE p_handle) : THandle(p_handle), m_sd(NULL) {}
	~TFileSD() { if (m_sd) LocalFree(m_sd); }

	bool	ReadSD();
	bool	WriteSD(PSID p_owner, PSID p_group, PACL p_dacl, PACL p_sacl);
	bool	WriteSD() { return WriteSD(m_owner, m_group, m_dacl, m_sacl); }
	bool	PrintSD() const;
};
