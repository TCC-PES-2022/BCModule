# version
VERSION = 0.1

# paths
DESTDIR 	?= /tmp
DEP_PATH 	?= $(DESTDIR)

DEPS 		:= ARINC615AManager BLSecurityManager

CXX 		?=
CXXFLAGS 	:= -Wall -Werror -std=c++11 -pthread
LDFLAGS  	:= -L$(DEP_PATH)/lib
LDLIBS   	:= $(DEP_PATH)/lib/libarinc615a.a $(DEP_PATH)/lib/libblsecurity.a 
LDLIBS		+= $(DEP_PATH)/lib/libtransfer.a $(DEP_PATH)/lib/libtftp.a 
LDLIBS		+= $(DEP_PATH)/lib/libtftpd.a
LDLIBS 		+= -lgcrypt -lgpg-error -lcjson -ltinyxml2
DBGFLAGS 	:= -g -ggdb
TESTFLAGS 	:= -fprofile-arcs -ftest-coverage --coverage

COBJFLAGS 	:= $(CXXFLAGS) -c
test: COBJFLAGS 	+= $(TESTFLAGS)
test: LINKFLAGS 	+= -fprofile-arcs -lgcov
debug: COBJFLAGS 	+= $(DBGFLAGS)
