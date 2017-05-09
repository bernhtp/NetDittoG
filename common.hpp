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
