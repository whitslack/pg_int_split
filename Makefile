PG_CONFIG ?= pg_config

MODULES = pg_int_split
EXTENSION = pg_int_split
DATA = $(addprefix pg_int_split--,$(addsuffix .sql,1.0))
PG_CFLAGS = -Wextra $(addprefix -Werror=,implicit-function-declaration incompatible-pointer-types int-conversion) -Wcast-qual -Wconversion -Wno-declaration-after-statement -Wdisabled-optimization -Wdouble-promotion -Wno-implicit-fallthrough -Wmissing-declarations -Wno-missing-field-initializers -Wpacked -Wno-parentheses -Wno-sign-conversion -Wstrict-aliasing $(addprefix -Wsuggest-attribute=,pure const noreturn malloc) -fstrict-aliasing

ifeq ($(shell command -v $(PG_CONFIG)),)
  $(error $(PG_CONFIG) was not found)
endif
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

override CPPFLAGS := $(patsubst -I%,-isystem %,$(filter-out -I. -I./,$(CPPFLAGS)))
