# version
VERSION = 0.1

# paths
DESTDIR 	?= /tmp
DEP_PATH 	?= $(DESTDIR)

DEPS 		:= ARINC615AManager BLSecurityManager

CXX 		?=
CXXFLAGS 	:= -Wall -Werror -std=c++11 -pthread
LDFLAGS  	:= -L$(DEP_PATH)/lib
LDLIBS   	:= -larinc615a -lblsecurity -ltransfer -ltftp -ltftpd -ltinyxml2
LDLIBS 		+= -lgcrypt -lgpg-error -lcjson
DBGFLAGS 	:= -g -ggdb
TESTFLAGS 	:= -fprofile-arcs -ftest-coverage --coverage

COBJFLAGS 	:= $(CXXFLAGS) -c
test: COBJFLAGS 	+= $(TESTFLAGS)
test: LINKFLAGS 	+= -fprofile-arcs -lgcov
debug: COBJFLAGS 	+= $(DBGFLAGS)
