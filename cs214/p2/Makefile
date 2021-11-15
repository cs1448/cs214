all : first

first : first.c
	gcc -Wall -Werror -fsanitize=address -std=c11 first.c -pthread -g -lm -o first

clean :
	rm -f first

