# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

.DEFAULT_GOAL := all

LIBNAME = openssl-gost-engine

include ../common/configure.mk

# Стандартный cmake для gost-engine не может собрать библиотеку для всех целевых платформ
# Потому мы собираем библиотеку вручную

prepare:
	$(call rule_rm,$(LIBNAME))
	$(call rule_mkdir,$(LIBNAME))

GOST_89_SOURCE_FILES := gost89.c

GOST_HASH_SOURCE_FILES := gosthash.c

GOST_HASH_2012_SOURCE_FILES := gosthash2012.c

GOST_GRASSHOPPER_SOURCE_FILES := \
	gost_grasshopper_core.c \
	gost_grasshopper_defines.c \
	gost_grasshopper_galois_precompiled.c \
	gost_grasshopper_precompiled.c \
	gost_grasshopper_cipher.c

GOST_ERR_SOURCE_FILES := e_gost_err.c

GOST_CORE_SOURCE_FILES := gost_ameth.c \
	gost_pmeth.c \
	gost_ctl.c \
	gost_asn1.c \
	gost_crypt.c \
	gost_keywrap.c \
	gost_md.c \
	gost_md2012.c \
	gost_omac.c \
	gost_omac_acpkm.c \
	gost_gost2015.c \
	gost_params.c \
	gost_keyexpimp.c

GOST_EC_SOURCE_FILES := gost_ec_keyx.c \
	gost_ec_sign.c \
	ecp_id_GostR3410_2001_CryptoPro_A_ParamSet.c \
	ecp_id_GostR3410_2001_CryptoPro_B_ParamSet.c \
	ecp_id_GostR3410_2001_CryptoPro_C_ParamSet.c \
	ecp_id_GostR3410_2001_TestParamSet.c \
	ecp_id_tc26_gost_3410_2012_256_paramSetA.c \
	ecp_id_tc26_gost_3410_2012_512_paramSetA.c \
	ecp_id_tc26_gost_3410_2012_512_paramSetB.c \
	ecp_id_tc26_gost_3410_2012_512_paramSetC.c

GOST_OMAC_SOURCE_FILES := gost_omac.c gost_omac_acpkm.c

GOST_LIB_SOURCE_FILES := \
	${GOST_CORE_SOURCE_FILES} \
	${GOST_89_SOURCE_FILES} \
	${GOST_HASH_SOURCE_FILES} \
	${GOST_HASH_2012_SOURCE_FILES} \
	${GOST_GRASSHOPPER_SOURCE_FILES} \
	${GOST_EC_SOURCE_FILES} \
	${GOST_OMAC_SOURCE_FILES}

GOST_ENGINE_SOURCE_FILES := gost_eng.c $(GOST_LIB_SOURCE_FILES) $(GOST_ERR_SOURCE_FILES)

SRCS := $(sort $(GOST_ENGINE_SOURCE_FILES))

HEADERS = gost-engine.h e_gost_err.h

OBJS := $(SRCS:.c=.o)

TARGET_CFLAGS := $(SP_CFLAGS) -DBUILDING_ENGINE_AS_LIBRARY -DL_ENDIAN \
	-Werror -Wall -Wno-unused-parameter -Wno-unused-function -Wno-missing-braces -Qunused-arguments -Wno-deprecated-declarations

ifeq ($(SP_ARCH),x86_64)
TARGET_CFLAGS += -DHAVE_ADDCARRY_U64
endif

$(LIBNAME)/%.o: $(LIB_SRC_DIR)/$(LIBNAME)/%.c | prepare
	$(SP_CC) $(TARGET_CFLAGS) -c $< -o $@

$(SP_INSTALL_PREFIX)/usr/include/%.h: $(LIB_SRC_DIR)/$(LIBNAME)/%.h | prepare
	$(call rule_cp,$<,$@)

$(LIBNAME)/$(call mklibname,gost_engine): $(addprefix $(LIBNAME)/,$(OBJS)) prepare
	$(SP_AR) rcs $@ $(addprefix $(LIBNAME)/,$(OBJS))

all: $(LIBNAME)/$(call mklibname,gost_engine) $(addprefix $(SP_INSTALL_PREFIX)/usr/include/,$(HEADERS))
	$(call rule_cp,$(LIBNAME)/$(call mklibname,gost_engine),$(SP_INSTALL_PREFIX)/usr/lib/$(call mklibname,gost_engine))
	$(call rule_rm,$(LIBNAME))

.PHONY: all
