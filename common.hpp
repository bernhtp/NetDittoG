/*
===============================================================================
Module      -  Common.hpp
System      -  Common
Author      -  Tom Bernhardt
Created     -  94/08/22
Description -  Common classes of general utility.
Updates     -
===============================================================================
*/
#include <time.h>

// WcsByteLen returns the byte length of a wchar string including its null terminator
#define WcsByteLen(s) ( sizeof *(s) * (wcslen(s) + 1) )

#define SECS(n) (n * 1000)
#define DIM(x) (sizeof (x) / sizeof ((x)[0]))	// gives the number of elements in an array

__int64	_inline INT64R(int lo, __int64 hi) { return (hi << 32) + lo; } // converts a split lo/hi int into __int64

#define INT64LE(lo) *(__int64 *)&lo	// for Litte-Endian machines, turns contiguous lowpart-hipart 32-bit pairs into __int64

class TTime
{
protected:
	__time64_t              tTime;

public:
                        TTime() { };
                        TTime(__time64_t t) { tTime = t; };

   void                 Set(__time64_t t) { tTime = t; };
   void                 SetNow() { time(&tTime); };
   time_t               Get() const { return tTime; };
   wchar_t *            YMD_HMS(wchar_t * timeStr) const;
   wchar_t *            YYMMDD(wchar_t * timeStr) const;
   int                  Compare(TTime const t) const
                        { if ( tTime == t.tTime)
                             return 0;
                          else if ( tTime > t.tTime )
                             return 1;
                          else
                             return -1;
                        };
   TTime                Elapsed() const { return TTime(time(NULL) - tTime); };
   TTime                Elapsed(TTime & t) const { return TTime(t.tTime - tTime); };

// TTime                operator= (int t) { tTime = t; return *this; };
                        operator time_t()   const { return tTime; };
   int                  operator> (TTime t) const { return tTime >  t.tTime; };
   int                  operator>=(TTime t) const { return tTime >= t.tTime; };
   int                  operator< (TTime t) const { return tTime <  t.tTime; };
   int                  operator<=(TTime t) const { return tTime <= t.tTime; };
   int                  operator==(TTime t) const { return tTime == t.tTime; };
   int                  operator!=(TTime t) const { return tTime != t.tTime; };
   int                  operator^ (TTime t) const { return Compare(t); };
   TTime                operator+ (TTime t) const { return TTime(tTime + t.tTime); };
   TTime                operator+=(TTime t)       { return TTime(tTime += t.tTime); };
   TTime                operator- (TTime t) const { return TTime(tTime - t.tTime); };
   TTime                operator-=(TTime t)       { return TTime(tTime =- t.tTime); };
};

wchar_t * _stdcall						 // ret-DDDdHH:MM:SS elapsed time str
ElapsedTimeStr(
	long				secs			,// in -number of seconds
	wchar_t				str[]			 // out-return buffer string
	);

__int64                                    // ret-numeric value of string
   TextToInt64(
      WCHAR          const * str          ,// in-string value to parse
      __int64                minVal       ,// in -min allowed value for result
      __int64                maxVal       ,// in -max allowed value for result
      WCHAR         const ** errMsg        // out-error message pointer or NULL
   );

WCHAR * __stdcall                          // ret-3rd parm message string
   ErrorCodeToText(
      DWORD                  code         ,// in -message code
      DWORD                  lenMsg       ,// in -length of message text area
      WCHAR                * msg           // out-returned message text
   );

WCHAR * _stdcall						 // ret-result buffer conversion of str
CommaStr(
	WCHAR                  result[]		,// out-result buffer - must be large enough for result with commas
	WCHAR const            str[]         // in -string to be converted
);

/// Handle object used for I/O and syncrhonization base class
/// The various types of functions that create handles are too numerous and complex to abstract here
class THandle
{
protected:
	HANDLE					m_handle;				// can be any handle type that supports sync objects
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


/// Thin Critical Section class to serialize code sections across threads
class TCriticalSection
{
	CRITICAL_SECTION		m_cs;
public:
	TCriticalSection() { InitializeCriticalSection(&m_cs); }	// defines CS object, but must use Enter()/Leave() methods to bracket-serialize code
	~TCriticalSection()	{ DeleteCriticalSection(&m_cs);	}
	void			Enter() { EnterCriticalSection(&m_cs); }	// starts serialized code for critical section
	void			Leave() { LeaveCriticalSection(&m_cs); }	// ends serialized code for critical section
};

/// Event signalling object
class TEvent : public THandle
{
public:
	TEvent(BOOL p_bSignal = false, BOOL p_bManual = false)		// creates an Event sync object, default auto-set
						{ m_handle = CreateEvent(NULL, p_bManual, p_bSignal, NULL); }
	BOOL            Signal() const								// sets to signalled so that the wait releases
						{ return SetEvent(m_handle); }
	BOOL         	Set() const { return ResetEvent(m_handle); }	// reset to not signalled so that the wait blocks
};

