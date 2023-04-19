/*
 * Copyright (c) 2021 - 2023 the ThorVG project. All rights reserved.

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
#include <thorvg.h>
#include <dirent.h>

using namespace std;
using namespace tvg;


void helpMsg()
{
    cout<<"Usage: \n   svg2tvg [SVG file] or [SVG folder]\n\nExamples: \n    $ svg2tvg input.svg\n    $ svg2tvg svgfolder\n\n";
}


bool convert(string& in, string& out)
{
    if (Initializer::init(CanvasEngine::Sw, 0) != Result::Success) return false;

    auto picture = Picture::gen();
    if (picture->load(in) != Result::Success) return false;

    auto saver = Saver::gen();
    if (saver->save(move(picture), out) != Result::Success) return false;
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
        cout<<"Generated TVG file : "<< tvgName << endl;
    } else {
        cout<<"Failed Converting TVG file : "<< svgName << endl;
    }
}


char* getpath(const char* input)
{
    static char buf[PATH_MAX];
#ifdef _WIN32
    return _fullpath(buf, input, PATH_MAX);
#else
    return realpath(input, buf);
#endif
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


void directory(const string& path, DIR* dir)
{
    //List directories
    while (auto entry = readdir(dir)) {
        if (*entry->d_name == '.' || *entry->d_name == '$') continue;
        if (entry->d_type == DT_DIR) {
            string subpath = string(path);
#ifdef _WIN32
            subpath += '\\';
#else
            subpath += '/';
#endif
            subpath += entry->d_name;

            //open directory
            if (auto subdir = opendir(subpath.c_str())) {
                cout << "Sub Directory: \"" << subpath << "\"" << endl;
                directory(subpath, subdir);
                closedir(dir);
            }

        } else {
            string svgName(entry->d_name);
            if (!validate(svgName)) continue;
            svgName = string(path);
#ifdef _WIN32
            svgName += '\\';
#else
            svgName += '/';
#endif
            svgName += entry->d_name;

            convert(svgName);
        }
    }
}



int main(int argc, char **argv)
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

        auto path = getpath(input);
        if (!path) {
            cout << "Invalid file or path name: \"" << input << "\"" << endl;
            continue;
        }

        if (auto dir = opendir(path)) {
            //load from directory
            cout << "Directory: \"" << path << "\"" << endl;
            directory(path, dir);
            closedir(dir);
        } else {
            string svgName(input);
            if (!validate(svgName)) continue;
            convert(svgName);
        }
    }

    return 0;
}
