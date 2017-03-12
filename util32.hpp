/*
===============================================================================
Module      -  Util32.hpp
System      -  Common
Author      -  Tom Bernhardt
Created     -  95/09/01
Description -
Updates     -
===============================================================================
*/

#define DIM(x) (sizeof (x) / sizeof (x[0]))

// forms an r-value __int64 value from high and low 32-bit values
#define INT64R(l, h) ( ((__int64)(h) << 32) | (l) )

int _stdcall
   CompareFILETIME(
      FILETIME               f1           ,// in -file time 1
      FILETIME               f2            // in -file time 2
   );

WCHAR * _stdcall                           // ret-result buffer conversion of str
   CommaStr(
      WCHAR                  result[]     ,// out-result buffer
      WCHAR          const   str[]         // in -string to be converted
   );
wchar_t * _stdcall                         // ret-DDDdHH:MM:SS elapsed time str
   ElapsedTimeStr(
      long                   secs         ,// in -number of seconds
      WCHAR                  str[]         // out-return buffer string
   );
