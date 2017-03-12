/*
Copyright (c) 1995-1997, Mission Critical Software, Inc. All rights reserved.
===============================================================================
Module      -  McsMsg.hpp
System      -  Common
Author      -  Rich Denham
Created     -  96/04/24
Description -  MCS error messages
Updates     -
===============================================================================
*/

struct
{
   DWORD                     msgnbr;       // message number
   wchar_t           const * message;      // message
}  McsMessage;

McsMessage static            McsMessages[] =
{
   {  ERROR_MCS_VerDll,
         L"Invalid version number. Client does not match DLL."                          },
   {  ERROR_MCS_VerClient,
         "Invalid version number. Client version is backlevel for this server. "       },
   {  ERROR_MCS_VerServer,
         "Invalid version number. Server version is backlevel for this client. "       },
   {  ERROR_MCS_SrvStartup,
         "Server initialization in progress. Try again in a minute."                   },
   {  ERROR_MCS_AccessDenied,
         "Request denied. You have not been delegated the Deputy Powers necessary "
         "to perform this action."                                                     },
   {  ERROR_MCS_AccessDeniedNotAdmin,
         "Access denied. You must be an NT administrator or a Marshal Admin "
         "to perform this action."                                                     },
   {  ERROR_MCS_DisallowedPower,
         "Access denied. Attempt to use power not allowed by policy."                  },
   {  ERROR_MCS_NameConventionFailure,
         "Access denied. Name does not conform to name convention rules."              },
   {  ERROR_MCS_AccessDeniedNotMarshal,
         "Access denied. You must be a Marshal to perform this action."                },
   {  ERROR_MCS_LicenseExpired,                                                        
         "EA License expired. Contact Mission Critical Software."                      },
   {  ERROR_MCS_LicenseInvalid,
         "EA License is missing or invalid."                                           },
   {  ERROR_MCS_UnsupportedArchitecture,
         "Machine architecture not supported."                                         },
   {  ERROR_MCS_TerritoryNotFound,
         "Territory not found"                                                         },
   {  ERROR_MCS_TerritoryExists,
         "Territory already exists"                                                    },
   {  ERROR_MCS_SectorNotFound,
         "Sector not found"                                                            },
   {  ERROR_MCS_SectorExists,
         "Sector already exists"                                                       },
   {  ERROR_MCS_DeputyNotFound,
         "Deputy not found"                                                            },
   {  ERROR_MCS_DeputyExists,
         "Deputy already exists"                                                       },
   {  ERROR_MCS_DomainNotFound,
         "Domain not found"                                                            },
   {  ERROR_MCS_DomainExists,
         "Domain already exists"                                                       },
   {  ERROR_MCS_GroupNotFound,
         "Group not found"                                                             },
   {  ERROR_MCS_GroupExists,
         "Group already exists"                                                        },
   {  ERROR_MCS_UserNotFound,
         "User not found"                                                              },
   {  ERROR_MCS_UserExists,
         "User already exists"                                                         },
   {  ERROR_MCS_MarshalNotFound,
         "Marshal not found"                                                           },
   {  ERROR_MCS_MarshalExists,
         "Marshal already exists"                                                      },
   {  ERROR_MCS_MemberNotFound,
         "Group member not found"                                                      },
   {  ERROR_MCS_MemberExists,
         "Group member already exists"                                                 },
   {  ERROR_MCS_TerritoryDtgNotFound,
         "Default Territory Group not found"                                           },
   {  ERROR_MCS_TerritoryDtgExists,
         "Default Territory Group already exists"                                      },
   {  ERROR_MCS_AccountNotFound,
         "Group or user not found"                                                     },
   {  ERROR_MCS_AccountExists,
         "Group or user already exists"                                                },
   { ERROR_MCS_InvalidTerritoryName,
         "Invalid territory name"                                                      },
   { ERROR_MCS_InvalidUserName,
         "Invalid user name"                                                           },
   { ERROR_MCS_InvalidGroupName,
         "Invalid group name"                                                          },
   {  ERROR_MCS_ReservedGroupName,
         "Request Denied. This action cannot be performed on EA Default "
         "Territory Groups."                                                           },
   {  ERROR_MCS_NameNotUnique,  
         "Name not unique. The specified name is not unique across domains trusted "
         "by the focus domain."                                                        },
   {  ERROR_MCS_CircularTerritorySector,
         "Circular territory definition. The territory being added as a sector "
         "contains the territory to which it is being added."                          },
   {  ERROR_MCS_GroupRenameMorePowers,   
         "Group rename failed because it results in the deputy having "
         "more powers over the group."                                                 },
   {  ERROR_MCS_GroupDeputyWithWildCard,   
         "Group deputy cannot contain a wildcard"                                      },
   {  ERROR_MCS_PasswordTooShort,
         "Password too short"                                                          },
   {  ERROR_MCS_IneligibleGGroupMember,
         "Account is ineligible for global group membership."                          },
   {  ERROR_MCS_IneligibleLGroupMember,
         "Account is ineligible for local group membership."                           },
   {  ERROR_MCS_GroupRenameNoPowers,    
         "Group rename failed because it results in the deputy having "
         "no powers over the group."                                                   },
   {  ERROR_MCS_HomeDirectoryAccessFail,
         "Home directory access failed."                                               },
   {  ERROR_MCS_HomeDirectoryCreateFail, 
         "Home directory create failed."                                               },
   {  ERROR_MCS_HomeDirectoryRenameFail, 
         "Home directory rename failed."                                               },
   {  ERROR_MCS_HomeDirectoryDeleteFail, 
         "Home directory delete failed."                                               },
   {  ERROR_MCS_HomeDirectoryPermFail,   
         "Error setting home directory file permissions."                              },
   {  ERROR_MCS_HomeDirectoryMalformed,  
         "Home directory name is not in UNC format."                                   },
   {  ERROR_MCS_HomeShareAccessFail,     
         "Home share access failed."                                                   },
   {  ERROR_MCS_HomeShareCreateFail,     
         "Home share create failed."                                                   },
   {  ERROR_MCS_HomeShareRenameFail,
         "Home share rename failed."                                                   },
   {  ERROR_MCS_HomeShareDeleteFail,
         "Home share delete failed."                                                   },
   {  ERROR_MCS_PrivilegesSetFail,
         "Unable to get backup and restore privileges."                                },

   {  ERROR_MCS_HomeDirCreate1,
         "An error occurred while EA was creating the home directory."                 },
   {  ERROR_MCS_HomeDirCreate2,
         "You will have to set up the user's home directory manually or"
         "contact your central administrator to resolve this matter."                  },
   {  ERROR_MCS_HomeDirDelete1,
         "An error occurred while EA was deleting the home directory."                 },
   {  ERROR_MCS_HomeDirDelete2,
         "You will have to set up the user's home directory manually or "
         "contact your central administrator to resolve this matter."                  },
   {  ERROR_MCS_HomeDirRename1,
         "An error occurred while EA was renaming the home directory."                 },
   {  ERROR_MCS_HomeDirRename2,
         "You will have to set up the user's home directory manually or "
         "contact your central administrator to resolve this matter."                  },

   {  ERROR_MCS_HomeShrCreate1,
         "An error occurred while EA was creating the home share."                     },
   {  ERROR_MCS_HomeShrCreate2,
         "You will have to set up the user's home share manually or "
         "contact your central administrator to resolve this matter."                  },
   {  ERROR_MCS_HomeShrDelete1,
         "An error occurred while EA was deleting the home share."                     },
   {  ERROR_MCS_HomeShrDelete2,
         "You will have to set up the user's home share manually or "
         "contact your central administrator to resolve this matter."                  },
   {  ERROR_MCS_HomeShrRename1,
         "An error occurred while EA was renaming the home share."                     },
   {  ERROR_MCS_HomeShrRename2,
         "You will have to set up the user's home share manually or "
         "contact your central administrator to resolve this matter."                  },

   {  ERROR_MCS_HomeDirShrCreate1,
         "An error occurred while EA was creating the home directory and home share."  },
   {  ERROR_MCS_HomeDirShrCreate2,
         "You will have to set up the user's home directory and home share manually or "
         "contact your central administrator to resolve this matter."                  },
   {  ERROR_MCS_HomeDirShrDelete1,
         "An error occurred while EA was deleting the home directory and home share."  },
   {  ERROR_MCS_HomeDirShrDelete2,
         "You will have to set up the user's home directory and home share manually or "
         "contact your central administrator to resolve this matter."                  },
   {  ERROR_MCS_HomeDirShrRename1,
         "An error occurred while EA was renaming the home directory and home share."  },
   {  ERROR_MCS_HomeDirShrRename2,
         "You will have to set up the user's home directory and home share manually or "
         "contact your central administrator to resolve this matter."                  },

   {  ERROR_MCS_OutOfMemory, 
         "Error allocating memory."                                                    },
   {  ERROR_MCS_WrongAlogorthm,                
         "Error trying to decrypt. Wrong algorithm."                                   },
   {  ERROR_MCS_ErrorOpeningFile,
         "Error Opening file."                                                         },
   {  ERROR_MCS_InvalidFilename,
         "Invalid filename."                                                           },
   {  ERROR_MCS_InvalidRegKey,
         "Invalid registry key."                                                       },
   {  ERROR_MCS_SameLicenseInReg,
         "Error installing the same license."                                          },
   {  ERROR_MCS_TrialExpired,
         "Trial License expired."                                                      },
   {  ERROR_MCS_ExceededFinalDate,
         "Trial exceeded final date."                                                  },
   {  ERROR_MCS_FPNWUserNotEnabled,
         "User does not have a Netware compatible login."                              },
   {  ERROR_MCS_FPNWPasswordRequired,
         "Password is required to allow the user to have a Netware compatible login."  },
   {  ERROR_MCS_FPNWGettingPWExpFlag,
         "Error getting the Netware password expired flag."                            },
   {  ERROR_MCS_FPNWSettingPWExpFlag,
         "Error setting the Netware password expired flag."                            },
   {  ERROR_MCS_FPNWGettingObjectId,
         "Error getting the Netware Object Id."                                        },
   {  ERROR_MCS_FPNWGettingUserInfo,                 
         "Error getting the Netware information for this user."                        },
   {  ERROR_MCS_FPNWEnableFail,
         "Unable to enable Netware Compatible Login for the user because of insufficient information." },
   {  ERROR_MCS_FPNWInvalidScriptPath,
         "Unable to write to the login script.  The server may be down or the script path is invalid." },
   {  ERROR_MCS_FPNWRegistryAccessError,
         "Unable to access the FPNW registry key on the FPNW Server. Can't get the script path." },

};

// McsMsg.hpp - end of file
