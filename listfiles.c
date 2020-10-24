/*
   Title: listfiles.c
   Author: Calvin Mohammed
   Purpose: Recursively traverse through the relative directory "." displaying
	        information about each file/directory encountered.
   Version: 1.0
*/
#define _XOPEN_SOURCE 500
#include <stdio.h> 
#include <time.h> 
#include <fcntl.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <dirent.h> 
#include <ftw.h> 
#include <sys/stat.h> 
#include <grp.h> 
#include <pwd.h> 
#include <string.h> 


// SYMBOLIC CONSTANTS
#define BUFFER_DIR (1024)
#define BUFFER_SIZE (1024)
#define TIME_SIZE (128)
#define INFOSTR_SIZE (2048)



/*
   Name: formatTime
   Purpose: Turn a generic time_t object into that of the format: 
            MONTH MONTH_DAY  HOUR:MINUTE
			for displaying information for a given file. Similar to ls -l
   Parameters: time_t t
   return: str (char*) 
*/
char* formatTime(time_t t) {
  // define array of months that map to the corresponding number
  const char * months[12] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
  };

  // define the string that will contain the time information
  char* str = malloc(TIME_SIZE);
  
  // utilize the struct tm to extract information from time_t
  struct tm timeInfo = *localtime(&t);
  snprintf(str, TIME_SIZE, "%s %2d  %02d:%02d", months[timeInfo.tm_mon],\
		   timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min);

  return str;
}


