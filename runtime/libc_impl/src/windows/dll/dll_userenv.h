#include "dlltable.h"

namespace sprt {

struct UserenvTable : DllTable {
	UserenvTable() : DllTable(L"userenv.dll", &__userenv_begin, &__userenv_end) { }

	DllTableRecord __userenv_begin;
	SPWIN_DEFINE_PROTO(CreateAppContainerProfile)
	SPWIN_DEFINE_PROTO(CreateEnvironmentBlock)
	SPWIN_DEFINE_PROTO(DestroyEnvironmentBlock)
	SPWIN_DEFINE_PROTO(EnterCriticalPolicySection)
	SPWIN_DEFINE_PROTO(ExpandEnvironmentStringsForUserA)
	SPWIN_DEFINE_PROTO(ExpandEnvironmentStringsForUserW)
	SPWIN_DEFINE_PROTO(GetAllUsersProfileDirectoryA)
	SPWIN_DEFINE_PROTO(GetAllUsersProfileDirectoryW)
	SPWIN_DEFINE_PROTO(GetAppliedGPOListW)
	SPWIN_DEFINE_PROTO(GetDefaultUserProfileDirectoryA)
	SPWIN_DEFINE_PROTO(GetDefaultUserProfileDirectoryW)
	SPWIN_DEFINE_PROTO(GetProfilesDirectoryA)
	SPWIN_DEFINE_PROTO(GetProfilesDirectoryW)
	SPWIN_DEFINE_PROTO(GetProfileType)
	SPWIN_DEFINE_PROTO(GetUserProfileDirectoryA)
	SPWIN_DEFINE_PROTO(GetUserProfileDirectoryW)
	SPWIN_DEFINE_PROTO(LeaveCriticalPolicySection)
	SPWIN_DEFINE_PROTO(LoadUserProfileA)
	SPWIN_DEFINE_PROTO(LoadUserProfileW)
	SPWIN_DEFINE_PROTO(RegisterGPNotification)
	SPWIN_DEFINE_PROTO(UnloadUserProfile)
	SPWIN_DEFINE_PROTO(UnregisterGPNotification)

	SPWIN_DEFINE_PROTO(DeriveAppContainerSidFromAppContainerName)
	SPWIN_DEFINE_PROTO(GetAppContainerFolderPath)
	SPWIN_DEFINE_PROTO(DeleteAppContainerProfile)

	DllTableRecord __userenv_end;
};

} // namespace sprt
