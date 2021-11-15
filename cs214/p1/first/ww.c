#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
	
int main(int argc, char* argv[argc+1]){
	int max = atoi(argv[1])+1;
	int wrapMax = max;//a holder for the max column length
	int fd = open(argv[2], O_RDONLY);
	int bytes_read;
	char* buf = (char*) calloc(max, sizeof(char));
	int isparagraph = 0;//a flag for if a paragraph must be printed
	int failed = 0;//a flag to see if the wrap method failed
	if(fd == -1){//read from stdinput
		fd = 0;
	}
	//read until end of file
	while((bytes_read = read(fd, buf, max)) > 0){
		//change all white spaces into " " to treat them uniformily
		int spaceIndex = 0;
		for(int i = 0; i < bytes_read; i++){
			if(isspace(buf[i]) != 0){
				strncpy(&buf[i], " ", sizeof(char));
			}
		}
		//if bytes_read is less than max, that means we are on last line of input. that means we print whats left
		if(bytes_read < max){
			for (int i = 0; i < bytes_read; i++){ //iterate through array
	       			if (isspace(buf[i]) != 0){ //if char == space
	            			while ((i + 1) < bytes_read){
						if (isspace(buf[i + 1]) != 0){ //checks if next char is valid
	                    			    isparagraph = 1;      //sets flag to true
						    i++;                 //continue iteration
	                			}
						else {
						    break;
						}
	            			}
			    		if (isparagraph == 1){ //if true (if 2 consecutive space)
			        		write(1, "\n", sizeof(char)); //print extra new line
			        		isparagraph = 0;
						i++;
			    		}
	        		}
	        		write(1, &buf[i], sizeof(char)); //prints each iteration of buf[i]
	   		}
			write(1, "\n", sizeof(char)); //prints new line char
			break;
		}
		//if the entire buffer is a word, then print the whole word w/o a new line
		int longWord = 1;
		for(int i = 0; i < bytes_read; i++){
			if(isspace(buf[i]) != 0){
				longWord = 0;
				break;
			}
		}
		if(longWord == 1){
			write(1, buf, bytes_read);
			char* endoflongword = malloc(sizeof(char));
			int longbytes_read;
			while((longbytes_read = read(fd, endoflongword, 1)) > 0){
				if(isspace(endoflongword[0]) != 0){
					write(1, "\n", sizeof(char));
					break;
				}
				write(1, endoflongword, longbytes_read);
			}
			free(endoflongword);
			failed = 1;
			continue;//continue to next buffer read so we can wrap the rest of the file, but set failed flag
		}
		if(isspace(buf[bytes_read-1]) != 0){//last char is a space
			for (int i = 0; i < bytes_read - 1; i++){ //iterate through array
	       			if (isspace(buf[i]) != 0){ //if char == space
	            			while ((i + 1) < bytes_read - 1){
						if (isspace(buf[i + 1]) != 0){ //checks if next char is valid
	                    			    isparagraph = 1;      //sets flag to true
						    i++;                 //continue iteration
	                			}
						else {
						    break;
						}
	            			}
			    		if (isparagraph == 1){ //if true (if 2 consecutive space)
			        		write(1, "\n", sizeof(char)); //print extra new line
			        		isparagraph = 0;
						i++;
			    		}
	        		}
	        		write(1, &buf[i], sizeof(char)); //prints each iteration of buf[i]
	   		}
			write(1, "\n", sizeof(char)); //prints new line char
		}
		else{//since its not finished on a complete word, find the last whitespace
			spaceIndex = 0;
			for(int i = bytes_read-1; i >= 0; i--){
				if(isspace(buf[i]) != 0){
					spaceIndex = i;
					break;
				}
			}
			write(1, buf, spaceIndex);
			//read one more char just to know if the last word read is an entire word. 
			//if it is, don't print a line. else, print a line
			char* fullwordcheck = malloc(sizeof(char));
			int fullword_read;
			while((fullword_read = read(fd, fullwordcheck, 1)) > 0){
				if(isspace(fullwordcheck[0]) != 0){
					write(1, "\n", sizeof(char));
					for(int i = spaceIndex+1; i < bytes_read; i++){
						write(1, &buf[i], sizeof(char));
					}
					write(1, " ", sizeof(char));
					break;
				}
				else{
					write(1, "\n", sizeof(char));
					for(int i = spaceIndex+1; i < bytes_read; i++){
						write(1, &buf[i], sizeof(char));
					}
					write(1, &fullwordcheck[0], sizeof(char));
					break;
				}	
			}
			free(fullwordcheck);
		}
		//logic for keeping track of how many chars were carried over onto next line, since max will not be the same
		if(spaceIndex != 0){
			max = spaceIndex + (wrapMax - bytes_read);
		}
		else{
			max = wrapMax;
		}
	} //end of while loop
	close(fd);
	free(buf);
	//failed will be 1 for errors in user input (i.e. a word length is longer than the max column length)
	if(failed == 1){
		fprintf(stderr, "Error: A word is longer than wrap limit!\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
