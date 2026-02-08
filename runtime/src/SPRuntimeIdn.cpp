/**
 Copyright (c) 2024-2025 Stappler LLC <admin@stappler.dev>
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

#include <sprt/runtime/idn.h>
#include <sprt/runtime/detail/emplace_ordered.h>
#include <sprt/runtime/mem/set.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_ctype.h>

#include "private/SPRTPrivate.h"

namespace sprt::idn {

// from https://data.iana.org/TLD/tlds-alpha-by-domain.txt
static constexpr StringView s_IdnTld[] = {
	"AAA",
	"AARP",
	"ABB",
	"ABBOTT",
	"ABBVIE",
	"ABC",
	"ABLE",
	"ABOGADO",
	"ABUDHABI",
	"AC",
	"ACADEMY",
	"ACCENTURE",
	"ACCOUNTANT",
	"ACCOUNTANTS",
	"ACO",
	"ACTOR",
	"AD",
	"ADS",
	"ADULT",
	"AE",
	"AEG",
	"AERO",
	"AETNA",
	"AF",
	"AFL",
	"AFRICA",
	"AG",
	"AGAKHAN",
	"AGENCY",
	"AI",
	"AIG",
	"AIRBUS",
	"AIRFORCE",
	"AIRTEL",
	"AKDN",
	"AL",
	"ALIBABA",
	"ALIPAY",
	"ALLFINANZ",
	"ALLSTATE",
	"ALLY",
	"ALSACE",
	"ALSTOM",
	"AM",
	"AMAZON",
	"AMERICANEXPRESS",
	"AMERICANFAMILY",
	"AMEX",
	"AMFAM",
	"AMICA",
	"AMSTERDAM",
	"ANALYTICS",
	"ANDROID",
	"ANQUAN",
	"ANZ",
	"AO",
	"AOL",
	"APARTMENTS",
	"APP",
	"APPLE",
	"AQ",
	"AQUARELLE",
	"AR",
	"ARAB",
	"ARAMCO",
	"ARCHI",
	"ARMY",
	"ARPA",
	"ART",
	"ARTE",
	"AS",
	"ASDA",
	"ASIA",
	"ASSOCIATES",
	"AT",
	"ATHLETA",
	"ATTORNEY",
	"AU",
	"AUCTION",
	"AUDI",
	"AUDIBLE",
	"AUDIO",
	"AUSPOST",
	"AUTHOR",
	"AUTO",
	"AUTOS",
	"AW",
	"AWS",
	"AX",
	"AXA",
	"AZ",
	"AZURE",
	"BA",
	"BABY",
	"BAIDU",
	"BANAMEX",
	"BAND",
	"BANK",
	"BAR",
	"BARCELONA",
	"BARCLAYCARD",
	"BARCLAYS",
	"BAREFOOT",
	"BARGAINS",
	"BASEBALL",
	"BASKETBALL",
	"BAUHAUS",
	"BAYERN",
	"BB",
	"BBC",
	"BBT",
	"BBVA",
	"BCG",
	"BCN",
	"BD",
	"BE",
	"BEATS",
	"BEAUTY",
	"BEER",
	"BERLIN",
	"BEST",
	"BESTBUY",
	"BET",
	"BF",
	"BG",
	"BH",
	"BHARTI",
	"BI",
	"BIBLE",
	"BID",
	"BIKE",
	"BING",
	"BINGO",
	"BIO",
	"BIZ",
	"BJ",
	"BLACK",
	"BLACKFRIDAY",
	"BLOCKBUSTER",
	"BLOG",
	"BLOOMBERG",
	"BLUE",
	"BM",
	"BMS",
	"BMW",
	"BN",
	"BNPPARIBAS",
	"BO",
	"BOATS",
	"BOEHRINGER",
	"BOFA",
	"BOM",
	"BOND",
	"BOO",
	"BOOK",
	"BOOKING",
	"BOSCH",
	"BOSTIK",
	"BOSTON",
	"BOT",
	"BOUTIQUE",
	"BOX",
	"BR",
	"BRADESCO",
	"BRIDGESTONE",
	"BROADWAY",
	"BROKER",
	"BROTHER",
	"BRUSSELS",
	"BS",
	"BT",
	"BUILD",
	"BUILDERS",
	"BUSINESS",
	"BUY",
	"BUZZ",
	"BV",
	"BW",
	"BY",
	"BZ",
	"BZH",
	"CA",
	"CAB",
	"CAFE",
	"CAL",
	"CALL",
	"CALVINKLEIN",
	"CAM",
	"CAMERA",
	"CAMP",
	"CANON",
	"CAPETOWN",
	"CAPITAL",
	"CAPITALONE",
	"CAR",
	"CARAVAN",
	"CARDS",
	"CARE",
	"CAREER",
	"CAREERS",
	"CARS",
	"CASA",
	"CASE",
	"CASH",
	"CASINO",
	"CAT",
	"CATERING",
	"CATHOLIC",
	"CBA",
	"CBN",
	"CBRE",
	"CC",
	"CD",
	"CENTER",
	"CEO",
	"CERN",
	"CF",
	"CFA",
	"CFD",
	"CG",
	"CH",
	"CHANEL",
	"CHANNEL",
	"CHARITY",
	"CHASE",
	"CHAT",
	"CHEAP",
	"CHINTAI",
	"CHRISTMAS",
	"CHROME",
	"CHURCH",
	"CI",
	"CIPRIANI",
	"CIRCLE",
	"CISCO",
	"CITADEL",
	"CITI",
	"CITIC",
	"CITY",
	"CK",
	"CL",
	"CLAIMS",
	"CLEANING",
	"CLICK",
	"CLINIC",
	"CLINIQUE",
	"CLOTHING",
	"CLOUD",
	"CLUB",
	"CLUBMED",
	"CM",
	"CN",
	"CO",
	"COACH",
	"CODES",
	"COFFEE",
	"COLLEGE",
	"COLOGNE",
	"COM",
	"COMMBANK",
	"COMMUNITY",
	"COMPANY",
	"COMPARE",
	"COMPUTER",
	"COMSEC",
	"CONDOS",
	"CONSTRUCTION",
	"CONSULTING",
	"CONTACT",
	"CONTRACTORS",
	"COOKING",
	"COOL",
	"COOP",
	"CORSICA",
	"COUNTRY",
	"COUPON",
	"COUPONS",
	"COURSES",
	"CPA",
	"CR",
	"CREDIT",
	"CREDITCARD",
	"CREDITUNION",
	"CRICKET",
	"CROWN",
	"CRS",
	"CRUISE",
	"CRUISES",
	"CU",
	"CUISINELLA",
	"CV",
	"CW",
	"CX",
	"CY",
	"CYMRU",
	"CYOU",
	"CZ",
	"DAD",
	"DANCE",
	"DATA",
	"DATE",
	"DATING",
	"DATSUN",
	"DAY",
	"DCLK",
	"DDS",
	"DE",
	"DEAL",
	"DEALER",
	"DEALS",
	"DEGREE",
	"DELIVERY",
	"DELL",
	"DELOITTE",
	"DELTA",
	"DEMOCRAT",
	"DENTAL",
	"DENTIST",
	"DESI",
	"DESIGN",
	"DEV",
	"DHL",
	"DIAMONDS",
	"DIET",
	"DIGITAL",
	"DIRECT",
	"DIRECTORY",
	"DISCOUNT",
	"DISCOVER",
	"DISH",
	"DIY",
	"DJ",
	"DK",
	"DM",
	"DNP",
	"DO",
	"DOCS",
	"DOCTOR",
	"DOG",
	"DOMAINS",
	"DOT",
	"DOWNLOAD",
	"DRIVE",
	"DTV",
	"DUBAI",
	"DUPONT",
	"DURBAN",
	"DVAG",
	"DVR",
	"DZ",
	"EARTH",
	"EAT",
	"EC",
	"ECO",
	"EDEKA",
	"EDU",
	"EDUCATION",
	"EE",
	"EG",
	"EMAIL",
	"EMERCK",
	"ENERGY",
	"ENGINEER",
	"ENGINEERING",
	"ENTERPRISES",
	"EPSON",
	"EQUIPMENT",
	"ER",
	"ERICSSON",
	"ERNI",
	"ES",
	"ESQ",
	"ESTATE",
	"ET",
	"EU",
	"EUROVISION",
	"EUS",
	"EVENTS",
	"EXCHANGE",
	"EXPERT",
	"EXPOSED",
	"EXPRESS",
	"EXTRASPACE",
	"FAGE",
	"FAIL",
	"FAIRWINDS",
	"FAITH",
	"FAMILY",
	"FAN",
	"FANS",
	"FARM",
	"FARMERS",
	"FASHION",
	"FAST",
	"FEDEX",
	"FEEDBACK",
	"FERRARI",
	"FERRERO",
	"FI",
	"FIDELITY",
	"FIDO",
	"FILM",
	"FINAL",
	"FINANCE",
	"FINANCIAL",
	"FIRE",
	"FIRESTONE",
	"FIRMDALE",
	"FISH",
	"FISHING",
	"FIT",
	"FITNESS",
	"FJ",
	"FK",
	"FLICKR",
	"FLIGHTS",
	"FLIR",
	"FLORIST",
	"FLOWERS",
	"FLY",
	"FM",
	"FO",
	"FOO",
	"FOOD",
	"FOOTBALL",
	"FORD",
	"FOREX",
	"FORSALE",
	"FORUM",
	"FOUNDATION",
	"FOX",
	"FR",
	"FREE",
	"FRESENIUS",
	"FRL",
	"FROGANS",
	"FRONTIER",
	"FTR",
	"FUJITSU",
	"FUN",
	"FUND",
	"FURNITURE",
	"FUTBOL",
	"FYI",
	"GA",
	"GAL",
	"GALLERY",
	"GALLO",
	"GALLUP",
	"GAME",
	"GAMES",
	"GAP",
	"GARDEN",
	"GAY",
	"GB",
	"GBIZ",
	"GD",
	"GDN",
	"GE",
	"GEA",
	"GENT",
	"GENTING",
	"GEORGE",
	"GF",
	"GG",
	"GGEE",
	"GH",
	"GI",
	"GIFT",
	"GIFTS",
	"GIVES",
	"GIVING",
	"GL",
	"GLASS",
	"GLE",
	"GLOBAL",
	"GLOBO",
	"GM",
	"GMAIL",
	"GMBH",
	"GMO",
	"GMX",
	"GN",
	"GODADDY",
	"GOLD",
	"GOLDPOINT",
	"GOLF",
	"GOO",
	"GOODYEAR",
	"GOOG",
	"GOOGLE",
	"GOP",
	"GOT",
	"GOV",
	"GP",
	"GQ",
	"GR",
	"GRAINGER",
	"GRAPHICS",
	"GRATIS",
	"GREEN",
	"GRIPE",
	"GROCERY",
	"GROUP",
	"GS",
	"GT",
	"GU",
	"GUCCI",
	"GUGE",
	"GUIDE",
	"GUITARS",
	"GURU",
	"GW",
	"GY",
	"HAIR",
	"HAMBURG",
	"HANGOUT",
	"HAUS",
	"HBO",
	"HDFC",
	"HDFCBANK",
	"HEALTH",
	"HEALTHCARE",
	"HELP",
	"HELSINKI",
	"HERE",
	"HERMES",
	"HIPHOP",
	"HISAMITSU",
	"HITACHI",
	"HIV",
	"HK",
	"HKT",
	"HM",
	"HN",
	"HOCKEY",
	"HOLDINGS",
	"HOLIDAY",
	"HOMEDEPOT",
	"HOMEGOODS",
	"HOMES",
	"HOMESENSE",
	"HONDA",
	"HORSE",
	"HOSPITAL",
	"HOST",
	"HOSTING",
	"HOT",
	"HOTELS",
	"HOTMAIL",
	"HOUSE",
	"HOW",
	"HR",
	"HSBC",
	"HT",
	"HU",
	"HUGHES",
	"HYATT",
	"HYUNDAI",
	"IBM",
	"ICBC",
	"ICE",
	"ICU",
	"ID",
	"IE",
	"IEEE",
	"IFM",
	"IKANO",
	"IL",
	"IM",
	"IMAMAT",
	"IMDB",
	"IMMO",
	"IMMOBILIEN",
	"IN",
	"INC",
	"INDUSTRIES",
	"INFINITI",
	"INFO",
	"ING",
	"INK",
	"INSTITUTE",
	"INSURANCE",
	"INSURE",
	"INT",
	"INTERNATIONAL",
	"INTUIT",
	"INVESTMENTS",
	"IO",
	"IPIRANGA",
	"IQ",
	"IR",
	"IRISH",
	"IS",
	"ISMAILI",
	"IST",
	"ISTANBUL",
	"IT",
	"ITAU",
	"ITV",
	"JAGUAR",
	"JAVA",
	"JCB",
	"JE",
	"JEEP",
	"JETZT",
	"JEWELRY",
	"JIO",
	"JLL",
	"JM",
	"JMP",
	"JNJ",
	"JO",
	"JOBS",
	"JOBURG",
	"JOT",
	"JOY",
	"JP",
	"JPMORGAN",
	"JPRS",
	"JUEGOS",
	"JUNIPER",
	"KAUFEN",
	"KDDI",
	"KE",
	"KERRYHOTELS",
	"KERRYPROPERTIES",
	"KFH",
	"KG",
	"KH",
	"KI",
	"KIA",
	"KIDS",
	"KIM",
	"KINDLE",
	"KITCHEN",
	"KIWI",
	"KM",
	"KN",
	"KOELN",
	"KOMATSU",
	"KOSHER",
	"KP",
	"KPMG",
	"KPN",
	"KR",
	"KRD",
	"KRED",
	"KUOKGROUP",
	"KW",
	"KY",
	"KYOTO",
	"KZ",
	"LA",
	"LACAIXA",
	"LAMBORGHINI",
	"LAMER",
	"LAND",
	"LANDROVER",
	"LANXESS",
	"LASALLE",
	"LAT",
	"LATINO",
	"LATROBE",
	"LAW",
	"LAWYER",
	"LB",
	"LC",
	"LDS",
	"LEASE",
	"LECLERC",
	"LEFRAK",
	"LEGAL",
	"LEGO",
	"LEXUS",
	"LGBT",
	"LI",
	"LIDL",
	"LIFE",
	"LIFEINSURANCE",
	"LIFESTYLE",
	"LIGHTING",
	"LIKE",
	"LILLY",
	"LIMITED",
	"LIMO",
	"LINCOLN",
	"LINK",
	"LIVE",
	"LIVING",
	"LK",
	"LLC",
	"LLP",
	"LOAN",
	"LOANS",
	"LOCKER",
	"LOCUS",
	"LOL",
	"LONDON",
	"LOTTE",
	"LOTTO",
	"LOVE",
	"LPL",
	"LPLFINANCIAL",
	"LR",
	"LS",
	"LT",
	"LTD",
	"LTDA",
	"LU",
	"LUNDBECK",
	"LUXE",
	"LUXURY",
	"LV",
	"LY",
	"MA",
	"MADRID",
	"MAIF",
	"MAISON",
	"MAKEUP",
	"MAN",
	"MANAGEMENT",
	"MANGO",
	"MAP",
	"MARKET",
	"MARKETING",
	"MARKETS",
	"MARRIOTT",
	"MARSHALLS",
	"MATTEL",
	"MBA",
	"MC",
	"MCKINSEY",
	"MD",
	"ME",
	"MED",
	"MEDIA",
	"MEET",
	"MELBOURNE",
	"MEME",
	"MEMORIAL",
	"MEN",
	"MENU",
	"MERCKMSD",
	"MG",
	"MH",
	"MIAMI",
	"MICROSOFT",
	"MIL",
	"MINI",
	"MINT",
	"MIT",
	"MITSUBISHI",
	"MK",
	"ML",
	"MLB",
	"MLS",
	"MM",
	"MMA",
	"MN",
	"MO",
	"MOBI",
	"MOBILE",
	"MODA",
	"MOE",
	"MOI",
	"MOM",
	"MONASH",
	"MONEY",
	"MONSTER",
	"MORMON",
	"MORTGAGE",
	"MOSCOW",
	"MOTO",
	"MOTORCYCLES",
	"MOV",
	"MOVIE",
	"MP",
	"MQ",
	"MR",
	"MS",
	"MSD",
	"MT",
	"MTN",
	"MTR",
	"MU",
	"MUSEUM",
	"MUSIC",
	"MV",
	"MW",
	"MX",
	"MY",
	"MZ",
	"NA",
	"NAB",
	"NAGOYA",
	"NAME",
	"NAVY",
	"NBA",
	"NC",
	"NE",
	"NEC",
	"NET",
	"NETBANK",
	"NETFLIX",
	"NETWORK",
	"NEUSTAR",
	"NEW",
	"NEWS",
	"NEXT",
	"NEXTDIRECT",
	"NEXUS",
	"NF",
	"NFL",
	"NG",
	"NGO",
	"NHK",
	"NI",
	"NICO",
	"NIKE",
	"NIKON",
	"NINJA",
	"NISSAN",
	"NISSAY",
	"NL",
	"NO",
	"NOKIA",
	"NORTON",
	"NOW",
	"NOWRUZ",
	"NOWTV",
	"NP",
	"NR",
	"NRA",
	"NRW",
	"NTT",
	"NU",
	"NYC",
	"NZ",
	"OBI",
	"OBSERVER",
	"OFFICE",
	"OKINAWA",
	"OLAYAN",
	"OLAYANGROUP",
	"OLLO",
	"OM",
	"OMEGA",
	"ONE",
	"ONG",
	"ONL",
	"ONLINE",
	"OOO",
	"OPEN",
	"ORACLE",
	"ORANGE",
	"ORG",
	"ORGANIC",
	"ORIGINS",
	"OSAKA",
	"OTSUKA",
	"OTT",
	"OVH",
	"PA",
	"PAGE",
	"PANASONIC",
	"PARIS",
	"PARS",
	"PARTNERS",
	"PARTS",
	"PARTY",
	"PAY",
	"PCCW",
	"PE",
	"PET",
	"PF",
	"PFIZER",
	"PG",
	"PH",
	"PHARMACY",
	"PHD",
	"PHILIPS",
	"PHONE",
	"PHOTO",
	"PHOTOGRAPHY",
	"PHOTOS",
	"PHYSIO",
	"PICS",
	"PICTET",
	"PICTURES",
	"PID",
	"PIN",
	"PING",
	"PINK",
	"PIONEER",
	"PIZZA",
	"PK",
	"PL",
	"PLACE",
	"PLAY",
	"PLAYSTATION",
	"PLUMBING",
	"PLUS",
	"PM",
	"PN",
	"PNC",
	"POHL",
	"POKER",
	"POLITIE",
	"PORN",
	"POST",
	"PR",
	"PRAXI",
	"PRESS",
	"PRIME",
	"PRO",
	"PROD",
	"PRODUCTIONS",
	"PROF",
	"PROGRESSIVE",
	"PROMO",
	"PROPERTIES",
	"PROPERTY",
	"PROTECTION",
	"PRU",
	"PRUDENTIAL",
	"PS",
	"PT",
	"PUB",
	"PW",
	"PWC",
	"PY",
	"QA",
	"QPON",
	"QUEBEC",
	"QUEST",
	"RACING",
	"RADIO",
	"RE",
	"READ",
	"REALESTATE",
	"REALTOR",
	"REALTY",
	"RECIPES",
	"RED",
	"REDUMBRELLA",
	"REHAB",
	"REISE",
	"REISEN",
	"REIT",
	"RELIANCE",
	"REN",
	"RENT",
	"RENTALS",
	"REPAIR",
	"REPORT",
	"REPUBLICAN",
	"REST",
	"RESTAURANT",
	"REVIEW",
	"REVIEWS",
	"REXROTH",
	"RICH",
	"RICHARDLI",
	"RICOH",
	"RIL",
	"RIO",
	"RIP",
	"RO",
	"ROCKS",
	"RODEO",
	"ROGERS",
	"ROOM",
	"RS",
	"RSVP",
	"RU",
	"RUGBY",
	"RUHR",
	"RUN",
	"RW",
	"RWE",
	"RYUKYU",
	"SA",
	"SAARLAND",
	"SAFE",
	"SAFETY",
	"SAKURA",
	"SALE",
	"SALON",
	"SAMSCLUB",
	"SAMSUNG",
	"SANDVIK",
	"SANDVIKCOROMANT",
	"SANOFI",
	"SAP",
	"SARL",
	"SAS",
	"SAVE",
	"SAXO",
	"SB",
	"SBI",
	"SBS",
	"SC",
	"SCB",
	"SCHAEFFLER",
	"SCHMIDT",
	"SCHOLARSHIPS",
	"SCHOOL",
	"SCHULE",
	"SCHWARZ",
	"SCIENCE",
	"SCOT",
	"SD",
	"SE",
	"SEARCH",
	"SEAT",
	"SECURE",
	"SECURITY",
	"SEEK",
	"SELECT",
	"SENER",
	"SERVICES",
	"SEVEN",
	"SEW",
	"SEX",
	"SEXY",
	"SFR",
	"SG",
	"SH",
	"SHANGRILA",
	"SHARP",
	"SHELL",
	"SHIA",
	"SHIKSHA",
	"SHOES",
	"SHOP",
	"SHOPPING",
	"SHOUJI",
	"SHOW",
	"SI",
	"SILK",
	"SINA",
	"SINGLES",
	"SITE",
	"SJ",
	"SK",
	"SKI",
	"SKIN",
	"SKY",
	"SKYPE",
	"SL",
	"SLING",
	"SM",
	"SMART",
	"SMILE",
	"SN",
	"SNCF",
	"SO",
	"SOCCER",
	"SOCIAL",
	"SOFTBANK",
	"SOFTWARE",
	"SOHU",
	"SOLAR",
	"SOLUTIONS",
	"SONG",
	"SONY",
	"SOY",
	"SPA",
	"SPACE",
	"SPORT",
	"SPOT",
	"SR",
	"SRL",
	"SS",
	"ST",
	"STADA",
	"STAPLES",
	"STAR",
	"STATEBANK",
	"STATEFARM",
	"STC",
	"STCGROUP",
	"STOCKHOLM",
	"STORAGE",
	"STORE",
	"STREAM",
	"STUDIO",
	"STUDY",
	"STYLE",
	"SU",
	"SUCKS",
	"SUPPLIES",
	"SUPPLY",
	"SUPPORT",
	"SURF",
	"SURGERY",
	"SUZUKI",
	"SV",
	"SWATCH",
	"SWISS",
	"SX",
	"SY",
	"SYDNEY",
	"SYSTEMS",
	"SZ",
	"TAB",
	"TAIPEI",
	"TALK",
	"TAOBAO",
	"TARGET",
	"TATAMOTORS",
	"TATAR",
	"TATTOO",
	"TAX",
	"TAXI",
	"TC",
	"TCI",
	"TD",
	"TDK",
	"TEAM",
	"TECH",
	"TECHNOLOGY",
	"TEL",
	"TEMASEK",
	"TENNIS",
	"TEVA",
	"TF",
	"TG",
	"TH",
	"THD",
	"THEATER",
	"THEATRE",
	"TIAA",
	"TICKETS",
	"TIENDA",
	"TIPS",
	"TIRES",
	"TIROL",
	"TJ",
	"TJMAXX",
	"TJX",
	"TK",
	"TKMAXX",
	"TL",
	"TM",
	"TMALL",
	"TN",
	"TO",
	"TODAY",
	"TOKYO",
	"TOOLS",
	"TOP",
	"TORAY",
	"TOSHIBA",
	"TOTAL",
	"TOURS",
	"TOWN",
	"TOYOTA",
	"TOYS",
	"TR",
	"TRADE",
	"TRADING",
	"TRAINING",
	"TRAVEL",
	"TRAVELERS",
	"TRAVELERSINSURANCE",
	"TRUST",
	"TRV",
	"TT",
	"TUBE",
	"TUI",
	"TUNES",
	"TUSHU",
	"TV",
	"TVS",
	"TW",
	"TZ",
	"UA",
	"UBANK",
	"UBS",
	"UG",
	"UK",
	"UNICOM",
	"UNIVERSITY",
	"UNO",
	"UOL",
	"UPS",
	"US",
	"UY",
	"UZ",
	"VA",
	"VACATIONS",
	"VANA",
	"VANGUARD",
	"VC",
	"VE",
	"VEGAS",
	"VENTURES",
	"VERISIGN",
	"VERSICHERUNG",
	"VET",
	"VG",
	"VI",
	"VIAJES",
	"VIDEO",
	"VIG",
	"VIKING",
	"VILLAS",
	"VIN",
	"VIP",
	"VIRGIN",
	"VISA",
	"VISION",
	"VIVA",
	"VIVO",
	"VLAANDEREN",
	"VN",
	"VODKA",
	"VOLVO",
	"VOTE",
	"VOTING",
	"VOTO",
	"VOYAGE",
	"VU",
	"WALES",
	"WALMART",
	"WALTER",
	"WANG",
	"WANGGOU",
	"WATCH",
	"WATCHES",
	"WEATHER",
	"WEATHERCHANNEL",
	"WEBCAM",
	"WEBER",
	"WEBSITE",
	"WED",
	"WEDDING",
	"WEIBO",
	"WEIR",
	"WF",
	"WHOSWHO",
	"WIEN",
	"WIKI",
	"WILLIAMHILL",
	"WIN",
	"WINDOWS",
	"WINE",
	"WINNERS",
	"WME",
	"WOLTERSKLUWER",
	"WOODSIDE",
	"WORK",
	"WORKS",
	"WORLD",
	"WOW",
	"WS",
	"WTC",
	"WTF",
	"XBOX",
	"XEROX",
	"XIHUAN",
	"XIN",
	"XN--11B4C3D",
	"XN--1CK2E1B",
	"XN--1QQW23A",
	"XN--2SCRJ9C",
	"XN--30RR7Y",
	"XN--3BST00M",
	"XN--3DS443G",
	"XN--3E0B707E",
	"XN--3HCRJ9C",
	"XN--3PXU8K",
	"XN--42C2D9A",
	"XN--45BR5CYL",
	"XN--45BRJ9C",
	"XN--45Q11C",
	"XN--4DBRK0CE",
	"XN--4GBRIM",
	"XN--54B7FTA0CC",
	"XN--55QW42G",
	"XN--55QX5D",
	"XN--5SU34J936BGSG",
	"XN--5TZM5G",
	"XN--6FRZ82G",
	"XN--6QQ986B3XL",
	"XN--80ADXHKS",
	"XN--80AO21A",
	"XN--80AQECDR1A",
	"XN--80ASEHDB",
	"XN--80ASWG",
	"XN--8Y0A063A",
	"XN--90A3AC",
	"XN--90AE",
	"XN--90AIS",
	"XN--9DBQ2A",
	"XN--9ET52U",
	"XN--9KRT00A",
	"XN--B4W605FERD",
	"XN--BCK1B9A5DRE4C",
	"XN--C1AVG",
	"XN--C2BR7G",
	"XN--CCK2B3B",
	"XN--CCKWCXETD",
	"XN--CG4BKI",
	"XN--CLCHC0EA0B2G2A9GCD",
	"XN--CZR694B",
	"XN--CZRS0T",
	"XN--CZRU2D",
	"XN--D1ACJ3B",
	"XN--D1ALF",
	"XN--E1A4C",
	"XN--ECKVDTC9D",
	"XN--EFVY88H",
	"XN--FCT429K",
	"XN--FHBEI",
	"XN--FIQ228C5HS",
	"XN--FIQ64B",
	"XN--FIQS8S",
	"XN--FIQZ9S",
	"XN--FJQ720A",
	"XN--FLW351E",
	"XN--FPCRJ9C3D",
	"XN--FZC2C9E2C",
	"XN--FZYS8D69UVGM",
	"XN--G2XX48C",
	"XN--GCKR3F0F",
	"XN--GECRJ9C",
	"XN--GK3AT1E",
	"XN--H2BREG3EVE",
	"XN--H2BRJ9C",
	"XN--H2BRJ9C8C",
	"XN--HXT814E",
	"XN--I1B6B1A6A2E",
	"XN--IMR513N",
	"XN--IO0A7I",
	"XN--J1AEF",
	"XN--J1AMH",
	"XN--J6W193G",
	"XN--JLQ480N2RG",
	"XN--JVR189M",
	"XN--KCRX77D1X4A",
	"XN--KPRW13D",
	"XN--KPRY57D",
	"XN--KPUT3I",
	"XN--L1ACC",
	"XN--LGBBAT1AD8J",
	"XN--MGB9AWBF",
	"XN--MGBA3A3EJT",
	"XN--MGBA3A4F16A",
	"XN--MGBA7C0BBN0A",
	"XN--MGBAAM7A8H",
	"XN--MGBAB2BD",
	"XN--MGBAH1A3HJKRD",
	"XN--MGBAI9AZGQP6J",
	"XN--MGBAYH7GPA",
	"XN--MGBBH1A",
	"XN--MGBBH1A71E",
	"XN--MGBC0A9AZCG",
	"XN--MGBCA7DZDO",
	"XN--MGBCPQ6GPA1A",
	"XN--MGBERP4A5D4AR",
	"XN--MGBGU82A",
	"XN--MGBI4ECEXP",
	"XN--MGBPL2FH",
	"XN--MGBT3DHD",
	"XN--MGBTX2B",
	"XN--MGBX4CD0AB",
	"XN--MIX891F",
	"XN--MK1BU44C",
	"XN--MXTQ1M",
	"XN--NGBC5AZD",
	"XN--NGBE9E0A",
	"XN--NGBRX",
	"XN--NODE",
	"XN--NQV7F",
	"XN--NQV7FS00EMA",
	"XN--NYQY26A",
	"XN--O3CW4H",
	"XN--OGBPF8FL",
	"XN--OTU796D",
	"XN--P1ACF",
	"XN--P1AI",
	"XN--PGBS0DH",
	"XN--PSSY2U",
	"XN--Q7CE6A",
	"XN--Q9JYB4C",
	"XN--QCKA1PMC",
	"XN--QXA6A",
	"XN--QXAM",
	"XN--RHQV96G",
	"XN--ROVU88B",
	"XN--RVC1E0AM3E",
	"XN--S9BRJ9C",
	"XN--SES554G",
	"XN--T60B56A",
	"XN--TCKWE",
	"XN--TIQ49XQYJ",
	"XN--UNUP4Y",
	"XN--VERMGENSBERATER-CTB",
	"XN--VERMGENSBERATUNG-PWB",
	"XN--VHQUV",
	"XN--VUQ861B",
	"XN--W4R85EL8FHU5DNRA",
	"XN--W4RS40L",
	"XN--WGBH1C",
	"XN--WGBL6A",
	"XN--XHQ521B",
	"XN--XKC2AL3HYE2A",
	"XN--XKC2DL3A5EE0H",
	"XN--Y9A3AQ",
	"XN--YFRO4I67O",
	"XN--YGBI2AMMX",
	"XN--ZFR164B",
	"XXX",
	"XYZ",
	"YACHTS",
	"YAHOO",
	"YAMAXUN",
	"YANDEX",
	"YE",
	"YODOBASHI",
	"YOGA",
	"YOKOHAMA",
	"YOU",
	"YOUTUBE",
	"YT",
	"YUN",
	"ZA",
	"ZAPPOS",
	"ZARA",
	"ZERO",
	"ZIP",
	"ZM",
	"ZONE",
	"ZUERICH",
	"ZW",
};

struct TldList {
	SpanView<StringView> values;

	TldList() : values(s_IdnTld) { }

	bool has(StringView val) {
		auto buf = __sprt_typed_malloca(char, val.size() + 1);
		__sprt_memcpy(buf, val.data(), val.size());
		buf[val.size()] = 0;

		for (size_t i = 0; i < val.size(); ++i) { buf[i] = __sprt_toupper(buf[i]); }

		auto ret = exists_ordered(values, StringView(buf, val.size()));

		__sprt_freea(buf);

		return ret;
	}
};

/* punycode parameters, see http://tools.ietf.org/html/rfc3492#section-5 */
static constexpr auto BASE = 36;
static constexpr auto TMIN = 1;
static constexpr auto TMAX = 26;
static constexpr auto SKEW = 38;
static constexpr auto DAMP = 700;
static constexpr uint32_t MAXINT = 0xFFFF'FFFF;
static constexpr auto INITIAL_N = 128;
static constexpr auto INITIAL_BIAS = 72;

