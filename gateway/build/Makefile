UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),armv7l)
TARGET_MAKEFILE := Makefile.rpi
else
TARGET_MAKEFILE := Makefile.lin
endif
endif
ifeq ($(UNAME_S),Darwin)
TARGET_MAKEFILE := Makefile.mac
endif

build:
	make -f $(TARGET_MAKEFILE) build

deploy: 
	make -f $(TARGET_MAKEFILE) deploy

clean:
	rm -rf ./dist/*