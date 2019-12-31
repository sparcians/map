# <Makefile>  -*- make -*-

define help_message

To build Sparta, perform the following actions depending on purpose:

   Optimized (NDEBUG, optimized):
      mkdir release; cd release
      CC=$(CC) CXX=$(CXX) cmake .. -DCMAKE_BUILD_TYPE=Release

   Debug (unoptimized):
      mkdir debug; cd debug
      CC=$(CC) CXX=$(CXX) cmake .. -DCMAKE_BUILD_TYPE=Debug

* Note: The values of CC and CXX can be changed to clang and clang++ if not already:
      CC=clang CXX=clang++ <etc>

endef
export help_message

all help::
	@echo "$$help_message"