static uint32_t adapt_bias(uint32_t delta, unsigned n_points, int is_first) {
	uint32_t k;

	delta /= is_first ? DAMP : 2;
	delta += delta / n_points;

	/* while delta > 455: delta /= 35 */
	for (k = 0; delta > ((BASE - TMIN) * TMAX) / 2; k += BASE) { delta /= (BASE - TMIN); }

	return k + (((BASE - TMIN + 1) * delta) / (delta + SKEW));
}

static char encode_digit(size_t c) {
	assert(c >= 0 && c <= BASE - TMIN);
	if (c > 25) {
		return c + 22; /* '0'..'9' */
	} else {
		return c + 'a'; /* 'a'..'z' */
	}
}

/* Encode as a generalized variable-length integer. Returns number of bytes written. */
template <typename Callback>
requires requires(const Callback &cb, char c) { cb(c); }
static size_t encode_var_int(const size_t bias, const size_t delta, const Callback &cb) {
	size_t i, k, q, t;

	i = 0;
	k = BASE;
	q = delta;

	while (true) {
		if (k <= bias) {
			t = TMIN;
		} else if (k >= bias + TMAX) {
			t = TMAX;
		} else {
			t = k - bias;
		}

		if (q < t) {
			break;
		}

		cb(encode_digit(t + (q - t) % (BASE - t)));

		q = (q - t) / (BASE - t);
		k += BASE;
	}

	cb(encode_digit(q));

	return i;
}

