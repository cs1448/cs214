#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
int wrap(int fd, int max, int output){
	int wrapMax = max; //a holder for the max column length
	int bytes_read;
	char* buf = (char*) calloc(max, sizeof(char)); //buffer array
	int isparagraph = 0; //to track whether a paragraph needs to be printed
	int failed = 0; //to indicate if the wrapping algorithm failed (i.e. a word length > max length)
	while((bytes_read = read(fd, buf, max)) > 0){
		int spaceIndex = 0;
		//transfer all white spaces into " " to treat them the same
		for(int i = 0; i < bytes_read; i++){
			if(isspace(buf[i]) != 0){
				strncpy(&buf[i], " ", sizeof(char));
			}
		}
		//if it is the last line of input, bytes_read < max. therefore we print whatever is left
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
			        		write(output, "\n", sizeof(char)); //print extra new line
			        		isparagraph = 0;
						i++;
			    		}
	        		}
	        		write(output, &buf[i], sizeof(char)); //prints each iteration of buf[i]
	   		}
			write(output, "\n", sizeof(char)); //prints new line char
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
			write(output, buf, bytes_read);
			char* endoflongword = malloc(sizeof(char));
			int longbytes_read;
			while((longbytes_read = read(fd, endoflongword, 1)) > 0){
				if(isspace(endoflongword[0]) != 0){
					write(output, "\n", sizeof(char));
					break;
				}
				write(output, endoflongword, longbytes_read);
			}
			free(endoflongword);
			failed = 1;
			continue; //continue to next iteration so the rest of the file is wrapped
		}
		//check if the last char in buffer is a space, if it is a space then we print the buffer
		if(isspace(buf[bytes_read-1]) != 0){
			for (int i = 0; i < bytes_read - 1; i++){ //iterate through array
	       			if (isspace(buf[i]) != 0){
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
			        		write(output, "\n", sizeof(char)); //print extra new line
			        		isparagraph = 0;
						i++;
			    		}
	        		}
	        		write(output, &buf[i], sizeof(char)); //prints each iteration of buf[i]
	   		}
			write(output, "\n", sizeof(char)); //prints new line char
		}
		else{//since its not finished on a complete word, find the last whitespace
			spaceIndex = 0;
			for(int i = bytes_read-1; i >= 0; i--){
				if(isspace(buf[i]) != 0){
					spaceIndex = i;
					break;
				}
			}
			write(output, buf, spaceIndex);
			//read one more char just to know if the last word read is an entire word. 
			//if it is, don't print a line. else, print a line
			char* fullwordcheck = malloc(sizeof(char));
			int fullword_read;
			while((fullword_read = read(fd, fullwordcheck, 1)) > 0){
				if(isspace(fullwordcheck[0]) != 0){
					write(output, "\n", sizeof(char));
					for(int i = spaceIndex+1; i < bytes_read; i++){
						write(output, &buf[i], sizeof(char));
					}
					write(output, " ", sizeof(char));
					break;
				}
				else{
					write(output, "\n", sizeof(char));
					for(int i = spaceIndex+1; i < bytes_read; i++){
						write(output, &buf[i], sizeof(char));
					}
					write(output, &fullwordcheck[0], sizeof(char));
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
	}
	free(buf);
	//check if the failed flag was set
	if(failed == 0){
		return 1;
	}
	else{
		return -1;
	}
}
int main(int argc, char* argv[argc+1]){
	
	int max = atoi(argv[1])+1;
	int fd = open(argv[2], O_RDONLY);
	int wrappedSuccessfully;//a flag to know if we return failure or success
	//if 3rd arg is not null, that means there is a directory or file
	if(argv[2] != NULL){
		DIR *dirp = opendir(argv[2]);
		if(dirp != NULL){//go thru directory
			chdir(argv[2]);
			struct dirent *de;
			while((de = readdir(dirp))){
				struct stat stbuf;
				stat(de->d_name, &stbuf);
				if(S_ISREG(stbuf.st_mode)){//regular file, wrap it
					if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0){ continue; }
					if(de->d_name[0] == '.'){ continue; }
					if(strncmp(de->d_name, "wrap.", 5) == 0){ continue; }
					//add wrap. subtext to the file, and use O_TRUNC to overwrite if it exists
					int outputFD;//file descriptor for the file we are writing to
					char outputFile[256];
					strcpy(outputFile, "wrap.");
					strcat(outputFile, de->d_name);
					outputFD = open(outputFile, O_WRONLY|O_TRUNC|O_CREAT, 0666);
					//the fd for the file we opened in the directory
					int direntfd = open(de->d_name, O_RDONLY);
					wrappedSuccessfully = wrap(direntfd, max, outputFD);
					close(direntfd);
				}
			}
		}
		else{//if there was no directory, this means we read from the file
			//fd will be -1 if the 3rd argument does not exist (its not a valid file or a directory)
			if(fd != -1){//read from the file
				wrappedSuccessfully = wrap(fd, max, 1);
			}
			else{//read from stdin
				wrappedSuccessfully = wrap(0, max, 1);
			}
		}
		closedir(dirp);
		close(fd);
		if(wrappedSuccessfully == 1){
			return EXIT_SUCCESS;
		}
		else{
			fprintf(stderr, "Error: A word is longer than wrap limit!\n");
			return EXIT_FAILURE;
		}
	}
	//if we got here, that means there was no 2nd argument, which means read from stdin
	wrappedSuccessfully = wrap(0, max, 1);
	close(fd);
	if(wrappedSuccessfully == 1){
		return EXIT_SUCCESS;
	}
	else{
		fprintf(stderr, "Error: A word is longer than wrap limit!\n");
		return EXIT_FAILURE;
	}
}

