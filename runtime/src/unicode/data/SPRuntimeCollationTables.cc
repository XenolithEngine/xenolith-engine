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

// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1999-2016, International Business Machines Corporation and others.
// All Rights Reserved.
//
// GENERATED FILE - do not edit.  Produced by
// runtime/src/unicode/data/gen-collation-tables.py from the icu4c checkout in
// runtime/toolchains/src/icu4c.  See data/README.adoc to regenerate.

// The CollationData aggregates over the arrays in the *Data*.cc files, and
// the table that maps a language tag to one of them.
//
// Which groups are compiled in is a build option (SPRT_COLLATION); a group
// that is switched off takes its locales out of the table with it, and
// hasCollation() then answers false for them.

///@ SP_EXCLUDE

#pragma once

namespace sprt::unicode::detail {

static constexpr CollationData s_collRootData = {
	{s_collRootTrieIndex, s_collRootTrieData, s_collRootTrieIndexLength, s_collRootTrieDataLength,
		s_collRootTrieHighStart, s_collRootTrieHighValueIndex},
	s_collRootCe32s, 6153,
	s_collRootCes, 2041,
	s_collRootContexts, 4619,
	nullptr,
	s_collRootCe32s + s_collRootJamoCe32sStart,
	s_collRootNumericPrimary,
	s_collRootCompressible,
	s_collRootUnsafeBwd, s_collRootUnsafeBwdLength,
	s_collRootFastLatin, 480,
	s_collRootFastLatinPrimaries,
	s_collRootNumScripts, s_collRootScripts + 1, s_collRootScripts + s_collRootScriptStartsOffset,
		s_collRootScriptStartsLength,
};

} // namespace sprt::unicode::detail

// Latin, Nordic and Germanic: 14 locales, 152208 bytes of built tables.
#if SPRT_COLLATION_LATINNORDIC
#include "SPRuntimeCollationDataLatinNordic.cc"
#endif

// Latin, Slavic and Baltic: 9 locales, 67924 bytes of built tables.
#if SPRT_COLLATION_LATINSLAVIC
#include "SPRuntimeCollationDataLatinSlavic.cc"
#endif

// Latin, Romance and the rest of the EU: 10 locales, 80264 bytes of built tables.
#if SPRT_COLLATION_LATINROMANCE
#include "SPRuntimeCollationDataLatinRomance.cc"
#endif

// Latin, Turkic languages and Vietnamese: 5 locales, 53406 bytes of built tables.
#if SPRT_COLLATION_LATINTURKIC
#include "SPRuntimeCollationDataLatinTurkic.cc"
#endif

// Latin, Africa, the Pacific and South-East Asia: 17 locales, 145142 bytes of built tables.
#if SPRT_COLLATION_LATINOTHER
#include "SPRuntimeCollationDataLatinOther.cc"
#endif

// Cyrillic: 10 locales, 41904 bytes of built tables.
#if SPRT_COLLATION_CYRILLIC
#include "SPRuntimeCollationDataCyrillic.cc"
#endif

// Greek, Armenian and Georgian: 3 locales, 7228 bytes of built tables.
#if SPRT_COLLATION_GREEK
#include "SPRuntimeCollationDataGreek.cc"
#endif

// Hebrew, Arabic and Persian: 9 locales, 59468 bytes of built tables.
#if SPRT_COLLATION_SEMITIC
#include "SPRuntimeCollationDataSemitic.cc"
#endif

// The Indic scripts: 14 locales, 87338 bytes of built tables.
#if SPRT_COLLATION_INDIC
#include "SPRuntimeCollationDataIndic.cc"
#endif

// South-East Asia and Tibetan: 5 locales, 53598 bytes of built tables.
#if SPRT_COLLATION_SOUTHEASTASIA
#include "SPRuntimeCollationDataSouthEastAsia.cc"
#endif

// Everything else: 3 locales, 16288 bytes of built tables.
#if SPRT_COLLATION_OTHER
#include "SPRuntimeCollationDataOther.cc"
#endif

// Chinese, Japanese and Korean: 3 locales, 465460 bytes of built tables.
#if SPRT_COLLATION_CJK
#include "SPRuntimeCollationDataCjk.cc"
#endif