template <typename Callback>
requires requires(const Callback &cb, char c) { cb(c); }
static bool punycode_encode(const char32_t *const src, const size_t srclen, const Callback &cb) {
	uint32_t b, h;
	uint32_t delta, bias;
	uint32_t m, n;
	uint32_t si, di;

	for (si = 0, di = 0; si < srclen; si++) {
		if (src[si] < 128) {
			++di;
			cb(char(src[si]));
		}
	}

	b = h = di;

	/* Write out delimiter if any basic code points were processed. */
	if (di > 0) {
		cb('-');
	}

	n = INITIAL_N;
	bias = INITIAL_BIAS;
	delta = 0;

	for (; h < srclen; n++, delta++) {
		/* Find next smallest non-basic code point. */
		for (m = MAXINT, si = 0; si < srclen; si++) {
			if (src[si] >= n && src[si] < m) {
				m = src[si];
			}
		}

		if ((m - n) > (MAXINT - delta) / (h + 1)) {
			return false;
		}

		delta += (m - n) * (h + 1);
		n = m;

		for (si = 0; si < srclen; si++) {
			if (src[si] < n) {
				if (++delta == 0) {
					return false;
				}
			} else if (src[si] == n) {
				di += encode_var_int(bias, delta, cb);
				bias = adapt_bias(delta, h + 1, h == b);
				delta = 0;
				h++;
			}
		}
	}

	return true;
}

