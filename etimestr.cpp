/*
================================================================================

 Module     - etimestr
 Program    - ElapsedTimeStr
 Class      - UTIL general system subroutine
 Author     - Tom Bernhardt
 Created    - 12/07/90
 Description- Converts the secs formal parameter to a string in the format
              "DDDdHH:MM:SS" (Days, Hours, Min, Secs).
 Updates -

================================================================================
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

wchar_t * _stdcall                        // ret-DDDdHH:MM:SS elapsed time str
   ElapsedTimeStr(
      long                   secs        ,// in -number of seconds
      wchar_t                str[]        // out-return buffer string
   )
{
   short                     h, m, s;

   s = (short)(secs % 60);                // ss = number of SS seconds
   secs /= 60;                            // secs = number of minutes

   m = (short)(secs % 60);                // mm = number of MM minutes
   secs /= 60;                            // secs = number of hours

   h = (short)(secs % 24);                // hh = number of HH hourcs
   secs /= 24;                            // secs = number of days

   if ( secs == 0 )                       // if no days
      _swprintf(str, L"    %2d:%02d:%02d", h, m, s);
   else
      _swprintf(str, L"%3ldd%2d:%02d:%02d", secs, h, m, s);

   return str;
}
