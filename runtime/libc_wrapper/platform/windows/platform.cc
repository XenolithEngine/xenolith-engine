/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#include <sprt/runtime/platform.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/log.h>
#include <sprt/cxx/detail/ctypes.h>
#include <sprt/wrappers/windows/winerror.h>
#include <sprt/wrappers/windows/basic_types.h>
#include <sprt/wrappers/windows/security_api.h>
#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/com_cxx.hpp>
#include <sprt/wrappers/windows/file_api.h>
#include <sprt/wrappers/windows/dl_api.h>
#include <sprt/c/bits/__sprt_errno.h>

#include "../src/private/SPRTFilesystem.h"

/*
	AI-Generated
*/

#define DEFINE_KNOWN_FOLDER(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        static const GUID name  = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

// legacy CSIDL value: CSIDL_NETWORK
// display name: "Network"
// legacy display name: "My Network Places"
// default path:
// {D20BEEC4-5CA8-4905-AE3B-BF251EA09B53}
DEFINE_KNOWN_FOLDER(FOLDERID_NetworkFolder, 0xD20B'EEC4, 0x5CA8, 0x4905, 0xAE, 0x3B, 0xBF, 0x25,
		0x1E, 0xA0, 0x9B, 0x53);

// {0AC0837C-BBF8-452A-850D-79D08E667CA7}
DEFINE_KNOWN_FOLDER(FOLDERID_ComputerFolder, 0x0AC0'837C, 0xBBF8, 0x452A, 0x85, 0x0D, 0x79, 0xD0,
		0x8E, 0x66, 0x7C, 0xA7);

// {4D9F7874-4E0C-4904-967B-40B0D20C3E4B}
DEFINE_KNOWN_FOLDER(FOLDERID_InternetFolder, 0x4D9F'7874, 0x4E0C, 0x4904, 0x96, 0x7B, 0x40, 0xB0,
		0xD2, 0x0C, 0x3E, 0x4B);

// {82A74AEB-AEB4-465C-A014-D097EE346D63}
DEFINE_KNOWN_FOLDER(FOLDERID_ControlPanelFolder, 0x82A7'4AEB, 0xAEB4, 0x465C, 0xA0, 0x14, 0xD0,
		0x97, 0xEE, 0x34, 0x6D, 0x63);

// {76FC4E2D-D6AD-4519-A663-37BD56068185}
DEFINE_KNOWN_FOLDER(FOLDERID_PrintersFolder, 0x76FC'4E2D, 0xD6AD, 0x4519, 0xA6, 0x63, 0x37, 0xBD,
		0x56, 0x06, 0x81, 0x85);

// {43668BF8-C14E-49B2-97C9-747784D784B7}
DEFINE_KNOWN_FOLDER(FOLDERID_SyncManagerFolder, 0x4366'8BF8, 0xC14E, 0x49B2, 0x97, 0xC9, 0x74, 0x77,
		0x84, 0xD7, 0x84, 0xB7);

// {0F214138-B1D3-4a90-BBA9-27CBC0C5389A}
DEFINE_KNOWN_FOLDER(FOLDERID_SyncSetupFolder, 0xf21'4138, 0xb1d3, 0x4a90, 0xbb, 0xa9, 0x27, 0xcb,
		0xc0, 0xc5, 0x38, 0x9a);

// {4bfefb45-347d-4006-a5be-ac0cb0567192}
DEFINE_KNOWN_FOLDER(FOLDERID_ConflictFolder, 0x4bfe'fb45, 0x347d, 0x4006, 0xa5, 0xbe, 0xac, 0x0c,
		0xb0, 0x56, 0x71, 0x92);

// {289a9a43-be44-4057-a41b-587a76d7e7f9}
DEFINE_KNOWN_FOLDER(FOLDERID_SyncResultsFolder, 0x289a'9a43, 0xbe44, 0x4057, 0xa4, 0x1b, 0x58, 0x7a,
		0x76, 0xd7, 0xe7, 0xf9);

// {B7534046-3ECB-4C18-BE4E-64CD4CB7D6AC}
DEFINE_KNOWN_FOLDER(FOLDERID_RecycleBinFolder, 0xB753'4046, 0x3ECB, 0x4C18, 0xBE, 0x4E, 0x64, 0xCD,
		0x4C, 0xB7, 0xD6, 0xAC);

// {6F0CD92B-2E97-45D1-88FF-B0D186B8DEDD}
DEFINE_KNOWN_FOLDER(FOLDERID_ConnectionsFolder, 0x6F0C'D92B, 0x2E97, 0x45D1, 0x88, 0xFF, 0xB0, 0xD1,
		0x86, 0xB8, 0xDE, 0xDD);

// {FD228CB7-AE11-4AE3-864C-16F3910AB8FE}
DEFINE_KNOWN_FOLDER(FOLDERID_Fonts, 0xFD22'8CB7, 0xAE11, 0x4AE3, 0x86, 0x4C, 0x16, 0xF3, 0x91, 0x0A,
		0xB8, 0xFE);

// display name:        "Desktop"
// default path:        "C:\Users\<UserName>\Desktop"
// legacy default path: "C:\Documents and Settings\<userName>\Desktop"
// legacy CSIDL value:  CSIDL_DESKTOP
// {B4BFCC3A-DB2C-424C-B029-7FE99A87C641}
DEFINE_KNOWN_FOLDER(FOLDERID_Desktop, 0xB4BF'CC3A, 0xDB2C, 0x424C, 0xB0, 0x29, 0x7F, 0xE9, 0x9A,
		0x87, 0xC6, 0x41);

// {B97D20BB-F46A-4C97-BA10-5E3608430854}
DEFINE_KNOWN_FOLDER(FOLDERID_Startup, 0xB97D'20BB, 0xF46A, 0x4C97, 0xBA, 0x10, 0x5E, 0x36, 0x08,
		0x43, 0x08, 0x54);

// {A77F5D77-2E2B-44C3-A6A2-ABA601054A51}
DEFINE_KNOWN_FOLDER(FOLDERID_Programs, 0xA77F'5D77, 0x2E2B, 0x44C3, 0xA6, 0xA2, 0xAB, 0xA6, 0x01,
		0x05, 0x4A, 0x51);

// {625B53C3-AB48-4EC1-BA1F-A1EF4146FC19}
DEFINE_KNOWN_FOLDER(FOLDERID_StartMenu, 0x625B'53C3, 0xAB48, 0x4EC1, 0xBA, 0x1F, 0xA1, 0xEF, 0x41,
		0x46, 0xFC, 0x19);

// {AE50C081-EBD2-438A-8655-8A092E34987A}
DEFINE_KNOWN_FOLDER(FOLDERID_Recent, 0xAE50'C081, 0xEBD2, 0x438A, 0x86, 0x55, 0x8A, 0x09, 0x2E,
		0x34, 0x98, 0x7A);

// {8983036C-27C0-404B-8F08-102D10DCFD74}
DEFINE_KNOWN_FOLDER(FOLDERID_SendTo, 0x8983'036C, 0x27C0, 0x404B, 0x8F, 0x08, 0x10, 0x2D, 0x10,
		0xDC, 0xFD, 0x74);

// {FDD39AD0-238F-46AF-ADB4-6C85480369C7}
DEFINE_KNOWN_FOLDER(FOLDERID_Documents, 0xFDD3'9AD0, 0x238F, 0x46AF, 0xAD, 0xB4, 0x6C, 0x85, 0x48,
		0x03, 0x69, 0xC7);

// {1777F761-68AD-4D8A-87BD-30B759FA33DD}
DEFINE_KNOWN_FOLDER(FOLDERID_Favorites, 0x1777'F761, 0x68AD, 0x4D8A, 0x87, 0xBD, 0x30, 0xB7, 0x59,
		0xFA, 0x33, 0xDD);

// {C5ABBF53-E17F-4121-8900-86626FC2C973}
DEFINE_KNOWN_FOLDER(FOLDERID_NetHood, 0xC5AB'BF53, 0xE17F, 0x4121, 0x89, 0x00, 0x86, 0x62, 0x6F,
		0xC2, 0xC9, 0x73);

// {9274BD8D-CFD1-41C3-B35E-B13F55A758F4}
DEFINE_KNOWN_FOLDER(FOLDERID_PrintHood, 0x9274'BD8D, 0xCFD1, 0x41C3, 0xB3, 0x5E, 0xB1, 0x3F, 0x55,
		0xA7, 0x58, 0xF4);

// {A63293E8-664E-48DB-A079-DF759E0509F7}
DEFINE_KNOWN_FOLDER(FOLDERID_Templates, 0xA632'93E8, 0x664E, 0x48DB, 0xA0, 0x79, 0xDF, 0x75, 0x9E,
		0x05, 0x09, 0xF7);

