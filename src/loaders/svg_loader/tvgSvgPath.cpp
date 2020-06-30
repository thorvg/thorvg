#ifndef __TVG_SVG_PATH_CPP_
#define __TVG_SVG_PATH_CPP_

#include "tvgSvgPath.h"


static char* _skipComma(const char* content)
{
    while (*content && isspace(*content)) {
        content++;
    }
    if (*content == ',') return (char*)content + 1;
    return (char*)content;
}


static inline bool _parseNumber(char** content, float* number)
{
    char* end = NULL;
    *number = strtof(*content, &end);
    //If the start of string is not number
    if ((*content) == end) return false;
    //Skip comma if any
    *content = _skipComma(end);
    return true;
}


static inline bool _parseLong(char** content, int* number)
{
    char* end = NULL;
    *number = strtol(*content, &end, 10) ? 1 : 0;
    //If the start of string is not number
    if ((*content) == end) return false;
    *content = _skipComma(end);
    return true;
}


static int _numberCount(char cmd)
{
    int count = 0;
    switch (cmd) {
        case 'M':
        case 'm':
        case 'L':
        case 'l': {
            count = 2;
            break;
        }
        case 'C':
        case 'c':
        case 'E':
        case 'e': {
            count = 6;
            break;
        }
        case 'H':
        case 'h':
        case 'V':
        case 'v': {
            count = 1;
            break;
        }
        case 'S':
        case 's':
        case 'Q':
        case 'q':
        case 'T':
        case 't': {
            count = 4;
            break;
        }
        case 'A':
        case 'a': {
            count = 7;
            break;
        }
        default:
            break;
    }
    return count;
}


static void _processCommand(vector<PathCommand>* cmds, vector<Point>* pts, char cmd, float* arr, int count, Point* cur, Point* curCtl)
{
    int i;
    switch (cmd) {
        case 'm':
        case 'l':
        case 'c':
        case 's':
        case 'q':
        case 't': {
            for (i = 0; i < count - 1; i += 2) {
                arr[i] = arr[i] + cur->x;
                arr[i + 1] = arr[i + 1] + cur->y;
            }
            break;
        }
        case 'h': {
            arr[0] = arr[0] + cur->x;
            break;
        }
        case 'v': {
            arr[0] = arr[0] + cur->y;
            break;
        }
        case 'a': {
            arr[5] = arr[5] + cur->x;
            arr[6] = arr[6] + cur->y;
            break;
        }
        default: {
            break;
        }
    }

    switch (cmd) {
        case 'm':
        case 'M': {
            Point p = {arr[0], arr[1]};
            cmds->push_back(PathCommand::MoveTo);
            pts->push_back(p);
            *cur = {arr[0] ,arr[1]};
            break;
        }
        case 'l':
        case 'L': {
            Point p = {arr[0], arr[1]};
            cmds->push_back(PathCommand::LineTo);
            pts->push_back(p);
            *cur = {arr[0] ,arr[1]};
            break;
        }
        case 'c':
        case 'C': {
            Point p[3];
            cmds->push_back(PathCommand::CubicTo);
            p[0] = {arr[0], arr[1]};
            p[1] = {arr[2], arr[3]};
            p[2] = {arr[4], arr[5]};
            pts->push_back(p[0]);
            pts->push_back(p[1]);
            pts->push_back(p[2]);
            *curCtl = p[1];
            *cur = p[2];
            break;
        }
        case 's':
        case 'S': {
            Point p[3], ctrl;
            if ((cmds->size() > 1) && (cmds->at(cmds->size() - 2) == PathCommand::CubicTo)) {
                ctrl.x = 2 * cur->x - curCtl->x;
                ctrl.y = 2 * cur->x - curCtl->y;
            } else {
                ctrl = *cur;
            }
            cmds->push_back(PathCommand::CubicTo);
            p[0] = ctrl;
            p[1] = {arr[0], arr[1]};
            p[2] = {arr[2], arr[3]};
            pts->push_back(p[0]);
            pts->push_back(p[1]);
            pts->push_back(p[2]);
            *curCtl = p[1];
            *cur = p[2];
            break;
        }
        case 'q':
        case 'Q': {
            tvg::Point p[3];
            float ctrl_x0 = (cur->x + 2 * arr[0]) * (1.0 / 3.0);
            float ctrl_y0 = (cur->y + 2 * arr[1]) * (1.0 / 3.0);
            float ctrl_x1 = (arr[2] + 2 * arr[0]) * (1.0 / 3.0);
            float ctrl_y1 = (arr[3] + 2 * arr[1]) * (1.0 / 3.0);
            cmds->push_back(tvg::PathCommand::CubicTo);
            p[0] = {ctrl_x0, ctrl_y0};
            p[1] = {ctrl_x1, ctrl_y1};
            p[2] = {arr[2], arr[3]};
            pts->push_back(p[0]);
            pts->push_back(p[1]);
            pts->push_back(p[2]);
            *curCtl = p[1];
            *cur = p[2];
            break;
        }
        case 't':
        case 'T': {
            Point p = {arr[0], arr[1]};
            cmds->push_back(PathCommand::MoveTo);
            pts->push_back(p);
            *cur = {arr[0] ,arr[1]};
            break;
        }
        case 'h':
        case 'H': {
            Point p = {arr[0], cur->y};
            cmds->push_back(PathCommand::LineTo);
            pts->push_back(p);
            cur->x = arr[0];
            break;
        }
        case 'v':
        case 'V': {
            Point p = {cur->x, arr[0]};
            cmds->push_back(PathCommand::LineTo);
            pts->push_back(p);
            cur->y = arr[0];
            break;
        }
        case 'z':
        case 'Z': {
            cmds->push_back(PathCommand::Close);
            break;
        }
        case 'a':
        case 'A': {
            //TODO: Implement arc_to
            break;
        }
        case 'E':
        case 'e': {
            //TODO: Implement arc
            break;
        }
        default: {
            break;
        }
    }
}


