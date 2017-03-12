#include <windows.h>
#include <ctype.h>

__int64
   TextToInt64(
      WCHAR const          * str          ,// in -string value to parse
      __int64                minVal       ,// in -min allowed value for result
      __int64                maxVal       ,// in -max allowed value for result
      WCHAR const         ** errMsg        // out-error message pointer or NULL
   )
{
   __int64                   result = 0;
   BOOL                      negative = FALSE;
   WCHAR const             * c;
   enum {Snone, Ssign, Sdigit, Smag} state = Snone;

   *errMsg = NULL;
   for ( c = str;  *c;  c++ )
   {
      if ( state == Smag )
      {
         *errMsg = L"cannot have chars after magnitude";
         break;
      }

      switch ( tolower(*c) )
      {
         case L'-':
            if ( state == Snone )
            {
               state = Ssign;
               negative = TRUE;
            }
            else
               *errMsg = L"sign must come before digits";
            break;
         case L'k':
            if ( state != Sdigit )
               *errMsg = L"must have digits before K";
            else
            {
               result *= 1024;
               state = Smag;
            }
            break;
         case L'm':
            if ( state != Sdigit )
               *errMsg = L"must have digits before M";
            else
            {
               result *= 1024 * 1024;
               state = Smag;
            }
            break;
         default:
            if ( isdigit(*c) )
            {
               result = result * 10 + *c - L'0';
               state = Sdigit;
            }
            else
               *errMsg = L"invalid non-digit char(s)";
      }
   }
   if ( result > maxVal  ||  result < minVal )
      *errMsg = L"out of range";
   if ( state < Sdigit )
      *errMsg = L"no digits found";
   if ( negative )
      result = -result;

   return result;
}

