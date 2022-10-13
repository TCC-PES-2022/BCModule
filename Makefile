include config.mk

# path macros
OUT_PATH := bin
OBJ_PATH := obj
SRC_PATH := src
INCLUDE_PATH := include

TARGET_NAME := blmodule
TARGET := $(OUT_PATH)/$(TARGET_NAME)

LDFLAGS += $(addsuffix /$(LIB_PATH), $(addprefix -Lmodules/,$(DEPS)))

INCDIRS := $(addprefix -I,$(shell find $(INCLUDE_PATH) -type d -print))
INCDIRS += $(addprefix -I,$(DEP_PATH)/include)

# src files & obj files
SRC := $(shell find $(SRC_PATH) -type f -name "*.cpp")
OBJ := $(subst $(SRC_PATH),$(OBJ_PATH),$(SRC:%.cpp=%.o))
OBJDIRS:=$(dir $(OBJ))

# clean files list
CLEAN_LIST := $(OBJ)

# default rule
default: all

# non-phony targets
$(DEPS): $@
	@echo "\n\n *** Building $@ *** \n\n"
	cd modules/$@ && $(MAKE) deps && \
	$(MAKE) $(DEP_RULE) -j$(shell echo $$((`nproc`))) && \
	$(MAKE) install DESTDIR=$(DEP_PATH)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LINKFLAGS) $(INCDIRS) $(LDFLAGS) $(LDLIBS)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c
	@echo "Building $<"
	$(CC) $(CCOBJFLAGS) -o $@ $< $(INCDIRS)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.cpp
	@echo "Building $<"
	$(CXX) $(COBJFLAGS) -o $@ $< $(INCDIRS)

# phony rules
.PHONY: makedir
makedir:
	@mkdir -p $(OBJDIRS) $(OUT_PATH)

.PHONY: deps
deps: makedir $(DEPS)

.PHONY: all
all: makedir $(TARGET)
	strip --strip-unneeded $(TARGET)

.PHONY: test
test: makedir $(TARGET)

.PHONY: debug
debug: makedir $(TARGET)

.PHONY: run
run: 
	LD_LIBRARY_PATH=$(DEP_PATH)/lib ./$(TARGET)

.PHONY: install
install:
	@echo "\n\n *** Installing BCModule to $(DESTDIR) *** \n\n"
	mkdir -p $(DESTDIR)/bin
	cp -f $(TARGET) $(DESTDIR)/bin

# TODO: create uninstall rule

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -rf $(CLEAN_LIST)
	@rm -rf $(OBJ_PATH)
	@rm -rf $(OUT_PATH)