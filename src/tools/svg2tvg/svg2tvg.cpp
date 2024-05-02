/*
 * Copyright (c) 2021 - 2024 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <iostream>
#include <vector>
#include <thread>
#include <thorvg.h>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #ifndef PATH_MAX
        #define PATH_MAX MAX_PATH
    #endif
#else
    #include <dirent.h>
    #include <unistd.h>
    #include <limits.h>
    #include <sys/stat.h>
#endif

using namespace std;
using namespace tvg;


struct App
{
private:
   char full[PATH_MAX];    //full path

   void helpMsg()
   {
      cout << "Usage: \n   svg2tvg [SVG file] or [SVG folder]\n\nExamples: \n    $ svg2tvg input.svg\n    $ svg2tvg svgfolder\n\n";
   }

   bool validate(string& svgName)
   {
      string extn = ".svg";

      if (svgName.size() <= extn.size() || svgName.substr(svgName.size() - extn.size()) != extn) {
         cout << "Error: \"" << svgName << "\" is invalid." << endl;
         return false;
      }
      return true;
   }

   bool convert(string& in, string& out)
   {
       //Threads Count
      auto threads = std::thread::hardware_concurrency();
      if (threads > 0) --threads;    //Allow the designated main thread capacity

      if (Initializer::init(threads, CanvasEngine::Sw) != Result::Success) return false;

      auto picture = Picture::gen();
      if (picture->load(in) != Result::Success) return false;

      auto saver = Saver::gen();
      if (saver->save(std::move(picture), out) != Result::Success) return false;
      if (saver->sync() != Result::Success) return false;

      if (Initializer::term(CanvasEngine::Sw) != Result::Success) return false;

      return true;
   }

   void convert(string& svgName)
   {
      //Get tvg file
      auto tvgName = svgName;
      tvgName.replace(tvgName.length() - 3, 3, "tvg");

      if (convert(svgName, tvgName)) {
         cout << "Generated TVG file : " << tvgName << endl;
      }
      else {
         cout << "Failed Converting TVG file : " << svgName << endl;
      }
   }

   const char* realPath(const char* path)
   {
#ifdef _WIN32
      return _fullpath(full, path, PATH_MAX);
#else
      return realpath(path, full);
#endif
   }

   bool isDirectory(const char* path)
   {
#ifdef _WIN32
      DWORD attr = GetFileAttributes(path);
      if (attr == INVALID_FILE_ATTRIBUTES) return false;
      return attr & FILE_ATTRIBUTE_DIRECTORY;
#else
      struct stat buf;
      if (stat(path, &buf) != 0) return false;
      return S_ISDIR(buf.st_mode);
#endif
   }

   bool handleDirectory(const string& path)
   {
#ifdef _WIN32
        //open directory
        WIN32_FIND_DATA fd;
        HANDLE h = FindFirstFileEx((path + "\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, 0);
        if (h == INVALID_HANDLE_VALUE) {
            cout << "Couldn't open directory \"" << path.c_str() << "\"." << endl;
            return false;
        }
        //List directories
        do {
            if (*fd.cFileName == '.' || *fd.cFileName == '$') continue;
            //sub directory
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                string subpath = string(path);
                subpath += '\\';
                subpath += fd.cFileName;
                if (!handleDirectory(subpath)) continue;
            //file
            } else {
                string svgName(fd.cFileName);
                if (!validate(svgName)) continue;
                svgName = string(path);
                svgName += '\\';
                svgName += fd.cFileName;
                convert(svgName);
            }
        } while (FindNextFile(h, &fd));

        FindClose(h);
#else
        //open directory
        auto dir = opendir(path.c_str());
        if (!dir) {
            cout << "Couldn't open directory \"" << path.c_str() << "\"." << endl;
            return false;
        }
        //List directories
        while (auto entry = readdir(dir)) {
            if (*entry->d_name == '.' || *entry->d_name == '$') continue;
            //sub directory
            if (entry->d_type == DT_DIR) {
                string subpath = string(path);
                subpath += '/';
                subpath += entry->d_name;
                if (!handleDirectory(subpath)) continue;
            //file
            } else {
                string svgName(entry->d_name);
                if (!validate(svgName)) continue;
                svgName = string(path);
                svgName += '/';
                svgName += entry->d_name;
                convert(svgName);
            }
        }
#endif
        return true;
    }

public:
   int setup(int argc, char** argv)
   {
      //Collect input files
      vector<const char*> inputs;

      for (int i = 1; i < argc; ++i) {
         inputs.push_back(argv[i]);
      }

      //No Input SVG
      if (inputs.empty()) {
         helpMsg();
         return 0;
      }

      for (auto input : inputs) {

         auto path = realPath(input);
         if (!path) {
            cout << "Invalid file or path name: \"" << input << "\"" << endl;
            continue;
         }

         if (isDirectory(path)) {
            //load from directory
            cout << "Directory: \"" << path << "\"" << endl;
            if (!handleDirectory(path)) break;
         }
         else {
            string svgName(input);
            if (!validate(svgName)) continue;
            convert(svgName);
         }
      }
      return 0;
   }
};


int main(int argc, char **argv)
{
   App app;
   return app.setup(argc, argv);
}