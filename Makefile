# list of modules to be compipled
CLIENT_MODULES  := client
SERVER_MODULES  := server keyregistry

# future extension: modules that are used by server and client (for example protocol definitions)
COMMON_MODULES  :=

# predefined macros
PREDEFS         :=

# handle make params
# app (mandatory)
#       server - compiles the Key-Value Server application
#       client - compiles the Key-Value Client application
#
# strict (optional)
#       yes - server won't allow to update already existing keys
#       anything else - update of keys are possible (default behavior)
#                       defines a feature switch FS_ALLOW_UPDATE
ifdef app
ifeq ($(app),server)

    APPLICATION := kvp_server
    ALL_MODULES := $(SERVER_MODULES) $(COMMON_MODULES)

ifneq ($(strict),yes)
    PREDEFS     := $(PREDEFS) -DFS_ALLOW_UPDATE
endif
    
else
ifeq ($(app),client)

    APPLICATION := kvp_client
    ALL_MODULES := $(CLIENT_MODULES) $(COMMON_MODULES)
    
else
    $(info Invalid application ...)
    $(info - app=server)
    $(info - app=client)
    $(error Please provide one of the make-variables on your command line.)
endif
endif
else
    $(info Application has not been selected ...)
    $(info - app=server)
    $(info - app=client)
    $(error Please provide one of the make-variables on your command line.)
endif

SRC_PATH    := src
OBJ_PATH    := obj
INC_PATH    := inc
BUILD_PATH  := .

CC          := gcc

CFLAGS      = -Wall                 \
               -c                   \
               -o $(OBJ_PATH)/$*.o  \
               -I$(INC_PATH)        \
               $(PREDEFS)
              
LINK_OPTS   = -o $(BUILD_PATH)/$(APPLICATION)

$(OBJ_PATH)/%.o : $(SRC_PATH)/%.c
	@if ! [ -d $(OBJ_PATH) ]; then mkdir $(OBJ_PATH); fi
	@echo " Compiling $< ..."
	@$(CC) $< $(CFLAGS)

OBJS := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(ALL_MODULES)))

$(BUILD_PATH)/$(APPLICATION) : $(OBJS)
	@echo Linking application ...
	@$(CC) $(LINK_OPTS) $(OBJS)
	@echo DONE

.PHONY: build
build : $(BUILD_PATH)/$(APPLICATION)
	@echo Application build complete!

.PHONY: clean
clean :
	@echo Cleaning application ...
	@rm -f $(OBJS)
	@rm -f $(BUILD_PATH)/$(APPLICATION)

.PHONY: all
all :
	@$(MAKE) --no-print-directory build

.PHONY: rebuild
rebuild :
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory build