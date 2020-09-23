gcc -Isrc/ -o parser-unit-test -g src/parser.c -g test/unit/parser/parser-unit-test.c \
-lknotprotocol -lell -ldl -ljson-c -lknotcloudsdkc
 valgrind --leak-check=full --track-fds=yes ./parser-unit-test
# gdb parser-unit-test
# ./parser-unit-test
