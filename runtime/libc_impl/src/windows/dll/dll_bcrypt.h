#include "dlltable.h"

namespace sprt {

struct BCryptTable : DllTable {
	BCryptTable() : DllTable(L"bcrypt.dll", &__bcrypt_begin, &__bcrypt_end, __preloads) { }

	DllTableRecord __bcrypt_begin;
	SPWIN_DEFINE_PROTO(BCryptAddContextFunction)
	SPWIN_DEFINE_PROTO(BCryptAddContextFunctionProvider)
	SPWIN_DEFINE_PROTO(BCryptCloseAlgorithmProvider)
	SPWIN_DEFINE_PROTO(BCryptConfigureContext)
	SPWIN_DEFINE_PROTO(BCryptConfigureContextFunction)
	SPWIN_DEFINE_PROTO(BCryptCreateContext)
	SPWIN_DEFINE_PROTO(BCryptCreateHash)
	SPWIN_DEFINE_PROTO(BCryptDecrypt)
	SPWIN_DEFINE_PROTO(BCryptDeleteContext)
	SPWIN_DEFINE_PROTO(BCryptDeriveKey)
	SPWIN_DEFINE_PROTO(BCryptDeriveKeyCapi)
	SPWIN_DEFINE_PROTO(BCryptDeriveKeyPBKDF2)
	SPWIN_DEFINE_PROTO(BCryptDestroyHash)
	SPWIN_DEFINE_PROTO(BCryptDestroyKey)
	SPWIN_DEFINE_PROTO(BCryptDestroySecret)
	SPWIN_DEFINE_PROTO(BCryptDuplicateHash)
	SPWIN_DEFINE_PROTO(BCryptDuplicateKey)
	SPWIN_DEFINE_PROTO(BCryptEncrypt)
	SPWIN_DEFINE_PROTO(BCryptEnumAlgorithms)
	SPWIN_DEFINE_PROTO(BCryptEnumContextFunctionProviders)
	SPWIN_DEFINE_PROTO(BCryptEnumContextFunctions)
	SPWIN_DEFINE_PROTO(BCryptEnumContexts)
	SPWIN_DEFINE_PROTO(BCryptEnumProviders)
	SPWIN_DEFINE_PROTO(BCryptEnumRegisteredProviders)
	SPWIN_DEFINE_PROTO(BCryptExportKey)
	SPWIN_DEFINE_PROTO(BCryptFinalizeKeyPair)
	SPWIN_DEFINE_PROTO(BCryptFinishHash)
	SPWIN_DEFINE_PROTO(BCryptFreeBuffer)
	SPWIN_DEFINE_PROTO(BCryptGenRandom)
	SPWIN_DEFINE_PROTO(BCryptGenerateKeyPair)
	SPWIN_DEFINE_PROTO(BCryptGenerateSymmetricKey)
	SPWIN_DEFINE_PROTO(BCryptGetFipsAlgorithmMode)
	SPWIN_DEFINE_PROTO(BCryptGetProperty)
	SPWIN_DEFINE_PROTO(BCryptHash)
	SPWIN_DEFINE_PROTO(BCryptHashData)
	SPWIN_DEFINE_PROTO(BCryptImportKey)
	SPWIN_DEFINE_PROTO(BCryptImportKeyPair)
	SPWIN_DEFINE_PROTO(BCryptKeyDerivation)
	SPWIN_DEFINE_PROTO(BCryptOpenAlgorithmProvider)
	SPWIN_DEFINE_PROTO(BCryptQueryContextConfiguration)
	SPWIN_DEFINE_PROTO(BCryptQueryContextFunctionConfiguration)
	SPWIN_DEFINE_PROTO(BCryptQueryContextFunctionProperty)
	SPWIN_DEFINE_PROTO(BCryptQueryProviderRegistration)
	SPWIN_DEFINE_PROTO(BCryptRegisterConfigChangeNotify)
	SPWIN_DEFINE_PROTO(BCryptRegisterProvider)
	SPWIN_DEFINE_PROTO(BCryptRemoveContextFunction)
	SPWIN_DEFINE_PROTO(BCryptRemoveContextFunctionProvider)
	SPWIN_DEFINE_PROTO(BCryptResolveProviders)
	SPWIN_DEFINE_PROTO(BCryptSecretAgreement)
	SPWIN_DEFINE_PROTO(BCryptSetAuditingInterface)
	SPWIN_DEFINE_PROTO(BCryptSetContextFunctionProperty)
	SPWIN_DEFINE_PROTO(BCryptSetProperty)
	SPWIN_DEFINE_PROTO(BCryptSignHash)
	SPWIN_DEFINE_PROTO(BCryptUnregisterConfigChangeNotify)
	SPWIN_DEFINE_PROTO(BCryptUnregisterProvider)
	SPWIN_DEFINE_PROTO(BCryptVerifySignature)
	SPWIN_DEFINE_PROTO(GetAsymmetricEncryptionInterface)
	SPWIN_DEFINE_PROTO(GetCipherInterface)
	SPWIN_DEFINE_PROTO(GetHashInterface)
	SPWIN_DEFINE_PROTO(GetRngInterface)
	SPWIN_DEFINE_PROTO(GetSecretAgreementInterface)
	SPWIN_DEFINE_PROTO(GetSignatureInterface)
	DllTableRecord __bcrypt_end;

	DllTableRecord *__preloads[2] = {
		&BCryptGenRandom,
		nullptr,
	};
};

} // namespace sprt
