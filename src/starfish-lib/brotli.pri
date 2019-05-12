INCLUDEPATH += $$PWD/../../3rd-party/brotli/c/include

SOURCES += $$PWD/../../3rd-party/brotli/c/common/dictionary.c
SOURCES += $$PWD/../../3rd-party/brotli/c/common/transform.c
SOURCES += $$PWD/../../3rd-party/brotli/c/dec/bit_reader.c
SOURCES += $$PWD/../../3rd-party/brotli/c/dec/decode.c
SOURCES += $$PWD/../../3rd-party/brotli/c/dec/huffman.c
SOURCES += $$PWD/../../3rd-party/brotli/c/dec/state.c

brotlifiles.commands += $(COPY) $$shell_path($$PWD/../../3rd-party/brotli/c/common/dictionary.bin) $$shell_path($$PWD/brotli.bin);
brotlifiles.commands += $(COPY) $$shell_path($$PWD/../../3rd-party/brotli/LICENSE) $$shell_path($$PWD/COPYING.brotli);
PRE_TARGETDEPS += brotlifiles
QMAKE_EXTRA_TARGETS += brotlifiles