// {82A5EA35-D9CD-47C5-9629-E15D2F714E6E}
DEFINE_KNOWN_FOLDER(FOLDERID_CommonStartup, 0x82A5'EA35, 0xD9CD, 0x47C5, 0x96, 0x29, 0xE1, 0x5D,
		0x2F, 0x71, 0x4E, 0x6E);

// {0139D44E-6AFE-49F2-8690-3DAFCAE6FFB8}
DEFINE_KNOWN_FOLDER(FOLDERID_CommonPrograms, 0x0139'D44E, 0x6AFE, 0x49F2, 0x86, 0x90, 0x3D, 0xAF,
		0xCA, 0xE6, 0xFF, 0xB8);

// {A4115719-D62E-491D-AA7C-E74B8BE3B067}
DEFINE_KNOWN_FOLDER(FOLDERID_CommonStartMenu, 0xA411'5719, 0xD62E, 0x491D, 0xAA, 0x7C, 0xE7, 0x4B,
		0x8B, 0xE3, 0xB0, 0x67);

// {C4AA340D-F20F-4863-AFEF-F87EF2E6BA25}
DEFINE_KNOWN_FOLDER(FOLDERID_PublicDesktop, 0xC4AA'340D, 0xF20F, 0x4863, 0xAF, 0xEF, 0xF8, 0x7E,
		0xF2, 0xE6, 0xBA, 0x25);

// {62AB5D82-FDC1-4DC3-A9DD-070D1D495D97}
DEFINE_KNOWN_FOLDER(FOLDERID_ProgramData, 0x62AB'5D82, 0xFDC1, 0x4DC3, 0xA9, 0xDD, 0x07, 0x0D, 0x1D,
		0x49, 0x5D, 0x97);

// {B94237E7-57AC-4347-9151-B08C6C32D1F7}
DEFINE_KNOWN_FOLDER(FOLDERID_CommonTemplates, 0xB942'37E7, 0x57AC, 0x4347, 0x91, 0x51, 0xB0, 0x8C,
		0x6C, 0x32, 0xD1, 0xF7);

// {ED4824AF-DCE4-45A8-81E2-FC7965083634}
DEFINE_KNOWN_FOLDER(FOLDERID_PublicDocuments, 0xED48'24AF, 0xDCE4, 0x45A8, 0x81, 0xE2, 0xFC, 0x79,
		0x65, 0x08, 0x36, 0x34);

// {3EB685DB-65F9-4CF6-A03A-E3EF65729F3D}
DEFINE_KNOWN_FOLDER(FOLDERID_RoamingAppData, 0x3EB6'85DB, 0x65F9, 0x4CF6, 0xA0, 0x3A, 0xE3, 0xEF,
		0x65, 0x72, 0x9F, 0x3D);

// {F1B32785-6FBA-4FCF-9D55-7B8E7F157091}
DEFINE_KNOWN_FOLDER(FOLDERID_LocalAppData, 0xF1B3'2785, 0x6FBA, 0x4FCF, 0x9D, 0x55, 0x7B, 0x8E,
		0x7F, 0x15, 0x70, 0x91);

// {A520A1A4-1780-4FF6-BD18-167343C5AF16}
DEFINE_KNOWN_FOLDER(FOLDERID_LocalAppDataLow, 0xA520'A1A4, 0x1780, 0x4FF6, 0xBD, 0x18, 0x16, 0x73,
		0x43, 0xC5, 0xAF, 0x16);

// {352481E8-33BE-4251-BA85-6007CAEDCF9D}
DEFINE_KNOWN_FOLDER(FOLDERID_InternetCache, 0x3524'81E8, 0x33BE, 0x4251, 0xBA, 0x85, 0x60, 0x07,
		0xCA, 0xED, 0xCF, 0x9D);

// {2B0F765D-C0E9-4171-908E-08A611B84FF6}
DEFINE_KNOWN_FOLDER(FOLDERID_Cookies, 0x2B0F'765D, 0xC0E9, 0x4171, 0x90, 0x8E, 0x08, 0xA6, 0x11,
		0xB8, 0x4F, 0xF6);

// {D9DC8A3B-B784-432E-A781-5A1130A75963}
DEFINE_KNOWN_FOLDER(FOLDERID_History, 0xD9DC'8A3B, 0xB784, 0x432E, 0xA7, 0x81, 0x5A, 0x11, 0x30,
		0xA7, 0x59, 0x63);

// {1AC14E77-02E7-4E5D-B744-2EB1AE5198B7}
DEFINE_KNOWN_FOLDER(FOLDERID_System, 0x1AC1'4E77, 0x02E7, 0x4E5D, 0xB7, 0x44, 0x2E, 0xB1, 0xAE,
		0x51, 0x98, 0xB7);

// {D65231B0-B2F1-4857-A4CE-A8E7C6EA7D27}
DEFINE_KNOWN_FOLDER(FOLDERID_SystemX86, 0xD652'31B0, 0xB2F1, 0x4857, 0xA4, 0xCE, 0xA8, 0xE7, 0xC6,
		0xEA, 0x7D, 0x27);

// {F38BF404-1D43-42F2-9305-67DE0B28FC23}
DEFINE_KNOWN_FOLDER(FOLDERID_Windows, 0xF38B'F404, 0x1D43, 0x42F2, 0x93, 0x05, 0x67, 0xDE, 0x0B,
		0x28, 0xFC, 0x23);

// {5E6C858F-0E22-4760-9AFE-EA3317B67173}
DEFINE_KNOWN_FOLDER(FOLDERID_Profile, 0x5E6C'858F, 0x0E22, 0x4760, 0x9A, 0xFE, 0xEA, 0x33, 0x17,
		0xB6, 0x71, 0x73);

// {33E28130-4E1E-4676-835A-98395C3BC3BB}
DEFINE_KNOWN_FOLDER(FOLDERID_Pictures, 0x33E2'8130, 0x4E1E, 0x4676, 0x83, 0x5A, 0x98, 0x39, 0x5C,
		0x3B, 0xC3, 0xBB);

// {7C5A40EF-A0FB-4BFC-874A-C0F2E0B9FA8E}
DEFINE_KNOWN_FOLDER(FOLDERID_ProgramFilesX86, 0x7C5A'40EF, 0xA0FB, 0x4BFC, 0x87, 0x4A, 0xC0, 0xF2,
		0xE0, 0xB9, 0xFA, 0x8E);

// {DE974D24-D9C6-4D3E-BF91-F4455120B917}
DEFINE_KNOWN_FOLDER(FOLDERID_ProgramFilesCommonX86, 0xDE97'4D24, 0xD9C6, 0x4D3E, 0xBF, 0x91, 0xF4,
		0x45, 0x51, 0x20, 0xB9, 0x17);

// {6D809377-6AF0-444b-8957-A3773F02200E}
DEFINE_KNOWN_FOLDER(FOLDERID_ProgramFilesX64, 0x6d80'9377, 0x6af0, 0x444b, 0x89, 0x57, 0xa3, 0x77,
		0x3f, 0x02, 0x20, 0x0e);

// {6365D5A7-0F0D-45e5-87F6-0DA56B6A4F7D}
DEFINE_KNOWN_FOLDER(FOLDERID_ProgramFilesCommonX64, 0x6365'd5a7, 0xf0d, 0x45e5, 0x87, 0xf6, 0xd,
		0xa5, 0x6b, 0x6a, 0x4f, 0x7d);

// {905e63b6-c1bf-494e-b29c-65b732d3d21a}
DEFINE_KNOWN_FOLDER(FOLDERID_ProgramFiles, 0x905e'63b6, 0xc1bf, 0x494e, 0xb2, 0x9c, 0x65, 0xb7,
		0x32, 0xd3, 0xd2, 0x1a);

// {F7F1ED05-9F6D-47A2-AAAE-29D317C6F066}
DEFINE_KNOWN_FOLDER(FOLDERID_ProgramFilesCommon, 0xF7F1'ED05, 0x9F6D, 0x47A2, 0xAA, 0xAE, 0x29,
		0xD3, 0x17, 0xC6, 0xF0, 0x66);

// {5cd7aee2-2219-4a67-b85d-6c9ce15660cb}
DEFINE_KNOWN_FOLDER(FOLDERID_UserProgramFiles, 0x5cd7'aee2, 0x2219, 0x4a67, 0xb8, 0x5d, 0x6c, 0x9c,
		0xe1, 0x56, 0x60, 0xcb);

// {bcbd3057-ca5c-4622-b42d-bc56db0ae516}
DEFINE_KNOWN_FOLDER(FOLDERID_UserProgramFilesCommon, 0xbcbd'3057, 0xca5c, 0x4622, 0xb4, 0x2d, 0xbc,
		0x56, 0xdb, 0x0a, 0xe5, 0x16);

