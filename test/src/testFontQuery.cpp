/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <thorvg.h>
#include "config.h"
#include "testFramework.h"

using namespace tvg;
using namespace std;

TEST_CASE("Empty font query", "[tvgFontQuery]")
{
    REQUIRE(Initializer::init() == Result::Success);
    const FontFace* faces = nullptr;
    REQUIRE(Text::faces() == 0);
    REQUIRE(Text::faces(nullptr, &faces) == 0);
    REQUIRE(faces == nullptr);
    REQUIRE(Text::faces("Missing family", &faces) == 0);
    REQUIRE(faces == nullptr);
    REQUIRE(Initializer::term() == Result::Success);
}

#ifdef THORVG_TTF_LOADER_SUPPORT

static uint16_t _u16(const vector<char>& data, uint32_t offset)
{
    return (static_cast<uint16_t>(static_cast<uint8_t>(data[offset])) << 8) | static_cast<uint8_t>(data[offset + 1]);
}

static uint32_t _u32(const vector<char>& data, uint32_t offset)
{
    return (static_cast<uint32_t>(_u16(data, offset)) << 16) | _u16(data, offset + 2);
}

static void _put16(vector<char>& data, uint32_t offset, uint16_t value)
{
    data[offset] = value >> 8;
    data[offset + 1] = value;
}

static void _put32(vector<char>& data, uint32_t offset, uint32_t value)
{
    _put16(data, offset, value >> 16);
    _put16(data, offset + 2, value);
}

static uint32_t _table(const vector<char>& data, const char* tag)
{
    for (uint32_t i = 0; i < _u16(data, 4); ++i) {
        auto record = 12 + i * 16;
        if (!memcmp(data.data() + record, tag, 4)) return record;
    }
    return 0;
}

static void _replace(vector<char>& data, const char* tag, const vector<char>& bytes)
{
    auto record = _table(data, tag);
    REQUIRE(record != 0);
    _put32(data, record + 8, data.size());
    _put32(data, record + 12, bytes.size());
    data.insert(data.end(), bytes.begin(), bytes.end());
}

