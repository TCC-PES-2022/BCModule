include config.mk

# path macros
BIN_PATH := bin
OBJ_PATH := obj
SRC_PATH := src
INCLUDE_PATH := include

TARGET_NAME := bcmodule
TARGET := $(BIN_PATH)/$(TARGET_NAME)

LDFLAGS += $(addsuffix /$(LIB_PATH), $(addprefix -Lmodules/,$(DEPS)))

INCDIRS := $(addprefix -I,$(shell find $(INCLUDE_PATH) -type d -print))
INCDIRS += $(addprefix -I,$(DEP_PATH)/include)

# src files & obj files
SRC := $(shell find $(SRC_PATH) -type f -name "*.cpp")
OBJ := $(subst $(SRC_PATH),$(OBJ_PATH),$(SRC:%.cpp=%.o))
OBJDIRS:=$(dir $(OBJ))

# clean files list
DISTCLEAN_LIST := $(OBJ)
CLEAN_LIST := $(DISTCLEAN_LIST)

CLEAN_DEPS := $(DEPS)

# default rule
default: target

# non-phony targets
ARINC615AManager:
	cd modules/ARINC615AManager && make dependencies && \
	make -j$(shell echo $$((`nproc`))) && make install

tinyxml2:
	cd modules/tinyxml2 && make -j16 && cp libtinyxml2.a $(DEP_PATH)/lib && \
	cp *.h $(DEP_PATH)/include

libgpg-error:
	cd modules/libgpg-error && ./autogen.sh &&  \
	./configure --enable-maintainer-mode --enable-static --disable-shared \
	--prefix=$(INSTALL_PATH) && make -j$(shell echo $$((`nproc`))) && make install

libgcrypt: libgpg-error
	cd modules/libgcrypt && ./autogen.sh &&  \
	./configure --enable-maintainer-mode --enable-static --disable-shared \
	--prefix=$(INSTALL_PATH) && make -j$(shell echo $$((`nproc`))) && make install

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LINKFLAGS) $(INCDIRS) $(LDFLAGS) $(LDLIBS)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c
	@echo "Compiling $<"
	$(CC) $(CCOBJFLAGS) -o $@ $< $(INCDIRS)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.cpp
	@echo "Compiling $<"
	$(CXX) $(COBJFLAGS) -o $@ $< $(INCDIRS)

# phony rules
.PHONY: makedir
makedir:
	@mkdir -p $(OBJDIRS) $(BIN_PATH)

.PHONY: deps
deps: makedir $(DEPS)

.PHONY: all
all: makedir $(TARGET)

.PHONY: target
target: makedir $(TARGET)

.PHONY: test
test: makedir $(TARGET)

.PHONY: debug
debug: makedir $(TARGET)

.PHONY: run
run: 
	LD_LIBRARY_PATH=/opt/fls/lib ./$(TARGET)

.PHONY: install
install:
	mkdir -p $(INSTALL_PATH)/lib $(INSTALL_PATH)/include
	cp -f $(BIN_PATH)/*.so $(INSTALL_PATH)/lib
	cp -f $(shell find $(INCLUDE_PATH) -type f -name "*.h") $(INSTALL_PATH)/include

# TODO: remove only what we installed
.PHONY: uninstall
uninstall:
	rm -rf $(INSTALL_PATH)/lib $(INSTALL_PATH)/include

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)

.PHONY: distclean
distclean:
	@echo CLEAN $(DISTCLEAN_LIST)
	@rm -f $(DISTCLEAN_LIST)
	@rm -rf $(OBJ_PATH)
	@rm -rf $(BIN_PATH)