// {724EF170-A42D-4FEF-9F26-B60E846FBA4F}
DEFINE_KNOWN_FOLDER(FOLDERID_AdminTools, 0x724E'F170, 0xA42D, 0x4FEF, 0x9F, 0x26, 0xB6, 0x0E, 0x84,
		0x6F, 0xBA, 0x4F);

// {D0384E7D-BAC3-4797-8F14-CBA229B392B5}
DEFINE_KNOWN_FOLDER(FOLDERID_CommonAdminTools, 0xD038'4E7D, 0xBAC3, 0x4797, 0x8F, 0x14, 0xCB, 0xA2,
		0x29, 0xB3, 0x92, 0xB5);

// {4BD8D571-6D19-48D3-BE97-422220080E43}
DEFINE_KNOWN_FOLDER(FOLDERID_Music, 0x4BD8'D571, 0x6D19, 0x48D3, 0xBE, 0x97, 0x42, 0x22, 0x20, 0x08,
		0x0E, 0x43);

// {18989B1D-99B5-455B-841C-AB7C74E4DDFC}
DEFINE_KNOWN_FOLDER(FOLDERID_Videos, 0x1898'9B1D, 0x99B5, 0x455B, 0x84, 0x1C, 0xAB, 0x7C, 0x74,
		0xE4, 0xDD, 0xFC);

// {C870044B-F49E-4126-A9C3-B52A1FF411E8}
DEFINE_KNOWN_FOLDER(FOLDERID_Ringtones, 0xC870'044B, 0xF49E, 0x4126, 0xA9, 0xC3, 0xB5, 0x2A, 0x1F,
		0xF4, 0x11, 0xE8);

// {B6EBFB86-6907-413C-9AF7-4FC2ABF07CC5}
DEFINE_KNOWN_FOLDER(FOLDERID_PublicPictures, 0xB6EB'FB86, 0x6907, 0x413C, 0x9A, 0xF7, 0x4F, 0xC2,
		0xAB, 0xF0, 0x7C, 0xC5);

// {3214FAB5-9757-4298-BB61-92A9DEAA44FF}
DEFINE_KNOWN_FOLDER(FOLDERID_PublicMusic, 0x3214'FAB5, 0x9757, 0x4298, 0xBB, 0x61, 0x92, 0xA9, 0xDE,
		0xAA, 0x44, 0xFF);

// {2400183A-6185-49FB-A2D8-4A392A602BA3}
DEFINE_KNOWN_FOLDER(FOLDERID_PublicVideos, 0x2400'183A, 0x6185, 0x49FB, 0xA2, 0xD8, 0x4A, 0x39,
		0x2A, 0x60, 0x2B, 0xA3);

// {E555AB60-153B-4D17-9F04-A5FE99FC15EC}
DEFINE_KNOWN_FOLDER(FOLDERID_PublicRingtones, 0xE555'AB60, 0x153B, 0x4D17, 0x9F, 0x04, 0xA5, 0xFE,
		0x99, 0xFC, 0x15, 0xEC);

// {8AD10C31-2ADB-4296-A8F7-E4701232C972}
DEFINE_KNOWN_FOLDER(FOLDERID_ResourceDir, 0x8AD1'0C31, 0x2ADB, 0x4296, 0xA8, 0xF7, 0xE4, 0x70, 0x12,
		0x32, 0xC9, 0x72);

// {2A00375E-224C-49DE-B8D1-440DF7EF3DDC}
DEFINE_KNOWN_FOLDER(FOLDERID_LocalizedResourcesDir, 0x2A00'375E, 0x224C, 0x49DE, 0xB8, 0xD1, 0x44,
		0x0D, 0xF7, 0xEF, 0x3D, 0xDC);

// {C1BAE2D0-10DF-4334-BEDD-7AA20B227A9D}
DEFINE_KNOWN_FOLDER(FOLDERID_CommonOEMLinks, 0xC1BA'E2D0, 0x10DF, 0x4334, 0xBE, 0xDD, 0x7A, 0xA2,
		0x0B, 0x22, 0x7A, 0x9D);

// {9E52AB10-F80D-49DF-ACB8-4330F5687855}
DEFINE_KNOWN_FOLDER(FOLDERID_CDBurning, 0x9E52'AB10, 0xF80D, 0x49DF, 0xAC, 0xB8, 0x43, 0x30, 0xF5,
		0x68, 0x78, 0x55);

// {0762D272-C50A-4BB0-A382-697DCD729B80}
DEFINE_KNOWN_FOLDER(FOLDERID_UserProfiles, 0x0762'D272, 0xC50A, 0x4BB0, 0xA3, 0x82, 0x69, 0x7D,
		0xCD, 0x72, 0x9B, 0x80);

// {DE92C1C7-837F-4F69-A3BB-86E631204A23}
DEFINE_KNOWN_FOLDER(FOLDERID_Playlists, 0xDE92'C1C7, 0x837F, 0x4F69, 0xA3, 0xBB, 0x86, 0xE6, 0x31,
		0x20, 0x4A, 0x23);

// {15CA69B3-30EE-49C1-ACE1-6B5EC372AFB5}
DEFINE_KNOWN_FOLDER(FOLDERID_SamplePlaylists, 0x15CA'69B3, 0x30EE, 0x49C1, 0xAC, 0xE1, 0x6B, 0x5E,
		0xC3, 0x72, 0xAF, 0xB5);

// {B250C668-F57D-4EE1-A63C-290EE7D1AA1F}
DEFINE_KNOWN_FOLDER(FOLDERID_SampleMusic, 0xB250'C668, 0xF57D, 0x4EE1, 0xA6, 0x3C, 0x29, 0x0E, 0xE7,
		0xD1, 0xAA, 0x1F);

// {C4900540-2379-4C75-844B-64E6FAF8716B}
DEFINE_KNOWN_FOLDER(FOLDERID_SamplePictures, 0xC490'0540, 0x2379, 0x4C75, 0x84, 0x4B, 0x64, 0xE6,
		0xFA, 0xF8, 0x71, 0x6B);

// {859EAD94-2E85-48AD-A71A-0969CB56A6CD}
DEFINE_KNOWN_FOLDER(FOLDERID_SampleVideos, 0x859E'AD94, 0x2E85, 0x48AD, 0xA7, 0x1A, 0x09, 0x69,
		0xCB, 0x56, 0xA6, 0xCD);

// {69D2CF90-FC33-4FB7-9A0C-EBB0F0FCB43C}
DEFINE_KNOWN_FOLDER(FOLDERID_PhotoAlbums, 0x69D2'CF90, 0xFC33, 0x4FB7, 0x9A, 0x0C, 0xEB, 0xB0, 0xF0,
		0xFC, 0xB4, 0x3C);

// {DFDF76A2-C82A-4D63-906A-5644AC457385}
DEFINE_KNOWN_FOLDER(FOLDERID_Public, 0xDFDF'76A2, 0xC82A, 0x4D63, 0x90, 0x6A, 0x56, 0x44, 0xAC,
		0x45, 0x73, 0x85);

// {df7266ac-9274-4867-8d55-3bd661de872d}
DEFINE_KNOWN_FOLDER(FOLDERID_ChangeRemovePrograms, 0xdf72'66ac, 0x9274, 0x4867, 0x8d, 0x55, 0x3b,
		0xd6, 0x61, 0xde, 0x87, 0x2d);

// {a305ce99-f527-492b-8b1a-7e76fa98d6e4}
DEFINE_KNOWN_FOLDER(FOLDERID_AppUpdates, 0xa305'ce99, 0xf527, 0x492b, 0x8b, 0x1a, 0x7e, 0x76, 0xfa,
		0x98, 0xd6, 0xe4);

// {de61d971-5ebc-4f02-a3a9-6c82895e5c04}
DEFINE_KNOWN_FOLDER(FOLDERID_AddNewPrograms, 0xde61'd971, 0x5ebc, 0x4f02, 0xa3, 0xa9, 0x6c, 0x82,
		0x89, 0x5e, 0x5c, 0x04);

// {374DE290-123F-4565-9164-39C4925E467B}
DEFINE_KNOWN_FOLDER(FOLDERID_Downloads, 0x374d'e290, 0x123f, 0x4565, 0x91, 0x64, 0x39, 0xc4, 0x92,
		0x5e, 0x46, 0x7b);

// {3D644C9B-1FB8-4f30-9B45-F670235F79C0}
DEFINE_KNOWN_FOLDER(FOLDERID_PublicDownloads, 0x3d64'4c9b, 0x1fb8, 0x4f30, 0x9b, 0x45, 0xf6, 0x70,
		0x23, 0x5f, 0x79, 0xc0);

