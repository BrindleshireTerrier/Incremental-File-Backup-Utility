/*
   Title: backupfiles.c
   Author: Calvin Mohammed
   Purpose: Recursively traverse through the  directory inputted
            by the user. displaying information about each file/directory 
			encountered that has a modification time newer than the time
			specified specified by the user in the command line.
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

// Stores time limit basis to skip nftw
static char* timeLimit;

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
  const char* months[12] = {
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
  snprintf(str, TIME_SIZE, "%s %2d  %02d:%02d", months[timeInfo.tm_mon], \
    timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min);

  return str;
}
/*
   Name: formatTimeStr
   Purpose: Turn a generic time_t object into that of the format: 
            YYYY-MM-DD hh:mm:ss
			
   Parameters: time_t t
   return: str (char*) the formatted string 
*/
char* formatTimeStr(time_t t) {
  char* str = malloc(TIME_SIZE);
  struct tm timeInfo = *localtime(&t);

  snprintf(str, TIME_SIZE, "%d-%02d-%02d %02d:%02d:%d", timeInfo.tm_year \
			+ 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, \
			timeInfo.tm_min, timeInfo.tm_sec);

  return str;
}

/*
   Name: getGroupName()
   Purpose: Retrieve the group name that a file belongs too given the ID.
   Parameters: int groupID, retrieved from the stat struct for a given file
   return: char* the name of the group
*/
char* getGroupName(int groupID) {
  struct group *gPoint;
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
  struct passwd *pPoint;
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
   Name: toPower
   Purpose: Raises a value to a given exponent and outputs the result. Multiply-
		    ing the value exponent times. 
				   
			Parameters: int val: the value to multiply multiple times (the base)
			            int ex: the exponent that represents the number of times
						        it will multiply the val
   return: return val 
*/
int toPower(int val, int ex) {
  int base = val;
  for (int i = 0; i < ex - 1; i++) {
    val *= base;
  }

  return val;
}

/*
   Name: convertStrToNum
   Purpose: Takes in a string and converts it to an integer.
            eg: "2500" == > 2500
				   
			Parameters: char* str: the string to convert
   return: return num (integer value of str)
*/
int convertStrToNum(char* str) {
  
  int num = 0; // will store the converted string
  int numLength = strlen(str); // get number of characters in string
                               // this will represent number of powers of 10
  int exponent = numLength - 1; // represents the exponent of 10^e
  
  for (int i = 0; i < numLength; i++) {
    int digit = str[i] - '0'; // gets difference between ascii value of 0 and 
	                          // ascii value of current character, this will
							  // give the digit
    if (exponent > 0) 
      digit *= toPower(10, exponent); // multiply the digit by 10^exponent 
    else
      digit *= 1; // else 10^0 = 1 therefore just multiply by 1
    exponent--; // decrement the exponent as it reads left to right and 
	            // therefore goes from highest power to lowest power
    num += digit; // store the result as a sum in num
  }

  return num;
}

/*
   Name: convertTimeToArr
   Purpose: Takes in the time string hh:mm:ss and outputs an array of ints
			[hh, mm, ss]
				   
			Parameters: char* time
   return: return array of integers (timeArr)
*/
int* convertTimeToArr(char* time) {
  int* timeArr = malloc(3);
  
  // define hours string
  char hours[3] = {
    time[0],
    time[1],
    '\0'
  };
  // define minutes string
  char minutes[3] = {
    time[3],
    time[4],
    '\0'
  };
  // define seconds string
  char seconds[3] = {
    time[6],
    time[7],
    '\0'
  };

  // convert strings to integers
  int hoursNum = convertStrToNum(hours);
  int minutesNum = convertStrToNum(minutes);
  int secondsNum = convertStrToNum(seconds);

  // set indexes of array to corresponding integer values
  timeArr[0] = hoursNum;
  timeArr[1] = minutesNum;
  timeArr[2] = secondsNum;

  return timeArr;
}

/*
   Name: convertCalToArr
   Purpose: Takes in the calendar string YYYY-MM-DD and outputs an array of ints
			[YYYY, MM, DD]
				   
			Parameters: char* cal
   return: return array of integers (calArr)
*/
int* convertCalToArr(char* cal) {
  int* calArr = malloc(3);
  
  // define year string
  char year[5] = {
    cal[0],
    cal[1],
    cal[2],
    cal[3],
    '\0'
  };
  // define month string
  char month[3] = {
    cal[5],
    cal[6],
    '\0'
  };
  // define day string
  char day[3] = {
    cal[8],
    cal[9],
    '\0'
  };
  // convert each string into an integer
  int yearNum = convertStrToNum(year);
  int monthNum = convertStrToNum(month);
  int dayNum = convertStrToNum(day);
  
  // set relevant indexes to corresponding integer values
  calArr[0] = yearNum;
  calArr[1] = monthNum;
  calArr[2] = dayNum;

  return calArr;
}


/*
   Name: t1GTt2
   Purpose: Compares two strings that are of the format:
			YYYY-MM-DD hh:mm:ss
			and returns 1 if t1 is greater than t2 else prints 0
			
			Parameters: string t1 of size 21, string t2 of size 21
   return: return 1 if t1 is greater and 0 if not
*/
int t1GTt2(char t1[21], char t2[21]) {
  char* t2Copy = malloc(48);
  strcpy(t2Copy, t2);

  const char split[2] = " ";
  
  // split the two strings into respective YYYY-MM-DD and hh:mm:ss
  // as seperate strings
  
  char* t1Cal = strtok(t1, split); // t1 YYYY-MM-DD
  char* t1Time = strtok(NULL, split); // t1 hh:mm:ss
  
  char* t2Cal = strtok(t2Copy, split); // t2 YYYY-MM-DD
  char* t2Time = strtok(NULL, split); // t2 hh:mm:ss
  
  // convert the strings to arrays of numbers
  // index mapping: calendar: [YYYY, MM, DD]
  // index mapping: time: [hh, mm, ss]
  
  // t1
  int* t1CalArr = convertCalToArr(t1Cal); 
  int* t1TimeArr = convertTimeToArr(t1Time);

  // t2
  int* t2CalArr = convertCalToArr(t2Cal);
  int* t2TimeArr = convertTimeToArr(t2Time);

  // t1 year > t2 year
  if (t1CalArr[0] > t2CalArr[0]) { 
    return 1;
	          // assume year equal, t1 month > t2 month
  } else if (t1CalArr[0] == t2CalArr[0] && t1CalArr[1] > t2CalArr[1]) { 
    return 1;
	         // assume year and month equal, t1 monthday > t2 monthday
  } else if (t1CalArr[0] == t2CalArr[0] && t1CalArr[2] > t2CalArr[2]) { 
    return 1;
	         //assume the calendar strings are equal, now compare time str
  } else if (t1CalArr[0] == t2CalArr[0] && t1CalArr[1] == t2CalArr[1] 
             && t1CalArr[2] == t2CalArr[2]) {
	     // t1 hour > t2 hour
	if (t1TimeArr[0] > t2TimeArr[0]) {
	  return 1;
	            // assume hours equal, t1 minute > t2 minute
	} else if (t1TimeArr[0] == t2TimeArr[0] && t1TimeArr[1] > t2TimeArr[1]) {
	  return 1;
	            // assume hours and minutes equal, t1 seconds > t2 seconds
	} else if (t1TimeArr[0] == t2TimeArr[0] && t1TimeArr[1] == t2TimeArr[1] 
	           && t1TimeArr[2] > t2TimeArr[2]) {
	  return 1;
	}
  }

  return 0;
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
   return: char* of size 10 representing the permission format of ls -l
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
  DIR * directPoint = opendir(dir);

  // if unable to read the directory exit
  if (directPoint == NULL) {
    printf("Error in readDir: Could not open directory");
    return -1;
  }

  // dirent struct provides file name information and determines whether
  // the file pointer is actually pointing to a file
  struct dirent * entry;

  // iterate through the directory until the directory pointer sees no file left
  // in the directory
  while ((entry = readdir(directPoint)) != NULL) {

    struct stat fileData; // to retrieve user ids, group ids and mod times
    // for a given file
    char buffer[BUFFER_SIZE]; // declare a string that stores the current
    // file being looked at
    char path[1024]; // declare a string that stores the absolute
    // path of the current file being looked at

    strcpy(path, dir);
    strcat(path, "/%s");
    snprintf(buffer, BUFFER_SIZE, path, entry -> d_name);

    stat(buffer, & fileData); // accesses a struct that contains information
    // for the current file stored in the buffer

    // determines whether the current file is newer than the cut off time
    if (t1GTt2(formatTimeStr(fileData.st_mtime), timeLimit) == 1) {
      // Retrieve/store information for given file			 
      char* name = entry -> d_name; // Name of file/directory
      char* time = formatTime(fileData.st_mtime); // last modification time 
      //of file
      int size = fileData.st_size; // Size of file in bytes
      int noOfLinks = fileData.st_nlink; // Number of links to file
      char* userName = getUserName(fileData.st_uid); // User who created file
      char* groupName = getGroupName(fileData.st_gid); // Group file belongs 
      //to
      char* permissions = getPermissions(fileData.st_mode); // Permission 
      //string

      // Output the fileInfo() of the current file
      printf("%s\n", fileInfo(name, time, noOfLinks, userName, groupName, \
        permissions, size));
    }
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
static int traverse(const char* fpath,
  const struct stat *fileInfo,
    int tflag, struct FTW *ftwbuf) {
  // Determine if file is a direcetory
  if (S_ISDIR(fileInfo -> st_mode) == 1) {
    // char* workDir = malloc(BUFFER_DIR);
    // Change current directory 
    chdir(fpath + ftwbuf -> base);
    printf("%s\n", fpath);

    // Output information of all files in given directory
    if ((readDir(fpath)) == -1) {
      perror("Couldn't read directory");
      return -1;
    }
  }

  return 0;
}
/*
   Name: isValidTime
   Purpose: Given a time string retrieved from the command line, see whether
            it passes a series of tests so that there is a standard way
			for comparing two time strings (t1 and t2).
			
			Parameters: time string
   return: return 1 if valid time
*/
int isValidTime(char* time) {


   // FORMAT TO FOLLOW: YYYY-MM-DD hh:mm:ss

  // See if first character of YYYY is inclusive of 0-9
  if (time[0] < '0' || time[0] > '9') {
    printf("Invalid value for first position of YYYY, must be in 0-9 range\n");
    return -1;
	         // see if second character of YYYY is inclusive of 0-9
  } else if (time[1] < '0' || time[1] > '9') {
    printf("Invalid value for second position of YYYY, must be in 0-9 range\n");
    return -1;
	         // see if third character of YYYY is inclusive of 0-9
  } else if (time[2] < '0' || time[2] > '9') {
    printf("Invalid value for third position of YYYY, must be in 0-9 range\n");
    return -1;
	          // see if fourth character of YYYY is inclusive of 0-9
  } else if (time[3] < '0' || time[3] > '9') {
    printf("Invalid value for last position of YYYY, must be in 0-9 range\n");
    return -1;
		// see if there is a delimiter between YYYY and MM
  } else if (time[4] != '-') {
    printf("- delimiter required between YYYY and MM\n");
    return -1;
	   // see if MM first character is either 0 or 1
  } else if (time[5] < '0' || time[5] > '1') {
    printf("Invalid value for first position of MM, must be in 0-1 range\n");
    return -1;
	    // see if last character is either of form: 0X where 0</= X <= 9 or
		// 1Y where 0<= Y <= 2
  } else if (time[6] < '0' || time[6] > '9' || time[6] > '2'\
             && time[5] == '1') {
    printf("Invalid value for last position of MM, must be in 0-9 range\n");
    return -1;
	    // ensures delimiter between MM and DD
  } else if (time[7] != '-') {
    printf("- delimiter required between MM and DD\n");
    return -1;
	    // ensures that day of month is within range 0-3 inclusive
  } else if (time[8] < '0' || time[8] > '3') {
    printf("Invalid value for first position of DD, must be in 0-3 range\n");
    return -1;
	    // ensures that day of month is of the form: 0|1|2X where X is within
		// range 0-9 or if 3Y where Y is either 0 or 1
  } else if (time[9] < '0' || time[9] > '9' || (time[9] > '1'\
             && time[8] == '3')) {
    printf("Invalid value for last position of DD, must be in 0-9 range\n");
    return -1;
	    // ensures space between YYYY-MM-DD and hh:mm:ss
  } else if (time[10] != ' ') {
    printf("Must have space between YYYY-MM-DD and hh:mm:ss\n");
    return -1;
	     // ensures hh is from 0-2 inclusive
  } else if (time[11] < '0' || time[11] > '2') {
    printf("Invalid value for first position of hh, must be 0-2 range\n");
    return -1;
	        // ensures that second character of hh does not exceed 23 hours
  } else if (time[12] < '0' || time[12] > '9' || (time[12] > '3'\
             && time[11] == '2')) {
    printf("Invalid value for second position of hh, must be 0-9 range\n");
    return -1;
	        // ensures : delimiter between hh and mm
  } else if (time[13] != ':') {
    printf("Must have : between hh and mm\n");
    return -1;
	       // ensures that first character of mm does not exceed 5 (so 
		   // no going over 59 minutes
  } else if (time[14] < '0' || time[14] > '5') {
    printf("Invalid value for first position of mm, must be in 0-6 range\n");
    return -1;
	      // ensures second character of mm is within range 0-9 inclusive
  } else if (time[15] < '0' || time[15] > '9') {
    printf("Invalid value for second position of mm, must be in 0-9 range\n");
    return -1;
	     // ensure : delimiter between mm and ss
  } else if (time[16] != ':') {
    printf("Must have : between mm and ss\n");
    return -1;
	      // ensure that first character of ss does not exceed 5 
		  // so no going over 59 seconds
  } else if (time[17] < '0' || time[17] > '5') {
    printf("Invalid value for first position of mm, must be in 0-6 range\n");
    return -1;
	      // ensure that second charcter of ss is range 0-9 inclusive
  } else if (time[18] < '0' || time[18] > '9') {
    printf("Invalid value for second position of mm, must be in 0-9 range\n");
    return -1;
  }

  return 1;
}

/*
   Name: commandLineSwitch
   Purpose: Iterates through the commands from argv[1] to argv[argc-1]
            and processes it based on the following rules:
			if -h, print help message then exit (return 1 to show success)
			if -t: look at argv[i+1] and test validity of the next argument
			       if the result is a valid time, then the command following
				   -t must be a cut off time, therefore set the cutoff time.
				   If it is not a valid string, it may be stating a file
				   to get the modification time of this. 
				   If the file doesn't exist it will exit and print an error 
				   message.
			If no filename or timestring is provided it will use the default
			1970-01-01 00:00:00 cutoff time
			
			The last argument HAS to be a directory to read
			
				   
			
			Parameters: char* argv[]: arrays of arguments
					   int sizeOfArgs: number of arguments
   return: return 1 if success
*/
int commandLineSwitch(char* argv [], int sizeOfArgs) {
	
	char* time = "1970-01-01 00:00:00"; 
	char* directory = "."; 
	char* file;


	for(int i = 1; i < sizeOfArgs; i++) { 
	  if(strcmp(argv[i], "-h") == 0) {
	    printf("\n");
	    printf("Switches: -t | -h (can appear in any order\n");
  	    printf("-t <filename|time> used to set last modification time\n"); 
	    printf("filename gets last modification time of file\n");
	    printf("time format YYYY-MM-DD hh:mm:ss to set cut off time\n");
	    printf("If no filename or time is present, it will use default \
		   1970-01-01 00:00:00\n");
	    printf("-h displays this current message\n");
	    printf("Last command must be the directory to look at\n");
	    printf("Example format: ./backupfiles -t -h .\n");
	     return 1;
	  }
	  if(strcmp(argv[i], "-t") == 0 &&\
	     strcmp(argv[i+1], "-h") != 0 && i != sizeOfArgs-2) { 
			
	     if(isValidTime(argv[i+1]) == 1) { 
               time = argv[i+1];
	     } else { // else it is filename
	        file = realpath(argv[i+1], NULL); 
	     	if(file == NULL) { 
	          printf("Error in commandLineSwitch: No such file\n"); 
	          return -1;
	        }
		// get modification time of file
		struct stat fileInfo; 
		stat(file, &fileInfo); 
		time = formatTimeStr(fileInfo.st_mtime);
		}
	     }	
	}
	directory = realpath(argv[sizeOfArgs-1], NULL);
	// test if directory exists
	if(opendir(directory) == NULL) {
		 printf("Error in commandLineSwitch: Directory doesn't exist\n");
		 return -1;
	}
			
	timeLimit = time; 
	nftw(directory, traverse, 20, FTW_D);
	
	return 1;
}
int main(int argc, char * argv[]) {
 if ((commandLineSwitch(argv, argc) == -1)) { 
	return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