namespace sprt::unicode::detail {

// Language tags, sorted, for a binary search. The tag is ICU's spelling:
// a language subtag, then an optional script or region, joined by '_'.
static constexpr CollationLocale s_collationLocales[] = {
#if SPRT_COLLATION_LATINNORDIC
	{"af", 2, &s_coll_afTailoring},
#endif
#if SPRT_COLLATION_OTHER
	{"am", 2, &s_coll_amTailoring},
#endif
#if SPRT_COLLATION_SEMITIC
	{"ar", 2, &s_coll_arTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"as", 2, &s_coll_asTailoring},
#endif
#if SPRT_COLLATION_LATINTURKIC
	{"az", 2, &s_coll_azTailoring},
#endif
#if SPRT_COLLATION_CYRILLIC
	{"be", 2, &s_coll_beTailoring},
#endif
#if SPRT_COLLATION_CYRILLIC
	{"bg", 2, &s_coll_bgTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"blo", 3, &s_coll_bloTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"bn", 2, &s_coll_bnTailoring},
#endif
#if SPRT_COLLATION_SOUTHEASTASIA
	{"bo", 2, &s_coll_boTailoring},
#endif
#if SPRT_COLLATION_LATINROMANCE
	{"br", 2, &s_coll_brTailoring},
#endif
#if SPRT_COLLATION_LATINSLAVIC
	{"bs", 2, &s_coll_bsTailoring},
#endif
#if SPRT_COLLATION_CYRILLIC
	{"bs_Cyrl", 7, &s_coll_bs_CyrlTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"ceb", 3, &s_coll_cebTailoring},
#endif
#if SPRT_COLLATION_OTHER
	{"chr", 3, &s_coll_chrTailoring},
#endif
#if SPRT_COLLATION_LATINSLAVIC
	{"cs", 2, &s_coll_csTailoring},
#endif
#if SPRT_COLLATION_LATINROMANCE
	{"cy", 2, &s_coll_cyTailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"da", 2, &s_coll_daTailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"dsb", 3, &s_coll_dsbTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"ee", 2, &s_coll_eeTailoring},
#endif
#if SPRT_COLLATION_GREEK
	{"el", 2, &s_coll_elTailoring},
#endif
#if SPRT_COLLATION_OTHER
	{"en_US_POSIX", 11, &s_coll_en_US_POSIXTailoring},
#endif
#if SPRT_COLLATION_LATINROMANCE
	{"eo", 2, &s_coll_eoTailoring},
#endif
#if SPRT_COLLATION_LATINROMANCE
	{"es", 2, &s_coll_esTailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"et", 2, &s_coll_etTailoring},
#endif
#if SPRT_COLLATION_SEMITIC
	{"fa", 2, &s_coll_faTailoring},
#endif
#if SPRT_COLLATION_SEMITIC
	{"fa_AF", 5, &s_coll_fa_AFTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"ff_Adlm", 7, &s_coll_ff_AdlmTailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"fi", 2, &s_coll_fiTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"fil", 3, &s_coll_cebTailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"fo", 2, &s_coll_foTailoring},
#endif
#if SPRT_COLLATION_LATINROMANCE
	{"fr_CA", 5, &s_coll_fr_CATailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"fy", 2, &s_coll_fyTailoring},
#endif
#if SPRT_COLLATION_LATINROMANCE
	{"gl", 2, &s_coll_esTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"gu", 2, &s_coll_guTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"ha", 2, &s_coll_haTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"haw", 3, &s_coll_hawTailoring},
#endif
#if SPRT_COLLATION_SEMITIC
	{"he", 2, &s_coll_heTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"hi", 2, &s_coll_hiTailoring},
#endif
#if SPRT_COLLATION_LATINSLAVIC
	{"hr", 2, &s_coll_bsTailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"hsb", 3, &s_coll_hsbTailoring},
#endif
#if SPRT_COLLATION_LATINROMANCE
	{"hu", 2, &s_coll_huTailoring},
#endif
#if SPRT_COLLATION_GREEK
	{"hy", 2, &s_coll_hyTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"ig", 2, &s_coll_igTailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"is", 2, &s_coll_isTailoring},
#endif
#if SPRT_COLLATION_CJK
	{"ja", 2, &s_coll_jaTailoring},
#endif
#if SPRT_COLLATION_GREEK
	{"ka", 2, &s_coll_kaTailoring},
#endif
#if SPRT_COLLATION_CYRILLIC
	{"kk", 2, &s_coll_kkTailoring},
#endif
#if SPRT_COLLATION_SEMITIC
	{"kk_Arab", 7, &s_coll_kk_ArabTailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"kl", 2, &s_coll_klTailoring},
#endif
#if SPRT_COLLATION_SOUTHEASTASIA
	{"km", 2, &s_coll_kmTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"kn", 2, &s_coll_knTailoring},
#endif
#if SPRT_COLLATION_CJK
	{"ko", 2, &s_coll_koTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"kok", 3, &s_coll_kokTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"ku", 2, &s_coll_kuTailoring},
#endif
#if SPRT_COLLATION_CYRILLIC
	{"ky", 2, &s_coll_kyTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"lkt", 3, &s_coll_lktTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"ln", 2, &s_coll_lnTailoring},
#endif
#if SPRT_COLLATION_SOUTHEASTASIA
	{"lo", 2, &s_coll_loTailoring},
#endif
#if SPRT_COLLATION_LATINSLAVIC
	{"lt", 2, &s_coll_ltTailoring},
#endif
#if SPRT_COLLATION_LATINSLAVIC
	{"lv", 2, &s_coll_lvTailoring},
#endif
#if SPRT_COLLATION_CYRILLIC
	{"mk", 2, &s_coll_mkTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"ml", 2, &s_coll_mlTailoring},
#endif
#if SPRT_COLLATION_CYRILLIC
	{"mn", 2, &s_coll_mnTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"mr", 2, &s_coll_mrTailoring},
#endif
#if SPRT_COLLATION_LATINROMANCE
	{"mt", 2, &s_coll_mtTailoring},
#endif
#if SPRT_COLLATION_SOUTHEASTASIA
	{"my", 2, &s_coll_myTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"ne", 2, &s_coll_neTailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"no", 2, &s_coll_noTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"nso", 3, &s_coll_nsoTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"om", 2, &s_coll_omTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"or", 2, &s_coll_orTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"pa", 2, &s_coll_paTailoring},
#endif
#if SPRT_COLLATION_LATINSLAVIC
	{"pl", 2, &s_coll_plTailoring},
#endif
#if SPRT_COLLATION_SEMITIC
	{"ps", 2, &s_coll_fa_AFTailoring},
#endif
#if SPRT_COLLATION_LATINROMANCE
	{"ro", 2, &s_coll_roTailoring},
#endif
#if SPRT_COLLATION_CYRILLIC
	{"ru", 2, &s_coll_bgTailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"se", 2, &s_coll_seTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"si", 2, &s_coll_siTailoring},
#endif
#if SPRT_COLLATION_LATINSLAVIC
	{"sk", 2, &s_coll_skTailoring},
#endif
#if SPRT_COLLATION_LATINSLAVIC
	{"sl", 2, &s_coll_slTailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"smn", 3, &s_coll_smnTailoring},
#endif
#if SPRT_COLLATION_LATINROMANCE
	{"sq", 2, &s_coll_sqTailoring},
#endif
#if SPRT_COLLATION_CYRILLIC
	{"sr", 2, &s_coll_bs_CyrlTailoring},
#endif
#if SPRT_COLLATION_LATINSLAVIC
	{"sr_Latn", 7, &s_coll_bsTailoring},
#endif
#if SPRT_COLLATION_LATINNORDIC
	{"sv", 2, &s_coll_svTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"ta", 2, &s_coll_taTailoring},
#endif
#if SPRT_COLLATION_INDIC
	{"te", 2, &s_coll_teTailoring},
#endif
#if SPRT_COLLATION_SOUTHEASTASIA
	{"th", 2, &s_coll_thTailoring},
#endif
#if SPRT_COLLATION_LATINTURKIC
	{"tk", 2, &s_coll_tkTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"tn", 2, &s_coll_nsoTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"to", 2, &s_coll_toTailoring},
#endif
#if SPRT_COLLATION_LATINTURKIC
	{"tr", 2, &s_coll_trTailoring},
#endif
#if SPRT_COLLATION_SEMITIC
	{"ug", 2, &s_coll_ugTailoring},
#endif
#if SPRT_COLLATION_CYRILLIC
	{"uk", 2, &s_coll_ukTailoring},
#endif
#if SPRT_COLLATION_SEMITIC
	{"ur", 2, &s_coll_urTailoring},
#endif
#if SPRT_COLLATION_LATINTURKIC
	{"uz", 2, &s_coll_uzTailoring},
#endif
#if SPRT_COLLATION_LATINTURKIC
	{"vi", 2, &s_coll_viTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"wo", 2, &s_coll_woTailoring},
#endif
#if SPRT_COLLATION_SEMITIC
	{"yi", 2, &s_coll_yiTailoring},
#endif
#if SPRT_COLLATION_LATINOTHER
	{"yo", 2, &s_coll_yoTailoring},
#endif
#if SPRT_COLLATION_CJK
	{"zh", 2, &s_coll_zhTailoring},
#endif
};

} // namespace sprt::unicode::detail