// {7d1d3a04-debb-4115-95cf-2f29da2920da}
DEFINE_KNOWN_FOLDER(FOLDERID_SavedSearches, 0x7d1d'3a04, 0xdebb, 0x4115, 0x95, 0xcf, 0x2f, 0x29,
		0xda, 0x29, 0x20, 0xda);

// {52a4f021-7b75-48a9-9f6b-4b87a210bc8f}
DEFINE_KNOWN_FOLDER(FOLDERID_QuickLaunch, 0x52a4'f021, 0x7b75, 0x48a9, 0x9f, 0x6b, 0x4b, 0x87, 0xa2,
		0x10, 0xbc, 0x8f);

// {56784854-C6CB-462b-8169-88E350ACB882}
DEFINE_KNOWN_FOLDER(FOLDERID_Contacts, 0x5678'4854, 0xc6cb, 0x462b, 0x81, 0x69, 0x88, 0xe3, 0x50,
		0xac, 0xb8, 0x82);

// {A75D362E-50FC-4fb7-AC2C-A8BEAA314493}
DEFINE_KNOWN_FOLDER(FOLDERID_SidebarParts, 0xa75d'362e, 0x50fc, 0x4fb7, 0xac, 0x2c, 0xa8, 0xbe,
		0xaa, 0x31, 0x44, 0x93);

// {7B396E54-9EC5-4300-BE0A-2482EBAE1A26}
DEFINE_KNOWN_FOLDER(FOLDERID_SidebarDefaultParts, 0x7b39'6e54, 0x9ec5, 0x4300, 0xbe, 0xa, 0x24,
		0x82, 0xeb, 0xae, 0x1a, 0x26);

// {DEBF2536-E1A8-4c59-B6A2-414586476AEA}
DEFINE_KNOWN_FOLDER(FOLDERID_PublicGameTasks, 0xdebf'2536, 0xe1a8, 0x4c59, 0xb6, 0xa2, 0x41, 0x45,
		0x86, 0x47, 0x6a, 0xea);

// {054FAE61-4DD8-4787-80B6-090220C4B700}
DEFINE_KNOWN_FOLDER(FOLDERID_GameTasks, 0x54f'ae61, 0x4dd8, 0x4787, 0x80, 0xb6, 0x9, 0x2, 0x20,
		0xc4, 0xb7, 0x0);

// {4C5C32FF-BB9D-43b0-B5B4-2D72E54EAAA4}
DEFINE_KNOWN_FOLDER(FOLDERID_SavedGames, 0x4c5c'32ff, 0xbb9d, 0x43b0, 0xb5, 0xb4, 0x2d, 0x72, 0xe5,
		0x4e, 0xaa, 0xa4);

// {CAC52C1A-B53D-4edc-92D7-6B2E8AC19434} - deprecated
DEFINE_KNOWN_FOLDER(FOLDERID_Games, 0xcac5'2c1a, 0xb53d, 0x4edc, 0x92, 0xd7, 0x6b, 0x2e, 0x8a, 0xc1,
		0x94, 0x34);

// {98ec0e18-2098-4d44-8644-66979315a281}
DEFINE_KNOWN_FOLDER(FOLDERID_SEARCH_MAPI, 0x98ec'0e18, 0x2098, 0x4d44, 0x86, 0x44, 0x66, 0x97, 0x93,
		0x15, 0xa2, 0x81);

// {ee32e446-31ca-4aba-814f-a5ebd2fd6d5e}
DEFINE_KNOWN_FOLDER(FOLDERID_SEARCH_CSC, 0xee32'e446, 0x31ca, 0x4aba, 0x81, 0x4f, 0xa5, 0xeb, 0xd2,
		0xfd, 0x6d, 0x5e);

// {bfb9d5e0-c6a9-404c-b2b2-ae6db6af4968}
DEFINE_KNOWN_FOLDER(FOLDERID_Links, 0xbfb9'd5e0, 0xc6a9, 0x404c, 0xb2, 0xb2, 0xae, 0x6d, 0xb6, 0xaf,
		0x49, 0x68);

// {f3ce0f7c-4901-4acc-8648-d5d44b04ef8f}
DEFINE_KNOWN_FOLDER(FOLDERID_UsersFiles, 0xf3ce'0f7c, 0x4901, 0x4acc, 0x86, 0x48, 0xd5, 0xd4, 0x4b,
		0x04, 0xef, 0x8f);

// {A302545D-DEFF-464b-ABE8-61C8648D939B}
DEFINE_KNOWN_FOLDER(FOLDERID_UsersLibraries, 0xa302'545d, 0xdeff, 0x464b, 0xab, 0xe8, 0x61, 0xc8,
		0x64, 0x8d, 0x93, 0x9b);

// {190337d1-b8ca-4121-a639-6d472d16972a}
DEFINE_KNOWN_FOLDER(FOLDERID_SearchHome, 0x1903'37d1, 0xb8ca, 0x4121, 0xa6, 0x39, 0x6d, 0x47, 0x2d,
		0x16, 0x97, 0x2a);

// {2C36C0AA-5812-4b87-BFD0-4CD0DFB19B39}
DEFINE_KNOWN_FOLDER(FOLDERID_OriginalImages, 0x2C36'C0AA, 0x5812, 0x4b87, 0xbf, 0xd0, 0x4c, 0xd0,
		0xdf, 0xb1, 0x9b, 0x39);

// {7b0db17d-9cd2-4a93-9733-46cc89022e7c}
DEFINE_KNOWN_FOLDER(FOLDERID_DocumentsLibrary, 0x7b0d'b17d, 0x9cd2, 0x4a93, 0x97, 0x33, 0x46, 0xcc,
		0x89, 0x02, 0x2e, 0x7c);

// {2112AB0A-C86A-4ffe-A368-0DE96E47012E}
DEFINE_KNOWN_FOLDER(FOLDERID_MusicLibrary, 0x2112'ab0a, 0xc86a, 0x4ffe, 0xa3, 0x68, 0xd, 0xe9, 0x6e,
		0x47, 0x1, 0x2e);

// {A990AE9F-A03B-4e80-94BC-9912D7504104}
DEFINE_KNOWN_FOLDER(FOLDERID_PicturesLibrary, 0xa990'ae9f, 0xa03b, 0x4e80, 0x94, 0xbc, 0x99, 0x12,
		0xd7, 0x50, 0x41, 0x4);

// {491E922F-5643-4af4-A7EB-4E7A138D8174}
DEFINE_KNOWN_FOLDER(FOLDERID_VideosLibrary, 0x491e'922f, 0x5643, 0x4af4, 0xa7, 0xeb, 0x4e, 0x7a,
		0x13, 0x8d, 0x81, 0x74);

// {1A6FDBA2-F42D-4358-A798-B74D745926C5}
DEFINE_KNOWN_FOLDER(FOLDERID_RecordedTVLibrary, 0x1a6f'dba2, 0xf42d, 0x4358, 0xa7, 0x98, 0xb7, 0x4d,
		0x74, 0x59, 0x26, 0xc5);

// {52528A6B-B9E3-4add-B60D-588C2DBA842D}
DEFINE_KNOWN_FOLDER(FOLDERID_HomeGroup, 0x5252'8a6b, 0xb9e3, 0x4add, 0xb6, 0xd, 0x58, 0x8c, 0x2d,
		0xba, 0x84, 0x2d);

// {9B74B6A3-0DFD-4f11-9E78-5F7800F2E772}
DEFINE_KNOWN_FOLDER(FOLDERID_HomeGroupCurrentUser, 0x9b74'b6a3, 0xdfd, 0x4f11, 0x9e, 0x78, 0x5f,
		0x78, 0x0, 0xf2, 0xe7, 0x72);

// {5CE4A5E9-E4EB-479D-B89F-130C02886155}
DEFINE_KNOWN_FOLDER(FOLDERID_DeviceMetadataStore, 0x5ce4'a5e9, 0xe4eb, 0x479d, 0xb8, 0x9f, 0x13,
		0x0c, 0x02, 0x88, 0x61, 0x55);

// {1B3EA5DC-B587-4786-B4EF-BD1DC332AEAE}
DEFINE_KNOWN_FOLDER(FOLDERID_Libraries, 0x1b3e'a5dc, 0xb587, 0x4786, 0xb4, 0xef, 0xbd, 0x1d, 0xc3,
		0x32, 0xae, 0xae);

// {48daf80b-e6cf-4f4e-b800-0e69d84ee384}
DEFINE_KNOWN_FOLDER(FOLDERID_PublicLibraries, 0x48da'f80b, 0xe6cf, 0x4f4e, 0xb8, 0x00, 0x0e, 0x69,
		0xd8, 0x4e, 0xe3, 0x84);

