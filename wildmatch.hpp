/*
===============================================================================
Module      -  WildMatch.hpp
System      -  Common
Author      -  Rich Denham
Created     -  1995-09-15
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

#ifndef  MCSINC_WildMatch_hpp
#define  MCSINC_WildMatch_hpp

#define  MAX_WILDSTR_LEN  (300)            // maximum supported length of wildcard string

// WildMatch using UNICODE strings
DWORD                                      // ret-0 if matched, 1 if not
   WildMatch(
      WCHAR          const * subjStr      ,// in -subject string
      WCHAR          const * wildStr       // in -wildcard string
   );

// WildMatchI using UNICODE strings
DWORD                                      // ret-0 if matched, 1 if not
   WildMatchI(
      WCHAR          const * subjStr      ,// in -subject string
      WCHAR          const * wildStr      ,// in -wildcard string
      USHORT                 wildMatch[MAX_WILDSTR_LEN] = NULL // out-optional matches
   );
// WildReplI - using UNICODE strings
BOOL                                       // ret-TRUE if any change was made
   WildReplI(
      WCHAR                * subjStr      ,// i/o-subject string
      WCHAR          const * wildStr       // in -wildcard string
   );

// WildMatch using ANSI strings
DWORD                                      // ret-0 if matched, 1 if not
   WildMatch(
      UCHAR          const * subjStr      ,// in -subject string
      UCHAR          const * wildStr       // in -wildcard string
   );

// WildMatchI using ANSI strings
DWORD                                      // ret-0 if matched, 1 if not
   WildMatchI(
      UCHAR          const * subjStr      ,// in -subject string
      UCHAR          const * wildStr      ,// in -wildcard string
      USHORT                 wildMatch[MAX_WILDSTR_LEN] = NULL // out-optional matches
   );
// WildReplI - using ANSI strings
BOOL                                       // ret-TRUE if any change was made
   WildReplI(
      UCHAR                * subjStr      ,// i/o-subject string
      UCHAR          const * wildStr       // in -wildcard string
   );

// WildMatch using ANSI strings - allow any combination of signed char and unsigned char
DWORD _inline WildMatch(  UCHAR const *  subjStr,   char const *  wildStr )
   {   return WildMatch(                 subjStr, (UCHAR const *) wildStr ); }
DWORD _inline WildMatch(   char const *  subjStr,  UCHAR const *  wildStr )
   {   return WildMatch( (UCHAR const *) subjStr,                 wildStr ); }
DWORD _inline WildMatch(   char const *  subjStr,   char const *  wildStr )
   {   return WildMatch( (UCHAR const *) subjStr, (UCHAR const *) wildStr ); }

// WildMatchI using ANSI strings - allow any combination of signed char and unsigned char
DWORD _inline WildMatchI(  UCHAR const *  subjStr,   char const *  wildStr )
   {   return WildMatchI(                 subjStr, (UCHAR const *) wildStr ); }
DWORD _inline WildMatchI(   char const *  subjStr,  UCHAR const *  wildStr )
   {   return WildMatchI( (UCHAR const *) subjStr,                 wildStr ); }
DWORD _inline WildMatchI(   char const *  subjStr,   char const *  wildStr )
   {   return WildMatchI( (UCHAR const *) subjStr, (UCHAR const *) wildStr ); }

#endif  // MCSINC_WildMatch_hpp

// WildMatch.hpp - end of file
