/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#include <thorvg.h>
#include <array>

using namespace std;
using namespace tvg;

   
void helpMsg()
{
    cout<<"Usage: \n   svg2tvg [SVG file]\n\nExamples: \n    $ svg2tvg input.svg\n\n";
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


int main(int argc, char **argv)
{
    //No Input SVG
    if (argc < 2) {
        helpMsg();
        return 0;
    }

    auto input = argv[1];

    array<char, 5000> memory;

#ifdef _WIN32
    input = _fullpath(memory.data(), input, memory.size());
#else
    input = realpath(input, memory.data());
#endif

    //Verify svg file.
    if (!input) {
        helpMsg();
        return 0;
    }

    string svgName(input);
    string extn = ".svg";

    if (svgName.size() <= extn.size() || svgName.substr(svgName.size() - extn.size()) != extn) {
        helpMsg();
        return 0;
    }

    //Get tvg file.
    auto tvgName = svgName.substr(svgName.find_last_of("/\\") + 1);
    tvgName.append(".tvg");

    //Convert!
    if (convert(svgName, tvgName)) {
        cout<<"Generated TVG file : "<< tvgName << endl;
    } else {
        cout<<"Failed Converting TVG file : "<< tvgName << endl;
    }

    return 0;
}
