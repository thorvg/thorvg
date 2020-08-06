#include "tvgSimpleXmlParser.h"

static const char* _simpleXmlFindWhiteSpace(const char* itr, const char* itrEnd)
{
    for (; itr < itrEnd; itr++) {
        if (isspace((unsigned char)*itr)) break;
    }
    return itr;
}


static const char* _simpleXmlSkipWhiteSpace(const char* itr, const char* itrEnd)
{
    for (; itr < itrEnd; itr++) {
        if (!isspace((unsigned char)*itr)) break;
    }
    return itr;
}


static const char* _simpleXmlUnskipWhiteSpace(const char* itr, const char* itrStart)
{
    for (itr--; itr > itrStart; itr--) {
        if (!isspace((unsigned char)*itr)) break;
    }
    return itr + 1;
}


static const char* _simpleXmlFindStartTag(const char* itr, const char* itrEnd)
{
    return (const char*)memchr(itr, '<', itrEnd - itr);
}


static const char* _simpleXmlFindEndTag(const char* itr, const char* itrEnd)
{
    bool insideQuote = false;
    for (; itr < itrEnd; itr++) {
        if (*itr == '"') insideQuote = !insideQuote;
        if (!insideQuote) {
            if ((*itr == '>') || (*itr == '<'))
                return itr;
        }
    }
    return nullptr;
}


static const char* _simpleXmlFindEndCommentTag(const char* itr, const char* itrEnd)
{
    for (; itr < itrEnd; itr++) {
        if ((*itr == '-') && ((itr + 1 < itrEnd) && (*(itr + 1) == '-')) && ((itr + 2 < itrEnd) && (*(itr + 2) == '>'))) return itr + 2;
    }
    return nullptr;
}


static const char* _simpleXmlFindEndCdataTag(const char* itr, const char* itrEnd)
{
    for (; itr < itrEnd; itr++) {
        if ((*itr == ']') && ((itr + 1 < itrEnd) && (*(itr + 1) == ']')) && ((itr + 2 < itrEnd) && (*(itr + 2) == '>'))) return itr + 2;
    }
    return nullptr;
}


static const char* _simpleXmlFindDoctypeChildEndTag(const char* itr, const char* itrEnd)
{
    for (; itr < itrEnd; itr++) {
        if (*itr == '>') return itr;
    }
    return nullptr;
}


bool simpleXmlParseAttributes(const char* buf, unsigned bufLength, simpleXMLAttributeCb func, const void* data)
{
    const char *itr = buf, *itrEnd = buf + bufLength;
    char* tmpBuf = (char*)alloca(bufLength + 1);

    if (!buf) return false;
    if (!func) return false;

    while (itr < itrEnd) {
        const char* p = _simpleXmlSkipWhiteSpace(itr, itrEnd);
        const char *key, *keyEnd, *value, *valueEnd;
        char* tval;

        if (p == itrEnd) return true;

        key = p;
        for (keyEnd = key; keyEnd < itrEnd; keyEnd++) {
            if ((*keyEnd == '=') || (isspace((unsigned char)*keyEnd))) break;
        }
        if (keyEnd == itrEnd) return false;
        if (keyEnd == key) continue;

        if (*keyEnd == '=') value = keyEnd + 1;
        else {
            value = (const char*)memchr(keyEnd, '=', itrEnd - keyEnd);
            if (!value) return false;
            value++;
        }
        for (; value < itrEnd; value++) {
            if (!isspace((unsigned char)*value)) break;
        }
        if (value == itrEnd) return false;

        if ((*value == '"') || (*value == '\'')) {
            valueEnd = (const char*)memchr(value + 1, *value, itrEnd - value);
            if (!valueEnd) return false;
            value++;
        } else {
            valueEnd = _simpleXmlFindWhiteSpace(value, itrEnd);
        }

        memcpy(tmpBuf, key, keyEnd - key);
        tmpBuf[keyEnd - key] = '\0';

        tval = tmpBuf + (keyEnd - key) + 1;
        memcpy(tval, value, valueEnd - value);
        tval[valueEnd - value] = '\0';

        if (!func((void*)data, tmpBuf, tval)) return false;

        itr = valueEnd + 1;
    }
    return true;
}


