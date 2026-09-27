//Compiled separately to exercise allocation across static-library boundaries.
#include <cstddef>

struct ExternalObject
{
    int value = 42;
};

extern "C" void* externalCreate(bool array)
{
    return array ? new ExternalObject[3] : new ExternalObject;
}

extern "C" int externalDestroy(void* ptr, bool array)
{
    auto object = static_cast<ExternalObject*>(ptr);
    auto value = object->value;
    if (array) delete[] object;
    else delete object;
    return value;
}