static int is_basic(unsigned int a) { return (a < 0x80) ? 1 : 0; }

static int digit_decoded(const unsigned char a) {
	if (a >= 0x41 && a <= 0x5A) {
		return a - 0x41;
	}

	if (a >= 0x61 && a <= 0x7A) {
		return a - 0x61;
	}

	if (a >= 0x30 && a <= 0x39) {
		return a - 0x30 + 26;
	}

	return -1;
}

static int adapt(uint32_t delta, uint32_t numpoints, uint32_t firsttime) {
	uint32_t k = 0;

	delta = (firsttime) ? delta / DAMP : delta / 2;
	delta = delta + delta / numpoints;

	while (delta > ((BASE - TMIN) * TMAX) / 2) {
		delta = delta / (BASE - TMIN);
		k = k + BASE;
	}

	return k + (((BASE - TMIN + 1) * delta) / (delta + SKEW));
}

static bool punycode_decode(const char *const pEncoded, const size_t enc_len,
		char32_t *const pDecoded, size_t *pout_length) {
	uint32_t n = INITIAL_N;
	uint32_t i = 0;
	uint32_t bias = INITIAL_BIAS;
	uint32_t processed_in = 0, written_out = 0;
	uint32_t max_out = pDecoded ? uint32_t(*pout_length) : Max<uint32_t>;
	uint32_t basic_count = 0;
	uint32_t loop;

	for (loop = 0; loop < enc_len; loop++) {
		if (pEncoded[loop] == '-') {
			basic_count = loop;
		}
	}

	if (basic_count > 0) {
		if (basic_count > max_out) {
			return false;
		}

		for (loop = 0; loop < basic_count; loop++) {
			if (is_basic(pEncoded[loop]) == 0) {
				return false;
			}

			if (pDecoded) {
				pDecoded[loop] = pEncoded[loop];
			}
			written_out++;
		}
		processed_in = basic_count + 1;
	}

	for (loop = processed_in; loop < enc_len;) {
		uint32_t oldi = i;
		uint32_t w = 1;
		uint32_t k, t;
		int digit;

		for (k = BASE;; k += BASE) {
			if (loop >= enc_len) {
				return false;
			}

			digit = digit_decoded(pEncoded[loop]);
			loop++;

			if (digit < 0) {
				return false;
			}

			if ((uint32_t)digit > (MAXINT - i) / w) {
				return false;
			}

			i = i + digit * w;
			t = (k <= bias) ? TMIN : (k >= bias + TMAX) ? TMAX : k - bias;

			if ((uint32_t)digit < t) {
				break;
			}

			if (w > MAXINT / (BASE - t)) {
				return false;
			}
			w = w * (BASE - t);
		}

		bias = adapt(i - oldi, written_out + 1, (oldi == 0));
		if (i / (written_out + 1) > MAXINT - n) {
			return 0;
		}
		n = n + i / (written_out + 1);
		i %= (written_out + 1);

		if (written_out >= max_out) {
			return 0;
		}

		if (pDecoded) {
			__sprt_memmove(pDecoded + i + 1, pDecoded + i, (written_out - i) * sizeof(*pDecoded));
			pDecoded[i] = n;
		}
		i++;
		written_out++;
	}

	*pout_length = written_out;
	return true;
}

