/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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
#include <string.h>
#include <vector>
#include <thorvg.h>
#ifdef _WIN32
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
   uint32_t fps = 30;
   uint32_t width = 600;
   uint32_t height = 600;
   unique_ptr<Shape> bg = nullptr; //transparent

   void helpMsg()
   {
      cout << "Usage: \n   lottie2gif [Lottie file] or [Lottie folder] [-r resolution] [-f fps] [-b background color]\n\nExamples: \n    $ lottie2gif input.json\n    $ lottie2gif input.json -r 600x600\n    $ lottie2gif input.json -f 30\n    $ lottie2gif input.json -r 600x600 -f 30\n    $ lottie2gif lottiefolder\n    $ lottie2gif lottiefolder -r 600x600 -f 30 -b fa7410\n\n";
   }

   bool validate(string& lottieName)
   {
      string extn = ".json";

      if (lottieName.size() <= extn.size() || lottieName.substr(lottieName.size() - extn.size()) != extn) {
         cout << "Error: \"" << lottieName << "\" is invalid." << endl;
         return false;
      }
      return true;
   }

   bool convert(string& in, string& out)
   {
      if (Initializer::init(CanvasEngine::Sw, 0) != Result::Success) return false;

      auto animation = Animation::gen();
      auto picture = animation->picture();
      if (picture->load(in) != Result::Success) return false;

      float width, height;
      picture->size(&width, &height);
      float scale =  static_cast<float>(this->width) / width;
      picture->size(width * scale, height * scale);

      auto saver = Saver::gen();

      //set a background color
      if (bg) {
         bg->appendRect(0, 0, width * scale, height * scale);
         saver->background(std::move(bg));
      }
      if (saver->save(std::move(animation), out, 100, fps) != Result::Success) return false;
      if (saver->sync() != Result::Success) return false;

      if (Initializer::term(CanvasEngine::Sw) != Result::Success) return false;

      return true;
   }

   void convert(string& lottieName)
   {
      //Get gif file
      auto gifName = lottieName;
      gifName.replace(gifName.length() - 4, 4, "gif");

      if (convert(lottieName, gifName)) {
         cout << "Generated Gif file : " << gifName << endl;
      } else {
         cout << "Failed Converting Gif file : " << lottieName << endl;
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
                string lottieName(fd.cFileName);
                if (!validate(lottieName)) continue;
                lottieName = string(path);
                lottieName += '\\';
                lottieName += fd.cFileName;
                convert(lottieName);
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
         const char* p = argv[i];
         if (*p == '-') {
            const char* p_arg = (i + 1 < argc) ? argv[++i] : nullptr;

            //image resolution
            if (p[1] == 'r') {
               if (!p_arg) {
                  cout << "Error: Missing resolution attribute. Expected eg. -r 600x600." << endl;
                  return 1;
               }

               const char* x = strchr(p_arg, 'x');
               if (x) {
                  width = atoi(p_arg);
                  height = atoi(x + 1);
               }
               if (!x || width <= 0 || height <= 0) {
                  cout << "Error: Resolution (" << p_arg << ") is corrupted. Expected eg. -r 600x600." << endl;
                  return 1;
               }
            //fps
            } else if (p[1] == 'f') {
               if (!p_arg) {
                  cout << "Error: Missing fps value. Expected eg. -f 30." << endl;
                  return 1;
               }
               fps = atoi(p_arg);
            } else if (p[1] == 'b') {
               //background color
               if (!p_arg) {
                  cout << "Error: Missing background color attribute. Expected eg. -b fa7410." << endl;
                  return 1;
               }
               auto bgColor = (uint32_t) strtol(p_arg, NULL, 16);
               auto r = (uint8_t)((bgColor & 0xff0000) >> 16);
               auto g = (uint8_t)((bgColor & 0x00ff00) >> 8);
               auto b = (uint8_t)((bgColor & 0x0000ff));
               bg = tvg::Shape::gen();
               bg->fill(r, g, b, 255);

            } else {
               cout << "Warning: Unknown flag (" << p << ")." << endl;
            }
         }else {
            inputs.push_back(argv[i]);
         }
      }

      //No Input Lottie
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
            string lottieName(input);
            if (!validate(lottieName)) continue;
            convert(lottieName);
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