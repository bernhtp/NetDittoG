/*
================================================================================

  Program    - CompareFILETIME
  Class      - NetDitto Utility
  Author     - Tom Bernhardt
  Created    - 06/13/92
  Description- Compares two FILETIME structures and returns
                0  if equal
               +1  if f1 > f2
               -1  if f1 < f2

  Updates -

================================================================================
*/
#include <windows.h>
#include "util32.hpp"


int _stdcall
   CompareFILETIME(
      FILETIME               f1           ,// in -file time 1
      FILETIME               f2            // in -file time 2
   )
{
   if ( f1.dwHighDateTime == f2.dwHighDateTime )
      return f1.dwLowDateTime - f2.dwLowDateTime;
   return f1.dwHighDateTime - f2.dwHighDateTime;
}