bool puny_encode(const callback<void(StringView)> &cb, StringView source, bool makeUrlPrefix) {
	StringViewUtf8 utfSource(source);
	auto uCodeSize = utfSource.code_size();

	auto buf = __sprt_typed_malloca(char32_t, uCodeSize + 1);
	auto target = buf;

	utfSource.foreach ([&](char32_t ch) { *(target++) = ch; });

	bool result = false;
	size_t retSize = 0;
	if (punycode_encode(buf, target - buf, [&](char ch) { ++retSize; })) {
		auto rbuf = __sprt_typed_malloca(char, retSize + 1 + (makeUrlPrefix ? 4 : 0));
		auto rtarget = rbuf;

		if (makeUrlPrefix) {
			__sprt_memcpy(rtarget, "xn--", 4);
			rtarget += 4;
		}

		punycode_encode(buf, target - buf, [&](char ch) { *(rtarget++) = ch; });

		*rtarget = 0;

		cb(StringView(rbuf, rtarget - rbuf));

		__sprt_freea(rbuf);

		result = true;
	}

	__sprt_freea(buf);
	return result;
}

bool puny_decode(const callback<void(StringView)> &cb, StringView source, bool prefixed) {
	if (prefixed) {
		if (source.starts_with<StringCaseComparator>("xn--")) {
			source += 4;
		}
	}

	size_t uCodeSize = 0;
	if (!punycode_decode(source.data(), source.size(), nullptr, &uCodeSize)) {
		return false;
	}

	auto buf = __sprt_typed_malloca(char32_t, uCodeSize + 1);

	if (!punycode_decode(source.data(), source.size(), buf, &uCodeSize)) {
		__sprt_freea(buf);
		return false;
	}

	size_t retSize = 0;
	auto view = SpanView(buf, uCodeSize);
	for (auto &it : view) { retSize += unicode::utf8EncodeLength(it); }

	auto rbuf = __sprt_typed_malloca(char, retSize + 1);
	auto rtarget = rbuf;
	size_t rBufSize = retSize;

	for (auto &it : view) {
		auto r = unicode::utf8EncodeBuf(rtarget, rBufSize, it);
		rtarget += r;
		rBufSize -= r;
	}

	*rtarget = 0;

	cb(StringView(rbuf, rtarget - rbuf));

	__sprt_freea(rbuf);
	__sprt_freea(buf);
	return true;
}

