/*
===============================================================================
Module      -  WildMatch.cpp
System      -  Common
Author      -  Rich Denham
Created     -  95/09/15
Description -
   WildMatch   Perform a pattern match upon a subject string using a wildcard
               string.  All but three character codes in the wildcard string
               match a corresponding character in the subject string.  The
               character '*' matches zero or more characters; the character
               '?' matches any one character; and the character '#' matches
               one numeric character or '#'.  If the wildcard string completely
               matches the subject string, the returned value is 0.  Otherwise
               the returned value is 1.  Comparison of alphabetic characters
               is case-sensitive.
               Functions are available when both arguments are UNICODE or when
               both arguments are ANSI.  In the latter case, both
               (signed char *) and (unsigned char *) are supported in any
               combination.

               Examples
               Subject string     Wildcard string   Returned value
               "foo.dat"          "*.dat"           0
               "nob_hill5"        "*#"              0
               "widget"           "*z*"             1

   WildMatchI  Same as WildMatch except that the case of the subject string
               characters and wildcard string characters is ignored.

   WildNormI   This is a companion service to WildMatchI that sets the case
               of characters in the subject string to the same case as the
               corresponding matching non-wildcard characters in the wildcard
               string.
Updates     -
  96/03/21 RED Add WildMatchI routines
  97/02/14 RED Add WildReplI routines
===============================================================================
*/

#include "windows.h"

#include "Common.hpp"
#include "WildMatch.hpp"

// WildMatch - using UNICODE strings
DWORD                                      // ret-0 if matched, 1 if not
   WildMatch(
      WCHAR          const * subjStr      ,// in -subject string
      WCHAR          const * wildStr       // in -wildcard string
   )
{
   DWORD                     retVal=0;
   DWORD                     wildIndex=0;
   USHORT                    wildMatch[MAX_WILDSTR_LEN+1];

   if ( wcslen( wildStr ) > MAX_WILDSTR_LEN )
   {
      retVal = 1;
   }
   else
   {
      for ( ;; )
      {
         if ( (*wildStr == L'\0') && (*subjStr == L'\0') )
         {
            break;                         // both strings at the end
         }
         if ( *wildStr == L'*' )
         {                                 // initial match of '*'
            if ( *(++wildStr) == L'\0' )
            {
               break;                      // '*' at end - automatic success
            }
            else
            {                              // '*' not at end
               wildMatch[wildIndex] = 0;
               wildIndex++;
               continue;
            }
         }
         if ( (*wildStr == *subjStr) ||
              ((*wildStr == L'?') && (*subjStr != L'\0')) ||
              ((*wildStr == L'#') && (*subjStr >= L'0') && (*subjStr <= L'9')) )
         {                                 // initial match of everything else
            wildMatch[wildIndex] = 1;
            wildIndex++;
            wildStr++;
            subjStr++;
            continue;
         }
         // failure - back up needle
         for ( ;; )
         {
            if ( !wildIndex )
            {
               retVal = 1;
               break;                      // pattern match failure
            }
            wildIndex--;
            wildStr--;
            subjStr -= wildMatch[wildIndex];
            // This is the only pattern match alternation.  For the asterisk, match one more
            // char, but not if the additional source string char is a null.
            if ( (*wildStr == L'*') && (*(subjStr+wildMatch[wildIndex]) != L'\0') )
            {
               wildMatch[wildIndex]++;
               wildStr++;
               subjStr += wildMatch[wildIndex];
               wildIndex++;
               break;
            }
         }
         if ( retVal == 1 )
         {
            break;
         }
      }
   }

   return retVal;
}

// WildMatchI - using UNICODE strings
DWORD                                      // ret-0 if matched, 1 if not
   WildMatchI(
      WCHAR          const * subjStr      ,// in -subject string
      WCHAR          const * wildStr      ,// in -wildcard string
      USHORT                 wildArray[MAX_WILDSTR_LEN] // out-optional matches
   )
{
   DWORD                     retVal = 0;
   DWORD                     wildIndex = 0;
   USHORT                    wildMatch[MAX_WILDSTR_LEN+1];

   if ( wcslen( wildStr ) > MAX_WILDSTR_LEN )
   {
      retVal = 1;
   }
   else
   {
      for ( ;; )
      {
         if ( (*wildStr == L'\0') && (*subjStr == L'\0') )
         {
            break;                         // both strings at the end
         }
         if ( *wildStr == L'*' )
         {                                 // initial match of '*'
            if ( *(++wildStr) == L'\0' )
            {
               if ( wildArray )
               {
                  wildMatch[wildIndex] = (USHORT)wcslen( subjStr );
               }
               break;                      // '*' at end - automatic success
            }
            else
            {                              // '*' not at end
               wildMatch[wildIndex] = 0;
               wildIndex++;
               continue;
            }
         }
         if ( (towlower(*wildStr) == towlower(*subjStr)) ||
              ((*wildStr == L'?') && (*subjStr != L'\0')) ||
              ((*wildStr == L'#') && (*subjStr >= L'0') && (*subjStr <= L'9')) )
         {                                 // initial match of everything else
            wildMatch[wildIndex] = 1;
            wildIndex++;
            wildStr++;
            subjStr++;
            continue;
         }
         // failure - back up needle
         for ( ;; )
         {
            if ( !wildIndex )
            {
               retVal = 1;
               break;                      // pattern match failure
            }
            wildIndex--;
            wildStr--;
            subjStr -= wildMatch[wildIndex];
            // This is the only pattern match alternation.  For the asterisk, match one more
            // char, but not if the additional source string char is a null.
            if ( (*wildStr == L'*') && (*(subjStr+wildMatch[wildIndex]) != L'\0') )
            {
               wildMatch[wildIndex]++;
               wildStr++;
               subjStr += wildMatch[wildIndex];
               wildIndex++;
               break;
            }
         }
         if ( retVal == 1 )
         {
            break;
         }
      }
   }

   if ( wildArray )
   {
      memcpy( wildArray, wildMatch, MAX_WILDSTR_LEN * sizeof wildArray[0] );
   }

   return retVal;
}

// WildReplI - using UNICODE strings
BOOL                                       // ret-TRUE if any change was made
   WildReplI(
      WCHAR                * subjStr      ,// i/o-subject string
      WCHAR          const * wildStr       // in -wildcard string
   )
{
   BOOL                      bChanged=FALSE; // TRUE if any change was made
   USHORT                    wildMatch[MAX_WILDSTR_LEN];
   WCHAR                   * pSubjStr;     // scan subject string
   WCHAR             const * pWildStr;     // scan wildcard string
   USHORT            const * pWildMatch;   // scan wildmatch array

   if ( !WildMatchI( subjStr, wildStr, wildMatch ) )
   {
      for ( pSubjStr = subjStr,
            pWildStr = wildStr,
            pWildMatch = wildMatch;
            *pWildStr;
            pSubjStr += *pWildMatch,
            pWildMatch++,
            pWildStr++ )
      {
         if ( *pWildMatch == 1 )
         {
            switch ( *pWildStr )
            {
               case L'*':
               case L'?':
               case L'#':
                  break;
               default:
                  if ( *pSubjStr != *pWildStr )
                  {
                     bChanged = TRUE;
                  }
                  *pSubjStr = *pWildStr;
                  break;

            }
         }
      }
   }

   return bChanged;
}

