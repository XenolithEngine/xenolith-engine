/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_SEARCH_THIRDPARTY_SNOWBALL_SPSNOWBALLSTEMMER_H_
#define CORE_SEARCH_THIRDPARTY_SNOWBALL_SPSNOWBALLSTEMMER_H_

#include "SPSearchEnum.h"

struct SN_env {
	void *(*memalloc)( void *userData, unsigned int size );
	void (*memfree)( void *userData, void *ptr );
	void* userData;	// User data passed to the allocator functions.

	int (*stem)(struct SN_env *);

	unsigned char * p;
	int c; int l; int lb; int bra; int ket;
	unsigned char * * S;
	int * I;
	unsigned char * B;

	const void *stopwords;
	struct stemmer_modules *mod;
};

struct stemmer_modules {
	enum SnowballLanguage name;
	struct SN_env * (*create)(struct SN_env *);
	void (*close)(struct SN_env *);
	int (*stem)(struct SN_env *);
};

SP_EXTERN_C SP_LOCAL struct stemmer_modules * sb_stemmer_get(enum SnowballLanguage lang);
SP_EXTERN_C SP_LOCAL const unsigned char * sb_stemmer_stem(struct SN_env * z, const unsigned char * word, int size);

#endif /* CORE_SEARCH_THIRDPARTY_SNOWBALL_SPSNOWBALLSTEMMER_H_ */
