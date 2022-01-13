#ifndef _TOOLS_HEAD_H_
#define _TOOLS_HEAD_H_

#include "stdfax.h"
#include <dirent.h>
#include <sys/types.h>
#include <algorithm>

class HelpToos
{
public:
    static void listFiles( const char* path, const char *Suffix, std::vector<std::string> &fileVec, bool gIgnoreHidden = true)
    {
        DIR* dirFile = opendir( path );
        if ( dirFile )
        {
            // std::string path_str = path;
            struct dirent* hFile;
            while (( hFile = readdir( dirFile )) != NULL )
            {
                if ( !strcmp( hFile->d_name, "."  )) continue;
                if ( !strcmp( hFile->d_name, ".." )) continue;

                // in linux hidden files all start with '.'
                if ( gIgnoreHidden && ( hFile->d_name[0] == '.' )) continue;

                // dirFile.name is the name of the file. Do whatever string comparison
                // you want here. Something like:
                if ( strstr( hFile->d_name, Suffix ))
                    fileVec.push_back(hFile->d_name);

                //fileVec.push_back(path_str + "/" + hFile->d_name);
                //printf( "found an .txt file: %s", hFile->d_name );
            }
            closedir( dirFile );
        }
    }
};

#endif  /* */
