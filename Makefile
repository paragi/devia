# Thanks to Job Vranish (https://spin.atomicobject.com/2016/08/26/makefile-c-projects/)

# Configuration
TARGET_EXEC := devia
DESTDIR = /usr
BUILD_DIR := ./build
SRC_DIRS := ./src
LDFLAGS += -lexplain -lusb-1.0 -lftdi -lpthread -ludev -lhidapi-libusb

# There is an unsolved issue with glib, both vscode and gcc paths...
# On Raspbarian libusb isn't found either
CFLAGS += `pkg-config --cflags glib-2.0` `pkg-config --cflags libusb-1.0`
LDFLAGS += `pkg-config --libs glib-2.0`  `pkg-config --libs libusb-1.0`


# File to auto-incrementing build number and date.
AUTOGEN_FILE := $(SRC_DIRS)/version.h
# Auto generate build number and date
#   Find #define BUILD_NUMMBER in version.h file
#   Extract the number and increment
#   When compiling, replace the lines, with a new one, with the new values
AUTOGEN_NEXT := $(shell expr $$(awk '/\#define BUILD_NUMBER/' $(AUTOGEN_FILE) | tr -cd "[0-9]") + 1) $(USER)

# Debug flags
# -Q will show which function in the test case is causing it to crash.
# -v shows how cc1 was invoked (useful for invoking cc1 manually in gdb).
# -da dumps the RTL to a file after each stage.
# -g default (g2) debug information level
CPP_DEBUG_FLAGS := -g -O0
# CPP_DEBUG_FLAGS := -v -da -Q -g -O0

# The -MMD and -MP flags together generate Makefiles for us!
# These files will have .d instead of .o as the output.
CPPFLAGS := $(INC_FLAGS)  $(CPP_DEBUG_FLAGS) -MMD -MP -Wformat=2 -Wall -Winline $(INCLUDE) -pipe -fPIC

# Find all the C and C++ files we want to compile
SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c)

# String substitution for every C/C++ file.
# As an example, hello.cpp turns into ./build/hello.cpp.o
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

# String substitution (suffix version without %).
# As an example, ./build/hello.cpp.o turns into ./build/hello.cpp.d
DEPS := $(OBJS:.o=.d)

# Every folder in ./src will need to be passed to GCC so that it can find header files
INC_DIRS := $(shell find $(SRC_DIRS) -type d)
# Add a prefix to INC_DIRS. So moduleA would become -ImoduleA. GCC understands this -I flag
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

# The final build step.
$(BUILD_DIR)/$(TARGET_EXEC):	$(OBJS)
	$(CXX)	$(OBJS) -o $@ $(LDFLAGS)
	if [ ! -f $(TARGET_EXEC) ] ; then ln -s $(BUILD_DIR)/$(TARGET_EXEC) ./; fi

# Build step for C source
$(BUILD_DIR)/%.c.o: %.c 
	mkdir -p $(dir $@)
	sed -i "s/\#define BUILD_NUMBER .*/\#define BUILD_NUMBER \"$(AUTOGEN_NEXT)\"/" $(AUTOGEN_FILE)
	sed -i "s/\#define BUILD_DATE.*/\#define BUILD_DATE \"$$(date +'%Y-%m-%d')\"/" $(AUTOGEN_FILE)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Build step for C++ source
$(BUILD_DIR)/%.cpp.o:	%.cpp 
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.PHONY:	clean

clean:
	rm -r $(BUILD_DIR)

install:
	sudo install --mode 755 --owner root --group root $(BUILD_DIR)/$(TARGET_EXEC) $(DESTDIR)/bin/$(TARGET_EXEC)

uninstall:
	sudo rm -f $(DESTDIR)/bin/$(TARGET_EXEC)


# Include the .d makefiles. The - at the front suppresses the errors of missing
# Makefiles. Initially, all the .d files will be missing, and we don't want those
# errors to show up.
-include $(DEPS)

MKDIR_P ?= mkdir -p