static char* _nextCommand(char* path, char* cmd, float* arr, int* count)
{
    int i = 0, large, sweep;

    path = _skipComma(path);
    if (isalpha(*path)) {
        *cmd = *path;
        path++;
        *count = _numberCount(*cmd);
    } else {
        if (*cmd == 'm') *cmd = 'l';
        else if (*cmd == 'M') *cmd = 'L';
    }
    if (*count == 7) {
        //Special case for arc command
        if (_parseNumber(&path, &arr[0])) {
            if (_parseNumber(&path, &arr[1])) {
                if (_parseNumber(&path, &arr[2])) {
                    if (_parseLong(&path, &large)) {
                        if (_parseLong(&path, &sweep)) {
                            if (_parseNumber(&path, &arr[5])) {
                                if (_parseNumber(&path, &arr[6])) {
                                    arr[3] = large;
                                    arr[4] = sweep;
                                    return path;
                                }
                            }
                        }
                    }
                }
            }
        }
        *count = 0;
        return NULL;
    }
    for (i = 0; i < *count; i++) {
        if (!_parseNumber(&path, &arr[i])) {
            *count = 0;
            return NULL;
        }
        path = _skipComma(path);
    }
    return path;
}


tuple<vector<PathCommand>, vector<Point>> svgPathToTvgPath(const char* svgPath)
{
    vector<PathCommand> cmds;
    vector<Point> pts;

    float numberArray[7];
    int numberCount = 0;
    Point cur = { 0, 0 };
    Point curCtl = { 0, 0 };
    char cmd = 0;
    char* path = (char*)svgPath;
    char* curLocale;

    curLocale = setlocale(LC_NUMERIC, NULL);
    if (curLocale) curLocale = strdup(curLocale);
    setlocale(LC_NUMERIC, "POSIX");

    while ((path[0] != '\0')) {
        path = _nextCommand(path, &cmd, numberArray, &numberCount);
        if (!path) break;
        _processCommand(&cmds, &pts, cmd, numberArray, numberCount, &cur, &curCtl);
    }

    setlocale(LC_NUMERIC, curLocale);
    if (curLocale) free(curLocale);

   return make_tuple(cmds, pts);
}

#endif //__TVG_SVG_PATH_CPP_
