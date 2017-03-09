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
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <utime.h>


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
	* allow timestamp backup filenames...
	* help output...
	* figure out how to catch when the file is deleted (currently doesnt work...)
*/

int copy_file(char* backup_path, char* file_name,  int target_fd, int rev_count, char** file_name_buffer)
{
	printf("biffer: %s\n", backup_path);
	char buffer[2056] = "";
	strcat(buffer, backup_path);
	printf("biffer: %s\n", buffer);
	char temp[1024] = "";
	strcat(temp, file_name);
	strcat(temp, "_rev");
	char count[32] = "";
	sprintf(count, "%d", rev_count);
	strcat(temp, count);
	*file_name_buffer = temp;
	strcat(buffer, temp);
	printf("biffer: %s\n", buffer);
	int backup_fd = open(buffer, O_CREAT | O_RDWR | O_APPEND);
	while(1)
	{
		ssize_t result = read(target_fd, &buffer[0], sizeof(buffer));
		if(!result) break;
		//assert(result > 0);
		write(backup_fd, &buffer[0], result);
	}
	printf("biffer: %s\n", backup_path);
	return 0;
}

int copy_meta(char* target_path, char* backup_path, char* current_backup_filename)
{
	// do some stuff to get the direct backup file path...
	char temp[1024] = "";
	strcpy(temp, backup_path);
	char temp2[1024] = "";
	strcpy(temp2, current_backup_filename);
	strcat(temp, temp2);
	char* current_full_path = temp;

	// make some structs to handle permissions...
	struct stat target_stats;
	struct stat backup_stats;
	// get the target file's permissions
	printf("%s\n", target_path);
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
	// copy the permissions from the target to the backup.
	if(chmod(current_full_path, target_stats.st_mode) == -1)
	{
		printf("dont have permissions to change file modes...\n");
		return 1;
	}

	// try to copy the user data...
	if(chown(current_full_path, target_stats.st_uid, target_stats.st_gid) == -1)
	{
		printf("failed to copy user flags to backup!\n");
		return 1;
	}

	// get the timestamp from the target file...
	struct utimbuf target_time;
	target_time.actime = target_stats.st_ctime;
	target_time.modtime = target_stats.st_mtime;
	// try to copy the timestamp to the backup...
	if(utime(current_full_path, &target_time) == -1)
	{
		printf("failed to creation time to backup!\n");
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
	int opt = getopt(argc, argv, "hdmt");
	while(opt != -1)
	{
		if(opt == 'h')
		{
			printf("Usage Information: FILE_NAME this watches the specified file, -d optional->BACKUP_LOCATION otherwise a backup folder will be made, -m: Sets default permissions for new copy, -t: Change the time of the original file to time backup occured\n");
			return EXIT_SUCCESS;
			//e_arg = argv[optind];
		}
		else if (opt == 'd')
		{
			opt_d = true;
			backup_path = argv[optind];
			printf("%s\n", backup_path);
		}
		else if (opt == 'm')
		{
			opt_m = true;
		}
		else if (opt == 't')
		{
			opt_t = true;
		}
		else if (opt == '?')
		{
			printf("Invalid Option, Available Options: -d optional->BACKUP_LOCATION otherwise a backup folder will be made, -m: Sets default permissions for new copy, -t: Change the time of the original file to time backup occured\n");
			return EXIT_SUCCESS;
		}
		opt = getopt(argc, argv, "hdmt");
	}

	if (argc == 1) {
		printf("No Argument! Usage Information: FILE_NAME this watches the specified file, -d optional->BACKUP_LOCATION otherwise a backup folder will be made, -m: Sets default permissions for new copy, -t: Change the time of the original file to time backup occured\n");
		return 1;
	}

	//get the file path...
	target_file_path = argv[argc-1];
	printf("%s\n", target_file_path);
	// figure out file name...
	file_name = strrchr(target_file_path, '/');
	printf("%s\n", target_file_path);
	if(file_name == NULL)
	{
		file_name = argv[argc-1];
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
		char cwd[1024] = "";
		if(getcwd(cwd, sizeof(cwd)) == NULL)
		{
			printf("failed to get current working directory!\n");
			return 1;
		}

		// if we can then make a backup directory inside cwd/backup/
		strcat(cwd, "/backup/");
		// hold this as our backup path for later use...
		backup_path = cwd;

		printf("bk%s\n", backup_path);
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
	}
	else
	{
		// check if the given directory exists...
		// stackoverflow.com/questions/12510874
		struct stat directory_stats;
		printf("%s\n", backup_path);
		int check = stat(backup_path, &directory_stats);
		if(check == -1)
		{
			printf("super fail!\n");
			return 1;
		}
		if((directory_stats.st_mode & S_IFDIR) == 0)
		{
			printf("directory specified does not exist!\n");
			return 1;
		}
	}

	// create a file in our directory!
	printf("bk%s\n", backup_path);
	target_fd = open(target_file_path, O_RDONLY);
	if(target_fd == -1)
	{
		printf("lost the file?\n");
		return 1;
	}
	// create a copy of the file to our backup directory...
	char* current_backup_filename;
	printf("sdfdsfdsf%s\n", backup_path);
	copy_file(backup_path, file_name, target_fd, revision_count, &current_backup_filename);
	//  check to see if we want to copy meta...
	if(!opt_m)
	{
		// copy meta data...
		copy_meta(target_file_path, backup_path, current_backup_filename);
	}
	
	// increment file count...
	revision_count++;

	printf("%s\n", target_file_path);
	// begin watching the file...
	int wd = inotify_add_watch(inotify_fd, target_file_path, IN_OPEN | IN_MODIFY | IN_DELETE | IN_DELETE_SELF | IN_IGNORED);
	if(wd)
	{
		int r = 0;
		char buf[512] = "";
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
					//  check to see if we want to copy meta...
					if(!opt_m)
					{
						// copy meta data...
						copy_meta(target_file_path, backup_path, current_backup_filename);
					}
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