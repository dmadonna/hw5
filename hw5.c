#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>


/* 
o7
 _________  ___  ___  _______           ________  ________  ________        ___  _______   ________ _________  ___       
|\___   ___\\  \|\  \|\  ___ \         |\   __  \|\   __  \|\   __  \      |\  \|\  ___ \ |\   ____\\___   ___\\  \      
\|___ \  \_\ \  \\\  \ \   __/|        \ \  \|\  \ \  \|\  \ \  \|\  \     \ \  \ \   __/|\ \  \___\|___ \  \_\ \  \     
     \ \  \ \ \   __  \ \  \_|/__       \ \   ____\ \   _  _\ \  \\\  \  __ \ \  \ \  \_|/_\ \  \       \ \  \ \ \  \    
      \ \  \ \ \  \ \  \ \  \_|\ \       \ \  \___|\ \  \\  \\ \  \\\  \|\  \\_\  \ \  \_|\ \ \  \____   \ \  \ \ \__\   
       \ \__\ \ \__\ \__\ \_______\       \ \__\    \ \__\\ _\\ \_______\ \________\ \_______\ \_______\  \ \__\ \|__|   
        \|__|  \|__|\|__|\|_______|        \|__|     \|__|\|__|\|_______|\|________|\|_______|\|_______|   \|__|     ___ 
                                                                                                                    |\__\
                                                                                                                    \|__|
                                                                                                                      
things to do:
	* allow custom backup directory... (Ryland can finish this)
	* allow timestamp backup filenames...
	* disable meta-data copying... (very easy) (Ryland can finish this)
	* help output...
	* need to change meta to copy group and owner data, as well as time and type... (Ryland can finish this)
	* figure out how to catch when the file is deleted (currently doesnt work...)
*/

int copy_file(char* backup_path, char* file_name, int target_fd, int rev_count, char** file_name_buffer)
{
	char buffer[2056];
	strcpy(buffer, backup_path);
	char temp[1024];
	strcat(temp, file_name);
	strcat(temp, "_rev");
	char count[32];
	sprintf(count, "%d", rev_count);
	strcat(temp, count);
	*file_name_buffer = temp;
	strcat(buffer, temp);
	int backup_fd = open(buffer, O_CREAT | O_RDWR | O_APPEND);
	while(1)
	{
		ssize_t result = read(target_fd, &buffer[0], sizeof(buffer));
		if(!result) break;
		assert(result > 0);
		write(backup_fd, &buffer[0], result);
	}
	return 0;
}

int copy_meta(char* target_path, char* backup_path, char* current_backup_filename)
{
	char temp[1024];
	strcpy(temp, backup_path);
	char temp2[1024];
	strcpy(temp2, current_backup_filename);
	strcat(temp, temp2);
	char* current_full_path = temp;
	struct stat target_stats;
	struct stat backup_stats;
	int err = stat(target_path, &target_stats);
	if(err == -1)
	{
		printf("failed to locate target file's meta!\n");
		return 1;
	}
	err = stat(current_full_path, &backup_stats);
	if(err == -1)
	{
		printf("failed to locate backup file's meta!\n");
		return 1;
	}

	if(chmod(current_full_path, target_stats.st_mode) == -1)
	{
		printf("dont have permissions to change file modes...\n");
		return 1;
	}
	return 0;
}


int main(int argc, char* argv[])
{

	//argument interpretation...
	// option flags
	bool opt_t = false, opt_d = false, opt_m = false;
	// the potential backup path
	char* backup_path;
	// the path to our file
	char* target_file_path;
	// backup file descriptor hook
	int backup_fd;
	// target file descriptor hook
	int target_fd;
	//	inotify hook
	int inotify_fd;
	// revision counter...
	int revision_count = 0;
	// file name...
	char* file_name;
	// newest backup filename...
	char* current_backup_file;
	int opt = getopt(argc, argv, "hd:mt");
	while(opt != -1)
	{
		if(opt == 'h')
		{
			printf("print useful info here...\n");
			return EXIT_SUCCESS;
			//e_arg = argv[optind];
		}
		else if (opt == 'd')
		{
			opt_d = true;
			backup_path = argv[optind];
		}
		else if (opt == 'm')
		{
			opt_m = true;
		}
		else if (opt == 't')
		{
			opt_t = true;
		}
		opt = getopt(argc, argv, "hd:mt");
	}

	//get the file path...
	target_file_path = argv[optind];
	// figure out file name...
	file_name = strrchr(target_file_path, '/');
	if(file_name == NULL)
	{
		file_name = argv[optind];
	}

	// see if we can create a valid inotify hook...
	inotify_fd = inotify_init();
	if(inotify_fd == -1)
	{
		printf("failed inotify hook! no idea how that happens...\n");
		return 1;
	}

	// did the user specify a directory?
	if(!opt_d)
	{
		// if not we need to find out if  we can work in the current working directory...
		// source: stackoverflow.com/questions/298510/
		char cwd[1024];
		if(getcwd(cwd, sizeof(cwd)) == NULL)
		{
			printf("failed to get current working directory!\n");
			return 1;
		}

		// if we can then make a backup directory inside cwd/backup/
		strcat(cwd, "/backup/");
		// hold this as our backup path for later use...
		backup_path = cwd;
		if(mkdir(cwd, S_IRWXU | S_IRWXG | S_IRWXO) == -1)
		{
			// did we fail because the directory already exists?

			if(errno != EEXIST)
			{	
				// if the directory doesnt exist then close...
				printf("failed to create backup directory!\n");
				return 1;
			}
			
		}

		// create a file in our directory!
		char buffer[2056];
		strcpy(buffer, target_file_path);
		target_fd = open(buffer, O_RDONLY);
		// create a copy of the file to our backup directory...
		char* current_backup_filename;
		copy_file(backup_path, file_name, target_fd, revision_count, &current_backup_filename);
		// copy meta data...
		copy_meta(file_name, backup_path, current_backup_filename);
		// increment file count...
		revision_count++;
	}

	// begin watching the file...
	int wd = inotify_add_watch(inotify_fd, target_file_path, IN_OPEN | IN_MODIFY | IN_DELETE | IN_DELETE_SELF | IN_IGNORED);
	if(wd)
	{
		int r = 0;
		char buf[512];
		int buf_len = 512;
		char* p;
		while(true)
		{
			r = read(inotify_fd, buf, buf_len);
			for(p = buf; p < buf + r;)
			{
				struct inotify_event* event = (struct inotify_event*)p;
				p += sizeof(struct inotify_event) + event->len;
				if(event->mask == IN_DELETE || event->mask == IN_DELETE_SELF || event->mask == IN_IGNORED){
					printf("original file deleted!\n");
					return EXIT_SUCCESS;
				}
				if(event->mask == IN_MODIFY)
				{
					printf("file modified!\n");
					copy_file(backup_path, file_name, target_fd, revision_count, &current_backup_filename);
					// copy meta data...
					copy_meta(file_name, backup_path, current_backup_filename);
					// increment file count...
					revision_count++;
					break;
				}
				if(event->mask == IN_OPEN)
				{
					printf("file opened!\n");
					break;
				}
			}
		}
	} 
	else
	{
		printf("failed to watch file...\n");
		return 1;
	}
	return EXIT_SUCCESS;
}