bool simpleXmlParse(const char* buf, unsigned bufLength, bool strip, simpleXMLCb func, const void* data)
{
    const char *itr = buf, *itrEnd = buf + bufLength;

    if (!buf) return false;
    if (!func) return false;

#define CB(type, start, end)                                     \
    do {                                                         \
        size_t _sz = end - start;                                \
        bool _ret;                                               \
        _ret = func((void*)data, type, start, _sz);              \
        if (!_ret)                                               \
            return false;                                        \
    } while (0)

    while (itr < itrEnd) {
        if (itr[0] == '<') {
            if (itr + 1 >= itrEnd) {
                CB(SimpleXMLType::Error, itr, itrEnd);
                return false;
            } else {
                SimpleXMLType type;
                size_t toff;
                const char* p;

                if (itr[1] == '/') {
                    type = SimpleXMLType::Close;
                    toff = 1;
                } else if (itr[1] == '?') {
                    type = SimpleXMLType::Processing;
                    toff = 1;
                } else if (itr[1] == '!') {
                    if ((itr + sizeof("<!DOCTYPE>") - 1 < itrEnd) && (!memcmp(itr + 2, "DOCTYPE", sizeof("DOCTYPE") - 1)) && ((itr[2 + sizeof("DOCTYPE") - 1] == '>') || (isspace((unsigned char)itr[2 + sizeof("DOCTYPE") - 1])))) {
                        type = SimpleXMLType::Doctype;
                        toff = sizeof("!DOCTYPE") - 1;
                    } else if ((itr + sizeof("<!---->") - 1 < itrEnd) && (!memcmp(itr + 2, "--", sizeof("--") - 1))) {
                        type = SimpleXMLType::Comment;
                        toff = sizeof("!--") - 1;
                    } else if ((itr + sizeof("<![CDATA[]]>") - 1 < itrEnd) && (!memcmp(itr + 2, "[CDATA[", sizeof("[CDATA[") - 1))) {
                        type = SimpleXMLType::CData;
                        toff = sizeof("![CDATA[") - 1;
                    } else if (itr + sizeof("<!>") - 1 < itrEnd) {
                        type = SimpleXMLType::DoctypeChild;
                        toff = sizeof("!") - 1;
                    } else {
                        type = SimpleXMLType::Open;
                        toff = 0;
                    }
                } else {
                    type = SimpleXMLType::Open;
                    toff = 0;
                }

                if (type == SimpleXMLType::CData) p = _simpleXmlFindEndCdataTag(itr + 1 + toff, itrEnd);
                else if (type == SimpleXMLType::DoctypeChild) p = _simpleXmlFindDoctypeChildEndTag(itr + 1 + toff, itrEnd);
                else if (type == SimpleXMLType::Comment) p = _simpleXmlFindEndCommentTag(itr + 1 + toff, itrEnd);
                else p = _simpleXmlFindEndTag(itr + 1 + toff, itrEnd);

                if ((p) && (*p == '<')) {
                    type = SimpleXMLType::Error;
                    toff = 0;
                }

                if (p) {
                    const char *start, *end;

                    start = itr + 1 + toff;
                    end = p;

                    switch (type) {
                        case SimpleXMLType::Open: {
                            if (p[-1] == '/') {
                                type = SimpleXMLType::OpenEmpty;
                                end--;
                            }
                            break;
                        }
                        case SimpleXMLType::CData: {
                            if (!memcmp(p - 2, "]]", 2)) end -= 2;
                            break;
                        }
                        case SimpleXMLType::Processing: {
                            if (p[-1] == '?') end--;
                            break;
                        }
                        case SimpleXMLType::Comment: {
                            if (!memcmp(p - 2, "--", 2)) end -= 2;
                            break;
                        }
                        case SimpleXMLType::OpenEmpty:
                        case SimpleXMLType::Close:
                        case SimpleXMLType::Data:
                        case SimpleXMLType::Error:
                        case SimpleXMLType::Doctype:
                        case SimpleXMLType::DoctypeChild:
                        case SimpleXMLType::Ignored: {
                            break;
                        }
                    }

                    if ((strip) && (type != SimpleXMLType::Error) && (type != SimpleXMLType::CData)) {
                        start = _simpleXmlSkipWhiteSpace(start, end);
                        end = _simpleXmlUnskipWhiteSpace(end, start + 1);
                    }

                    CB(type, start, end);

                    if (type != SimpleXMLType::Error) itr = p + 1;
                    else itr = p;
                } else {
                    CB(SimpleXMLType::Error, itr, itrEnd);
                    return false;
                }
            }
        } else {
            const char *p, *end;

            if (strip) {
                p = _simpleXmlSkipWhiteSpace(itr, itrEnd);
                if (p) {
                    CB(SimpleXMLType::Ignored, itr, p);
                    itr = p;
                }
            }

            p = _simpleXmlFindStartTag(itr, itrEnd);
            if (!p) p = itrEnd;

            end = p;
            if (strip) end = _simpleXmlUnskipWhiteSpace(end, itr);

            if (itr != end) CB(SimpleXMLType::Data, itr, end);

            if ((strip) && (end < p)) CB(SimpleXMLType::Ignored, end, p);

            itr = p;
        }
    }

#undef CB

    return true;
}


bool simpleXmlParseW3CAttribute(const char* buf, simpleXMLAttributeCb func, const void* data)
{
    const char* end;
    char* key;
    char* val;
    char* next;

    if (!buf) return false;

    end = buf + strlen(buf);
    key = (char*)alloca(end - buf + 1);
    val = (char*)alloca(end - buf + 1);

    if (buf == end) return true;

    do {
        char* sep = (char*)strchr(buf, ':');
        next = (char*)strchr(buf, ';');

        key[0] = '\0';
        val[0] = '\0';

        if (next == nullptr && sep != nullptr) {
            memcpy(key, buf, sep - buf);
            key[sep - buf] = '\0';

            memcpy(val, sep + 1, end - sep - 1);
            val[end - sep - 1] = '\0';
        } else if (sep < next && sep != nullptr) {
            memcpy(key, buf, sep - buf);
            key[sep - buf] = '\0';

            memcpy(val, sep + 1, next - sep - 1);
            val[next - sep - 1] = '\0';
        } else if (next) {
            memcpy(key, buf, next - buf);
            key[next - buf] = '\0';
        }

        if (key[0]) {
            if (!func((void*)data, key, val)) return false;
        }

        buf = next + 1;
    } while (next != nullptr);

    return true;
}


const char* simpleXmlFindAttributesTag(const char* buf, unsigned bufLength)
{
    const char *itr = buf, *itrEnd = buf + bufLength;

    for (; itr < itrEnd; itr++) {
        if (!isspace((unsigned char)*itr)) {
            //User skip tagname and already gave it the attributes.
            if (*itr == '=') return buf;
        } else {
            itr = _simpleXmlSkipWhiteSpace(itr + 1, itrEnd);
            if (itr == itrEnd) return nullptr;
            return itr;
        }
    }

    return nullptr;
}

