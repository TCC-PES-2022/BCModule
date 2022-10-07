# version
VERSION = 0.1

# paths
DESTDIR 	?= /tmp
DEP_PATH 	?= $(DESTDIR)

DEPS 		:= ARINC615AManager tinyxml2 libgpg-error libgcrypt

CXX 		?=
CXXFLAGS 	:= -Wall -Werror -std=c++11 -pthread
LDFLAGS  	:= -L$(DEP_PATH)/lib
LDLIBS   	:= -larinc615a -ltransfer -ltftp -ltftpd -ltinyxml2
LDLIBS 		+= -lgcrypt -lgpg-error
LDLIBS 		+= $(DEP_PATH)/lib/libcjson.a
DBGFLAGS 	:= -g -ggdb
TESTFLAGS 	:= -fprofile-arcs -ftest-coverage --coverage

COBJFLAGS 	:= $(CXXFLAGS) -c
test: COBJFLAGS 	+= $(TESTFLAGS)
test: LINKFLAGS 	+= -fprofile-arcs -lgcov
debug: COBJFLAGS 	+= $(DBGFLAGS)
