/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "SPLocaleInfo.h"
#include "SPBytesReader.h"
#include "SPSpanView.h"
#include "SPString.h"
#include "SPLog.h"

namespace STAPPLER_VERSIONIZED stappler {

// clang-format off

static constexpr std::array<StringView, 256> s_languagesArray({
	StringView(),
	StringView("th:Thai|ไทย|th|th|tha|tha|TH"),
	StringView("ny:Chichewa|chiCheŵa|ny|ny|nya|nya|MW"),
	StringView("sn:Shona|chiShona|sn|sn|sna|sna|ZW"),
	StringView("ti:Tigrinya|ትግርኛ|ti|ti|tir|tir|ER"),
	StringView(),
	StringView(),
	StringView("el:Greek (modern)|ελληνικά|el|el|ell|ell|GR;CY"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("be:Belarusian|беларуская мова|be|be|bel|bel|BY"),
	StringView("byn:Bilen|ብሊና|byn||byn||ER"),
	StringView(),
	StringView("ko:Korean|한국어|ko|ko|kor|kor|KP;KR"),
	StringView(),
	StringView("ssy:Saho|Saho|ssy||ssy||ER"),
	StringView(),
	StringView(),
	StringView("de:German|Deutsch|de|de|deu|deu|BE;DE;LI;LU;AT;CH;VA"),
	StringView("ru:Russian|Русский|ru|ru|rus|rus|AQ;BY;KZ;KG;RU;TJ;TM;UZ"),
	StringView(),
	StringView(),
	StringView("ht:Haitian|Kreyòl ayisyen|ht|ht|hat|hat|HT"),
	StringView("kl:Greenlandic|kalaallisut|kl|kl|kal|kal|GL"),
	StringView("kg:Kongo|Kikongo|kg|kg|kon|kon|CD"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("bn:Bengali|বাংলা|bn|bn|ben|ben|BD"),
	StringView("to:Tonga (Tonga Islands)|faka Tonga|to|to|ton|ton|TO"),
	StringView(),
	StringView("lb:Luxembourgish|Lëtzebuergesch|lb|lb|ltz|ltz|LU"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("fr:French|français|fr|fr|fra|fra|GQ;BE;BJ;BF;BI;CD;DJ;CI;FR;GF;PF;TF;MC;GA;GP;GG;GN;HT;JE;CM;CA;KM;LB;LU;MG;ML;MQ;YT;NC;NE;CG;RE;RW;MF;BL;CH;SN;SC;PM;TG;TD;VU;VA;WF;CF"),
	StringView("fa:Persian (Farsi)|فارسی|fa|fa|fas|fas|IR"),
	StringView("km:Khmer|ខ្មែរ|km|km|khm|khm|KH"),
	StringView("tr:Turkish|Türkçe|tr|tr|tur|tur|TR;CY"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("rm:Romansh|Rumantsch|rm|rm|roh|roh|CH"),
	StringView(),
	StringView("pl:Polish|polski|pl|pl|pol|pol|PL"),
	StringView(),
	StringView("pa:(Eastern) Punjabi|ਪੰਜਾਬੀ|pa|pa|pan|pan|AW;CW"),
	StringView("sk:Slovak|slovenčina|sk|sk|slk|slk|SK;CZ"),
	StringView("sr:Serbian|српски језик|sr|sr|srp|srp|BA;XK;ME;RS"),
	StringView(),
	StringView(),
	StringView("cs:Czech|čeština|cs|cs|ces|ces|CZ"),
	StringView("ay:Aymara|aymar aru|ay|ay|aym|aym|BO"),
	StringView("ja:Japanese|日本語 (にほんご)|ja|ja|jpn|jpn|JP"),
	StringView("ms:Malay|bahasa Melayu|ms|ms|msa|msa|BN;SG"),
	StringView("nn:Norwegian Nynorsk|Norsk nynorsk|nn|nn|nno|nno|BV;NO"),
	StringView("rtm:Rotuman|Fäeag Rotuma|rtm||rtm||FJ"),
	StringView(),
	StringView(),
	StringView("sw:Swahili|Kiswahili|sw|sw|swa|swa|CD;KE;TZ;UG"),
	StringView("fj:Fijian|vosa Vakaviti|fj|fj|fij|fij|FJ"),
	StringView(),
	StringView("nr:Southern Ndebele|isiNdebele|nr|nr|nbl|nbl|ZA"),
	StringView(),
	StringView("kk:Kazakh|қазақ тілі|kk|kk|kaz|kaz|KZ"),
	StringView(),
	StringView(),
	StringView("es:Spanish|Español|es|es|spa|spa|GQ;AR;BZ;BO;CL;CR;DO;EC;SV;GU;GT;HN;CO;CU;MX;NI;PA;PY;PE;PR;ES;UY;VE;EH"),
	StringView(),
	StringView(),
	StringView(),
	StringView("bs:Bosnian|bosanski jezik|bs|bs|bos|bos|BA;ME"),
	StringView(),
	StringView(),
	StringView("sv:Swedish|svenska|sv|sv|swe|swe|AX;FI;SE"),
	StringView(),
	StringView(),
	StringView("nd:Northern Ndebele|isiNdebele|nd|nd|nde|nde|ZW"),
	StringView(),
	StringView(),
	StringView("hi:Hindi|हिन्दी|hi|hi|hin|hin|IN"),
	StringView("da:Danish|dansk|da|da|dan|dan|DK"),
	StringView("so:Somali|Soomaaliga|so|so|som|som|SO"),
	StringView("sq:Albanian|Shqip|sq|sq|sqi|sqi|AL;XK;ME"),
	StringView("rw:Kinyarwanda|Ikinyarwanda|rw|rw|kin|kin|RW"),
	StringView("kun:Kunama|Kunama|kun||kun||ER"),
	StringView("az:Azerbaijani|azərbaycan dili|az|az|aze|aze|AZ"),
	StringView("ss:Swati|SiSwati|ss|ss|ssw|ssw|SZ;ZA"),
	StringView("xh:Xhosa|isiXhosa|xh|xh|xho|xho|ZA"),
	StringView(),
	StringView("aa:Afar|Afar|aa|aa|aar|aar|ER"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("hu:Hungarian|magyar|hu|hu|hun|hun|HU"),
	StringView(),
	StringView(),
	StringView("ku:Kurdish|Kurdî|ku|ku|kur|kur|IQ"),
	StringView("lo:Lao|ພາສາລາວ|lo|lo|lao|lao|LA"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("si:Sinhalese|සිංහල|si|si|sin|sin|LK"),
	StringView(),
	StringView("lt:Lithuanian|lietuvių kalba|lt|lt|lit|lit|LT"),
	StringView("mh:Marshallese|Kajin M̧ajeļ|mh|mh|mah|mah|MH"),
	StringView(),
	StringView("it:Italian|Italiano|it|it|ita|ita|IT;SM;CH;VA"),
	StringView(),
	StringView("mt:Maltese|Malti|mt|mt|mlt|mlt|MT"),
	StringView("ve:Venda|Tshivenḓa|ve|ve|ven|ven|ZA"),
	StringView(),
	StringView("sl:Slovene|slovenski jezik|sl|sl|slv|slv|SI"),
	StringView(),
	StringView("hy:Armenian|Հայերեն|hy|hy|hye|hye|AM;CY"),
	StringView(),
	StringView(),
	StringView("gn:Guaraní|Avañe'ẽ|gn|gn|grn|grn|AR;PY"),
	StringView("na:Nauruan|Dorerin Naoero|na|na|nau|nau|NR"),
	StringView("dz:Dzongkha|རྫོང་ཁ|dz|dz|dzo|dzo|BT"),
	StringView("he:Hebrew (modern)|עברית|he|he|heb|heb|IL"),
	StringView(),
	StringView(),
	StringView(),
	StringView("fo:Faroese|føroyskt|fo|fo|fao|fao|FO"),
	StringView("la:Latin|latine|la|la|lat|lat|VA"),
	StringView("rn:Kirundi|Ikirundi|rn|rn|run|run|BI"),
	StringView(),
	StringView(),
	StringView(),
	StringView("ts:Tsonga|Xitsonga|ts|ts|tso|tso|ZA"),
	StringView("ky:Kyrgyz|Кыргызча|ky|ky|kir|kir|KG"),
	StringView(),
	StringView(),
	StringView("nrb:Nara|Nara|nrb||nrb||ER"),
	StringView("tig:Tigre|ትግረ|tig||tig||ER"),
	StringView(),
	StringView(),
	StringView("tn:Tswana|Setswana|tn|tn|tsn|tsn|BW;ZA"),
	StringView(),
	StringView("uk:Ukrainian|Українська|uk|uk|ukr|ukr|UA"),
	StringView("uz:Uzbek|Oʻzbek|uz|uz|uzb|uzb|AF;UZ"),
	StringView("am:Amharic|አማርኛ|am|am|amh|amh|ET"),
	StringView("lu:Luba-Katanga|Tshiluba|lu|lu|lub|lub|CD"),
	StringView(),
	StringView("ar:Arabic|العربية|ar|ar|ara|ara|EG;DZ;BH;DJ;ER;IQ;IL;YE;JO;QA;KM;KW;LB;LY;MA;MR;OM;PS;SA;SO;SD;SY;TD;TN;AE"),
	StringView("ln:Lingala|Lingála|ln|ln|lin|lin|CD;CG"),
	StringView(),
	StringView(),
	StringView("ta:Tamil|தமிழ்|ta|ta|tam|tam|SG;LK"),
	StringView("tg:Tajik|тоҷикӣ|tg|tg|tgk|tgk|TJ"),
	StringView(),
	StringView(),
	StringView("mk:Macedonian|македонски јазик|mk|mk|mkd|mkd|MK"),
	StringView(),
	StringView("ka:Georgian|ქართული|ka|ka|kat|kat|GE"),
	StringView("ff:Fula|Fulfulde|ff|ff|ful|ful|BF;GN"),
	StringView("nb:Norwegian Bokmål|Norsk bokmål|nb|nb|nob|nob|BV;NO"),
	StringView(),
	StringView(),
	StringView("hif:Fiji Hindi|फ़िजी बात|hif||hif||FJ"),
	StringView(),
	StringView(),
	StringView(),
	StringView("nl:Dutch|Nederlands|nl|nl|nld|nld|AW;BE;CW;BQ;NL;MF;SX;SR"),
	StringView(),
	StringView("my:Burmese|ဗမာစာ|my|my|mya|mya|MM"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("qu:Quechua|Runa Simi|qu|qu|que|que|BO"),
	StringView(),
	StringView("no:Norwegian|Norsk|no|no|nor|nor|BV;NO;SJ"),
	StringView(),
	StringView(),
	StringView(),
	StringView("ca:Catalan|català|ca|ca|cat|cat|AD"),
	StringView("zh:Chinese|中文 (Zhōngwén)|zh|zh|zho|zho|CN;HK;MO;SG;TW"),
	StringView(),
	StringView("bg:Bulgarian|български език|bg|bg|bul|bul|BG"),
	StringView("is:Icelandic|Íslenska|is|is|isl|isl|IS"),
	StringView("hr:Croatian|hrvatski jezik|hr|hr|hrv|hrv|BA;HR;ME"),
	StringView(),
	StringView(),
	StringView("bi:Bislama|Bislama|bi|bi|bis|bis|VU"),
	StringView(),
	StringView("lv:Latvian|latviešu valoda|lv|lv|lav|lav|LV"),
	StringView("sg:Sango|yângâ tî sängö|sg|sg|sag|sag|CF"),
	StringView("mn:Mongolian|Монгол хэл|mn|mn|mon|mon|MN"),
	StringView(),
	StringView(),
	StringView("mi:Māori|te reo Māori|mi|mi|mri|mri|NZ"),
	StringView("sm:Samoan|gagana fa'a Samoa|sm|sm|smo|smo|AS;WS"),
	StringView("st:Southern Sotho|Sesotho|st|st|sot|sot|LS;ZA"),
	StringView("tk:Turkmen|Türkmen|tk|tk|tuk|tuk|AF;TM"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("id:Indonesian|Bahasa Indonesia|id|id|ind|ind|ID"),
	StringView("ps:Pashto|پښتو|ps|ps|pus|pus|AF"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("ch:Chamorro|Chamoru|ch|ch|cha|cha|GU;MP"),
	StringView(),
	StringView(),
	StringView("mg:Malagasy|fiteny malagasy|mg|mg|mlg|mlg|MG"),
	StringView("ne:Nepali|नेपाली|ne|ne|nep|nep|NP"),
	StringView(),
	StringView(),
	StringView(),
	StringView("rar:Cook Islands Māori|Māori|rar||rar||CK"),
	StringView("ga:Irish|Gaeilge|ga|ga|gle|gle|IE"),
	StringView("pt:Portuguese|Português|pt|pt|por|por|AO;GQ;BR;GW;CV;MO;MZ;TL;PT;ST"),
	StringView("zu:Zulu|isiZulu|zu|zu|zul|zul|ZA"),
	StringView("dv:Divehi|ދިވެހި|dv|dv|div|div|MV"),
	StringView("fan:Fang|Fang|fan||fan||GQ"),
	StringView(),
	StringView("et:Estonian|eesti|et|et|est|est|EE"),
	StringView("gv:Manx|Gaelg|gv|gv|glv|glv|IM"),
	StringView(),
	StringView("ro:Romanian|Română|ro|ro|ron|ron|MD;RO"),
	StringView("en:English|English|en|en|eng|eng|AS;AI;AQ;AG;AU;BS;BB;BZ;BM;BW;IO;CK;CW;DM;ER;SZ;FK;FJ;FM;GM;GH;GI;GD;GU;GG;GY;HM;HK;IN;IM;IE;JM;JE;VG;VI;KY;CM;CA;KE;KI;UM;CC;LS;LR;MW;MT;MH;MU;MS;NA;NR;NZ;NG;NU;MP;NF;PK;PW;PG;PH;PN;PR;RW;MF;SB;ZM;WS;SC;SL;ZW;SG;SX;SH;KN;LC;VC;ZA;SD;GS;SS;TZ;TK;TO;TT;TC;TV;UG;VU;US;GB;CX"),
	StringView("vi:Vietnamese|Tiếng Việt|vi|vi|vie|vie|VN"),
	StringView("af:Afrikaans|Afrikaans|af|af|afr|afr|NA;ZA"),
	StringView("ur:Urdu|اردو|ur|ur|urd|urd|PK"),
	StringView(),
	StringView("fi:Finnish|suomi|fi|fi|fin|fin|FI"),
	StringView(),
	StringView(),
});

static constexpr std::array<StringView, 512> s_countriesArray({
	StringView(),
	StringView("ge:Georgia|საქართველო / Sakartwelo|GE|Asia|Western Asia|Tbilisi|Georgian Lari|lari|GEL|₾|Tetri|ka|🇬🇪|UTC+04:00|AM;AZ;RU;TR|####|GE|GEO|ge|GE|GE|268"),
	StringView("td:Chad|جمهوريّة تشاد / Tchad|TD|Africa|Middle Africa|N'Djamena|Central African Franc|Central African CFA franc|XAF|Fr||fr;ar|🇹🇩|UTC+01:00|CM;CF;LY;NE;NG;SD||TD|TCD|td|TD|TD|148"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("bv:Bouvet Island|Bouvetøya|BV|South America|||Norwegian Krone||NOK||Øre|no;nb;nn|🇧🇻|UTC+01:00|||BV|BVT|bv|||74"),
	StringView("tj:Tajikistan|Toçikiston / Тоҷикистон|TJ|Asia|Central Asia|Dushanbe|Somoni|Tajikistani somoni|TJS|ЅМ|Dirham|tg;ru|🇹🇯|UTC+05:00|AF;CN;KG;UZ|######|TJ|TJK|tj|TJ|TJ|762"),
	StringView(),
	StringView(),
	StringView("be:Belgium|België / Belgique|BE|Europe|Western Europe|Brussels|Euro|Euro|EUR|€|Cent|nl;fr;de|🇧🇪|UTC+01:00|FR;DE;LU;NL|####|BE|BEL|be|B|BE|56"),
	StringView("cr:Costa Rica|Costa Rica|CR|Central America|Central America|San José|Colón|Costa Rican colón|CRC|₡|Céntimos|es|🇨🇷|UTC-06:00|NI;PA|####|CR|CRI|cr|CR|CR|188"),
	StringView("uy:Uruguay|Uruguay|UY|South America|South America|Montevideo|Uruguay Peso|Uruguayan peso|UYU|$|Centesimos|es|🇺🇾|UTC-03:00|AR;BR|#####|UY|URY|uy|ROU|UY|858"),
	StringView("bq:Caribbean Netherlands|Caribisch Nederland|BQ|Central America|Caribbean||US Dollar|United States dollar|USD|$|Cents|nl|🇧🇶|UTC-04:00|||BQ|BES|bq|NL||535"),
	StringView(),
	StringView("kp:North Korea|Choson Minjujuui In´min Konghwaguk (Bukhan)|KP|Asia|Eastern Asia|Pyongyang|North Korean Won|North Korean won|KPW|₩|Chon|ko|🇰🇵|UTC+09:00|CN;KR;RU|###-###|KP|PRK|kp|KP|KP|408"),
	StringView("io:British Indian Ocean Territory|British Indian Ocean Territory|IO|Africa|Eastern Africa||US Dollar|United States dollar|USD|$|Cents|en|🇮🇴|UTC+06:00|||IO|IOT|io||IO|86"),
	StringView(),
	StringView("de:Germany|Deutschland|DE|Europe|Western Europe|Berlin|Euro|Euro|EUR|€|Cent|de|🇩🇪|UTC+01:00|AT;BE;CZ;DK;FR;LU;NL;PL;CH|#####|DE|DEU|de|D|DE|276"),
	StringView("fk:Falkland Islands|Falkland Islands|FK|South America|South America|Stanley|Falklands Pound|Falkland Islands pound|FKP|£|Pence|en|🇫🇰|UTC-04:00|||FK|FLK|fk||FK|238"),
	StringView(),
	StringView("sb:Solomon Islands|Solomon Islands|SB|Oceania|Melanesia|Honiara|Salomon Dollar|Solomon Islands dollar|SBD|$|Cents|en|🇸🇧|UTC+11:00|||SB|SLB|sb|SOL|SB|90"),
	StringView(),
	StringView(),
	StringView("cv:Cape Verde|Cabo Verde|CV|Africa|Western Africa|Praia|Cape Verdean Escudo|Cape Verdean escudo|CVE|Esc|Centavos|pt|🇨🇻|UTC-01:00||####|CV|CPV|cv|CV|CV|132"),
	StringView(),
	StringView("gh:Ghana|Ghana|GH|Africa|Western Africa|Accra|Ghana Cedi|Ghanaian cedi|GHS|₵|Pesewas|en|🇬🇭|UTC|BF;CI;TG||GH|GHA|gh|GH|GH|288"),
	StringView(),
	StringView("sc:Seychelles|Sese l /Seychelles|SC|Africa|Eastern Africa|Victoria|Seychelles Rupee|Seychellois rupee|SCR|₨|Cents|fr;en|🇸🇨|UTC+04:00|||SC|SYC|sc|SY|SC|690"),
	StringView("to:Tonga|Puleʻanga Fakatuʻi ʻo Tonga|TO|Oceania|Polynesia|Nuku‘alofa|Pa'anga|Tongan paʻanga|TOP|T$|Seniti|en;to|🇹🇴|UTC+13:00|||TO|TON|to|TON|TO|776"),
	StringView("mp:Northern Mariana Islands|Northern Mariana Islands|MP|Oceania|Micronesia|Saipan|US Dollar|United States dollar|USD|$|Cents|en;ch|🇲🇵|UTC+10:00|||MP|MNP|mp||MP|580"),
	StringView("gl:Greenland|Kalaallit Nunaat / Grønland|GL|North America|North America|Nuuk|Danish Krone|krone|DKK|kr.|øre|kl|🇬🇱|UTC-04:00;UTC-03:00;UTC-01:00;UTC+00:00||####|GL|GRL|gl|KN|GL|304"),
	StringView(),
	StringView(),
	StringView(),
	StringView("gw:Guinea-Bissau|Guiné-Bissau|GW|Africa|Western Africa|Bissau|West African Franc|West African CFA franc|XOF|Fr||pt|🇬🇼|UTC|GN;SN|####|GW|GNB|gw|GUB|GW|624"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("jp:Japan|日本国|JP|Asia|Eastern Asia|Tokyo|Japanese Yen|Japanese yen|JPY|¥|Sen|ja|🇯🇵|UTC+09:00||###-####|JP|JPN|jp|J|JP|392"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("ai:Anguilla|Anguilla|AI|Central America|Caribbean|The Valley|East Caribbean Dollar|Eastern Caribbean dollar|XCD|$|Cents|en|🇦🇮|UTC-04:00|||AI|AIA|ai|AXA|AI|660"),
	StringView(),
	StringView("sa:Saudi Arabia|المملكة العربية السعودية / Al-´Arabiya as-Sa´|SA|Asia|Western Asia|Riyadh|Saudi Rial|Saudi riyal|SAR|ر.س|Qirshes|ar|🇸🇦|UTC+03:00|IQ;JO;KW;OM;QA;AE;YE|#####|SA|SAU|sa|KSA|SA|682"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("gs:South Georgia and South Sandwich Islands|South Georgia and the South Sandwich Islands|GS|South America||King Edward Point|Sterling Pound||GBP||Pence|en|🇬🇸|UTC-02:00|||GS|SGS|gs||GS|239"),
	StringView("jo:Jordan|الأُرْدُنّ  / Al-Urdunn|JO|Asia|Western Asia|Amman|Jordanian Dinar|Jordanian dinar|JOD|د.ا|Piaster|ar|🇯🇴|UTC+03:00|IQ;IL;PS;SA;SY|#####|JO|JOR|jo|JOR|JO|400"),
	StringView(),
	StringView(),
	StringView(),
	StringView("tc:Turks and Caicos Islands|The Turks and Caicos Islands|TC|Central America|Caribbean|Cockburn Town|US Dollar|United States dollar|USD|$|Cents|en|🇹🇨|UTC-04:00||TKCA 1ZZ|TC|TCA|tc||TC|796"),
	StringView(),
	StringView("mu:Mauritius|Maurice|MU|Africa|Eastern Africa|Port Louis|Mauritian Rupee|Mauritian rupee|MUR|₨|Cents|en|🇲🇺|UTC+04:00|||MU|MUS|mu|MS|MU|480"),
	StringView(),
	StringView(),
	StringView("sd:Sudan|جمهورية السودان / As-Sūdān|SD|Africa|Northern Africa|Khartoum|Sudanese Pound|Sudanese pound|SDG||Piaster|ar;en|🇸🇩|UTC+03:00|CF;TD;EG;ER;ET;LY;SS|#####|SD|SDN|sd|SUD|SD|729"),
	StringView("bl:Saint Barthelemy|Saint-Barthélemy|BL|Central America|Caribbean|Gustavia|Euro|Euro|EUR|€|Cent|fr|🇧🇱|UTC-04:00||### ###|BL|BLM|bl|||652"),
	StringView("cn:China|中國 / 中国 / Zhōngguó|CN|Asia|Eastern Asia|Beijing|Renminbi Yuan|Chinese yuan|CNY|¥|Fen|zh|🇨🇳|UTC+08:00|AF;BT;MM;HK;IN;KZ;NP;KP;KG;LA;MO;MN;PK;RU;TJ;VN|######|CN|CHN|cn|CHN|CN|156"),
	StringView("pw:Palau|Belau/Palau|PW|Oceania|Micronesia||US Dollar|United States dollar|USD|$|Cents|en|🇵🇼|UTC+09:00||96940|PW|PLW|pw|PAL|PW|585"),
	StringView(),
	StringView("hk:Hong Kong|香港 / Xiānggǎng|HK|Asia|Eastern Asia|Hong Kong|Hong Kong Dollar|Hong Kong dollar|HKD|$|Cents|en;zh|🇭🇰|UTC+08:00|CN||HK|HKG|hk|HK|HK|344"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("nr:Nauru|Naoero/Nauru|NR|Oceania|Micronesia|Yaren|Australian Dollar|Australian dollar|AUD|$|Cents|en;na|🇳🇷|UTC+12:00|||NR|NRU|nr|NAU|NR|520"),
	StringView(),
	StringView("ml:Mali|Mali|ML|Africa|Western Africa|Bamako|West African Franc|West African CFA franc|XOF|Fr||fr|🇲🇱|UTC|DZ;BF;GN;CI;MR;NE;SN||ML|MLI|ml|RMM|ML|466"),
	StringView(),
	StringView(),
	StringView("um:United States Minor Outlying Islands|United States Minor Outlying Islands|UM|Oceania|North America||US Dollar|United States dollar|USD|$|Cents|en|🇺🇲|UTC-11:00;UTC-10:00;UTC+12:00|||UM|UMI|us||UM|581"),
	StringView("es:Spain|España|ES|Europe|Southern Europe|Madrid|Euro|Euro|EUR|€|Cent|es|🇪🇸|UTC;UTC+01:00|AD;FR;GI;PT;MA|#####|ES|ESP|es|E|ES|724"),
	StringView(),
	StringView(),
	StringView("bs:Bahamas|The Bahamas|BS|Central America|Caribbean|Nassau|Bahamian Dollar|Bahamian dollar|BSD|$|Cents|en|🇧🇸|UTC-05:00|||BS|BHS|bs|BS|BS|44"),
	StringView("bh:Bahrain|البحرين al-Bahrain|BH|Asia|Western Asia|Manama|Bahrain Dinar|Bahraini dinar|BHD|.د.ب|Fils|ar|🇧🇭|UTC+03:00||####|###|BH|BHR|bh|BRN|BH|48"),
	StringView("ae:United Arab Emirates|الإمارات العربية المتحدة / Al-Imarat al-´Arab|AE|Asia|Western Asia|Abu Dhabi|Arabic Dirham|United Arab Emirates dirham|AED|د.إ|Fils|ar|🇦🇪|UTC+04:00|OM;SA||AE|ARE|ae|UAE|AE|784"),
	StringView(),
	StringView("cg:Republic of the Congo|Congo|CG|Africa|Middle Africa|Brazzaville|Central African Franc|Central African CFA franc|XAF|Fr||fr;ln|🇨🇬|UTC+01:00|AO;CM;CF;CD;GA||CG|COG|cg|RCB|CG|178"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("ad:Andorra||AD|Europe|Southern Europe|Andorra la Vella|Euro|Euro|EUR|€|Cent|ca|🇦🇩|UTC+01:00|FR;ES|AD###|AD|AND|ad|AND|AD|20"),
	StringView("as:American Samoa|Sāmoa Amelika|AS|Oceania|Polynesia|Pago Pago|US Dollar|United States dollar|USD|$|Cents|en;sm|🇦🇸|UTC-11:00|||AS|ASM|as|USA|AS|16"),
	StringView(),
	StringView(),
	StringView("rw:Rwanda|Rwanda / Urwanda|RW|Africa|Eastern Africa|Kigali|Rwandan Franc|Rwandan franc|RWF|Fr|Centimes|rw;en;fr|🇷🇼|UTC+02:00|BI;CD;TZ;UG||RW|RWA|rw|RWA|RW|646"),
	StringView("cm:Cameroon|Cameroun / Cameroon|CM|Africa|Middle Africa|Yaounde|Central African Franc|Central African CFA franc|XAF|Fr||en;fr|🇨🇲|UTC+01:00|CF;TD;CG;GQ;GA;NG||CM|CMR|cm|CAM|CM|120"),
	StringView("ss:South Sudan||SS|Africa|Middle Africa|Juba|South Sudanese Pound|South Sudanese pound|SSP|£|Piaster|en|🇸🇸|UTC+03:00|CF;CD;ET;KE;SD;UG||SS|SSD|ss|SSD||728"),
	StringView(),
	StringView(),
	StringView("ph:Philippines|Pilipinas|PH|Asia|South-Eastern Asia|Manila|Philippine Peso|Philippine peso|PHP|₱|Centavos|en|🇵🇭|UTC+08:00||####|PH|PHL|ph|RP|PH|608"),
	StringView("pe:Peru|Perú/Piruw|PE|South America|South America|Lima|Nuevo Sol|Peruvian sol|PEN|S/ |Céntimos|es|🇵🇪|UTC-05:00|BO;BR;CL;CO;EC|#####|PE|PER|pe|PE|PE|604"),
	StringView(),
	StringView(),
	StringView("ly:Libya|Libiya|LY|Africa|Northern Africa|Tripoli|Libyan Dinar|Libyan dinar|LYD|ل.د|Dirhams|ar|🇱🇾|UTC+01:00|DZ;TD;EG;NE;SD;TN||LY|LBY|ly|LAR|LY|434"),
	StringView(),
	StringView("vu:Vanuatu|Vanuatu|VU|Oceania|Melanesia|Port-Vila|Vatu|Vanuatu vatu|VUV|Vt|Centimes|bi;en;fr|🇻🇺|UTC+11:00|||VU|VUT|vu|VAN|VU|548"),
	StringView(),
	StringView(),
	StringView("bj:Benin|Bénin|BJ|Africa|Western Africa|Porto-Novo|West African Franc|West African CFA franc|XOF|Fr||fr|🇧🇯|UTC+01:00|BF;NE;NG;TG||BJ|BEN|bj|BJ|BJ|204"),
	StringView("wf:Wallis and Futuna|Wallis-et-Futuna|WF|Oceania|Polynesia|Mata-Utu|Pacific Franc|CFP franc|XPF|₣||fr|🇼🇫|UTC+12:00||#####|WF|WLF|wf||WF|876"),
	StringView("hn:Honduras|Honduras|HN|Central America|Central America|Tegucigalpa|Lempira|Honduran lempira|HNL|L|Centavos|es|🇭🇳|UTC-06:00|GT;SV;NI|@@####|HN|HND|hn|HN|HN|340"),
	StringView("jm:Jamaica|Jamaica|JM|Central America|Caribbean|Kingston|Jamaica Dollar|Jamaican dollar|JMD|$|Cents|en|🇯🇲|UTC-05:00|||JM|JAM|jm|JA|JM|388"),
	StringView(),
	StringView(),
	StringView(),
	StringView("er:Eritrea|ኤርትራ Ertra / إرتريا Iritriyyā|ER|Africa|Eastern Africa|Asmara|Nakfa|Eritrean nakfa|ERN|Nfk|Cents|ti;ar;en;tig;kun;ssy;byn;nrb;aa|🇪🇷|UTC+03:00|DJ;ET;SD||ER|ERI|er|ER|ER|232"),
	StringView("nf:Norfolk Island|Norfolk Island|NF|Australia|Australia and New Zealand|Kingston|Australian Dollar|Australian dollar|AUD|$|Cents|en|🇳🇫|UTC+11:30|||NF|NFK|nf||NF|574"),
	StringView("lt:Lithuania|Lietuva|LT|Europe|Northern Europe|Vilnius|Euro|Euro|EUR|€|Cent|lt|🇱🇹|UTC+02:00|BY;LV;PL;RU|LT-#####|LT|LTU|lt|LT|LT|440"),
	StringView(),
	StringView(),
	StringView("lk:Sri Lanka|ශ්‍රී ලංකා / இலங்கை|LK|Asia|Southern Asia|Colombo|Sri Lanka Rupee|Sri Lankan rupee|LKR|Rs  රු|Cents|si;ta|🇱🇰|UTC+05:30|IN|#####|LK|LKA|lk|CL|LK|144"),
	StringView("bd:Bangladesh|বাংলাদেশ Bāṃlādeś|BD|Asia|Southern Asia|Dhaka|Taka|Bangladeshi taka|BDT|৳|Poisha|bn|🇧🇩|UTC+06:00|MM;IN|####|BD|BGD|bd|BD|BD|50"),
	StringView("gt:Guatemala|República de Guatemala|GT|Central America|Central America|Guatemala City|Quetzal|Guatemalan quetzal|GTQ|Q|Centavos|es|🇬🇹|UTC-06:00|BZ;SV;HN;MX|#####|GT|GTM|gt|GCA|GT|320"),
	StringView(),
	StringView("au:Australia|Australia|AU|Australia|Australia and New Zealand|Canberra|Australian Dollar|Australian dollar|AUD|$|Cents|en|🇦🇺|UTC+05:00;UTC+06:30;UTC+07:00;UTC+08:00;UTC+09:30;UTC+10:00;UTC+10:30;UTC+11:30||####|AU|AUS|au|AUS|AU|36"),
	StringView("kw:Kuwait|Al-Kuwayt|KW|Asia|Western Asia|Kuwait City|Kuwaiti Dinar|Kuwaiti dinar|KWD|د.ك|Fils|ar|🇰🇼|UTC+03:00|IQ;SA|#####|KW|KWT|kw|KWT|KW|414"),
	StringView("sl:Sierra Leone|Sierra Leone|SL|Africa|Western Africa|Freetown|Leone|Sierra Leonean leone|SLL|Le|Cents|en|🇸🇱|UTC|GN;LR||SL|SLE|sl|WAL|SL|694"),
	StringView("zm:Zambia|Zambia|ZM|Africa|Eastern Africa|Lusaka|Zambian Kwacha|Zambian kwacha|ZMW|ZK|Ngwee|en|🇿🇲|UTC+02:00|AO;BW;CD;MW;MZ;NA;TZ;ZW|#####|ZM|ZMB|zm|Z|ZM|894"),
	StringView("cf:Central African Republic|Centrafrique / Bê-Afrîka|CF|Africa|Middle Africa|Bangui|Central African Franc|Central African CFA franc|XAF|Fr||fr;sg|🇨🇫|UTC+01:00|CM;TD;CD;CG;SS;SD||CF|CAF|cf|RCA|CF|140"),
	StringView(),
	StringView("na:Namibia|Namibia|NA|Africa|Southern Africa|Windhoek|Namibian Dollar|Namibian dollar|NAD|$|Cents|en;af|🇳🇦|UTC+01:00|AO;BW;ZA;ZM||NA|NAM|na|NAM|NA|516"),
	StringView("cl:Chile|Chile|CL|South America|South America|Santiago|Chilean Peso|Chilean peso|CLP|$|Centavos|es|🇨🇱|UTC-06:00;UTC-04:00|AR;BO;PE|#######|CL|CHL|cl|RCH|CL|152"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("gb:United Kingdom|United Kingdom|GB|Europe|Northern Europe|London|Sterling Pound|British pound|GBP|£|Pence|en|🇬🇧|UTC-08:00;UTC-05:00;UTC-04:00;UTC-03:00;UTC-02:00;UTC;UTC+01:00;UTC+02:00;UTC+06:00|IE|@# #@@|@## #@@|@@# #@@|@@## #@@|@#@ #@@|@@#@ #@@|GIR0AA|GB|GBR|uk|GBM|GB|826"),
	StringView(),
	StringView("je:Jersey|Bailiwick of Jersey|JE|Europe|Northern Europe|Saint Helier|Jersey Sterling Pound|Jersey pound|JEP|£|Penny|en;fr|🇯🇪|UTC+01:00||@# #@@|@## #@@|@@# #@@|@@## #@@|@#@ #@@|@@#@ #@@|GIR0AA|JE|JEY|je|GBJ||832"),
	StringView("im:Isle of Man|Isle of Man / Mannin / Ellan Vannin|IM|Europe|Northern Europe|Douglas|Manx Pound|Manx pound|IMP|£|Pence|en;gv|🇮🇲|UTC+00:00||@# #@@|@## #@@|@@# #@@|@@## #@@|@#@ #@@|@@#@ #@@|GIR0AA|IM|IMN|im|||833"),
	StringView(),
	StringView(),
	StringView("ma:Morocco|Al-Maghrib|MA|Africa|Northern Africa|Rabat|Moroccan Dirham|Moroccan dirham|MAD|د.م.|Centimes|ar|🇲🇦|UTC|DZ;EH;ES|#####|MA|MAR|ma|MA|MA|504"),
	StringView("il:Israel|ישראל / Yisra’el / Isra’il|IL|Asia|Western Asia|Jerusalem|New Israeli Sheqel|Israeli new shekel|ILS|₪|Agorot|he;ar|🇮🇱|UTC+02:00|EG;JO;LB;PS;SY|#####|IL|ISR|il|IL|IL|376"),
	StringView("us:United States of America||US|North America|North America|Washington, D.C.|US Dollar|United States dollar|USD|$|Cents|en|🇺🇸|UTC-12:00;UTC-11:00;UTC-10:00;UTC-09:00;UTC-08:00;UTC-07:00;UTC-06:00;UTC-05:00;UTC-04:00;UTC+10:00;UTC+12:00|CA;MX|#####-####|US|USA|us|USA|US|840"),
	StringView("kn:Saint Kitts and Nevis|Saint Kitts and Nevis|KN|Central America|Caribbean|Basseterre|East Caribbean Dollar|Eastern Caribbean dollar|XCD|$|Cents|en|🇰🇳|UTC-04:00|||KN|KNA|kn|KAN|KN|659"),
	StringView(),
	StringView(),
	StringView("tn:Tunisia|تونس / الجمهورية التونسية / Tūnisiyya|TN|Africa|Northern Africa|Tunis|Tunesian Dinar|Tunisian dinar|TND|د.ت|Millimes|ar|🇹🇳|UTC+01:00|DZ;LY|####|TN|TUN|tn|TN|TN|788"),
	StringView(),
	StringView("nz:New Zealand|New Zealand/Aotearoa|NZ|Australia|Australia and New Zealand|Wellington|New Zealand Dollar|New Zealand dollar|NZD|$|Cents|en;mi|🇳🇿|UTC-11:00;UTC-10:00;UTC+12:00;UTC+12:45;UTC+13:00||####|NZ|NZL|nz|NZ|NZ|554"),
	StringView("pr:Puerto Rico|Puerto Rico|PR|Central America|Caribbean|San Juan|US Dollar|United States dollar|USD|$|Cents|es;en|🇵🇷|UTC-04:00||#####-####|PR|PRI|pr|PRI|PR|630"),
	StringView("am:Armenia|Հայաստան Hajastan|AM|Asia|Western Asia|Yerevan|Dram|Armenian dram|AMD|֏|Lumma|hy|🇦🇲|UTC+04:00|AZ;GE;IR;TR|######|AM|ARM|am|AM|AM|51"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("eg:Egypt|مصر Miṣr|EG|Africa|Northern Africa|Cairo|Egypt Pound|Egyptian pound|EGP|£|Piasters|ar|🇪🇬|UTC+02:00|IL;LY;PS;SD|#####|EG|EGY|eg|ET|EG|818"),
	StringView(),
	StringView(),
	StringView("pm:Saint Pierre and Miquelon|Saint-Pierre-et-Miquelon|PM|North America|North America|Saint-Pierre|Euro|Euro|EUR|€|Cent|fr|🇵🇲|UTC-03:00||#####|PM|SPM|pm||PM|666"),
	StringView(),
	StringView("nu:Niue|Niue|NU|Oceania|Polynesia|Alofi|New Zealand Dollar|New Zealand dollar|NZD|$|Cents|en|🇳🇺|UTC-11:00|||NU|NIU|nu||NU|570"),
	StringView("ug:Uganda|Uganda|UG|Africa|Eastern Africa|Kampala|Ugandan Schilling|Ugandan shilling|UGX|Sh|Cents|en;sw|🇺🇬|UTC+03:00|CD;KE;RW;SS;TZ||UG|UGA|ug|EAU|UG|800"),
	StringView("kz:Kazakhstan|Қазақстан /  Qazaqstan|KZ|Asia|Central Asia|Nursultan|Tenge|Kazakhstani tenge|KZT|₸|Tyin|kk;ru|🇰🇿|UTC+05:00;UTC+06:00|CN;KG;RU;TM;UZ|######|KZ|KAZ|kz|KZ|KZ|398"),
	StringView(),
	StringView("ax:Åland Islands|Ahvenanmaa|AX|Europe|Northern Europe|Mariehamn|Euro|Euro|EUR|€|Cent|sv|🇦🇽|UTC+02:00|||AX|ALA|ax|AX||248"),
	StringView(),
	StringView("nl:Netherlands|Nederland|NL|Europe|Western Europe|Amsterdam|Euro|Euro|EUR|€|Cent|nl|🇳🇱|UTC-04:00;UTC+01:00|BE;DE|#### @@|NL|NLD|nl|NL|NL|528"),
	StringView(),
	StringView("ba:Bosnia and Herzegovina|Bosna i Hercegovina / Босна и Херцеговина|BA|Europe|Southeast Europe|Sarajevo|Convertible Mark|Bosnia and Herzegovina convertible mark|BAM||Fening|bs;hr;sr|🇧🇦|UTC+01:00|HR;ME;RS|#####|BA|BIH|ba|BIH|BA|70"),
	StringView("pf:French Polynesia|Polynésie française|PF|Oceania|Polynesia|Papeete|Pacific Franc|CFP franc|XPF|₣||fr|🇵🇫|UTC-10:00;UTC-09:30;UTC-09:00||#####|PF|PYF|pf||PF|258"),
	StringView("my:Malaysia|Malaysia|MY|Asia|South-Eastern Asia|Kuala Lumpur|Ringgit|Malaysian ringgit|MYR|RM|Sen|ms|🇲🇾|UTC+08:00|BN;ID;TH|#####|MY|MYS|my|MAL|MY|458"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("no:Norway|Norge|NO|Europe|Northern Europe|Oslo|Norwegian Krone|Norwegian krone|NOK|kr|Øre|no;nb;nn|🇳🇴|UTC+01:00|FI;SE;RU|####|NO|NOR|no|N|NO|578"),
	StringView(),
	StringView(),
	StringView("ec:Ecuador|Ecuador|EC|South America|South America|Quito|US Dollar|United States dollar|USD|$|Cents|es|🇪🇨|UTC-06:00;UTC-05:00|CO;PE|@####@|EC|ECU|ec|EC|EC|218"),
	StringView(),
	StringView(),
	StringView(),
	StringView("gq:Equatorial Guinea|Guinea Ecuatorial|GQ|Africa|Middle Africa|Malabo|Central African Franc|Central African CFA franc|XAF|Fr||es;fr;pt;fan|🇬🇶|UTC+01:00|CM;GA||GQ|GNQ|gq|GQ|GQ|226"),
	StringView("hm:Heard Island and McDonald Islands|Heard and McDonald Islands|HM|Australia|||Australian Dollar||AUD||Cents|en|🇭🇲|UTC+05:00|||HM|HMD|hm||HM|334"),
	StringView("hr:Croatia|Hrvatska|HR|Europe|Southeast Europe|Zagreb|Euro|Euro|EUR|€|Cent|hr|🇭🇷|UTC+01:00|BA;HU;ME;RS;SI|HR-#####|HR|HRV|hr|HR|HR|191"),
	StringView("is:Iceland|Ísland|IS|Europe|Northern Europe|Reykjavík|Icelandic Krone|Icelandic króna|ISK|kr|Aurar|is|🇮🇸|UTC||###|IS|ISL|is|IS|IS|352"),
	StringView("sx:Sint Maarten|Sint Maarten|SX|Central America|Caribbean|Philipsburg|Caribbean guilder|Caribische gulden|XCG|Cg|Cents|nl;en|🇸🇽|UTC-04:00|MF||SX|SXM|sx|||534"),
	StringView("bi:Burundi|Burundi / Uburundi|BI|Africa|Eastern Africa|Bujumbura|Burundi Franc|Burundian franc|BIF|Fr|Centimes|fr;rn|🇧🇮|UTC+02:00|CD;RW;TZ||BI|BDI|bi|RU|BI|108"),
	StringView("tv:Tuvalu|Tuvalu / Fakavae Aliki-Malo|TV|Oceania|Polynesia|Funafuti|Tuvaluan Dollar|Tuvaluan dollar|TVD|$|Cents|en|🇹🇻|UTC+12:00|||TV|TUV|tv|TUV|TV|798"),
	StringView("sg:Singapore|Singapore / Singapura / 新加坡共和国 / சிங்கப்பூர்|SG|Asia|South-Eastern Asia|Singapore|Singapore Dollar|Singapore dollar|SGD|$|Cents|en;ms;ta;zh|🇸🇬|UTC+08:00||######|SG|SGP|sg|SGP|SG|702"),
	StringView("cd:Democratic Republic of the Congo|République Démocratique du Congo|CD|Africa|Middle Africa|Kinshasa|Congolais Franc|Congolese franc|CDF|FC|Centimes|fr;ln;kg;sw;lu|🇨🇩|UTC+01:00;UTC+02:00|AO;BI;CF;CG;RW;SS;TZ;UG;ZM||CD|COD|cd|CGO|CD|180"),
	StringView(),
	StringView("ee:Estonia|Eesti|EE|Europe|Northern Europe|Tallinn|Euro|Euro|EUR|€|Cent|et|🇪🇪|UTC+02:00|LV;RU|#####|EE|EST|ee|EST|EE|233"),
	StringView(),
	StringView("tk:Tokelau|Tokelau|TK|Oceania|Polynesia||New Zealand Dollar|New Zealand dollar|NZD|$|Cents|en|🇹🇰|UTC+13:00|||TK|TKL|tk||TK|772"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("ps:Palestine|Filastin|PS|Asia|Western Asia|Ramallah|New Israeli Sheqel|Israeli new shekel|ILS|₪|Agorot|ar|🇵🇸|UTC+02:00|IL;EG;JO||PS|PSE|ps|WB||275"),
	StringView("id:Indonesia|Indonesia|ID|Asia|South-Eastern Asia|Jakarta|Indonesian Rupiah|Indonesian rupiah|IDR|Rp|Sen|id|🇮🇩|UTC+07:00;UTC+08:00;UTC+09:00|TL;MY;PG|#####|ID|IDN|id|RI|ID|360"),
	StringView("np:Nepal|Nepal|NP|Asia|Southern Asia|Kathmandu|Nepalese Rupee|Nepalese rupee|NPR|₨|Mohur|ne|🇳🇵|UTC+05:45|CN;IN|#####|NP|NPL|np|NEP|NP|524"),
	StringView("cx:Christmas Island|Christmas Island|CX|Australia|Australia and New Zealand|Flying Fish Cove|Australian Dollar|Australian dollar|AUD|$|Cents|en|🇨🇽|UTC+07:00||####|CX|CXR|cx||CX|162"),
	StringView(),
	StringView("do:Dominican Republic|República Dominicana|DO|Central America|Caribbean|Santo Domingo|Dominican Peso|Dominican peso|DOP|$|Centavos|es|🇩🇴|UTC-04:00|HT|#####|DO|DOM|do|DOM|DO|214"),
	StringView("bt:Bhutan|འབྲུག་ཡུལ་ Dzongkha|BT|Asia|Southern Asia|Thimphu|Ngultrum|Bhutanese ngultrum|BTN|Nu.|Chetrum|dz|🇧🇹|UTC+06:00|CN;IN||BT|BTN|bt|BHT|BT|64"),
	StringView("ke:Kenya|Kenya|KE|Africa|Eastern Africa|Nairobi|Kenian Schilling|Kenyan shilling|KES|Sh|Cents|en;sw|🇰🇪|UTC+03:00|ET;SO;SS;TZ;UG|#####|KE|KEN|ke|EAK|KE|404"),
	StringView("ye:Yemen|الجمهورية اليمنية / Al-Yaman|YE|Asia|Western Asia|Sanaa|Jemen Rial|Yemeni rial|YER|﷼|Fils|ar|🇾🇪|UTC+03:00|OM;SA||YE|YEM|ye|YEM|YE|887"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("bf:Burkina Faso|Burkina Faso|BF|Africa|Western Africa|Ouagadougou|West African Franc|West African CFA franc|XOF|Fr||fr;ff|🇧🇫|UTC|BJ;CI;GH;ML;NE;TG||BF|BFA|bf|BF|BF|854"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("ro:Romania|România|RO|Europe|Southeast Europe|Bucharest|Romanian Leu|Romanian leu|RON|lei|Bani|ro|🇷🇴|UTC+02:00|BG;HU;MD;RS;UA|######|RO|ROU|ro|RO|RO|642"),
	StringView("mf:Saint Martin|Saint Martin|MF|Central America|Caribbean|Marigot|Euro|Euro|EUR|€|Cent|en;fr;nl|🇲🇫|UTC-04:00|SX|### ###|MF|MAF|mf|F||663"),
	StringView(),
	StringView(),
	StringView("gi:Gibraltar|Gibraltar|GI|Europe|Southern Europe|Gibraltar|Gibraltar Pound|Gibraltar pound|GIP|£|Pence|en|🇬🇮|UTC+01:00|ES||GI|GIB|gi|GBZ|GI|292"),
	StringView("cy:Cyprus|Κύπρος / Kypros / Kıbrıs|CY|Asia|Southern Europe|Nicosia|Euro|Euro|EUR|€|Cent|el;tr;hy|🇨🇾|UTC+02:00||####|CY|CYP|cy|CY|CY|196"),
	StringView(),
	StringView("bo:Bolivia|Bolivia|BO|South America|South America|Sucre|Boliviano|Bolivian boliviano|BOB|Bs.|Centavos|es;ay;qu|🇧🇴|UTC-04:00|AR;BR;CL;PY;PE||BO|BOL|bo|BOL|BO|68"),
	StringView("bz:Belize|Belize|BZ|Central America|Central America|Belmopan|Belize Dollar|Belize dollar|BZD|$|Cents|en;es|🇧🇿|UTC-06:00|GT;MX||BZ|BLZ|bz|BZ|BZ|84"),
	StringView(),
	StringView("ck:Cook Islands|Cook Islands / Kūki 'Āirani|CK|Oceania|Polynesia|Avarua|Cook Dollar|Cook Islands dollar|CKD|$|Cents|en;rar|🇨🇰|UTC-10:00|||CK|COK|ck||CK|184"),
	StringView("gy:Guyana|Guyana|GY|South America|South America|Georgetown|Guyana Dollar|Guyanese dollar|GYD|$|Cents|en|🇬🇾|UTC-04:00|BR;SR;VE||GY|GUY|gy|GUY|GY|328"),
	StringView("gm:Gambia|The Gambia|GM|Africa|Western Africa|Banjul|Dalasi|dalasi|GMD|D|Bututs|en|🇬🇲|UTC+00:00|SN||GM|GMB|gm|WAG|GM|270"),
	StringView("sn:Senegal|Sénégal / Sounougal|SN|Africa|Western Africa|Dakar|West African Franc|West African CFA franc|XOF|Fr||fr|🇸🇳|UTC|GM;GN;GW;ML;MR|#####|SN|SEN|sn|SN|SN|686"),
	StringView("bw:Botswana|Botswana|BW|Africa|Southern Africa|Gaborone|Pula|Botswana pula|BWP|P|Thebe|en;tn|🇧🇼|UTC+02:00|NA;ZA;ZM;ZW||BW|BWA|bw|RB|BW|72"),
	StringView("th:Thailand|Prathet Thai / ประเทศไทย|TH|Asia|South-Eastern Asia|Bangkok|Thai Baht|Thai baht|THB|฿|Satang|th|🇹🇭|UTC+07:00|MM;KH;LA;MY|#####|TH|THA|th|T|TH|764"),
	StringView(),
	StringView("md:Moldova|Moldova|MD|Europe|Eastern Europe|Chisinau|Moldovan Leu|Moldovan leu|MDL|L|Bani|ro|🇲🇩|UTC+02:00|RO;UA|MD-####|MD|MDA|md|MD|MD|498"),
	StringView("iq:Iraq|جمهورية العراق / Al-´Iraq|IQ|Asia|Western Asia|Baghdad|Iraqi Dinar|Iraqi dinar|IQD|ع.د|Fils|ar;ku|🇮🇶|UTC+03:00|IR;JO;KW;SA;SY;TR|#####|IQ|IRQ|iq|IRQ|IQ|368"),
	StringView(),
	StringView("pg:Papua New Guinea|Papua New Guinea/Papua Niugini|PG|Oceania|Melanesia|Port Moresby|Kina|Papua New Guinean kina|PGK|K|Toea|en|🇵🇬|UTC+10:00|ID|###|PG|PNG|pg|PNG|PG|598"),
	StringView(),
	StringView("qa:Qatar|قطر / Qatar|QA|Asia|Western Asia|Doha|Qatari Rial|Qatari riyal|QAR|ر.ق|Dirham|ar|🇶🇦|UTC+03:00|SA||QA|QAT|qa|Q|QA|634"),
	StringView(),
	StringView("fm:Federated States of Micronesia||FM|Oceania|Micronesia||US Dollar|United States dollar|USD|$|Cents|en|🇫🇲|UTC+10:00;UTC+11:00||#####|FM|FSM|fm|FSM|FM|583"),
	StringView("by:Belarus|Беларусь|BY|Europe|Eastern Europe|Minsk|Belarus Rubel||BYR||Kapejek|be;ru|🇧🇾|UTC+03:00|LV;LT;PL;RU;UA|######|BY|BLR|by|BY|BY|112"),
	StringView(),
	StringView(),
	StringView(),
	StringView("ci:Ivory Coast|Côte d’Ivoire|CI|Africa|Western Africa|Yamoussoukro|West African Franc|West African CFA franc|XOF|Fr||fr|🇨🇮|UTC|BF;GH;GN;LR;ML||CI|CIV|ci|CI|CI|384"),
	StringView("ru:Russia|Россия / Rossija|RU|Europe|Eastern Europe|Moscow|Russian Rubel|Russian ruble|RUB|₽|Kopeken|ru|🇷🇺|UTC+03:00;UTC+04:00;UTC+06:00;UTC+07:00;UTC+08:00;UTC+09:00;UTC+10:00;UTC+11:00;UTC+12:00|AZ;BY;CN;EE;FI;GE;KZ;KP;LV;LT;MN;NO;PL;UA|######|RU|RUS|ru|RUS|RU|643"),
	StringView(),
	StringView(),
	StringView("ht:Haiti|Ayiti / Haïti|HT|Central America|Caribbean|Port-au-Prince|Gourde|Haitian gourde|HTG|G|Centimes|fr;ht|🇭🇹|UTC-05:00|DO|HT####|HT|HTI|ht|RH|HT|332"),
	StringView(),
	StringView("kg:Kyrgyzstan|Кыргызстан / Kyrgyzstan|KG|Asia|Central Asia|Bishkek|Som|Kyrgyzstani som|KGS|с|Tyiyn|ky;ru|🇰🇬|UTC+06:00|CN;KZ;TJ;UZ|######|KG|KGZ|kg|KS|KG|417"),
	StringView(),
	StringView("cz:Czechia|Česko, Česká republika|CZ|Europe|Central Europe|Prague|Czech Krone|Czech koruna|CZK|Kč|Haleru|cs;sk|🇨🇿|UTC+01:00|AT;DE;PL;SK|### ##|CZ|CZE|cz|CZ|CZ|203"),
	StringView("py:Paraguay|Paraguay|PY|South America|South America|Asunción|Guaraní|Paraguayan guaraní|PYG|₲|Centimos|es;gn|🇵🇾|UTC-04:00|AR;BO;BR|####|PY|PRY|py|PY|PY|600"),
	StringView("mx:Mexico|México|MX|Central America|North America|Mexico City|Mexican Peso|Mexican peso|MXN|$|Centavos|es|🇲🇽|UTC-08:00;UTC-07:00;UTC-06:00|BZ;GT;US|#####|MX|MEX|mx|MEX|MX|484"),
	StringView("bn:Brunei|نݢارا بروني دار السلام|BN|Asia|South-Eastern Asia|Bandar Seri Begawan|Brunei Dollar|Brunei dollar|BND|$|Cents|ms|🇧🇳|UTC+08:00|MY|@@####|BN|BRN|bn|BRU|BN|96"),
	StringView(),
	StringView("kr:South Korea|대한민국 / 大韓民國|KR|Asia|Eastern Asia|Seoul|South Korean Won|South Korean won|KRW|₩|Chon|ko|🇰🇷|UTC+09:00|KP|SEOUL ###-###|KR|KOR|kr|ROK|KR|410"),
	StringView("lb:Lebanon|Lubnan|LB|Asia|Western Asia|Beirut|Lebanese Pound|Lebanese pound|LBP|ل.ل|Piastres|ar;fr|🇱🇧|UTC+02:00|IL;SY|#### ####|####|LB|LBN|lb|RL|LB|422"),
	StringView("sy:Syria|Sūriyya / Suriya / الجمهورية العربية السورية|SY|Asia|Western Asia|Damascus|Syrian Pound|Syrian pound|SYP|£|Piastres|ar|🇸🇾|UTC+02:00|IQ;IL;JO;LB;TR||SY|SYR|sy|SYR|SY|760"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("ki:Kiribati|Kiribati|KI|Oceania|Micronesia|Tarawa|Kiribati Dollar|Kiribati dollar|KID|$|Cents|en|🇰🇮|UTC+12:00;UTC+13:00;UTC+14:00|||KI|KIR|ki|KIR|KI|296"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("fr:France|France|FR|Europe|Western Europe|Paris|Euro|Euro|EUR|€|Cent|fr|🇫🇷|UTC-10:00;UTC-09:30;UTC-09:00;UTC-08:00;UTC-04:00;UTC-03:00;UTC+01:00;UTC+02:00;UTC+03:00;UTC+04:00;UTC+05:00;UTC+10:00;UTC+11:00;UTC+12:00|AD;BE;DE;IT;LU;MC;ES;CH|#####|FR|FRA|fr|F|FR|250"),
	StringView("ls:Lesotho|Lesotho|LS|Africa|Southern Africa|Maseru|Lesotho Loti|Lesotho loti|LSL|L|Lisente|en;st|🇱🇸|UTC+02:00|ZA|###|LS|LSO|ls|LS|LS|426"),
	StringView("km:Comoros|القمر جزر / قمر / Comores|KM|Africa|Eastern Africa|Moroni|Comorian Franc|Comorian franc|KMF|Fr|Centimes|ar;fr|🇰🇲|UTC+03:00|||KM|COM|km|COM|KM|174"),
	StringView("tr:Turkey|Türkiye Cumhuriyeti|TR|Asia|Western Asia|Ankara|Turkish Lira|Turkish lira|TRY|₺|Kurus|tr|🇹🇷|UTC+03:00|AM;AZ;BG;GE;GR;IR;IQ;SY|#####|TR|TUR|tr|TR|TR|792"),
	StringView("va:Vatican City|Vaticanæ / Santa Sede / Città del Vaticano|VA|Europe|Southern Europe|Vatican City|Euro|Euro|EUR|€|Cent|la;it;fr;de|🇻🇦|UTC+01:00|IT||VA|VAT|va|V|VA|336"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("cu:Cuba|Cuba|CU|Central America|Caribbean|Havana|Cuban Peso|Cuban peso|CUP|$|Centavos|es|🇨🇺|UTC-05:00||CP #####|CU|CUB|cu|C|CU|192"),
	StringView(),
	StringView("bm:Bermuda|Bermuda|BM|North America|North America|Hamilton|Bermudian Dollar|Bermudian dollar|BMD|$|Cent|en|🇧🇲|UTC-04:00||@@ ##|BM|BMU|bm||BM|60"),
	StringView("pl:Poland|Polska|PL|Europe|Central Europe|Warsaw|Zloty|Polish złoty|PLN|zł|Groszy|pl|🇵🇱|UTC+01:00|BY;CZ;DE;LT;RU;SK;UA|##-###|PL|POL|pl|PL|PL|616"),
	StringView(),
	StringView("pa:Panama|Panamá|PA|Central America|Central America|Panama City|Panamanian Balboa|Panamanian balboa|PAB|B/.|Centesimos|es|🇵🇦|UTC-05:00|CO;CR||PA|PAN|pa|PA|PA|591"),
	StringView("sk:Slovakia|Slovensko|SK|Europe|Central Europe|Bratislava|Euro|Euro|EUR|€|Cent|sk|🇸🇰|UTC+01:00|AT;CZ;HU;PL;UA|###  ##|SK|SVK|sk|SK|SK|703"),
	StringView("sr:Suriname|Suriname|SR|South America|South America|Paramaribo|Surinam Dollar|Surinamese dollar|SRD|$|Cents|nl|🇸🇷|UTC-03:00|BR;GF;GY||SR|SUR|sr|SME|SR|740"),
	StringView("lc:Saint Lucia|Saint Lucia|LC|Central America|Caribbean|Castries|East Caribbean Dollar|Eastern Caribbean dollar|XCD|$|Cents|en|🇱🇨|UTC-04:00|||LC|LCA|lc|WL|LC|662"),
	StringView("ao:Angola|Ngola|AO|Africa|Middle Africa|Luanda|Kwanza|Angolan kwanza|AOA|Kz|Lwei|pt|🇦🇴|UTC+01:00|CG;CD;ZM;NA||AO|AGO|ao|ANG|AO|24"),
	StringView("tm:Turkmenistan|Türkmenostan|TM|Asia|Central Asia|Ashgabat|Turkmen Manat|Turkmenistan manat|TMT|m|Tenge|tk;ru|🇹🇲|UTC+05:00|AF;IR;KZ;UZ|######|TM|TKM|tm|TM|TM|795"),
	StringView(),
	StringView(),
	StringView("ms:Montserrat|Montserrat|MS|Central America|Caribbean|Brades|East Caribbean Dollar|Eastern Caribbean dollar|XCD|$|Cents|en|🇲🇸|UTC-04:00|||MS|MSR|ms||MS|500"),
	StringView(),
	StringView("tf:French Southern and Antarctic Lands|Terres australes françaises|TF|Oceania||Port-aux-Français|Euro|Euro|EUR|€|Cent|fr|🇹🇫|UTC+05:00|||TF|ATF|tf|||260"),
	StringView("tz:Tanzania|Tanzania|TZ|Africa|Eastern Africa|Dodoma|Tansanian Shilling|Tanzanian shilling|TZS|Sh|Cents|sw;en|🇹🇿|UTC+03:00|BI;CD;KE;MW;MZ;RW;UG;ZM||TZ|TZA|tz|EAT|TZ|834"),
	StringView("vn:Vietnam|Viêt Nam|VN|Asia|South-Eastern Asia|Hanoi|Dong|Vietnamese đồng|VND|₫|Hào|vi|🇻🇳|UTC+07:00|KH;CN;LA|######|VN|VNM|vn|VN|VN|704"),
	StringView(),
	StringView("fj:Fiji|Viti / फ़िजी गणराज्य / Fiji|FJ|Oceania|Melanesia|Suva|Fiji Dollar|Fijian dollar|FJD|$|Cent|en;fj;hif;rtm|🇫🇯|UTC+12:00|||FJ|FJI|fj|FJI|FJ|242"),
	StringView(),
	StringView(),
	StringView("ni:Nicaragua|Nicaragua|NI|Central America|Central America|Managua|Córdoba Oro|Nicaraguan córdoba|NIO|C$|Centavos|es|🇳🇮|UTC-06:00|CR;HN|###-###-#|NI|NIC|ni|NIC|NI|558"),
	StringView(),
	StringView("pn:Pitcairn Islands||PN|Oceania|Polynesia|Adamstown|New Zealand Dollar|New Zealand dollar|NZD|$|Cents|en|🇵🇳|UTC-08:00|||PN|PCN|pn||PN|612"),
	StringView(),
	StringView("om:Oman|´Uman|OM|Asia|Western Asia|Muscat|Omani Rial|Omani rial|OMR|ر.ع.|Baizas|ar|🇴🇲|UTC+04:00|SA;AE;YE|###|OM|OMN|om|OM|OM|512"),
	StringView("pk:Pakistan|Pakistan|PK|Asia|Southern Asia|Islamabad|Pakistanian Rupee|Pakistani rupee|PKR|₨|Paisa|ur;en|🇵🇰|UTC+05:00|AF;CN;IN;IR|#####|PK|PAK|pk|PK|PK|586"),
	StringView(),
	StringView("rs:Serbia|Србија Srbija|RS|Europe|Southeast Europe|Belgrade|Serbian Dinar|Serbian dinar|RSD|дин.|Para|sr|🇷🇸|UTC+01:00|BA;BG;HR;HU;XK;MK;ME;RO|######|RS|SRB|rs|SRB|RS|688"),
	StringView(),
	StringView(),
	StringView(),
	StringView("sv:El Salvador|El Salvador|SV|Central America|Central America|San Salvador|US Dollar|United States dollar|USD|$|Cents|es|🇸🇻|UTC-06:00|GT;HN|CP ####|SV|SLV|sv|ES|SV|222"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("mq:Martinique|Martinique / Matinik / Matnik|MQ|Central America|Caribbean|Fort-de-France|Euro|Euro|EUR|€|Cent|fr|🇲🇶|UTC-04:00||#####|MQ|MTQ|mq||MQ|474"),
	StringView("ws:Samoa|Sāmoa|WS|Oceania|Polynesia|Apia|Tala|Samoan tālā|WST|T|Sene|sm;en|🇼🇸|UTC+13:00|||WS|WSM|ws|WS|WS|882"),
	StringView(),
	StringView("so:Somalia|Soomaaliya / Somalia / الصومال|SO|Africa|Eastern Africa|Mogadishu|Somalian Shilling|Somali shilling|SOS|Sh|Centesimi|so;ar|🇸🇴|UTC+03:00|DJ;ET;KE|@@  #####|SO|SOM|so|SO|SO|706"),
	StringView("bb:Barbados|Barbados|BB|Central America|Caribbean|Bridgetown|Barbadian Dollar|Barbadian dollar|BBD|$|Cents|en|🇧🇧|UTC-04:00||BB#####|BB|BRB|bb|BDS|BB|52"),
	StringView("tt:Trinidad and Tobago|Trinidad and Tobago|TT|Central America|Caribbean|Port-of-Spain|Trinidad and Tobago Dollar|Trinidad and Tobago dollar|TTD|$|Cents|en|🇹🇹|UTC-04:00|||TT|TTO|tt|TT|TT|780"),
	StringView("br:Brazil|Brasil|BR|South America|South America|Brasilia|Brazilian Real|Brazilian real|BRL|R$|Centavos|pt|🇧🇷|UTC-05:00;UTC-04:00;UTC-03:00;UTC-02:00|AR;BO;CO;GF;GY;PY;PE;SR;UY;VE|#####-###|BR|BRA|br|BR|BR|76"),
	StringView("az:Azerbaijan|Azərbaycan|AZ|Asia|Western Asia|Baku|Manat|Azerbaijani manat|AZN|₼|Qäpi|az|🇦🇿|UTC+04:00|AM;GE;IR;RU;TR|AZ ####|AZ|AZE|az|AZ|AZ|31"),
	StringView("mw:Malawi|Malawi|MW|Africa|Eastern Africa|Lilongwe|Malawian Kwacha|Malawian kwacha|MWK|MK|Tambala|en;ny|🇲🇼|UTC+02:00|MZ;TZ;ZM||MW|MWI|mw|MW|MW|454"),
	StringView("mo:Macao|Macau, Aomen|MO|Asia|Eastern Asia|Concelho de Macau|Macanese Pataca|Macanese pataca|MOP|P|Avos|zh;pt|🇲🇴|UTC+08:00|CN||MO|MAC|mo||MO|446"),
	StringView("eh:Western Sahara|الصحراء الغربية / aṣ-Ṣaḥrāʾ al-Ġarbiyya|EH|Africa|Northern Africa|El Aaiún|Moroccan Dirham|Moroccan dirham|MAD|DH|Centimes|es|🇪🇭|UTC+00:00|DZ;MR;MA||EH|ESH|eh|WSA|EH|732"),
	StringView("al:Albania|Republika e Shqipërisë|AL|Europe|Southeast Europe|Tirana|Lek|Albanian lek|ALL|L|Qindarka|sq|🇦🇱|UTC+01:00|ME;GR;MK;XK||AL|ALB|al|AL|AL|8"),
	StringView("xk:Kosovo|Kosova|XK|Europe|Southeast Europe|Pristina|Euro|Euro|EUR|€|Cent|sq;sr|🇽🇰|UTC+01:00|AL;MK;ME;RS||XK|XKX|ko|RKS||0"),
	StringView(),
	StringView(),
	StringView(),
	StringView("hu:Hungary|Magyarország|HU|Europe|Central Europe|Budapest|Hungarian Forint|Hungarian forint|HUF|Ft|Fillér|hu|🇭🇺|UTC+01:00|AT;HR;RO;RS;SK;SI;UA|####|HU|HUN|hu|H|HU|348"),
	StringView("gu:Guam|Guam|GU|Oceania|Micronesia|Hagåtña|US Dollar|United States dollar|USD|$|Cents|en;ch;es|🇬🇺|UTC+10:00||969##|GU|GUM|gu||GU|316"),
	StringView("aq:Antarctica||AQ|Oceania||||||||en;ru|🇦🇶|UTC-03:00;UTC+03:00;UTC+05:00;UTC+06:00;UTC+07:00;UTC+08:00;UTC+10:00;UTC+12:00|||AQ|ATA|aq||AQ|10"),
	StringView("ie:Ireland|Éire / Airlann|IE|Europe|Northern Europe|Dublin|Euro|Euro|EUR|€|Cent|ga;en|🇮🇪|UTC|GB||IE|IRL|ie|IRL|IE|372"),
	StringView(),
	StringView("gr:Greece|Ελλάδα / Elláda|GR|Europe|Southern Europe|Athens|Euro|Euro|EUR|€|Cent|el|🇬🇷|UTC+02:00|AL;BG;TR;MK|### ##|GR|GRC|gr|GR|GR|300"),
	StringView(),
	StringView(),
	StringView("kh:Cambodia|ព្រះរាជាណាចក្រកម្ពុជា / Preăh Réachéanachâk K|KH|Asia|South-Eastern Asia|Phnom Penh|Cambodian Riel|Cambodian riel|KHR|៛|Karak|km|🇰🇭|UTC+07:00|LA;TH;VN|#####|KH|KHM|kh|K|KH|116"),
	StringView(),
	StringView("in:India|Bharat  / भारत गणराज्य|IN|Asia|Southern Asia|New Delhi|Indian Rupee|Indian rupee|INR|₹|Paise|hi;en|🇮🇳|UTC+05:30|BD;BT;MM;CN;NP;PK|######|IN|IND|in|IND|IN|356"),
	StringView("si:Slovenia|Slovenija|SI|Europe|Central Europe|Ljubljana|Euro|Euro|EUR|€|Cent|sl|🇸🇮|UTC+01:00|AT;HR;IT;HU|SI- ####|SI|SVN|si|SLO|SI|705"),
	StringView(),
	StringView("mh:Marshall Islands|Marshall Islands/Majol|MH|Oceania|Micronesia|Majuro|US Dollar|United States dollar|USD|$|Cents|en;mh|🇲🇭|UTC+12:00|||MH|MHL|mh|MH|MH|584"),
	StringView("mr:Mauritania|Muritaniya/Mauritanie|MR|Africa|Western Africa|Nouakchott|Mauritanian Ouguiya||MRO||Khoums|ar|🇲🇷|UTC|DZ;ML;SN;EH||MR|MRT|mr|RIM|MR|478"),
	StringView("it:Italy|Italia|IT|Europe|Southern Europe|Rome|Euro|Euro|EUR|€|Cent|it|🇮🇹|UTC+01:00|AT;FR;SM;SI;CH;VA|#####|IT|ITA|it|I|IT|380"),
	StringView(),
	StringView("mt:Malta|Malta|MT|Europe|Southern Europe|Valletta|Euro|Euro|EUR|€|Cent|mt;en|🇲🇹|UTC+01:00||@@@ ###|@@@ ##|MT|MLT|mt|M|MT|470"),
	StringView("me:Montenegro|Црна Гора / Crna Gora / Mali i Zi|ME|Europe|Southeast Europe|Podgorica|Euro|Euro|EUR|€|Cent|sr;bs;sq;hr|🇲🇪|UTC+01:00|AL;BA;HR;XK;RS|#####|ME|MNE|me|MNE|ME|499"),
	StringView("zw:Zimbabwe|Zimbabwe|ZW|Africa|Southern Africa|Harare|Zimbabwe Dollar|Zimbabwean dollar|ZWL|$|Cents|en;sn;nd|🇿🇼|UTC+02:00|BW;MZ;ZA;ZM||ZW|ZWE|zw|ZW|ZW|716"),
	StringView("sh:Saint Helena, Ascension and Tristan da Cunha|Saint Helena|SH|Africa|Western Africa|Jamestown|St.-Helena Pound|Saint Helena pound|SHP|£|Pence|en|🇸🇭|UTC+00:00||STHL 1ZZ|SH|SHN|sh||SH|654"),
	StringView("mm:Myanmar|Myanma Pye|MM|Asia|South-Eastern Asia|Nay Pyi Taw|Kyat|Burmese kyat|MMK|Ks|Pyas|my|🇲🇲|UTC+06:30|BD;CN;IN;LA;TH|#####|MM|MMR|mm|MYA|MM|104"),
	StringView("ve:Venezuela|Venezuela|VE|South America|South America|Caracas|Bolivar digital||VED||Céntimos|es|🇻🇪|UTC-04:00|BR;CO;GY|####|VE|VEN|ve|YV|VE|862"),
	StringView("se:Sweden|Sverige|SE|Europe|Northern Europe|Stockholm|Swedish Krone|Swedish krona|SEK|kr|Öre|sv|🇸🇪|UTC+01:00|FI;NO|SE-### ##|SE|SWE|se|S|SE|752"),
	StringView(),
	StringView("gn:Guinea|La Guinée|GN|Africa|Western Africa|Conakry|Guinea Franc|Guinean franc|GNF|Fr|Centimes|fr;ff|🇬🇳|UTC|CI;GW;LR;ML;SN;SL||GN|GIN|gn|RG|GN|324"),
	StringView("sj:Svalbard|Svalbard og Jan Mayen|SJ|Europe|Northern Europe|Longyearbyen|Norwegian Krone|krone|NOK|kr|Øre|no|🇸🇯|UTC+01:00|||SJ|SJM|sj||SJ|744"),
	StringView("dz:Algeria|الجزائر al-Dschazā’ir|DZ|Africa|Northern Africa|Algiers|Algerian Dinar|Algerian dinar|DZD|د.ج|Centimes|ar|🇩🇿|UTC+01:00|TN;LY;NE;EH;MR;ML;MA|#####|DZ|DZA|dz|DZ|DZ|12"),
	StringView(),
	StringView(),
	StringView("vc:Saint Vincent and the Grenadines|Saint Vincent and the Grenadines|VC|Central America|Caribbean|Kingstown|East Caribbean Dollar|Eastern Caribbean dollar|XCD|$|Cents|en|🇻🇨|UTC-04:00|||VC|VCT|vc|WV|VC|670"),
	StringView(),
	StringView("fo:Faroe Islands|Føroyar / Færøerne|FO|Europe|Northern Europe|Tórshavn|Faroese Krona|Faroese króna|FOK|kr|Oyra|fo|🇫🇴|UTC+00:00||FO-###|FO|FRO|fo|FO|FO|234"),
	StringView("la:Laos|Lao|LA|Asia|South-Eastern Asia|Vientiane|Kip|Lao kip|LAK|₭|At|lo|🇱🇦|UTC+07:00|MM;KH;CN;TH;VN|#####|LA|LAO|la|LAO|LA|418"),
	StringView(),
	StringView("za:South Africa|Suid-Afrika / South Africa / Sewula Afrika|ZA|Africa|Southern Africa|Pretoria|South African Rand|South African rand|ZAR|R|Cents|af;en;nr;st;ss;tn;ts;ve;xh;zu|🇿🇦|UTC+02:00|BW;LS;MZ;NA;SZ;ZW|####|ZA|ZAF|za|ZA|ZA|710"),
	StringView(),
	StringView("cw:Curacao|Kòrsou|CW|Central America|Caribbean|Willemstad|Caribbean guilder|Caribische gulden|XCG|Cg|Cents|nl;pa;en|🇨🇼|UTC-04:00|||CW|CUW|cw|||531"),
	StringView("gp:Guadeloupe|Guadeloupe / Gwadloup|GP|Central America|Caribbean|Basse-Terre|Euro|Euro|EUR|€|Cent|fr|🇬🇵|UTC-04:00||#####|GP|GLP|gp||GP|312"),
	StringView("ky:Cayman Islands|Cayman Islands|KY|Central America|Caribbean|George Town|Cayman Dollar|Cayman Islands dollar|KYD|$|Cent|en|🇰🇾|UTC-05:00|||KY|CYM|ky||KY|136"),
	StringView("gg:Guernsey|Bailiwick of Guernsey|GG|Europe|Northern Europe|Saint Peter Port|Guernsey Pound|Guernsey pound|GGP|£|Pence|en;fr|🇬🇬|UTC+00:00||@# #@@|@## #@@|@@# #@@|@@## #@@|@#@ #@@|@@#@ #@@|GIR0AA|GG|GGY|gg|GBG||831"),
	StringView("yt:Mayotte|Mayotte|YT|Africa|Eastern Africa|Mamoudzou|Euro|Euro|EUR|€|Cent|fr|🇾🇹|UTC+03:00||#####|YT|MYT|yt||YT|175"),
	StringView(),
	StringView(),
	StringView("sz:Eswatini|Umbuso weSwatini|SZ|Africa|Southern Africa|Mbabane|Swazi Lilangeni|Swazi lilangeni|SZL|L|Cents|en;ss|🇸🇿|UTC+02:00|MZ;ZA|@###|SZ|SWZ|sz|SD|SZ|748"),
	StringView("mc:Principality of Monaco|Monaco|MC|Europe|Western Europe|Monaco|Euro|Euro|EUR|€|Cent|fr|🇲🇨|UTC+01:00|FR|#####|MC|MCO|mc|MC|MC|492"),
	StringView(),
	StringView("ir:Iran|ايران / Īrān|IR|Asia|Southern Asia|Tehran|Iranian Rial|Iranian rial|IRR|﷼|Dinars|fa|🇮🇷|UTC+03:30|AF;AM;AZ;IQ;PK;TR;TM|##########|IR|IRN|ir|IR|IR|364"),
	StringView(),
	StringView("uz:Uzbekistan|Oʻzbekiston|UZ|Asia|Central Asia|Tashkent|Uzbekistan Sum|Uzbekistani soʻm|UZS|so'm|Tiyin|uz;ru|🇺🇿|UTC+05:00|AF;KZ;KG;TJ;TM|######|UZ|UZB|uz|UZ|UZ|860"),
	StringView("lu:Luxembourg|Luxembourg/Lëtzebuerg|LU|Europe|Western Europe|Luxembourg|Euro|Euro|EUR|€|Cent|fr;de;lb|🇱🇺|UTC+01:00|BE;FR;DE|####|LU|LUX|lu|L|LU|442"),
	StringView(),
	StringView(),
	StringView("dj:Djibouti|جيبوتي / Dschībūtī / Djibouti / Jabuuti / Gab|DJ|Africa|Eastern Africa|Djibouti|Djibouti Franc|Djiboutian franc|DJF|Fr|Centimes|fr;ar|🇩🇯|UTC+03:00|ER;ET;SO||DJ|DJI|dj|DJI|DJ|262"),
	StringView("ar:Argentina|Argentina|AR|South America|South America|Buenos Aires|Argentine Peso|Argentine peso|ARS|$|Centavos|es;gn|🇦🇷|UTC-03:00|BO;BR;CL;PY;UY|@####@@@|AR|ARG|ar|RA|AR|32"),
	StringView("co:Colombia|Colombia|CO|South America|South America|Bogota|Colombian Peso|Colombian peso|COP|$|Centavos|es|🇨🇴|UTC-05:00|BR;EC;PA;PE;VE||CO|COL|co|CO|CO|170"),
	StringView(),
	StringView(),
	StringView("lr:Liberia|Liberia|LR|Africa|Western Africa|Monrovia|Liberian Dollar|Liberian dollar|LRD|$|Cents|en|🇱🇷|UTC|GN;CI;SL|####|LR|LBR|lr|LB|LR|430"),
	StringView("gf:French Guiana|Guyane française|GF|South America|South America|Cayenne|Euro|Euro|EUR|€|Cent|fr|🇬🇫|UTC-03:00|BR;SR|#####|GF|GUF|gf||GF|254"),
	StringView("tg:Togo|Togo|TG|Africa|Western Africa|Lomé|West African Franc|West African CFA franc|XOF|Fr||fr|🇹🇬|UTC|BJ;BF;GH||TG|TGO|tg|TG|TG|768"),
	StringView("mk:North Macedonia|Северна Македонија / Maqedonisë së Veriut|MK|Europe|Southeast Europe|Skopje|Denar|denar|MKD|den|Deni|mk|🇲🇰|UTC+01:00|AL;BG;GR;XK;RS|####|MK|MKD|mk|MK|MK|807"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("re:Reunion|Réunion|RE|Africa|Eastern Africa|Saint-Denis|Euro|Euro|EUR|€|Cent|fr|🇷🇪|UTC+04:00||#####|RE|REU|re||RE|638"),
	StringView(),
	StringView("ag:Antigua and Barbuda|Antigua and Barbuda|AG|Central America|Caribbean|Saint John’s|East Caribbean Dollar|Eastern Caribbean dollar|XCD|$|Cents|en|🇦🇬|UTC-04:00|||AG|ATG|ag|AG|AG|28"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("nc:New Caledonia|Nouvelle-Calédonie|NC|Oceania|Melanesia|Nouméa|Pacific Franc|CFP franc|XPF|₣||fr|🇳🇨|UTC+11:00||#####|NC|NCL|nc|NCL|NC|540"),
	StringView("ua:Ukraine|Ukrajina / Україна|UA|Europe|Eastern Europe|Kyiv|Hrywnja|Ukrainian hryvnia|UAH|₴|Kopeken|uk|🇺🇦|UTC+02:00|BY;HU;MD;PL;RO;RU;SK|#####|UA|UKR|ua|UA|UA|804"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("cc:Cocos (Keeling) Islands||CC|Australia|Australia and New Zealand|West Island|Australian Dollar|Australian dollar|AUD|$|Cents|en|🇨🇨|UTC+06:30|||CC|CCK|cc||CC|166"),
	StringView(),
	StringView(),
	StringView("ca:Canada|Canada|CA|North America|North America|Ottawa|Canadian Dollar|Canadian dollar|CAD|$|Cents|en;fr|🇨🇦|UTC-08:00;UTC-07:00;UTC-06:00;UTC-05:00;UTC-04:00;UTC-03:30|US|@#@ #@#|CA|CAN|ca|CDN|CA|124"),
	StringView(),
	StringView(),
	StringView("bg:Bulgaria|България|BG|Europe|Southeast Europe|Sofia|Bulgarian Lev|Bulgarian lev|BGN|лв|Stotinki|bg|🇧🇬|UTC+02:00|GR;MK;RO;RS;TR|####|BG|BGR|bg|BG|BG|100"),
	StringView("vg:British Virgin Islands|British Virgin Islands|VG|Central America|Caribbean|Road Town|US Dollar|United States dollar|USD|$|Cents|en|🇻🇬|UTC-04:00|||VG|VGB|vg|VG|VG|92"),
	StringView(),
	StringView("dm:Dominica|Dominica|DM|Central America|Caribbean|Roseau|East Caribbean Dollar|Eastern Caribbean dollar|XCD|$|Cents|en|🇩🇲|UTC-04:00|||DM|DMA|dm|WD|DM|212"),
	StringView(),
	StringView(),
	StringView(),
	StringView("lv:Latvia|Latvija|LV|Europe|Northern Europe|Riga|Euro|Euro|EUR|€|Cent|lv|🇱🇻|UTC+02:00|BY;EE;LT;RU|LV-####|LV|LVA|lv|LV|LV|428"),
	StringView(),
	StringView("mn:Mongolia|Mongol Uls|MN|Asia|Eastern Asia|Ulaanbaatar|Tugrik|Mongolian tögrög|MNT|₮|Möngö|mn|🇲🇳|UTC+07:00;UTC+08:00|CN;RU|######|MN|MNG|mn|MGL|MN|496"),
	StringView(),
	StringView(),
	StringView("sm:San Marino|San Marino|SM|Europe|Southern Europe|San Marino|Euro|Euro|EUR|€|Cent|it|🇸🇲|UTC+01:00|IT|4789#|SM|SMR|sm|RSM|SM|674"),
	StringView(),
	StringView("st:Sao Tome and Principe|São Tomé e Príncipe|ST|Africa|Middle Africa|São Tomé|Dobra||STD||Centimes|pt|🇸🇹|UTC|||ST|STP|st|STP|ST|678"),
	StringView(),
	StringView(),
	StringView(),
	StringView("at:Austria|Österreich|AT|Europe|Central Europe|Vienna|Euro|Euro|EUR|€|Cent|de|🇦🇹|UTC+01:00|CZ;DE;HU;IT;LI;SK;SI;CH|####|AT|AUT|at|A|AT|40"),
	StringView(),
	StringView(),
	StringView(),
	StringView(),
	StringView("tl:East Timor|Timor Timur|TL|Asia|South-Eastern Asia|Dili|US Dollar|United States dollar|USD|$|Cents|pt|🇹🇱|UTC+09:00|ID||TL|TLS|tl|TL|TL|626"),
	StringView(),
	StringView(),
	StringView("ch:Switzerland|Schweiz / Suisse / Svizzera / Svizra|CH|Europe|Western Europe|Bern|Swiss Franc|Swiss franc|CHF|Fr.|Rappen|de;fr;it;rm|🇨🇭|UTC+01:00|AT;FR;IT;LI;DE|####|CH|CHE|ch|CH|CH|756"),
	StringView(),
	StringView(),
	StringView("mg:Madagascar|Madagasikara/Madagascar|MG|Africa|Eastern Africa|Antananarivo|Malagasy Ariary|Malagasy ariary|MGA|Ar|Iraimbilanja|fr;mg|🇲🇬|UTC+03:00||###|MG|MDG|mg|RM|MG|450"),
	StringView("ne:Niger|Niger|NE|Africa|Western Africa|Niamey|West African Franc|West African CFA franc|XOF|Fr||fr|🇳🇪|UTC+01:00|DZ;BJ;BF;TD;LY;ML;NG|####|NE|NER|ne|RN|NE|562"),
	StringView("tw:Taiwan|中華民國 / T’ai-wan|TW|Asia|Eastern Asia|Taipei|New Taiwan Dollar|New Taiwan dollar|TWD|$|Cents|zh|🇹🇼|UTC+08:00||#####|TW|TWN|tw|RC|TW|158"),
	StringView(),
	StringView(),
	StringView("gd:Grenada|Grenada|GD|Central America|Caribbean|Saint George's|East Caribbean Dollar|Eastern Caribbean dollar|XCD|$|Cents|en|🇬🇩|UTC-04:00|||GD|GRD|gd|WG|GD|308"),
	StringView("li:Liechtenstein|Liechtenstein|LI|Europe|Western Europe|Vaduz|Swiss Franc|Swiss franc|CHF|Fr|Rappen|de|🇱🇮|UTC+01:00|AT;CH|####|LI|LIE|li|FL|LI|438"),
	StringView("ng:Nigeria|Nigeria|NG|Africa|Western Africa|Abuja|Naira|Nigerian naira|NGN|₦|Kobo|en|🇳🇬|UTC+01:00|BJ;CM;TD;NE|######|NG|NGA|ng|NGR|NG|566"),
	StringView("ga:Gabon|Le Gabon|GA|Africa|Middle Africa|Libreville|Central African Franc|Central African CFA franc|XAF|Fr||fr|🇬🇦|UTC+01:00|CM;CG;GQ||GA|GAB|ga|G|GA|266"),
	StringView("mz:Mozambique|Moçambique|MZ|Africa|Eastern Africa|Maputo|Metical|Mozambican metical|MZN|MT|Centavos|pt|🇲🇿|UTC+02:00|MW;ZA;SZ;TZ;ZM;ZW|####|MZ|MOZ|mz|MOC|MZ|508"),
	StringView("pt:Portugal|Portugal|PT|Europe|Southern Europe|Lisbon|Euro|Euro|EUR|€|Cent|pt|🇵🇹|UTC-01:00;UTC|ES|####-###|PT|PRT|pt|P|PT|620"),
	StringView(),
	StringView("et:Ethiopia|ኢትዮጵያ Ityop̣p̣əya|ET|Africa|Eastern Africa|Addis Ababa|Birr|Ethiopian birr|ETB|Br|Cents|am|🇪🇹|UTC+03:00|DJ;ER;KE;SO;SS;SD|####|ET|ETH|et|ETH|ET|231"),
	StringView(),
	StringView(),
	StringView(),
	StringView("vi:Virgin Islands|Virgin Islands of the United States|VI|Central America|Caribbean|Charlotte Amalie|US Dollar|United States dollar|USD|$|Cents|en|🇻🇮|UTC-04:00|||VI|VIR|vi||VI|850"),
	StringView("mv:Maldives|Dhivehi Raajje/Maldives|MV|Asia|Southern Asia|Malé|Maldivian Rufiyaa|Maldivian rufiyaa|MVR|.ރ|Laari|dv|🇲🇻|UTC+05:00||#####|MV|MDV|mv|MV|MV|462"),
	StringView("af:Afghanistan|افغانستان Afghānestān|AF|Asia|Southern Asia|Kabul|Afghani|Afghan afghani|AFN|؋|Puls|ps;uz;tk|🇦🇫|UTC+04:30|IR;PK;TM;UZ;TJ;CN||AF|AFG|af|AFG|AF|4"),
	StringView(),
	StringView(),
	StringView("fi:Finland|Suomi|FI|Europe|Northern Europe|Helsinki|Euro|Euro|EUR|€|Cent|fi;sv|🇫🇮|UTC+02:00|NO;SE;RU|#####|FI|FIN|fi|FIN|FI|246"),
	StringView("aw:Aruba|Aruba|AW|Central America|Caribbean|Oranjestad|Guilder|Aruban florin|AWG|ƒ|Cents|nl;pa|🇦🇼|UTC-04:00|||AW|ABW|aw|ARU|AW|533"),
	StringView("dk:Denmark|Danmark|DK|Europe|Northern Europe|Copenhagen|Danish Krone|Danish krone|DKK|kr|øre|da|🇩🇰|UTC-04:00;UTC-03:00;UTC-01:00;UTC;UTC+01:00|DE|####|DK|DNK|dk|DK|DK|208"),
});

// clang-format on

static uint32_t getSymbolicIndex(StringView str, uint32_t cap) { return str.hash32() & (cap - 1); }

StringView findString(SpanView<StringView> data, StringView key) {
	auto idx = getSymbolicIndex(key, data.size() - 1);
	while (!data[idx].starts_with(key)) { ++idx; }
	return data[idx];
}

LanguageInfo LanguageInfo::get(StringView key) {
	LanguageInfo ret;
	auto str = findString(s_languagesArray, key);
	if (!str.empty()) {
		str.skipUntil<StringView::Chars<':'>>();
		++str;

		uint32_t idx = 0;
		auto target = &ret.name;
		while (!str.empty() && idx < 7) {
			*target = str.readUntil<StringView::Chars<'|'>>();
			if (str.is('|')) {
				++str;
				++idx;
				++target;
			} else {
				break;
			}
		}
	}
	return ret;
}

CountryInfo CountryInfo::get(StringView key) {
	std::array< char, 4> buf = {0};

	::memcpy(buf.data(), key.data(), std::min(size_t(3), key.size()));

	string::apply_tolower_c(buf);

	CountryInfo ret;
	auto str = findString(s_countriesArray, StringView(buf.data(), key.size()));
	if (!str.empty()) {
		str.skipUntil<StringView::Chars<':'>>();
		++str;

		uint32_t idx = 0;
		auto target = &ret.name;
		while (!str.empty() && idx < 21) {
			*target = str.readUntil<StringView::Chars<'|'>>();
			if (str.is('|')) {
				++str;
				++idx;
				++target;
			} else {
				break;
			}
		}
		if (idx == 21) {
			ret.iso3166_1numeric = str.readInteger(10).get(0);
		}
	}
	return ret;
}

LocaleIdentifier::LocaleIdentifier(StringView iloc) noexcept {
	StringView loc = iloc;

	StringView lang;
	StringView terr;
	StringView cp;

	if (!loc.empty()) {
		lang = loc.readChars<StringView::Latin>();
		if (loc.is('-') || loc.is('_')) {
			++loc;
			if (!loc.empty()) {
				terr = loc.readChars<StringView::Latin>();
			}
			valid = true;
		} else {
			log::source().error("LocaleManager", "Invalid locale name: ", iloc);
			valid = false;
			return;
		}
		if (loc.is('.')) {
			++loc;
			cp = loc.readChars<StringView::Alphanumeric, StringView::Chars<'-'>>();
		}
	}

	::memset(data.data(), 0, data.size());

	uint32_t remains = data.size();
	uint32_t offset = 0;
	if (!lang.empty() && remains > lang.size()) {
		::memcpy(data.data() + offset, lang.data(), lang.size());
		language = StringView(data.data() + offset, lang.size());
		offset += lang.size();
		remains -= lang.size();
	} else {
		log::source().error("LocaleManager", "Invalid locale name: ", iloc);
		valid = false;
		return;
	}

	if (remains > 1) {
		data[offset++] = '-';
		--remains;
	} else {
		log::source().error("LocaleManager", "Invalid locale name: ", iloc);
		valid = false;
		return;
	}

	if (!terr.empty() && remains > lang.size()) {
		::memcpy(data.data() + offset, terr.data(), terr.size());
		country = StringView(data.data() + offset, terr.size());
		for (uint32_t i = 0; i < terr.size(); ++i) {
			data[offset + i] = ::tolower(data[offset + i]);
		}
		offset += terr.size();
		remains -= terr.size();
	} else {
		log::source().error("LocaleManager", "Invalid locale name: ", iloc);
		valid = false;
		return;
	}

	id = StringView(data.data(), offset);

	if (!cp.empty()) {
		if (remains > 1) {
			data[offset++] = '.';
			--remains;
		} else {
			log::source().error("LocaleManager", "Invalid locale name: ", iloc);
			valid = false;
			return;
		}

		if (!cp.empty() && remains > cp.size()) {
			::memcpy(data.data() + offset, cp.data(), cp.size());
			codeset = StringView(data.data() + offset, cp.size());
			offset += cp.size();
			remains -= cp.size();
		} else {
			log::source().error("LocaleManager", "Invalid locale name: ", iloc);
			valid = false;
			return;
		}
	}
}

LocaleIdentifier::LocaleIdentifier(const LocaleIdentifier &other) noexcept {
	memcpy(data.data(), other.data.data(), data.size());

	language = StringView(data.data() + (other.language.data() - other.data.data()),
			other.language.size());
	country = StringView(data.data() + (other.country.data() - other.data.data()),
			other.country.size());
	codeset = StringView(data.data() + (other.codeset.data() - other.data.data()),
			other.codeset.size());
	id = StringView(data.data() + (other.id.data() - other.data.data()), other.id.size());
}

LocaleIdentifier &LocaleIdentifier::operator=(const LocaleIdentifier &other) noexcept {
	memcpy(data.data(), other.data.data(), data.size());

	language = StringView(data.data() + (other.language.data() - other.data.data()),
			other.language.size());
	country = StringView(data.data() + (other.country.data() - other.data.data()),
			other.country.size());
	codeset = StringView(data.data() + (other.codeset.data() - other.data.data()),
			other.codeset.size());
	id = StringView(data.data() + (other.id.data() - other.data.data()), other.id.size());
	return *this;
}

LocaleInfo LocaleInfo::get(StringView key) {
	LocaleInfo ret;
	ret.id = LocaleIdentifier(key);
	if (ret.id.valid) {
		ret.country = CountryInfo::get(ret.id.country);
		ret.language = LanguageInfo::get(ret.id.language);
	}
	return ret;
}

LocaleInfo LocaleInfo::get(LocaleIdentifier id) {
	LocaleInfo ret;
	ret.id = id;
	if (ret.id.valid) {
		ret.country = CountryInfo::get(ret.id.country);
		ret.language = LanguageInfo::get(ret.id.language);
	}
	return ret;
}

} // namespace STAPPLER_VERSIONIZED stappler