static vector<char> _font(const vector<pair<uint16_t, u16string>>& names, uint16_t weight = 700, uint16_t width = 5, uint16_t flags = 0)
{
    ifstream file(TEST_DIR "/PublicSans-Regular.ttf", ios::binary);
    REQUIRE(file.is_open());
    auto data = vector<char>(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
    auto strings = static_cast<uint16_t>(6 + names.size() * 12);
    auto table = vector<char>(strings);
    _put16(table, 2, names.size());
    _put16(table, 4, strings);
    for (uint32_t i = 0; i < names.size(); ++i) {
        auto record = 6 + i * 12;
        _put16(table, record, 3);
        _put16(table, record + 2, 1);
        _put16(table, record + 4, 0x409);
        _put16(table, record + 6, names[i].first);
        _put16(table, record + 8, names[i].second.size() * 2);
        _put16(table, record + 10, table.size() - strings);
        for (auto code : names[i].second) {
            table.push_back(code >> 8);
            table.push_back(code);
        }
    }
    _replace(data, "name", table);
    auto os2 = _u32(data, _table(data, "OS/2") + 8);
    _put16(data, os2, 4);
    _put16(data, os2 + 4, weight);
    _put16(data, os2 + 6, width);
    _put16(data, os2 + 62, flags);
    return data;
}

TEST_CASE("Font families expose selectable Unicode metadata", "[tvgFontQuery]")
{
    REQUIRE(Initializer::init() == Result::Success);
    auto heavy = _font({{1, u"Legacy Sans"}, {2, u"Regular"}, {16, u"Query Sans"}, {17, u"ExtraBlack Oblique"}, {4, u"Caf\u00e9 \uc0c8 \ud3f0\ud2b8 \U0001f680"}, {6, u"Query-Heavy"}}, 950, 3, 513);
    auto regular = _font({{1, u"Query Sans"}, {2, u"Regular"}, {4, u"Query Sans Regular"}, {6, u"Query-Regular"}}, 400);
    auto other = _font({{1, u"Caf\u00e9 \uc0c8 \ud3f0\ud2b8 \U0001f680"}, {2, u"Regular"}}, 1000, 9);
    REQUIRE(Text::load("query-heavy", heavy.data(), heavy.size(), "ttf", true) == Result::Success);
    REQUIRE(Text::load("query-regular", regular.data(), regular.size(), "ttf", true) == Result::Success);
    REQUIRE(Text::load("query-other", other.data(), other.size(), "ttf", true) == Result::Success);

    const FontFace* faces = nullptr;
    REQUIRE(Text::faces() == 3);
    REQUIRE(Text::faces("Query Sans", &faces) == 2);
    REQUIRE(faces != nullptr);
    REQUIRE(!strcmp(faces[0].name, "query-heavy"));
    REQUIRE(!strcmp(faces[0].family, "Query Sans"));
    REQUIRE(static_cast<uint16_t>(faces[0].style.weight) == 950);
    REQUIRE(faces[0].style.width == FontWidth::Condensed);
    REQUIRE(faces[0].style.slant == FontSlant::Oblique);
    REQUIRE(faces[1].style.weight == FontWeight::Normal);

    const FontFace* remaining = nullptr;
    REQUIRE(Text::faces("Caf\xc3\xa9 \xec\x83\x88 \xed\x8f\xb0\xed\x8a\xb8 \xf0\x9f\x9a\x80", &remaining) == 1);
    REQUIRE(static_cast<uint16_t>(remaining[0].style.weight) == 1000);
    REQUIRE(remaining[0].style.width == FontWidth::UltraExpanded);
    REQUIRE(Text::faces("Missing", &remaining) == 0);
    REQUIRE(remaining == nullptr);
    REQUIRE(!strcmp(faces[0].name, "query-heavy"));
    REQUIRE(Text::faces("Query Sans", &remaining) == 2);
    REQUIRE(remaining == faces);
    REQUIRE(Text::faces("query sans") == 0);

    auto text = Text::gen();
    REQUIRE(text->font(faces[0].name) == Result::Success);
    REQUIRE(text->font("Query-Heavy") == Result::InsufficientCondition);
    REQUIRE(text->font("Caf\xc3\xa9 \xec\x83\x88 \xed\x8f\xb0\xed\x8a\xb8 \xf0\x9f\x9a\x80") == Result::InsufficientCondition);
    Paint::rel(text);
    REQUIRE(Initializer::term() == Result::Success);
    REQUIRE(Initializer::init() == Result::Success);
    REQUIRE(Text::faces(nullptr, &remaining) == 0);
    REQUIRE(remaining == nullptr);
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Internal font names do not override explicit aliases", "[tvgFontQuery]")
{
    REQUIRE(Initializer::init() == Result::Success);
    auto first = _font({{1, u"Collision Sans"}, {2, u"Bold"}, {4, u"Shared Full"}, {6, u"SharedPS"}});
    auto second = first;
    REQUIRE(Text::load("collision-one", first.data(), first.size(), "ttf", true) == Result::Success);
    REQUIRE(Text::load("collision-two", second.data(), second.size(), "ttf", true) == Result::Success);
    REQUIRE(Text::faces("Collision Sans") == 2);
    auto text = Text::gen();
    REQUIRE(text->font("SharedPS") == Result::InsufficientCondition);
    REQUIRE(text->font("Shared Full") == Result::InsufficientCondition);
    REQUIRE(text->font("collision-one") == Result::Success);
    REQUIRE(text->font("collision-two") == Result::Success);

    auto alias = _font({{1, u"Explicit Alias"}, {2, u"Regular"}}, 400);
    REQUIRE(Text::load("SharedPS", alias.data(), alias.size(), "ttf", true) == Result::Success);
    const FontFace* faces = nullptr;
    REQUIRE(Text::faces("Explicit Alias", &faces) == 1);
    REQUIRE(!strcmp(faces[0].name, "SharedPS"));
    REQUIRE(text->font("SharedPS") == Result::Success);
    Paint::rel(text);
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Malformed optional name metadata preserves a usable font", "[tvgFontQuery]")
{
    REQUIRE(Initializer::init() == Result::Success);
    auto data = _font({{1, u"Safety"}});
    auto record = _table(data, "name");
    auto names = _u32(data, record + 8);
    SUBCASE("string outside the declared table")
    {
        _put16(data, names + 14, 0xffff);
    }
    SUBCASE("odd UTF-16 length")
    {
        _put16(data, names + 14, 3);
    }
    SUBCASE("unpaired surrogate")
    {
        _put16(data, names + 18, 0xd800);
    }
    SUBCASE("embedded NUL")
    {
        _put16(data, names + 18, 0);
    }
    SUBCASE("invalid string storage")
    {
        _put16(data, names + 4, 0xffff);
    }
    SUBCASE("record exceeds the declared table")
    {
        _put32(data, record + 12, 6);
    }
    SUBCASE("table exceeds the file")
    {
        _put32(data, record + 8, 0xfffffff0);
    }
    REQUIRE(Text::load("malformed-name", data.data(), data.size(), "ttf", true) == Result::Success);
    const FontFace* faces = nullptr;
    REQUIRE(Text::faces(nullptr, &faces) == 1);
    REQUIRE(!strcmp(faces[0].name, "malformed-name"));
    REQUIRE(faces[0].family == nullptr);
    auto text = Text::gen();
    REQUIRE(text->font(faces[0].name) == Result::Success);
    Paint::rel(text);
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Unsupported Mac Roman names preserve the registered alias and default style", "[tvgFontQuery]")
{
    REQUIRE(Initializer::init() == Result::Success);
    auto data = _font({});
    auto names = vector<char>(22);
    _put16(names, 2, 1);
    _put16(names, 4, 18);
    _put16(names, 6, 1);
    _put16(names, 12, 1);
    _put16(names, 14, 4);
    memcpy(names.data() + 18, "Caf\x8e", 4);
    _replace(data, "name", names);
    _put32(data, _table(data, "OS/2") + 12, 0);
    REQUIRE(Text::load("mac-roman", data.data(), data.size(), "ttf", true) == Result::Success);
    const FontFace* faces = nullptr;
    REQUIRE(Text::faces("Caf\xc3\xa9", &faces) == 0);
    REQUIRE(faces == nullptr);
    REQUIRE(Text::faces(nullptr, &faces) == 1);
    REQUIRE(!strcmp(faces[0].name, "mac-roman"));
    REQUIRE(faces[0].family == nullptr);
    REQUIRE(faces[0].style.weight == FontWeight::Normal);
    REQUIRE(faces[0].style.width == FontWidth::Normal);
    REQUIRE(faces[0].style.slant == FontSlant::Normal);
    auto text = Text::gen();
    REQUIRE(text->font(faces[0].name) == Result::Success);
    Paint::rel(text);
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Font names prefer English and support language-tagged Unicode records", "[tvgFontQuery]")
{
    REQUIRE(Initializer::init() == Result::Success);
    auto data = _font({{1, u"Famille"}, {1, u"English Sans"}});
    auto offset = _u32(data, _table(data, "name") + 8);
    _put16(data, offset + 10, 0x40c);
    REQUIRE(Text::load("english", data.data(), data.size(), "ttf", true) == Result::Success);
    REQUIRE(Text::faces("English Sans") == 1);
    REQUIRE(Text::faces("Famille") == 0);
    REQUIRE(Initializer::term() == Result::Success);
    REQUIRE(Initializer::init() == Result::Success);

    auto names = vector<char>(24);
    auto family = u16string(u"Tagged Sans");
    _put16(names, 0, 1);
    _put16(names, 2, 1);
    _put16(names, 4, 24);
    _put16(names, 8, 4);
    _put16(names, 10, 0x8000);
    _put16(names, 12, 16);
    _put16(names, 14, family.size() * 2);
    _put16(names, 18, 1);
    _put16(names, 20, 10);
    _put16(names, 22, family.size() * 2);
    for (auto code : family + u"en-US") {
        names.push_back(code >> 8);
        names.push_back(code);
    }
    _replace(data, "name", names);
    REQUIRE(Text::load("tagged", data.data(), data.size(), "ttf", true) == Result::Success);
    REQUIRE(Text::faces("Tagged Sans") == 1);
    REQUIRE(Initializer::term() == Result::Success);
    REQUIRE(Initializer::init() == Result::Success);
    _put16(names, 10, 0x8001);
    _replace(data, "name", names);
    REQUIRE(Text::load("invalid-tag", data.data(), data.size(), "ttf", true) == Result::Success);
    const FontFace* faces = nullptr;
    REQUIRE(Text::faces(nullptr, &faces) == 1);
    REQUIRE(faces[0].family == nullptr);
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Face queries skip fonts hidden by duplicate registered keys", "[tvgFontQuery]")
{
    REQUIRE(Initializer::init() == Result::Success);
    auto first = _font({{1, u"Borrowed Sans"}, {6, u"Borrowed-First"}}, 400);
    auto second = _font({{1, u"Borrowed Sans"}, {6, u"Borrowed-Second"}});
    REQUIRE(Text::load("borrowed", first.data(), first.size()) == Result::Success);
    REQUIRE(Text::load("borrowed", second.data(), second.size()) == Result::Success);
    const FontFace* faces = nullptr;
    REQUIRE(Text::faces("Borrowed Sans", &faces) == 1);
    REQUIRE(!strcmp(faces[0].name, "borrowed"));
    REQUIRE(faces[0].style.weight == FontWeight::Normal);
    auto text = Text::gen();
    REQUIRE(text->font(faces[0].name) == Result::Success);
    REQUIRE(text->font("Borrowed-First") == Result::InsufficientCondition);
    Paint::rel(text);
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("File font queries retain the filename key", "[tvgFontQuery]")
{
    REQUIRE(Initializer::init() == Result::Success);
    REQUIRE(Text::load(TEST_DIR "/PublicSans-Regular.ttf") == Result::Success);
    REQUIRE(Text::load(TEST_DIR "/PublicSans-Regular.ttf") == Result::Success);
    const FontFace* faces = nullptr;
    REQUIRE(Text::faces("Public Sans", &faces) == 1);
    REQUIRE(!strcmp(faces[0].name, "PublicSans-Regular"));
    REQUIRE(faces[0].style.weight == FontWeight::Normal);
    auto text = Text::gen();
    REQUIRE(text->font(faces[0].name) == Result::Success);
    REQUIRE(text->font("Public Sans Regular") == Result::InsufficientCondition);
    Paint::rel(text);
    REQUIRE(Initializer::term() == Result::Success);
}

#endif

#ifdef THORVG_OTF_LOADER_SUPPORT

TEST_CASE("CFF fonts share SFNT name and style queries", "[tvgFontQuery]")
{
    REQUIRE(Initializer::init() == Result::Success);
    REQUIRE(Text::load(TEST_DIR "/DMSans.otf") == Result::Success);
    const FontFace* faces = nullptr;
    REQUIRE(Text::faces("DM Sans", &faces) == 1);
    REQUIRE(!strcmp(faces[0].name, "DMSans"));
    REQUIRE(faces[0].style.weight == FontWeight::Normal);
    REQUIRE(faces[0].style.width == FontWidth::Normal);
    REQUIRE(faces[0].style.slant == FontSlant::Normal);
    auto text = Text::gen();
    REQUIRE(text->font(faces[0].name) == Result::Success);
    REQUIRE(text->font("DMSans-Regular") == Result::InsufficientCondition);
    Paint::rel(text);
    REQUIRE(Initializer::term() == Result::Success);
}

#endif
