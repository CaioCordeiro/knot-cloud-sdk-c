gcc -Isrc/ -o parser-unit-test src/parser.c test/unit/parser/parser-unit-test.c \
-lknotprotocol -lell -ldl -ljson-c
./parser-unit-test
