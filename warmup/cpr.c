#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "common.h"

/* make sure to use syserror() when a system call fails. see common.h */

void usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}


void copy_file(const char* destination, const char* source){
	// file descriptor fd for the source and the destination
	int fd_sour, fd_dest;

	// open the source file for open read only
	/* The argument flags must include one of the following access modes:
       O_RDONLY, O_WRONLY, or O_RDWR. These request opening the file read-
       only, write-only, or read/write, respectively.
	   */
	fd_sour = open(source, O_RDONLY);
	if (fd_sour < 0){
		syserror(open, source);
	}

	// open the destination file for write only
	/* creat(): A call to creat() is equivalent to calling open() with flags equal to
       O_CREAT|O_WRONLY|O_TRUNC.
	   */
	// int creat(const char *pathname, mode_t mode);
	fd_dest = creat(destination, O_WRONLY);
	if (fd_dest < 0){
		syserror(creat, destination);
	}

	// reading from source and write to the destination
	char buf[4096];
	int read_ret, write_ret;
	while (1){
		read_ret = read(fd_sour, buf, 4096);
		if (read_ret < 0){
			syserror(read, source);
		}
		if (read_ret == 0){
			break;
		} //end of file has been reached.
		write_ret = write(fd_dest, buf, read_ret);
		if (write_ret < 0){
			syserror(write, destination);
		}
	}

	// open copy permission through system calls 
	/* int stat(const char *path, struct stat *buf);
		struct stat {
			....
			mode_t    st_mode;    // protection 
			....
		};
	*/
	struct stat stat_buf;
	stat(source, &stat_buf);
	int fd_chmod = chmod(destination, stat_buf.st_mode);
	if (fd_chmod < 0){
		syserror(chmod, destination);
	}

 	// close both files
	int fd_sour_close, fd_dest_close;

	fd_sour_close = close(fd_sour);
	if (fd_sour_close < 0){
		syserror(close, source);
	}
	
	fd_dest_close = close(fd_dest);
	if (fd_dest_close < 0){
		syserror(close, destination);
	}

}

void copy_directory(const char* destination, const char* source){
	
	//copy permission
	struct stat stat_buf;
	stat(source, &stat_buf);
	
	//check whether the source folder exist
	DIR* dest_pointer = opendir(source);
	if (dest_pointer == NULL){
		syserror(opendir, source);
	}
	
	char subsource[4096];
	char subdest[4096];

	//for read a directory
	DIR* the_directory = opendir(source); //the directory we currently want to read
	struct dirent* dir; //return type
	/* ino_t  d_ino       file serial number
	   char   d_name[]    name of entry
	*/

	while((dir = readdir(the_directory)) != NULL){
		if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0){
			strcpy(subsource, source);
			strcpy(subdest, destination);
			strcat(subsource, "/");
			strcat(subsource, dir->d_name);
			strcat(subdest, "/");
			strcat(subdest, dir->d_name);
			
			struct stat stat_buf_sub;
			stat(subsource, &stat_buf_sub);
			if (S_ISREG(stat_buf_sub.st_mode)){
				//the sub is a file invoke copy file
				copy_file(subdest, subsource);
			}else if (S_ISDIR(stat_buf_sub.st_mode)){
				int fd_sub_dir = mkdir(subdest, 0777);
				if (fd_sub_dir < 0){
					syserror(mkdir, subdest);
				}
				copy_directory(subdest, subsource);
				int fd_chmod_sub = chmod(subdest, stat_buf_sub.st_mode);
				if (fd_chmod_sub < 0){
					syserror(chmod, subdest);
				}
			}
		}
	}

	int fd_chmod = chmod(destination, stat_buf.st_mode);
	if (fd_chmod < 0){
		syserror(chmod, destination);
	}

	// close directory
	int fd_sour_close;

	fd_sour_close = closedir(the_directory);
	if (fd_sour_close < 0){
		syserror(closedir, source);
	}

	int fd_dest_close;
	fd_dest_close = closedir(dest_pointer);
	if (fd_dest_close < 0){
		syserror(closedir, destination);
	}
	
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		usage();
	}

	int fd_dest = mkdir(argv[2], 0777);
	if (fd_dest < 0){
		syserror(mkdir, argv[2]);
	}
	copy_directory(argv[2], argv[1]);

	return 0;
}
