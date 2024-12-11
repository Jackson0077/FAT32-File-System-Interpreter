#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>


#define WHITESPACE " \t\n"

#define MAX_COMMAND_SIZE 255

#define MAX_NUM_ARGUMENTS 32    

uint16_t BPB_BytsPerSec;
uint8_t BPB_SecPerClus;
uint16_t BPB_RsvdSecCnt;
uint8_t BPB_NumFats;
uint32_t BPB_FatSz32;
uint16_t BPB_ExtFlags;
uint32_t BPB_RootClus;
uint16_t BPB_FSInfo;

uint32_t fat1Start;
uint32_t totalFatSize;
uint32_t clusterStart;
uint32_t clusterSize;

FILE *file = NULL;
uint32_t currentDirectory;
char path[250] = "";

void setUp(FILE *file)
{
  fseek(file, 11, SEEK_SET);
  fread(&BPB_BytsPerSec, 2, 1, file);

  fseek(file, 13, SEEK_SET);
  fread(&BPB_SecPerClus, 1, 1, file);

  fseek(file, 14, SEEK_SET);
  fread(&BPB_RsvdSecCnt, 2, 1, file);

  fseek(file, 16, SEEK_SET);
  fread(&BPB_NumFats, 1, 1, file);

  fseek(file, 36, SEEK_SET);
  fread(&BPB_FatSz32, 4, 1, file);

  fseek(file, 40, SEEK_SET);
  fread(&BPB_ExtFlags, 2, 1, file);

  fseek(file, 44, SEEK_SET);
  fread(&BPB_RootClus, 4, 1, file);

  fseek(file, 48, SEEK_SET);
  fread(&BPB_FSInfo, 2, 1, file);

  fat1Start = BPB_RsvdSecCnt * BPB_BytsPerSec;
  totalFatSize = BPB_NumFats * BPB_FatSz32 * BPB_BytsPerSec;
  clusterStart = totalFatSize + fat1Start;
  clusterSize = BPB_SecPerClus * BPB_BytsPerSec;

  currentDirectory = BPB_RootClus;
  strcpy(path, "");
}

void convertName(char *input, char *expanded_name)
{
  memset(expanded_name, ' ', 12);

  char *temp = strtok((char *)input, ".");

  strncpy(expanded_name, temp, strlen(temp));

  temp = strtok(NULL, ".");

  if (temp) 
  {
    strncpy(expanded_name + 8, temp, strlen(temp));
  }

  expanded_name[11] = '\0';
}

int isCommand(char *token)
{
  if(strcmp(token, "EXIT") == 0 || strcmp(token, "QUIT") == 0 || strcmp(token, "SAVE") == 0 || strcmp(token, "OPEN") == 0 ||
      strcmp(token, "STAT") == 0 || strcmp(token, "GET") == 0 || strcmp(token, "PUT") == 0 || strcmp(token, "CD") == 0 ||
      strcmp(token, "LS") == 0 || strcmp(token, "READ") == 0 || strcmp(token, "DEL") == 0 || strcmp(token, "UNDEL") == 0)
  {
    return 1; // is a valid command
  }
  else
  {
    return 0;
  }
}
// Parameters: The current sector number that points to a block of data
// Returns: The value of the address for that block of data
// Description: Finds the starting address of a block of data given the sector corresponding to that data block
int LABToOffset(int32_t sector)
{
  return ((sector - 2) * BPB_BytsPerSec) + (BPB_BytsPerSec * BPB_RsvdSecCnt) + (BPB_NumFats * BPB_FatSz32 * BPB_BytsPerSec);
}

// Purpose: Given a logical block address, look up into the first FAT and return the logical block address of the block in the file.
// If there is no further blocks then return -1
int32_t NextLB(uint32_t sector)
{
  uint32_t FATAddress = (BPB_BytsPerSec * BPB_RsvdSecCnt) + (sector * 4);
  int16_t val;
  fseek(file, FATAddress, SEEK_SET);
  fread(&val, 2, 1, file);
  return val;
}

