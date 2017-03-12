WCHAR *
   ServerNameGet(
      WCHAR                * serverName   ,// out-server name with leading "\\\\"
      WCHAR const          * path          // in -path to extract server name from
   );

WCHAR const *                             // ret-message text
   netmsg(
      DWORD                  code         // in -error code
   );
