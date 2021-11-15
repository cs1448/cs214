all : ww

ww : ww.c
	gcc -Wall -Werror -fsanitize=address -std=c11 ww.c -g -o ww

clean :
	rm -f ww

