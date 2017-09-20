#include "netditto.hpp"

class TFile
{
	HANDLE					m_hFile;			// file handle
	DirList				  * m_dirlist;			// dirlist for this file/dir
	DirEntry const		  * m_direntry;			// DirEntry for this file/dir

public:
	TFile(DirList * p_dirlist, DirEntry const * p_direntry)
		: m_hFile(INVALID_HANDLE_VALUE), m_dirlist(p_dirlist), m_direntry(p_direntry) {}
	~TFile() { if (m_hFile != INVALID_HANDLE_VALUE) CloseHandle(m_hFile); }
};


/// Handle object used for I/O and syncrhonization base class
/// The various types of functions that create handles are too numerous and complex to abstract here
class THandle
{
protected:
	HANDLE					m_handle;				// can be any handle type that supports sync operations
public:
	THandle(HANDLE p_handle = INVALID_HANDLE_VALUE)				// constructor does not create the handle
							: m_handle(p_handle) {}				
	~THandle()													// destructor always closes open handle
							{ Close(); }			
	operator HANDLE() const										// conversion operator to return the handle
							{ return m_handle; }										
	THandle	&		operator=(HANDLE p_handle)					// allow a HANDLE to be assigned, e.g., from a CreateFile call
							{ m_handle = p_handle; return *this; }	
	DWORD			WaitSingle(DWORD p_msec = INFINITE) const 	// waits on a single object handle
							{ return WaitForSingleObject(m_handle, p_msec); }
	DWORD 			WaitMultiple(BOOL p_bWaitAll, DWORD p_msec, HANDLE p_hFirst, ...) const;	// waits on multiple object handles; last one MUST be INVALID_HANDLE_VALUE
	HANDLE			GetHandle() const { return m_handle; }		// returns handle value
	void			Close() 
							{ if (m_handle != INVALID_HANDLE_VALUE) CloseHandle(m_handle);  m_handle = INVALID_HANDLE_VALUE; }
};


/// Critical section object to serialize code sections across threads
class TCriticalSection
{
	CRITICAL_SECTION		m_cs;
public:
	TCriticalSection()											// defines CS object, but must use Enter()/Leave() methods to bracket-serialize code
							{ InitializeCriticalSection(&m_cs); }			
	~TCriticalSection() 
							{ DeleteCriticalSection(&m_cs); }
	void			Enter()										// starts serialized code for critical section
							{ EnterCriticalSection(&m_cs); }	
	void			Leave()										// ends serialized code for critical section
							{ LeaveCriticalSection(&m_cs); }	
};


/// Event signalling object
class TEvent : public THandle
{
public:
	TEvent(BOOL p_bSignal = false, BOOL p_bManual = false)		// creates an Event sync object, default auto-set
							{ m_handle = CreateEvent(NULL, p_bManual, p_bSignal, NULL); } 
	BOOL            Signal() const								// sets to signalled so that the wait releases
							{ return SetEvent(m_handle); }					
	BOOL         	Clear() const								// reset to not signalled so that the wait blocks
							{ return ResetEvent(m_handle); }
};

static int const BuffSize = 1 << 14;


/// Data and methods needed for OVERLAPPED reads and writes
class TOverlapped
{
	friend class TCopyBuffer;
	TEvent					m_event;
	OVERLAPPED				m_ovl;
public:
	TOverlapped()
		{ memset(&m_ovl, 0, sizeof m_ovl); m_ovl.hEvent = m_event; m_ovl.hEvent = m_event; }
	LPOVERLAPPED		GetOVERLAPPED() { return &m_ovl; }
};


/// The copy buffer with its control structures regulating reads, writes, and synchronization
class TCopyBuffer
{
	int const				m_index;			// number of sub-buffer in gOptions.copybuffer
	bool					m_EOF;
	DWORD					m_lastbytes;		// bytes read on last read operation to this buffer
	__int64					m_lastoffset;		// last offset for subsequent write
	byte				  *	m_buffer;
	TOverlapped				m_ovlRead;
	TOverlapped				m_ovlWrite;
public:
	TCopyBuffer(int p_index)
		: m_index(p_index), m_buffer(gOptions.copyBuffer + p_index * BuffSize), m_EOF(false) {}
	bool				Read(HANDLE p_handle, __int64 p_offset);
	bool				WriteNextRead(HANDLE p_handle, __int64 p_offset);
	bool				IsEOF() const { return m_EOF; }
};


bool TCopyBuffer::Read(HANDLE p_handle, __int64 p_offset)
{
	INT64LE(m_ovlRead.m_ovl.Offset) = p_offset;
	BOOL success = ReadFile(p_handle, m_buffer, BuffSize, NULL, &m_ovlRead.m_ovl);
	return success;
}


/// Wait for the previous read in the buffer to complete and then write the contents
bool TCopyBuffer::WriteNextRead(HANDLE p_handle, __int64 p_offset)
{
	DWORD rc = m_ovlRead.m_event.WaitSingle(60000);		// wait up to 1 min
	if ( rc )											// if the event expired or failed
		return false;
	BOOL success = GetOverlappedResult(p_handle, &m_ovlRead.m_ovl, &m_lastbytes, false);
	if (!success)
	{
		rc = GetLastError();
		if ( rc == ERROR_HANDLE_EOF )
			m_EOF = true;
		return false;
	}

	m_ovlWrite.m_ovl.Offset = m_ovlRead.m_ovl.Offset;	// copy offset and length from read to write		
	m_ovlWrite.m_ovl.OffsetHigh = m_ovlRead.m_ovl.OffsetHigh;
	success = WriteFile(p_handle, m_buffer, m_lastbytes, NULL, &m_ovlWrite.m_ovl);
	rc = m_ovlWrite.m_event.WaitSingle(60000);		// wait up to 1 min for write to complete
	if ( rc )										// if the event expired or failed
		return false;

	// the buffer is now free for the next read
	return Read(p_handle, p_offset);		// Read the next requested block
}

bool CopyOverlapped(HANDLE p_hSrc, HANDLE p_hTgt)
{
	__int64					offset = 0;
	TCopyBuffer				copybuff[8] = { 0,1,2,3,4,5,6,7 };
	int						n;
	bool					success = true;

	// prime the pump by filling read buffers
	for ( n = 0;  n < DIM(copybuff) && success;  n++, offset += BuffSize)
	{
		success = copybuff[n].Read(p_hSrc, offset);
	}
	for ( success = true, n = 0, offset = 0;  success;  offset += BuffSize, n++)
	{
		success = copybuff[n % DIM(copybuff)].WriteNextRead(p_hSrc, offset);
		if ( !success )
			break;
	}
	if ( !copybuff[n % DIM(copybuff)].IsEOF() )
		err.SysMsgWrite(2661, L"Overlapped I/O failure=%ld");
	
	CloseHandle(p_hSrc);
	CloseHandle(p_hTgt);
}