static TldList s_tldList;

bool is_known_tld(StringView data) {
	if (sprt::detail::caseCompare_c(StringView("XN--"), data.sub(0, 4)) == 0) {
		return s_tldList.has(data);
	} else {
		auto tmp = data;
		tmp.skipChars<StringView::Alphanumeric>();
		if (tmp.empty()) {
			return s_tldList.has(data);
		}

		bool result = false;
		puny_encode([&](StringView str) {
			result = s_tldList.has(str); //
		}, data, true);
		return result;
	}
}

using HostUnicodeChars = chars::Compose<char, chars::CharGroup<char, CharGroupId::Alphanumeric>,
		chars::Chars<char, '.', '-'>, chars::Range<char, char(128), char(255)>>;

using HostAsciiChars = chars::Compose<char, chars::CharGroup<char, CharGroupId::Alphanumeric>,
		chars::Chars<char, '.', '-'>>;

bool to_ascii(const callback<void(StringView)> &cb, StringView source, bool validate) {
	if (source.empty()) {
		return false;
	}

	if (validate) {
		StringView r(source);
		r.skipChars<HostUnicodeChars>();
		if (!r.empty()) {
			return false;
		}
	}

	return unicode::idnToAscii(cb, source);
}

bool to_unicode(const callback<void(StringView)> &cb, StringView source, bool validate) {
	if (source.empty()) {
		return false;
	}

	if (validate) {
		StringView r(source);
		r.skipChars<HostAsciiChars>();
		if (!r.empty()) {
			return false;
		}
	}

	return unicode::idnToUnicode(cb, source);
}

} // namespace sprt::idn
