#ifndef _TVG_SIMPLE_XML_PARSER_H_
#define _TVG_SIMPLE_XML_PARSER_H_

#include <ctype.h>
#include <cstring>
#include <alloca.h>

enum class SimpleXMLType
{
    Open = 0,     //!< \<tag attribute="value"\>
    OpenEmpty,   //!< \<tag attribute="value" /\>
    Close,        //!< \</tag\>
    Data,         //!< tag text data
    CData,        //!< \<![cdata[something]]\>
    Error,        //!< error contents
    Processing,   //!< \<?xml ... ?\> \<?php .. ?\>
    Doctype,      //!< \<!doctype html
    Comment,      //!< \<!-- something --\>
    Ignored,      //!< whatever is ignored by parser, like whitespace
    DoctypeChild //!< \<!doctype_child
};

typedef bool (*simpleXMLCb)(void* data, SimpleXMLType type, const char* content, unsigned offset, unsigned length);
typedef bool (*simpleXMLAttributeCb)(void* data, const char* key, const char* value);

bool simpleXmlParseAttributes(const char* buf, unsigned buflen, simpleXMLAttributeCb func, const void* data);
bool simpleXmlParse(const char* buf, unsigned buflen, bool strip, simpleXMLCb func, const void* data);
bool simpleXmlParseW3CAttribute(const char* buf, simpleXMLAttributeCb func, const void* data);
const char *simpleXmlFindAttributesTag(const char* buf, unsigned buflen);

#endif //_TVG_SIMPLE_XML_PARSER_H_