/*
   Name: getGroupName()
   Purpose: Retrieve the group name that a file belongs too given the ID.
   Parameters: int groupID, retrieved from the stat struct for a given file
   return: char* the name of the group
*/
char* getGroupName(int groupID) {
  struct group* gPoint;
  gPoint = getgrgid(groupID);
  return gPoint -> gr_name;
}
/*
   Name: getUserName()
   Purpose: Retrieve the username of the account that created the file.
   Parameters: int userID, retrieved from the stat struct for a given file
   return: char* the name of the user
*/
char* getUserName(int userID) {
  struct passwd* pPoint;
  pPoint = getpwuid(userID);
  return pPoint -> pw_name;
}
/*
   Name: getPermissions()
   Purpose: Given a file, determine what access permissions this given file has
            and present them in the following format:
			(1)d|-  : d = directory, - = file
			(2)r|-  : r = user read permission, - = user no read permission
			(3)w|-  : w = user write permission, - = user no write permission
			(4)x|-  : x = user execute permission, - = user no execute permiss-
							ion
			(5)r|-  : r = group read permission, - = user no read permission
			(6)w|-  : w = group write permission, - = user no write permission
			(7)x|-  : x = group execute permission, - = user no execute permiss-
							ion
			(8)r|-  : r = other read permission, - = user no read permission
			(9)w|-  : w = other write permission, - = user no write permission
			(10)x|- : x = other execute permission, - = user no execute permiss-
							ion				
   Parameters: int fileMode
   return: char* of size 10 representing the permission format of ls -l
*/
char* getPermissions(int fileMode) {
  char* perStr = malloc(10);
  // directory or not
  if (S_ISDIR(fileMode) == 1) {
    perStr[0] = 'd';
  } else {
    perStr[0] = '-';
  }

  // User permissions
  // read
  if ((fileMode & S_IRUSR) > 0) {
    perStr[1] = 'r';
  } else {
    perStr[1] = '-';
  }
  // write
  if ((fileMode & S_IWUSR) > 0) {
    perStr[2] = 'w';
  } else {
    perStr[2] = '-';
  }
  // execute
  if ((fileMode & S_IXUSR) > 0) {
    perStr[3] = 'x';
  } else {
    perStr[3] = '-';
  }

  // Group permissions
  // read
  if ((fileMode & S_IRGRP) > 0) {
    perStr[4] = 'r';
  } else {
    perStr[4] = '-';
  }
  // write
  if ((fileMode & S_IWGRP) > 0) {
    perStr[5] = 'w';
  } else {
    perStr[5] = '-';
  }
  // execute
  if ((fileMode & S_IXGRP) > 0) {
    perStr[6] = 'x';
  } else {
    perStr[6] = '-';
  }

  // other permissions
  // read
  if ((fileMode & S_IROTH) > 0) {
    perStr[7] = 'r';
  } else {
    perStr[7] = '-';
  }
  // write
  if ((fileMode & S_IWOTH) > 0) {
    perStr[8] = 'w';
  } else {
    perStr[8] = '-';
  }
  // execute
  if ((fileMode & S_IXOTH) > 0) {
    perStr[9] = 'x';
  } else {
    perStr[9] = '-';
  }
  // terminate the permission string
  perStr[10] = '\0';
  return perStr;
}
/*
   Name: fileInfo
   Purpose: Given all information retrieved from the file, present it in a 
		    format that of ls -l:
			[permissionString links username groupname size date time filename]
			Parameters: char* name: Name of the file
						char* time: formatted time string (from formatTime())
									it is most recent modification time of the
									file
						int links: No of links to the file
						char* userName: User who created the file
						char* groupName: The group the file belongs to
						char* permissions: Permission string from 
											getPermissions()
						int size: Size of the file in bytes
			it will print it out in a format that of ls -l.
   return: char* string
*/
char* fileInfo(char* name, char* time, int links, char* userName, 
			   char* groupName, char* permissions, int size) {
  char* fileInfo = malloc(INFOSTR_SIZE);

  snprintf(fileInfo, INFOSTR_SIZE, "%s %2d %s %10s %8d %12s %s",
	   permissions, links, userName, groupName, 
	   size, time, name);

  return fileInfo;
}
/*
   Name: readDir
   Purpose: Given a directory, the subroutine determines all files that exist
			in the directory and retrieves the following information:
			- Name of file
			- Latest modification time of file
			- Size of file
			- No of links to file
			- username who created the file
			- groupname the file belongs to
			- permissions of the file
			It them displays the information using fileInfo(). 
			
			Parameters: Directory to traverse
   return: return 0 on success
*/
int readDir(const char* dir) {
	
  
  // declare a pointer to the directory argument
  DIR* directPoint = opendir(dir);
  

  // dirent struct provides file name information and determines whether
  // the file pointer is actually pointing to a file
  struct dirent *entry;

  // iterate through the directory until the directory pointer sees no file left
  // in the directory
  while ((entry = readdir(directPoint)) != NULL) {
	  
	
    struct stat fileData; // to retrieve user ids, group ids and mod times
						  // for a given file
    char buffer[BUFFER_SIZE]; // declare a string that stores the current
							  // file being looked at
    char path[1024];		 // declare a string that stores the absolute
							  // path of the current file being looked at
	
	
    strcpy(path, dir);
    strcat(path, "/%s");
    snprintf(buffer, BUFFER_SIZE, path, entry -> d_name);

    stat(buffer, &fileData); // accesses a struct that contains information
							 // for the current file stored in the buffer
							 
	// Retrieve/store information for given file			 
    char* name = entry -> d_name; // Name of file/directory
    char* time = formatTime(fileData.st_mtime); //last modification time of file
    int size = fileData.st_size; // Size of file in bytes
    int noOfLinks = fileData.st_nlink; // Number of links to file
    char* userName = getUserName(fileData.st_uid); // User who created file
    char* groupName = getGroupName(fileData.st_gid); // Group file belongs to
    char* permissions = getPermissions(fileData.st_mode); // Permission string
	
	// Output the fileInfo() of the current file
    printf("%s\n", fileInfo(name, time, noOfLinks, userName, groupName, \
							permissions, size));
    
  }
  
  // clear memory
  closedir(directPoint);
  
  return 0;
}
/*
   Name: traverse
   Purpose: Navigates through the directory tree, distinguishing between file
			and directory and then performs the readDir() routine for each
			directory in the hierarchy. 
		
			Parameters: const char *fpath: File path to start from
			            const struct stat *sb: Access information to determine
						 whether directory or not
						int tflag: 
						FTW *ftwbuf: To determine the current file being looked
						             at in the hierarchy
   return: returns 0 to tell the nftw subroutine to keep searching the tree
*/
static int traverse(const char * fpath, const struct stat *fileInfo, 
				     int tflag, struct FTW * ftwbuf) {
  // Determine if file is a direcetory
  if (S_ISDIR(fileInfo -> st_mode) == 1) {
	  	
	// Change current directory relative to root of fpath
    chdir(fpath + ftwbuf -> base);

	// State what directory you are currently in
	printf("%s\n", fpath);	
	
	// Output information of all files in given directory
    if((readDir(fpath)) == -1) {
		perror("Couldn't read directory");
		return -1;
	}
  }

  return 0; 
}

int main(int argc, char * argv[]) {
  char* path = realpath(".", NULL); // get absolute path of current directory
  
  /*
    Traverses the tree structure of the directory given the absolute
	path of the current directory.
  */
  if ((nftw(path, traverse, 20, FTW_D) == -1)) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