int main()
{

  char * command_string = (char*) malloc( MAX_COMMAND_SIZE );
  char error_message[30] = "An error has occurred\n";
  int fileOpened = 0;
  char orignalFAT32Name[MAX_COMMAND_SIZE];

  struct __attribute__((__packed__)) DirectoryEntry {
    char DIR_Name[11];            // Filename in FAT32 8.3 format
    uint8_t DIR_Attr;             // File attributes
    uint8_t Unused1[8];           // Unused bytes for alignment/padding
    uint16_t DIR_FirstClusterHigh; // High part of the first cluster number (for FAT32)
    uint8_t Unused2[4];           // More unused bytes
    uint16_t DIR_FirstClusterLow;  // Low part of the first cluster number
    uint32_t DIR_FileSize;        // Size of the file in bytes
  };
  

  while( 1 )
  {
    printf ("msf> ");

    while( !fgets (command_string, MAX_COMMAND_SIZE, stdin) );

    char *token[MAX_NUM_ARGUMENTS];
    int token_count = 0;
    char *originalToken[MAX_NUM_ARGUMENTS];                         
                                                           
    char *argument_pointer;                                         
    char *working_string  = strdup( command_string );                
    
    char *head_ptr = working_string;
    
    while ( ( (argument_pointer = strsep(&working_string, WHITESPACE ) ) != NULL) &&
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( argument_pointer, MAX_COMMAND_SIZE );
      originalToken[token_count] = strndup( argument_pointer, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    // make case insensitive by toupper everything
    for(int i = 0; i < token_count; i++)
    {
      if(token[i] != NULL)
      {
        for(int j = 0; j < strlen(token[i]); j++)
        {
          token[i][j] = toupper((char)token[i][j]);
        }
      }
    }

    // built in exit/quit
    if (token[0] != NULL && (strcmp(token[0], "EXIT") == 0 || strcmp(token[0], "QUIT") == 0)) 
    {
      if(token_count == 2)
      {
        free(working_string);
        for (int i = 0; i < token_count; i++) 
        {
          free(token[i]);
        }
        exit(0);
      }
      else
      {
        write(STDERR_FILENO, error_message, strlen(error_message));
        continue;
      }
    }

    // allows for multiple commands in one line (put all commands here)
    for(int i = 0; i < token_count; i++)
    {
      // open
      if(token[i] != NULL && strcmp(token[i], "OPEN") == 0)
      {
        if(fileOpened == 1)
        {
          printf("Error: File system image already open.\n");
        }
        else if(token_count < 3)
        {
          write(STDERR_FILENO, error_message, strlen(error_message));
          continue;
        }
        else
        {
          file = fopen(originalToken[i+1], "r+");
          if(file == NULL)
          {
            printf("Error: File system image not found.\n");
          }
          else // successful
          {
            fileOpened = 1;
            // set the original FAT32 image name
            strncpy(orignalFAT32Name, originalToken[i+1], MAX_COMMAND_SIZE);
            // get info about system
            setUp(file);
          }
        }
      }

      // close
      if(token[i] != NULL && strcmp(token[i], "CLOSE") == 0)
      {
        if(fileOpened == 0)
        {
          printf("Error: File system not open.\n");
          continue;
        }
        else
        {
          fclose(file);
          fileOpened = 0;
          if(token[i+1] != NULL && strcmp(token[i+1], "OPEN") != 0)
          {
            printf("Error: File system image must be opened first.\n");
            goto forContinue;
          }
        }
      }

      // info
      if(token[i] != NULL && strcmp(token[i], "INFO") == 0 && fileOpened == 1)
      {
        printf("BPB_BytePerSec: %d\n", BPB_BytsPerSec);
        printf("BPB_SecPerClus: %d\n", BPB_SecPerClus);
        printf("BPB_RsvdSecCnt: %d\n", BPB_RsvdSecCnt);
        printf("BPB_NumFats: %d\n", BPB_NumFats);
        printf("BPB_FatSz32: %d\n", BPB_FatSz32);
        printf("BPB_ExtFlags: %d\n", BPB_ExtFlags);
        printf("BPB_RootClus: %d\n", BPB_RootClus);
        printf("BPB_FSInfo: %d\n", BPB_FSInfo);
      }

      // stat ------------------------------------------------------------------------
      if(token[i] != NULL && strcmp(token[i], "STAT") == 0)
      {
        if(token[i+1] == NULL)
        {
          printf("Error. Please provide a file or directory\n");
        }

        char expanded_name[12];
        convertName(token[i+1], expanded_name);

        fseek(file, clusterStart, SEEK_SET);

        struct DirectoryEntry dir;
        int fileFound = 0;

        // Read through the directory entries
        while (fread(&dir, sizeof(struct DirectoryEntry), 1, file) > 0) 
        {
          if (strncmp(dir.DIR_Name, expanded_name, 11) == 0) 
          {
            fileFound = 1;
            int startingClusNum =  (dir.DIR_FirstClusterHigh << 16) | dir.DIR_FirstClusterLow;
            printf("Name: %.11s\n", dir.DIR_Name);
            printf("Attributes: %d\n", dir.DIR_Attr);
            printf("First Cluster High: %d\n", dir.DIR_FirstClusterHigh);
            printf("First Cluster Low: %d\n", dir.DIR_FirstClusterLow);
            if(dir.DIR_Attr & 0x10)
            {
              printf("Size: 0\n");
            }
            else
            {
              printf("File Size: %d\n", dir.DIR_FileSize);
            }
            printf("Starting Cluster Number: %d\n", startingClusNum);
            break;
          }
        }
        if(fileFound == 0)
        {
          printf("Error: File not found\n");
        }
      }
      
      // get
      if(token[i] != NULL && strcmp(token[i], "GET") == 0 && fileOpened == 1)
      {
        if(token[i+1] == NULL)
        {
          printf("Error. Please provide a file name\n");
          continue;
        }

        char expanded_name[12];
        convertName(token[i+1], expanded_name);

        int newFileName = 0;
        int fileFound = 0;
        if(token[i + 2] != NULL && !isCommand(token[i+2]))
        {
          newFileName = 1;
        }

        uint32_t cluster = BPB_RootClus;
        struct DirectoryEntry dir;

        while (1) 
        {
          int addToSeekTo = LABToOffset(cluster);
          fseek(file, addToSeekTo, SEEK_SET);

          // loop through directories
          while (fread(&dir, sizeof(struct DirectoryEntry), 1, file) > 0) 
          {
            if (strncmp(dir.DIR_Name, expanded_name, 11) == 0) 
            {
              fileFound = 1;
              break;
            }
          }

          if (fileFound == 1)
          {
            break;
          }

          // check for the next cluster in the FAT
          cluster = NextLB(cluster);
          if (cluster == -1) 
          {
            break;
          }
        }

        if (!fileFound)
        {
          printf("Error: File not found\n");
          continue;
        }

        // check for name change
        FILE *fileToWriteTo;
        if(newFileName)
        {
          fileToWriteTo = fopen(originalToken[i+2], "w");
        }
        else
        {
          fileToWriteTo = fopen(originalToken[i+1], "w");
        }
          
        // get starting cluster num and calculate offset
        cluster = (dir.DIR_FirstClusterHigh << 16) | dir.DIR_FirstClusterLow;

        // read file starting at starting cluster
        while (cluster != -1)
        {
          int blockStartAdd = LABToOffset(cluster);
          fseek(file, blockStartAdd, SEEK_SET);

          char buffer[BPB_BytsPerSec];
          size_t bytesRead = fread(buffer, 1, sizeof(buffer), file);
          fwrite(buffer, 1, bytesRead, fileToWriteTo);

          // get next cluster in chain
          cluster = NextLB(cluster);
        }
        fclose(fileToWriteTo);
      }

      // put
      if(token[i] != NULL && strcmp(token[i], "PUT") == 0 && fileOpened == 1)
      {
        if (token[i+1] == NULL || isCommand(token[i+1])) 
        {
          printf("Error. Enter a file to be put\n");
          continue;
        }

        char expanded_name[12];
        char expanded_name2[12] = "NULL";
        convertName(token[i+1], expanded_name);

        if (token[i+2] != NULL && !isCommand(token[i+2])) 
        {
          convertName(token[i+2], expanded_name2);
        }

        // Open the source file to read
        FILE *fileToRead = fopen(originalToken[i+1], "rb");
        if (fileToRead == NULL)
        {
          printf("Error: File not found\n");
          continue;
        }

        // change the name if needed
        struct DirectoryEntry dir;
        if (strcmp(expanded_name2, "NULL") == 0) 
        {
          strncpy(dir.DIR_Name, expanded_name, 11);
        } 
        else 
        {
          strncpy(dir.DIR_Name, expanded_name2, 11);
        }

        char buffer[BPB_BytsPerSec];
        //uint32_t cluster = BPB_RootClus;
        //int clusterFound = 0;
        int addToSeekTo;

        // set filesize
        fseek(fileToRead, 0, SEEK_END);
        int bytesToCopy = ftell(fileToRead);
        dir.DIR_FileSize = bytesToCopy;
        //int clusterLow = 0;
        //int clusterHigh = 0;
        int32_t val;

        // loop through and look for empty cluster
        int lastCluster = -1;
        fseek( file, fat1Start, SEEK_SET);

        while(bytesToCopy > BPB_BytsPerSec)
        {
          if( lastCluster != -1 )
          {
            fseek( file, fat1Start + ( lastCluster * 4 ), SEEK_SET );
            fwrite( &lastCluster, 4, 1,  file);
            fseek( file, fat1Start, SEEK_SET);
          }

          for(int j = 0; j < BPB_FatSz32; j++)
          {
            
            fread(&val, 4, 1, file);
            
            if(val >= 0)
            {
              continue;
            }
            
            if(lastCluster == -1)
            {
              int cluster = val;
              dir.DIR_FirstClusterHigh = cluster << 16;
              cluster = val;
              dir.DIR_FirstClusterLow = cluster >> 16;
            }

            int offset = LABToOffset( val );
            fseek( file, offset ,SEEK_SET );
            fread( buffer, 1, 512, fileToRead);
            fwrite( buffer, 1, 512, file );

            lastCluster = val;
          }
          bytesToCopy -= BPB_BytsPerSec;
        }

        if( bytesToCopy > 0 )
        {
          fseek( file, fat1Start + ( lastCluster * 4 ), SEEK_SET );
          fwrite( &lastCluster, 4, 1, file );
          fseek( file, fat1Start, SEEK_SET);

          for(int j = 0; j < BPB_FatSz32; j++)
          {
            fread(&val, 4, 1, file);
            
            if(val >= 0)
            {
              continue;
            }

            int offset = LABToOffset( val );
            fseek( file, offset ,SEEK_SET );
            fread( buffer, 1, 512, fileToRead);
            fwrite( buffer, 1, 512, file );

            lastCluster = val;
            bytesToCopy -= 512;
          }
        }

        // add to dir
        addToSeekTo = LABToOffset(BPB_RootClus);
        fseek(file, addToSeekTo, SEEK_SET);
        struct DirectoryEntry dirbuff;
        //int ready = 0;
        dir.DIR_Attr = 0x01;

        while(fread(&dirbuff, sizeof(struct DirectoryEntry), 1, file) > 0)
        {
          if((uint8_t)dirbuff.DIR_Name[0] == 0xe5 && (dirbuff.DIR_Attr != 0x01 && dirbuff.DIR_Attr != 0x10 && dirbuff.DIR_Attr != 0x20))
          { 
            // print out first char and attribute to see what is available
            printf("%02X\n", dirbuff.DIR_Name[0]);
            fseek(file, (-1 * sizeof(struct DirectoryEntry)), SEEK_CUR);
            fwrite(&dir, 32, 1, file);

            printf("WRITTEN\n");
            break;
          }
        }
        fclose(fileToRead);
      }

      // ls - fix
      if(token[i] != NULL && strcmp(token[i], "LS") == 0 && fileOpened == 1)
      {
        uint32_t cluster = currentDirectory;
        int addr = LABToOffset(cluster);
        fseek(file, addr, SEEK_SET);
        struct DirectoryEntry dir;

        if(token[i+1] != NULL && strcmp(token[i+1], "..") == 0)
        {
          int fileFound = 0;
          while (fread(&dir, sizeof(struct DirectoryEntry), 1, file) > 0) 
          {
            if (strncmp(dir.DIR_Name, "..", 2) == 0) 
            {
              fileFound = 1;
              if(dir.DIR_FirstClusterHigh == 0 && dir.DIR_FirstClusterLow == 0)
              {
                // parent is the root which is diff from other directories
                cluster = BPB_RootClus;
              }
              else
              {
                cluster = (dir.DIR_FirstClusterHigh << 16) | dir.DIR_FirstClusterLow;
              }

              addr = LABToOffset(cluster);
              fseek(file, addr, SEEK_SET);
              break;
            }
          }
          if(fileFound == 0)
          {
            printf("Error. Parent directory not found\n");
            continue;
          }
        }

        while(fread(&dir, sizeof(struct DirectoryEntry), 1, file) > 0)
        {
          if((uint8_t)dir.DIR_Name[0] == 0xe5)
          {
            continue;
          }
          if(dir.DIR_Name[0] == 0x00)
          {
            break;
          }

          if(dir.DIR_Attr != 0x01 && dir.DIR_Attr != 0x10 && dir.DIR_Attr != 0x20)
          {
            continue;
          }
                
          char fileName[12];
          strncpy(fileName, dir.DIR_Name, 11);
          fileName[11] = '\0';

          printf("%s\n", fileName);
        }
      }

      // cd
      if(token[i] != NULL && strcmp(token[i], "CD") == 0) 
      {
        uint32_t cluster = BPB_RootClus;
        struct DirectoryEntry dir;
        char cdToken[MAX_COMMAND_SIZE][MAX_COMMAND_SIZE];
        int directoryFoundDot = 0;
        int directoryFoundReg = 0;
        int directoryFoundAbs = 0;

        // cd <name> (just one cd)
        if(token[i+1][0] != '.' && token[i+1][0] != '/')
        {
          directoryFoundReg = 0;
          int addToSeekTo = LABToOffset(cluster);
          fseek(file, addToSeekTo, SEEK_SET);

          // look for current directory in cluster
          while (fread(&dir, sizeof(struct DirectoryEntry), 1, file) > 0)
          {
            if ((dir.DIR_Attr & 0x10) == 0x10 && strncmp(dir.DIR_Name, token[i+1], strlen(token[i+1])) == 0)
            {
              cluster = (dir.DIR_FirstClusterHigh << 16) | dir.DIR_FirstClusterLow;
              directoryFoundReg = 1;
              strcat(path, "/");
              strcat(path, dir.DIR_Name);
              break;
            }
            }
          if (directoryFoundReg == 0)
          {
            printf("Error: Directory not found in reg cd\n");
            goto extraContinue;
          }
          
          currentDirectory = cluster;
        }

        // parse input (token[i+1]) into an array of strings to be itterated over
        int n = 0;
        int count = 0;  

        while (token[i+1][n] != '\0') 
        {
          if (strncmp(&token[i+1][n], "..", 2) == 0) 
          {
            strcpy(cdToken[count], "..");
            count++;
            n += 2;
          } 
          else if (token[i+1][n] == '/') 
          {
            int start = n;
            while (token[i+1][n] != '\0' && strncmp(&token[i+1][n], "..", 2) != 0) 
            {
              n++; // keep track of how much copying in case theres another .. after /'s
            }
            strncpy(cdToken[count], &token[i+1][start], n - start);
            cdToken[count][n - start] = '\0';
            count++;
          } 
          else
          {
            n++;
          }
        }

        // loop through cd commands
        for(int m = 0; m < count; m++)
        {
          // chained path
          if(cdToken[m][0] == '/')
          {
            char *directories = strtok(cdToken[m], "/");

            if(cdToken[m-1] != NULL && cdToken[m-1][0] != '.')
            {
              // absolute path, reset path
              strcpy(path, "");
            }
            else
            {
              printf("directory to search for: %s\n", directories);
            }

            // search for directories
            while(directories != NULL)
            {
              directoryFoundAbs = 0;
              int addToSeekTo = LABToOffset(cluster);
              fseek(file, addToSeekTo, SEEK_SET);

              // look for current directory in cluster
              while (fread(&dir, sizeof(struct DirectoryEntry), 1, file) > 0)
              {
                if ((dir.DIR_Attr & 0x10) == 0x10 && strncmp(dir.DIR_Name, directories, strlen(directories)) == 0)
                {
                  cluster = (dir.DIR_FirstClusterHigh << 16) | dir.DIR_FirstClusterLow;
                  directoryFoundAbs = 1;
                  strcat(path, "/");
                  strcat(path, dir.DIR_Name);
                  break;
                }
              }
              if (directoryFoundAbs == 0)
              {
                printf("Error: Directory not found in abs\n");
                goto extraContinue;
              }
              directories = strtok(NULL, "/");
            }
            currentDirectory = cluster;
          }
          
          // cd ..
          if(strcmp(cdToken[m], "..") == 0)
          {
            // check to make sure not already in home directory
            if(strcmp(path, "") == 0)
            {
              continue;
            }

            cluster = BPB_RootClus;
            char buffer[strlen(path)];

            // trim off current directory
            char *currDirectory = strrchr(path, '/');
            strncpy(buffer, path, strlen(path) - strlen(currDirectory));
            strcpy(path, buffer);

            char *lastDirectory = strrchr(path, '/');
            lastDirectory++;


            char pathCopy[strlen(path) + 1];
            strcpy(pathCopy, path);
            char *directories = strtok(pathCopy, "/");

            // search for directories
            while(directories != NULL)
            {
              directoryFoundDot = 0;
              int addToSeekTo = LABToOffset(cluster);
              fseek(file, addToSeekTo, SEEK_SET);

              // look for current directory in cluster
              while (fread(&dir, sizeof(struct DirectoryEntry), 1, file) > 0)
              {
                if ((dir.DIR_Attr & 0x10) == 0x10 && strncmp(dir.DIR_Name, directories, strlen(directories)) == 0)
                {
                  cluster = (dir.DIR_FirstClusterHigh << 16) | dir.DIR_FirstClusterLow;
                  directoryFoundDot = 1;
                  break;
                }
              }
              if (directoryFoundDot == 0)
              {
                printf("Error: Directory not found in ..\n");
                goto extraContinue;
              }

              directories = strtok(NULL, "/");
            }
            currentDirectory = cluster;

            for(int k = 0; k < strlen(path); k++)
            {
              if((isprint(path[k]) == 0) && path[k] != '\0' && path[k] != '/' && path[k] != '\t' && path[k] != ' ' && path[k] != 16)
              {
                strcpy(path, "");
                break;
              }
            }
          }
        }
        extraContinue:
        if(directoryFoundReg == 0 || directoryFoundAbs == 0 || directoryFoundDot == 0)
        {
          continue;
        }
      }

      // read
      if(token[i] != NULL && strcmp(token[i], "READ") == 0)
      {
        if(token[i+1] == NULL || isCommand(token[i+1]) || token[i+2] == NULL || isCommand(token[i+2]) || token[i+3] == NULL
            || isCommand(token[i+3]))
        {
          printf("Error. Not enough arguments\n");
          continue;
        }

        struct DirectoryEntry dir;
        int fileFound = 0;
        uint32_t cluster = BPB_RootClus;
        char expanded_name[12]; 
        convertName(token[i+1], expanded_name);

        fseek(file, LABToOffset(cluster), SEEK_SET);
        while (fread(&dir, sizeof(struct DirectoryEntry), 1, file) > 0) 
        {
          if (strncmp(dir.DIR_Name, expanded_name, 11) == 0) 
          {
            fileFound = 1;
            break;
          }
        }

        if(fileFound == 0)
        {
          printf("Error. File not found\n");
          continue;
        }

        cluster = (dir.DIR_FirstClusterHigh << 16) | dir.DIR_FirstClusterLow;
        int pos = atoi(token[i+2]);

        // fseek to the correct offset/position
        fseek(file, (LABToOffset(cluster) + pos), SEEK_SET);

        int numOfBytes = atoi(token[i+3]);
        unsigned char buffer[numOfBytes];
        fread(buffer, 1, numOfBytes, file);

        for (int j = 0; j < numOfBytes; j++)
        {
          if (token[i+4] == NULL || isCommand(token[i+4])) 
          {
            printf("0x%02X ", buffer[j]);
          } 
          else if (strcmp(token[i+4], "-ASCII") == 0) 
          {
            printf("%c", buffer[j]);
          } 
          else if (strcmp(token[i+4], "-DEC") == 0) 
          {
            printf("%d ", buffer[j]);
          } 
        }
        printf("\n");
      }

      // del
      if(token[i] != NULL && strcmp(token[i], "DEL") == 0 && fileOpened == 1)
      {
          if(token[i+1] == NULL || isCommand(token[i+1]))
          {
              printf("Error. Please enter a file to be deleted");
              continue;
          }

          int fileFound = 0;
          char expanded_name[12];
          convertName(token[i+1], expanded_name);

          uint32_t cluster = BPB_RootClus;
          struct DirectoryEntry dir;

          while(1) 
          {
              int addToSeekTo = LABToOffset(cluster);
              fseek(file, addToSeekTo, SEEK_SET);

              // loop through directories
              while(fread(&dir, sizeof(struct DirectoryEntry), 1, file) > 0) 
              {
                if(strncmp(dir.DIR_Name, expanded_name, 11) == 0)
                {
                  fileFound = 1;
                  dir.DIR_Name[0] = 0xe5;

                  fseek(file, -sizeof(struct DirectoryEntry), SEEK_CUR);
                  fwrite(&dir, sizeof(struct DirectoryEntry), 1, file);
                  break;
                }
              }

              if(fileFound) 
              {
                break;
              }
              // check for the next cluster in the FAT
              // printf("SEARCHING FOR NEXT CLUSTER\n");
              cluster = NextLB(cluster);
              if(cluster == -1) 
              {
                  break; // No more clusters, exit the loop
              }
          }

          if(!fileFound) 
          {
            printf("Error: File not found\n");
            continue;
          }
      }

      // undel
      if(token[i] != NULL && strcmp(token[i], "UNDEL") == 0 && fileOpened == 1)
      {
        if(token[i+1] == NULL || isCommand(token[i+1]))
        {
          printf("Error. Please enter a file to be recovered\n");
          continue;
        }

        int fileFound = 0;
        char expanded_name[12];
        convertName(token[i+1], expanded_name);
        expanded_name[0] = 0xe5;

        uint32_t cluster = BPB_RootClus;
        struct DirectoryEntry dir;

        while(1) 
        {
          int addToSeekTo = LABToOffset(cluster);
          fseek(file, addToSeekTo, SEEK_SET);

          // loop through directories
          while(fread(&dir, sizeof(struct DirectoryEntry), 1, file) > 0) 
          {
            if(strncmp(dir.DIR_Name, expanded_name, 11) == 0)
            {
              fileFound = 1;
              dir.DIR_Name[0] = token[i+1][0];

              fseek(file, -sizeof(struct DirectoryEntry), SEEK_CUR);
              fwrite(&dir, sizeof(struct DirectoryEntry), 1, file);
              break;
            }
          }

          if(fileFound) 
          {
            break;
          }
          cluster = NextLB(cluster);
          if(cluster == -1) 
          {
            break;
          }
        }

        if(!fileFound) 
        {
          printf("Error: File not found\n");
          continue;
        }
      }
    }

    forContinue:
    // for( int token_index = 0; token_index < token_count; token_index ++ ) 
    // {
    //   printf("token[%d] = %s\n", token_index, token[token_index] );  
    // }
    free( head_ptr );
    continue;
  }
  fclose(file);
  return 0;
}