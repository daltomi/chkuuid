ifeq ("$(shell which pkg-config  2> /dev/null)","")
$(error 'pkg-config' NOT FOUND)
endif

ifeq ("$(shell pkg-config --libs blkid 2> /dev/null)","")
$(error 'pkg-config: blkid library' NOT FOUND)
endif

ifeq ("$(shell pkg-config --libs libudev 2> /dev/null)","")
$(error 'pkg-config: libudev library' NOT FOUND)
endif

ifeq ("$(shell pkg-config --libs mount 2> /dev/null)","")
$(error 'pkg-config: mount library' NOT FOUND)
endif

ifeq ("$(shell which install 2> /dev/null)","")
$(error 'install (coreutils)' NOT FOUND)
endif

APP := chkuuid

APP_VER := "1.7"
PKG_REV := "1"

ZIP := $(APP)-$(APP_VER)-$(PKG_REV).zip

CC ?= gcc

CLIBS :=-lc  $(shell pkg-config --libs blkid libudev mount) -Wl,-z,relro,-z,now



CFLAGS += -Wall -std=c99 -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE $(shell pkg-config --cflags blkid libudev mount)
CFLAGS_RELEASE:= -O2 -DNDEBUG
CFLAGS_DEBUG:= -O1 -g -DDEBUG -Wextra -Werror -Wimplicit-fallthrough


SOURCE := $(wildcard src/*.c)

OBJ := $(patsubst %.c,%.o,$(SOURCE))

default: release

version:
	-@sed 's/@APP_VER@/$(APP_VER)/g' src/config.h.in > src/config.h

release: CFLAGS+=$(CFLAGS_RELEASE)
release: CLIBS+=-Wl,-s
release: version $(APP)

debug: CFLAGS+=$(CFLAGS_DEBUG)
debug: version $(APP)


$(APP): $(OBJ)
	$(CC) $(OBJ) $(CLIBS) -o $(APP)

.o:
	$(CC) -c $<

.PHONY: dist

install:
	-@install -Dt $(PREFIX)/bin/ -m755 $(APP)

dist:
	zip $(ZIP) Makefile src/*.c src/*.in README.md LICENSE

clean:
	rm  -v src/*.o $(APP) src/config.h $(ZIP)
