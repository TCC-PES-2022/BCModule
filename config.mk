# version
VERSION = 0.1

# paths
DEST 	:= /opt/fls
DEPS 	:= ARINC615AManager tinyxml2 libgpg-error libgcrypt

INSTALL_PATH 	:= $(DEST)
DEP_PATH 		:= $(DEST)

CXX 		?=
CXXFLAGS 	:= -Wall -Werror -std=c++11 -pthread
DBGFLAGS 	:= -g -ggdb
TESTFLAGS 	:= -fprofile-arcs -ftest-coverage --coverage
LINKFLAGS 	:= 
LDFLAGS  	:= -L$(DEP_PATH)/lib
LDLIBS   	:= -larinc615a -ltransfer -ltftp -ltftpd -lcjson -ltinyxml2
LDLIBS 		+= -lgcrypt -lgpg-error

COBJFLAGS 	:= $(CXXFLAGS) -c
test: COBJFLAGS 	+= $(TESTFLAGS)
test: LINKFLAGS 	+= -fprofile-arcs -lgcov
debug: COBJFLAGS 	+= $(DBGFLAGS)