// {9e3995ab-1f9c-4f13-b827-48b24b6c7174}
DEFINE_KNOWN_FOLDER(FOLDERID_UserPinned, 0x9e39'95ab, 0x1f9c, 0x4f13, 0xb8, 0x27, 0x48, 0xb2, 0x4b,
		0x6c, 0x71, 0x74);

// {bcb5256f-79f6-4cee-b725-dc34e402fd46}
DEFINE_KNOWN_FOLDER(FOLDERID_ImplicitAppShortcuts, 0xbcb5'256f, 0x79f6, 0x4cee, 0xb7, 0x25, 0xdc,
		0x34, 0xe4, 0x2, 0xfd, 0x46);

// {008ca0b1-55b4-4c56-b8a8-4de4b299d3be}
DEFINE_KNOWN_FOLDER(FOLDERID_AccountPictures, 0x008c'a0b1, 0x55b4, 0x4c56, 0xb8, 0xa8, 0x4d, 0xe4,
		0xb2, 0x99, 0xd3, 0xbe);

// {0482af6c-08f1-4c34-8c90-e17ec98b1e17}
DEFINE_KNOWN_FOLDER(FOLDERID_PublicUserTiles, 0x0482'af6c, 0x08f1, 0x4c34, 0x8c, 0x90, 0xe1, 0x7e,
		0xc9, 0x8b, 0x1e, 0x17);

// {1e87508d-89c2-42f0-8a7e-645a0f50ca58}
DEFINE_KNOWN_FOLDER(FOLDERID_AppsFolder, 0x1e87'508d, 0x89c2, 0x42f0, 0x8a, 0x7e, 0x64, 0x5a, 0x0f,
		0x50, 0xca, 0x58);

// {F26305EF-6948-40B9-B255-81453D09C785}
DEFINE_KNOWN_FOLDER(FOLDERID_StartMenuAllPrograms, 0xf263'05ef, 0x6948, 0x40b9, 0xb2, 0x55, 0x81,
		0x45, 0x3d, 0x9, 0xc7, 0x85);

// {A440879F-87A0-4F7D-B700-0207B966194A}
DEFINE_KNOWN_FOLDER(FOLDERID_CommonStartMenuPlaces, 0xa440'879f, 0x87a0, 0x4f7d, 0xb7, 0x0, 0x2,
		0x7, 0xb9, 0x66, 0x19, 0x4a);

// {A3918781-E5F2-4890-B3D9-A7E54332328C}
DEFINE_KNOWN_FOLDER(FOLDERID_ApplicationShortcuts, 0xa391'8781, 0xe5f2, 0x4890, 0xb3, 0xd9, 0xa7,
		0xe5, 0x43, 0x32, 0x32, 0x8c);

// {00BCFC5A-ED94-4e48-96A1-3F6217F21990}
DEFINE_KNOWN_FOLDER(FOLDERID_RoamingTiles, 0xbc'fc5a, 0xed94, 0x4e48, 0x96, 0xa1, 0x3f, 0x62, 0x17,
		0xf2, 0x19, 0x90);

// {AAA8D5A5-F1D6-4259-BAA8-78E7EF60835E}
DEFINE_KNOWN_FOLDER(FOLDERID_RoamedTileImages, 0xaaa8'd5a5, 0xf1d6, 0x4259, 0xba, 0xa8, 0x78, 0xe7,
		0xef, 0x60, 0x83, 0x5e);

// {b7bede81-df94-4682-a7d8-57a52620b86f}
DEFINE_KNOWN_FOLDER(FOLDERID_Screenshots, 0xb7be'de81, 0xdf94, 0x4682, 0xa7, 0xd8, 0x57, 0xa5, 0x26,
		0x20, 0xb8, 0x6f);

// {AB5FB87B-7CE2-4F83-915D-550846C9537B}
DEFINE_KNOWN_FOLDER(FOLDERID_CameraRoll, 0xab5f'b87b, 0x7ce2, 0x4f83, 0x91, 0x5d, 0x55, 0x8, 0x46,
		0xc9, 0x53, 0x7b);

// {A52BBA46-E9E1-435f-B3D9-28DAA648C0F6} - deprecated
// Same GUID as FOLDERID_OneDrive
DEFINE_KNOWN_FOLDER(FOLDERID_SkyDrive, 0xa52b'ba46, 0xe9e1, 0x435f, 0xb3, 0xd9, 0x28, 0xda, 0xa6,
		0x48, 0xc0, 0xf6);

// {A52BBA46-E9E1-435f-B3D9-28DAA648C0F6}
// Same GUID as FOLDERID_SkyDrive
DEFINE_KNOWN_FOLDER(FOLDERID_OneDrive, 0xa52b'ba46, 0xe9e1, 0x435f, 0xb3, 0xd9, 0x28, 0xda, 0xa6,
		0x48, 0xc0, 0xf6);

// {24D89E24-2F19-4534-9DDE-6A6671FBB8FE}
DEFINE_KNOWN_FOLDER(FOLDERID_SkyDriveDocuments, 0x24d8'9e24, 0x2f19, 0x4534, 0x9d, 0xde, 0x6a, 0x66,
		0x71, 0xfb, 0xb8, 0xfe);

// {339719B5-8C47-4894-94C2-D8F77ADD44A6}
DEFINE_KNOWN_FOLDER(FOLDERID_SkyDrivePictures, 0x3397'19b5, 0x8c47, 0x4894, 0x94, 0xc2, 0xd8, 0xf7,
		0x7a, 0xdd, 0x44, 0xa6);

// {C3F2459E-80D6-45DC-BFEF-1F769F2BE730}
DEFINE_KNOWN_FOLDER(FOLDERID_SkyDriveMusic, 0xc3f2'459e, 0x80d6, 0x45dc, 0xbf, 0xef, 0x1f, 0x76,
		0x9f, 0x2b, 0xe7, 0x30);

// {767E6811-49CB-4273-87C2-20F355E1085B}
DEFINE_KNOWN_FOLDER(FOLDERID_SkyDriveCameraRoll, 0x767e'6811, 0x49cb, 0x4273, 0x87, 0xc2, 0x20,
		0xf3, 0x55, 0xe1, 0x08, 0x5b);

// {0D4C3DB6-03A3-462F-A0E6-08924C41B5D4}
DEFINE_KNOWN_FOLDER(FOLDERID_SearchHistory, 0x0d4c'3db6, 0x03a3, 0x462f, 0xa0, 0xe6, 0x08, 0x92,
		0x4c, 0x41, 0xb5, 0xd4);

// {7E636BFE-DFA9-4D5E-B456-D7B39851D8A9}
DEFINE_KNOWN_FOLDER(FOLDERID_SearchTemplates, 0x7e63'6bfe, 0xdfa9, 0x4d5e, 0xb4, 0x56, 0xd7, 0xb3,
		0x98, 0x51, 0xd8, 0xa9);

// {2B20DF75-1EDA-4039-8097-38798227D5B7}
DEFINE_KNOWN_FOLDER(FOLDERID_CameraRollLibrary, 0x2b20'df75, 0x1eda, 0x4039, 0x80, 0x97, 0x38, 0x79,
		0x82, 0x27, 0xd5, 0xb7);

// {3B193882-D3AD-4eab-965A-69829D1FB59F}
DEFINE_KNOWN_FOLDER(FOLDERID_SavedPictures, 0x3b19'3882, 0xd3ad, 0x4eab, 0x96, 0x5a, 0x69, 0x82,
		0x9d, 0x1f, 0xb5, 0x9f);

// {E25B5812-BE88-4bd9-94B0-29233477B6C3}
DEFINE_KNOWN_FOLDER(FOLDERID_SavedPicturesLibrary, 0xe25b'5812, 0xbe88, 0x4bd9, 0x94, 0xb0, 0x29,
		0x23, 0x34, 0x77, 0xb6, 0xc3);

// {12D4C69E-24AD-4923-BE19-31321C43A767}
DEFINE_KNOWN_FOLDER(FOLDERID_RetailDemo, 0x12d4'c69e, 0x24ad, 0x4923, 0xbe, 0x19, 0x31, 0x32, 0x1c,
		0x43, 0xa7, 0x67);

// {1C2AC1DC-4358-4B6C-9733-AF21156576F0}
DEFINE_KNOWN_FOLDER(FOLDERID_Device, 0x1C2A'C1DC, 0x4358, 0x4B6C, 0x97, 0x33, 0xAF, 0x21, 0x15,
		0x65, 0x76, 0xF0);

