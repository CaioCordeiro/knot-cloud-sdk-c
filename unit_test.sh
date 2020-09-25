>test/unit/parser/out.txt
gcc -Isrc/ -o parser_unit_test src/parser.c \
test/unit/parser/parser_unit_test.c -lell -ldl -ljson-c -lknotcloudsdkc
./parser_unit_test >> test/unit/parser/out.txt 2>&1
diff test/unit/parser/expected.txt test/unit/parser/out.txt
