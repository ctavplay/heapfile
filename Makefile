SOURCE_DIR  = src
LIB_DIR     = lib
OBJ_DIR     = obj
INCLUDE_DIR = include

vpath %.cpp $(SOURCE_DIR)
vpath %.h $(INCLUDE_DIR)

ifdef NODEBUG
DEFS = -DNDEBUG
else
CDEBUG = -g
endif	

CPPFLAGS = $(CDEBUG) -pedantic -pedantic-errors -Wall -Werror -I include $(DEFS)
SOURCES = \
	byte_order.cpp  \
	heap_blob.cpp   \
	heap_file.cpp   \
	heap_index.cpp  \
	mmap_file.cpp   \
	simple_encrypt.cpp


OBJECTS       := $(subst .cpp,.o,$(SOURCES))
OBJECTS       := $(addprefix $(OBJ_DIR)/, $(OBJECTS))
TEST_SOURCES  := $(subst .cpp,.t.cpp,$(SOURCES))
TEST_SOURCES += unit_test.cpp
TEST_OBJS     := $(subst .cpp,.o,$(TEST_SOURCES))
TEST_OBJS     := $(addprefix $(OBJ_DIR)/, $(TEST_OBJS))

ALLSOURCES := $(SOURCES) $(TEST_SOURCES)
ALLSOURCES := $(addprefix $(SOURCE_DIR)/, $(ALLSOURCES))

DEPENDENCIES  := $(subst .o,.d,$(OBJECTS) $(TEST_OBJS))

LIB_BASE_NAME=heapfile
LIBNAME = $(LIB_DIR)/lib$(LIB_BASE_NAME).a
TEST_TASK= test_$(LIB_BASE_NAME)

REQUIRED_DIRS = $(LIB_DIR) $(OBJ_DIR)
_MKDIRS := $(shell for d in $(REQUIRED_DIRS); \
	     do                               \
		[[ -d $$d ]] || mkdir -p $$d; \
	     done)

all: $(LIBNAME)

$(LIBNAME): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $?

test: $(TEST_TASK)

check: $(TEST_TASK)
	./$(TEST_TASK)

$(TEST_TASK): $(TEST_OBJS) $(LIBNAME)
	$(LINK.cpp) $^ -o $@

clean:
	$(RM) $(TEST_TASK)
	$(RM) $(OBJECTS) $(TEST_OBJS)
	$(RM) $(DEPENDENCIES)
	$(RM) $(LIBNAME)

$(OBJ_DIR)/%.o: %.cpp
	$(COMPILE.cpp) $< -o $@

ifneq "$(MAKECMDGOALS)" "clean"
  -include $(DEPENDENCIES)
endif

$(OBJ_DIR)/%.d: %.cpp
	$(COMPILE.cpp) -MM $< -MT '$$(OBJ_DIR)/$*.o' > $@