// The file system directory that contains development files that have been copied to this device by a development tool. A
// typical path is C:\Users\username\AppData\Local\DevelopmentFiles. This directory is used by development tools that need
// to copy files to a device. This may include copying application binaries for temporary registration and execution in order
// to allow a developer to test their application without having to go through the full app packaging process. It could also
// include development time only components such as a remote debugger. Recommended practice is to create sub-directories rather
// than copying files to the DevelopmentFiles directory. Development tools should be careful to use a naming convention that
// avoids conflicts. For example application binaries should be copied to a directory with a unique name such as the app package
// full name. This information is per user and will not roam.
// {DBE8E08E-3053-4BBC-B183-2A7B2B191E59}
DEFINE_KNOWN_FOLDER(FOLDERID_DevelopmentFiles, 0xdbe8'e08e, 0x3053, 0x4bbc, 0xb1, 0x83, 0x2a, 0x7b,
		0x2b, 0x19, 0x1e, 0x59);

// {31C0DD25-9439-4F12-BF41-7FF4EDA38722}
DEFINE_KNOWN_FOLDER(FOLDERID_Objects3D, 0x31c0'dd25, 0x9439, 0x4f12, 0xbf, 0x41, 0x7f, 0xf4, 0xed,
		0xa3, 0x87, 0x22);

// {EDC0FE71-98D8-4F4A-B920-C8DC133CB165}
DEFINE_KNOWN_FOLDER(FOLDERID_AppCaptures, 0xedc0'fe71, 0x98d8, 0x4f4a, 0xb9, 0x20, 0xc8, 0xdc, 0x13,
		0x3c, 0xb1, 0x65);

// {f42ee2d3-909f-4907-8871-4c22fc0bf756}
DEFINE_KNOWN_FOLDER(FOLDERID_LocalDocuments, 0xf42e'e2d3, 0x909f, 0x4907, 0x88, 0x71, 0x4c, 0x22,
		0xfc, 0x0b, 0xf7, 0x56);

// {0ddd015d-b06c-45d5-8c4c-f59713854639 }
DEFINE_KNOWN_FOLDER(FOLDERID_LocalPictures, 0x0ddd'015d, 0xb06c, 0x45d5, 0x8c, 0x4c, 0xf5, 0x97,
		0x13, 0x85, 0x46, 0x39);

// {35286a68-3c57-41a1-bbb1-0eae73d76c95}
DEFINE_KNOWN_FOLDER(FOLDERID_LocalVideos, 0x3528'6a68, 0x3c57, 0x41a1, 0xbb, 0xb1, 0x0e, 0xae, 0x73,
		0xd7, 0x6c, 0x95);

// {a0c69a99-21c8-4671-8703-7934162fcf1d}
DEFINE_KNOWN_FOLDER(FOLDERID_LocalMusic, 0xa0c6'9a99, 0x21c8, 0x4671, 0x87, 0x03, 0x79, 0x34, 0x16,
		0x2f, 0xcf, 0x1d);

// {7d83ee9b-2244-4e70-b1f5-5393042af1e4}
DEFINE_KNOWN_FOLDER(FOLDERID_LocalDownloads, 0x7d83'ee9b, 0x2244, 0x4e70, 0xb1, 0xf5, 0x53, 0x93,
		0x04, 0x2a, 0xf1, 0xe4);

// {2f8b40c2-83ed-48ee-b383-a1f157ec6f9a}
DEFINE_KNOWN_FOLDER(FOLDERID_RecordedCalls, 0x2f8b'40c2, 0x83ed, 0x48ee, 0xb3, 0x83, 0xa1, 0xf1,
		0x57, 0xec, 0x6f, 0x9a);

// {7ad67899-66af-43ba-9156-6aad42e6c596}
DEFINE_KNOWN_FOLDER(FOLDERID_AllAppMods, 0x7ad6'7899, 0x66af, 0x43ba, 0x91, 0x56, 0x6a, 0xad, 0x42,
		0xe6, 0xc5, 0x96);

// {3db40b20-2a30-4dbe-917e-771dd21dd099}
DEFINE_KNOWN_FOLDER(FOLDERID_CurrentAppMods, 0x3db4'0b20, 0x2a30, 0x4dbe, 0x91, 0x7e, 0x77, 0x1d,
		0xd2, 0x1d, 0xd0, 0x99);

// {B2C5E279-7ADD-439F-B28C-C41FE1BBF672}
DEFINE_KNOWN_FOLDER(FOLDERID_AppDataDesktop, 0xb2c5'e279, 0x7add, 0x439f, 0xb2, 0x8c, 0xc4, 0x1f,
		0xe1, 0xbb, 0xf6, 0x72);

// {7BE16610-1F7F-44AC-BFF0-83E15F2FFCA1}
DEFINE_KNOWN_FOLDER(FOLDERID_AppDataDocuments, 0x7be1'6610, 0x1f7f, 0x44ac, 0xbf, 0xf0, 0x83, 0xe1,
		0x5f, 0x2f, 0xfc, 0xa1);

// {7CFBEFBC-DE1F-45AA-B843-A542AC536CC9}
DEFINE_KNOWN_FOLDER(FOLDERID_AppDataFavorites, 0x7cfb'efbc, 0xde1f, 0x45aa, 0xb8, 0x43, 0xa5, 0x42,
		0xac, 0x53, 0x6c, 0xc9);

// {559D40A3-A036-40FA-AF61-84CB430A4D34}
DEFINE_KNOWN_FOLDER(FOLDERID_AppDataProgramData, 0x559d'40a3, 0xa036, 0x40fa, 0xaf, 0x61, 0x84,
		0xcb, 0x43, 0xa, 0x4d, 0x34);

// {B3EB08D3-A1F3-496B-865A-42B536CDA0EC}
DEFINE_KNOWN_FOLDER(FOLDERID_LocalStorage, 0xB3EB'08D3, 0xA1F3, 0x496B, 0x86, 0x5A, 0x42, 0xB5,
		0x36, 0xCD, 0xA0, 0xEC);

namespace sprt::platform {

uint32_t getRid(void *sid);

int lastErrorToErrno(unsigned long winerr);

static DWORD s_defaultAppContainerCaps[] = {
	SECURITY_CAPABILITY_INTERNET_CLIENT_SERVER,
	SECURITY_CAPABILITY_PICTURES_LIBRARY,
	SECURITY_CAPABILITY_VIDEOS_LIBRARY,
	SECURITY_CAPABILITY_MUSIC_LIBRARY,
	SECURITY_CAPABILITY_DOCUMENTS_LIBRARY,
	SECURITY_CAPABILITY_SHARED_USER_CERTIFICATES,
	SECURITY_CAPABILITY_REMOVABLE_STORAGE,
};


static const KNOWNFOLDERID *s_knownFoldersToAllow[] = {&FOLDERID_Profile, &FOLDERID_Public};

struct StaticInit {
	StaticInit() {
		initResult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	}

	~StaticInit() {
		if (!SUCCEEDED(initResult)) {
			CoUninitialize();
		}

		if (containerId) {
			FreeSid(containerId);
		}
	}

	HRESULT initResult;
	PSID containerId = nullptr;
};

static StaticInit s_staticInit __attribute__((used));

void destroyAppContainer(StringView appConfigBundleName) {
	char16_t profileName[64] = {0};
	unicode::toUtf16(profileName, 63, appConfigBundleName);

	auto hr = ::DeleteAppContainerProfile((const wchar_t *)profileName);
	if (!SUCCEEDED(hr)) {
		oslog::vpwarn(__SPRT_LOCATION, "rt-libc",
				"Fail to delete app profile: ", appConfigBundleName);
	}
}

static bool allowNamedObjectAccess(PSID appContainerSid, PWSTR name, SE_OBJECT_TYPE type,
		ACCESS_MASK accessMask) {
	PACL oldAcl, newAcl = nullptr;
	DWORD status;
	EXPLICIT_ACCESSW access;
	do {
		access.grfAccessMode = GRANT_ACCESS;
		access.grfAccessPermissions = accessMask;
		access.grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
		access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
		access.Trustee.pMultipleTrustee = nullptr;
		access.Trustee.ptstrName = (PWSTR)appContainerSid;
		access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
		access.Trustee.TrusteeType = TRUSTEE_IS_GROUP;

		status = ::GetNamedSecurityInfoW(name, type, DACL_SECURITY_INFORMATION, nullptr, nullptr,
				&oldAcl, nullptr, nullptr);
		if (status != ERROR_SUCCESS) {
			return false;
		}

		status = ::SetEntriesInAclW(1, &access, oldAcl, &newAcl);
		if (status != ERROR_SUCCESS) {
			return false;
		}

		status = ::SetNamedSecurityInfoW(name, type, DACL_SECURITY_INFORMATION, nullptr, nullptr,
				newAcl, nullptr);
		if (status != ERROR_SUCCESS) {
			break;
		}
	} while (false);

	if (newAcl) {
		::LocalFree(newAcl);
	}

	return status == ERROR_SUCCESS;
}

bool initAppContainer(StringView appConfigBundleName, StringView appConfigName) {
	char16_t profileName[64] = {0};
	char16_t publicName[512] = {0};
	const wchar_t *desc = L"Stappler Application";

	unicode::toUtf16(profileName, 63, appConfigBundleName);
	unicode::toUtf16(publicName, 511, appConfigName);

	HRESULT hr = ::CreateAppContainerProfile((const wchar_t *)profileName,
			(const wchar_t *)publicName, desc, nullptr, 0, &s_staticInit.containerId);
	if (!SUCCEEDED(hr)) {
		if (hr == E_ACCESSDENIED) {
			oslog::vpwarn(__SPRT_LOCATION, "rt-libc",
					"Fail to create temporary profile: E_ACCESSDENIED");
		} else if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
			hr = ::DeriveAppContainerSidFromAppContainerName((const wchar_t *)profileName,
					&s_staticInit.containerId);
			if (!SUCCEEDED(hr)) {
				oslog::vpwarn(__SPRT_LOCATION, "rt-libc",
						"Fail to create temporary profile: ERROR_ALREADY_EXISTS");
			}
			return true;
		} else if (hr == E_INVALIDARG) {
			oslog::vpwarn(__SPRT_LOCATION, "rt-libc",
					"Fail to create temporary profile: E_INVALIDARG");
		}
		return false;
	} else {
		wchar_t *commonDirPath = nullptr;
		for (auto &it : s_knownFoldersToAllow) {
			SHGetKnownFolderPath(*it, 0, nullptr, &commonDirPath);
			if (commonDirPath) {
				allowNamedObjectAccess(s_staticInit.containerId, commonDirPath, SE_FILE_OBJECT,
						FILE_GENERIC_READ);
				CoTaskMemFree(commonDirPath);
				commonDirPath = nullptr;
			}
		}
		return true;
	}
}

bool runSelfInContainer(int &resultCode) {
	// run self in app container
	constexpr auto capsCount = sizeof(s_defaultAppContainerCaps) / sizeof(DWORD);

	SID_AND_ATTRIBUTES capsAttrs[capsCount];
	SID_IDENTIFIER_AUTHORITY authority = SECURITY_APP_PACKAGE_AUTHORITY;

	int i = 0;
	for (auto &it : s_defaultAppContainerCaps) {
		if (!AllocateAndInitializeSid(&authority, SECURITY_BUILTIN_CAPABILITY_RID_COUNT,
					SECURITY_CAPABILITY_BASE_RID, it, 0, 0, 0, 0, 0, 0, &capsAttrs[i].Sid)) {
			oslog::vpwarn(__SPRT_LOCATION, "rt-libc", "Fail to allocate capability SID");
		}
		capsAttrs[i].Attributes = SE_GROUP_ENABLED;
		++i;
	}

	// Run self in container
	SECURITY_CAPABILITIES sc;
	sc.AppContainerSid = s_staticInit.containerId;
	sc.Capabilities = nullptr;
	sc.CapabilityCount = 0;
	sc.Reserved = 0;
	sc.Capabilities = capsAttrs;
	sc.CapabilityCount = capsCount;

	STARTUPINFOEXW si;
	sprt::memset(&si, 0, sizeof(STARTUPINFOEXW));
	si.StartupInfo.cb = sizeof(STARTUPINFOEXW);

	PROCESS_INFORMATION pi;
	sprt::memset(&pi, 0, sizeof(PROCESS_INFORMATION));

	SIZE_T AttributesSize;
	InitializeProcThreadAttributeList(NULL, 1, 0, &AttributesSize);
	si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			AttributesSize);
	InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &AttributesSize);

	if (!::UpdateProcThreadAttribute(si.lpAttributeList, 0,
				PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES, &sc, sizeof(SECURITY_CAPABILITIES),
				nullptr, nullptr)) {
		oslog::vperror(__SPRT_LOCATION, "rt-libc",
				"Fail to update proc attributes for AppContainer: ",
				status::lastErrorToStatus(GetLastError()));
		return false;
	}

	wchar_t fullpath[32'768] = {0};
	fullpath[GetModuleFileNameW(NULL, fullpath, 32'768)] = 0;

	auto commandLine = GetCommandLineW();

	BOOL created = ::CreateProcessW(fullpath, commandLine, nullptr, nullptr, TRUE,
			EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, (LPSTARTUPINFOW)&si, &pi);

	if (created) {
		WaitForSingleObject(pi.hProcess, INFINITE);

		DWORD code = 0;
		GetExitCodeProcess(pi.hProcess, &code);

		resultCode = code;
	} else {
		auto lastError = GetLastError();
		oslog::vperror(__SPRT_LOCATION, "rt-libc",
				"Fail to create AppContainer process: ", status::lastErrorToStatus(lastError));
		resultCode = -1'024;
	}

	DeleteProcThreadAttributeList(si.lpAttributeList);
	for (auto &it : capsAttrs) { FreeSid(it.Sid); }

	return false;
}

bool getAppPath(const callback<void(StringView)> &cb) {
	wchar_t fullpath[32'768] = {0};
	auto n = GetModuleFileNameW(NULL, fullpath, 32'768);

	if (n == 0) {
		return false;
	}

	unicode::toUtf8([&](StringView str) {
		auto len =
				__sprt_fpath_to_posix(str.data(), str.size(), (char *)str.data(), str.size() + 1);
		cb(StringView(str.data(), len));
	}, WideStringView((char16_t *)fullpath, n));
	return true;
}

bool getHomePath(const callback<void(StringView)> &cb) {
	PWSTR pPath = NULL;
	// Use FOLDERID_Documents to get the path to the Documents folder
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, NULL, &pPath);
	if (SUCCEEDED(hr)) {
		unicode::toUtf8([&](StringView str) {
			auto len = __sprt_fpath_to_posix(str.data(), str.size(), (char *)str.data(),
					str.size() + 1);
			cb(StringView(str.data(), len));
		}, WideStringView((char16_t *)pPath));
		CoTaskMemFree(pPath); // Free memory allocated by the function
		return true;
	}
	return false;
}

bool getMachineId(const callback<void(StringView)> &cb) {
	HKEY hKey;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey)
			!= ERROR_SUCCESS) {
		return false;
	}

	DWORD type;
	DWORD dataSize = 0;
	// Call RegGetValue once to get the buffer size
	LONG result =
			RegGetValueW(hKey, nullptr, L"MachineGuid", RRF_RT_REG_SZ, &type, nullptr, &dataSize);

	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Allocate a buffer of the required size
	auto wbuf = __sprt_typed_malloca(wchar_t, dataSize + 1);
	result = RegGetValueW(hKey, nullptr, L"MachineGuid", RRF_RT_REG_SZ, &type, wbuf, &dataSize);

	if (result == ERROR_SUCCESS) {
		unicode::toUtf8(cb, WideStringView((char16_t *)wbuf, dataSize));
	}

	RegCloseKey(hKey);

	__sprt_freea(wbuf);
	return false;
}

} // namespace sprt::platform

#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/filesystem/filepath.h>

namespace sprt::filesystem::detail {

struct KnownFolderInfo {
	const KNOWNFOLDERID *folder;
	LocationCategory category;
	LookupFlags flags;
};

static KnownFolderInfo s_defaultKnownFolders[] = {
	KnownFolderInfo{&FOLDERID_AppDataDesktop, LocationCategory::UserDesktop, LookupFlags::Private},
	KnownFolderInfo{&FOLDERID_Desktop, LocationCategory::UserDesktop, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicDesktop, LocationCategory::UserDesktop, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Pictures, LocationCategory::UserPictures, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicPictures, LocationCategory::UserPictures, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Videos, LocationCategory::UserVideos, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicVideos, LocationCategory::UserVideos, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Music, LocationCategory::UserMusic, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicMusic, LocationCategory::UserMusic, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Downloads, LocationCategory::UserDownload, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicDownloads, LocationCategory::UserDownload, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Documents, LocationCategory::UserDocuments, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicDocuments, LocationCategory::UserDocuments,
		LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Profile, LocationCategory::UserHome, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_Public, LocationCategory::UserHome, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Fonts, LocationCategory::Fonts, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_InternetCache, LocationCategory::CommonData, LookupFlags::Private},
	KnownFolderInfo{&FOLDERID_LocalAppData, LocationCategory::CommonData, LookupFlags::Private},
	KnownFolderInfo{&FOLDERID_RoamingAppData, LocationCategory::CommonData, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_ProgramData, LocationCategory::CommonData, LookupFlags::Shared},
};

StringView _readEnvExt(memory::pool_t *pool, StringView key) {
	if (key == "EXEC_DIR") {
		return filepath::root(platform::getExecPath()).pdup(pool);
	} else if (key == "CWD") {
		StringView ret;
		getCurrentDir([&](StringView path) { ret = path.pdup(pool); });
		return ret;
	} else {
		auto env = __sprt_getenv_impl(key.data());
		if (env) {
			return StringView(env).pdup(pool);
		}
	}
	return StringView();
}

static void processKnownDirPath(LookupData &data, const KnownFolderInfo &info,
		wchar_t *pathAppWide) {
	unicode::toUtf8([&](StringView uPath) {
		auto len = __sprt_fpath_to_posix(uPath.data(), uPath.size(), (char *)uPath.data(),
				uPath.size() + 1);

		auto &res = data._resourceLocations[toInt(info.category)];

		res.paths.emplace_back(LocationInfo{
			.path = StringView(uPath.data(), len).pdup(data._pool),
			.lookupType = info.flags,
			.locationFlags = LocationFlags::Locateable,
			.interface = getDefaultInterface(),
		});
		res.init = true;
	}, WideStringView((const char16_t *)pathAppWide));
}

static void processKnownDir(LookupData &data, const KnownFolderInfo &info, IKnownFolder *dir) {
	KNOWNFOLDER_DEFINITION def;
	dir->GetFolderDefinition(&def);

	wchar_t *pathAppWide = nullptr;

	auto dirFlagsAppWide = KF_FLAG_DONT_UNEXPAND | KF_FLAG_NO_ALIAS
			| KF_FLAG_RETURN_FILTER_REDIRECTION_TARGET | KF_FLAG_CREATE;

	dir->GetPath(dirFlagsAppWide, &pathAppWide);

	if (pathAppWide) {
		processKnownDirPath(data, info, pathAppWide);
		CoTaskMemFree(pathAppWide);
	}
}

static void defineAppPathFromCommon(LookupData &data, StringView bundleName) {
	// init with CommonData and CommonCache paths
	auto makeLocation = [&](LocationCategory cat, StringView root, StringView subname) {
		auto &res = data._resourceLocations[toInt(cat)];
		filepath::merge([&](StringView path) {
			res.paths.emplace_back(LocationInfo{
				.path = path.pdup(data._pool),
				.lookupType = LookupFlags::Private | LookupFlags::Writable,
				.locationFlags = LocationFlags::Locateable,
				.interface = getDefaultInterface(),
			});
		}, root, bundleName, subname);
	};

	auto commonData = &data._resourceLocations[toInt(LocationCategory::CommonData)];
	auto commonCache = &data._resourceLocations[toInt(LocationCategory::CommonCache)];

	if (commonCache->paths.empty()) {
		commonCache = commonData;
	}

	makeLocation(LocationCategory::AppData, commonData->paths.front().path, "Data");
	makeLocation(LocationCategory::AppConfig, commonData->paths.front().path, "Config");
	makeLocation(LocationCategory::AppState, commonData->paths.front().path, "State");
	makeLocation(LocationCategory::AppCache, commonCache->paths.front().path, "Cache");
	makeLocation(LocationCategory::AppRuntime, commonCache->paths.front().path, "Runtime");
}

static __malloc_string getAppContainerPath(PSID sid) {
	__malloc_string ret;
	PWSTR str = nullptr, path = nullptr;
	::ConvertSidToStringSidW(sid, &str);

	if (SUCCEEDED(::GetAppContainerFolderPath(str, &path))) {
		unicode::toUtf8([&](StringView str) {
			ret.resize(str.size() + 1);
			ret.resize(__sprt_fpath_to_posix(str.data(), str.size(), ret.data(), ret.size()));
		}, WideStringView((const char16_t *)path));
		::CoTaskMemFree(path);
	}
	::LocalFree(str);
	return ret;
}

static __malloc_string s_containerPath;

void _initSystemPaths(LookupData &data) {
	auto exeecPath = platform::getExecPath();
	auto defaultInterface = getDefaultInterface();

	CLSID _CLSID_KnownFolderManager;
	CLSIDFromString(L"4df0c730-df9d-4ae3-9153-aa6b82e9795a", &_CLSID_KnownFolderManager);

	IID _IID_IKnownFolderManager;
	IIDFromString(L"8BE2D872-86AA-4d47-B776-32CCA40C7018", &_IID_IKnownFolderManager);

	IKnownFolderManager *manager = nullptr;
	auto hr = CoCreateInstance(_CLSID_KnownFolderManager, 0, CLSCTX_INPROC_SERVER,
			_IID_IKnownFolderManager, (LPVOID *)&manager);
	if (SUCCEEDED(hr)) {
		IKnownFolder *pKnownFolder = nullptr;
		for (auto &it : s_defaultKnownFolders) {
			hr = manager->GetFolder(*it.folder, &pKnownFolder);
			if (SUCCEEDED(hr)) {
				processKnownDir(data, it, pKnownFolder);
				pKnownFolder->Release();
			}
		}
		manager->Release();
	} else {
		auto notMs = GetWinApiProvider() != WinApiProviderMicrosoft;
		auto dirFlagsAppWide = KF_FLAG_DONT_UNEXPAND | KF_FLAG_NO_ALIAS
				| KF_FLAG_RETURN_FILTER_REDIRECTION_TARGET | KF_FLAG_CREATE;
		auto dirFlagsLocal = KF_FLAG_DONT_UNEXPAND | KF_FLAG_NO_ALIAS | KF_FLAG_CREATE;

		// Wine will not support KF_FLAG_RETURN_FILTER_REDIRECTION_TARGET
		if (notMs) {
			dirFlagsAppWide = dirFlagsLocal;
		}

		wchar_t *commonDirPath = nullptr;
		for (auto &it : s_defaultKnownFolders) {
			hr = SHGetKnownFolderPath(*it.folder, dirFlagsAppWide, nullptr, &commonDirPath);
			if (SUCCEEDED(hr)) {
				processKnownDirPath(data, it, commonDirPath);
				CoTaskMemFree(commonDirPath); // Free memory allocated by the function
			} else if (!notMs) {
				hr = SHGetKnownFolderPath(*it.folder, dirFlagsLocal, nullptr, &commonDirPath);
				if (SUCCEEDED(hr)) {
					processKnownDirPath(data, it, commonDirPath);
					CoTaskMemFree(commonDirPath); // Free memory allocated by the function
				}
			}
		}
	}

	auto &appConfig = getAppConfig();

	if (!appConfig.bundlePath.empty()) {
		auto &bundledLoc = data._resourceLocations[toInt(LocationCategory::Bundled)];

		appConfig.bundlePath.split<StringView::Chars<':'>>([&](StringView str) {
			auto value = readVariable(data._pool, str);
			if (!value.empty()) {
				bundledLoc.paths.emplace_back(LocationInfo{
					value,
					LookupFlags::Private,
					LocationFlags::Locateable,
					defaultInterface,
				});
			}
		});
	}

	auto pathEnv = __sprt_getenv("PATH");
	if (pathEnv) {
		auto &res = data._resourceLocations[toInt(LocationCategory::Exec)];
		StringView(pathEnv).split<StringView::Chars<':'>>([&](StringView value) {
			res.paths.emplace_back(LocationInfo{
				value.pdup(data._pool),
				LookupFlags::Shared,
				LocationFlags::Locateable,
				defaultInterface,
			});
		});
		res.init = true;
	}

	if (appConfig.pathScheme == AppLocationScheme::ExecutableRelative) {
		auto rootPath = filepath::root(exeecPath);
		data.initAppPaths(rootPath);
	} else if (appConfig.pathScheme == AppLocationScheme::SystemRelative) {
		defineAppPathFromCommon(data, appConfig.bundleName);
	} else if (appConfig.pathScheme == AppLocationScheme::ContainerRelative) {
		// first, determine app container path, then - set paths within it

		s_containerPath = getAppContainerPath(sprt::platform::s_staticInit.containerId);

		if (!s_containerPath.empty()) {
			data.initAppPaths(s_containerPath);
		} else {
			defineAppPathFromCommon(data, appConfig.bundleName);
		}
	} else if (appConfig.pathScheme == AppLocationScheme::ForceContainer) {
		// CommonData and CommonCache already within container
		DWORD returnLength = 0;
		TOKEN_APPCONTAINER_INFORMATION info = {nullptr};
		::GetTokenInformation(::GetCurrentThreadEffectiveToken(), TokenAppContainerSid, &info,
				sizeof(TOKEN_APPCONTAINER_INFORMATION), &returnLength);
		if (info.TokenAppContainer) {
			s_containerPath = getAppContainerPath(info.TokenAppContainer);
		} else {
			s_containerPath = getAppContainerPath(sprt::platform::s_staticInit.containerId);
		}

		if (!s_containerPath.empty()) {
			data.initAppPaths(s_containerPath);
		} else {
			defineAppPathFromCommon(data, appConfig.bundleName);
		}
	}
}

void _termSystemPaths(LookupData &data) { }

} // namespace sprt::filesystem::detail
