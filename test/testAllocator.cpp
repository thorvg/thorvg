#include <thorvg.h>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <type_traits>

extern "C" void* externalCreate(bool array);
extern "C" int externalDestroy(void* ptr, bool array);

static unsigned allocations = 0;
static unsigned deallocations = 0;

//A host application must be able to provide these without colliding with ThorVG.
void* operator new(std::size_t size)
{
    ++allocations;
    if (auto ptr = std::malloc(size ? size : 1)) return ptr;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    throw std::bad_alloc();
#else
    std::abort();
#endif
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* ptr) noexcept
{
    ++deallocations;
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept
{
    ::operator delete(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
    ::operator delete(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
    ::operator delete[](ptr);
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "Allocator check failed: %s (line %d)\n", #condition, __LINE__); \
        return 1; \
    } \
} while (false)

static unsigned liveObjects = 0;

struct AllocatedBase : tvg::Allocator
{
    virtual ~AllocatedBase() = default;
};

struct AllocatedObject : AllocatedBase
{
    int value = 42;
    AllocatedObject() { ++liveObjects; }
    ~AllocatedObject() override { --liveObjects; }
};

struct AllocatedTask : tvg::Allocator
{
    virtual ~AllocatedTask() = default;
};

// Like the loaders/savers, both bases inherit the same static allocator operators.
struct AllocatedDiamond : AllocatedObject, AllocatedTask
{
};

struct AllocatedValue : tvg::Allocator
{
    int value = 42;
};

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
struct ThrowingObject : AllocatedObject
{
    ThrowingObject() { throw 42; }
};
#endif

int main()
{
    static_assert(std::is_empty<tvg::Allocator>::value, "Allocator must not store per-object state");
    CHECK(tvg::Initializer::init(0) == tvg::Result::Success);

    auto beforeAlloc = allocations;
    auto beforeFree = deallocations;
    auto external = externalCreate(false);
    auto externalArray = externalCreate(true);
    CHECK(externalDestroy(external, false) == 42);
    CHECK(externalDestroy(externalArray, true) == 42);
    CHECK(allocations == beforeAlloc + 2);
    CHECK(deallocations == beforeFree + 2);

    beforeAlloc = allocations;
    beforeFree = deallocations;
    AllocatedBase* object = new AllocatedObject;
    auto array = new AllocatedObject[3];
    CHECK(liveObjects == 4);
    CHECK(array[2].value == 42);
    delete object;
    delete[] array;
    CHECK(liveObjects == 0);

    object = new (std::nothrow) AllocatedObject;
    array = new (std::nothrow) AllocatedObject[3];
    CHECK(object && array);
    CHECK(liveObjects == 4);
    CHECK(array[2].value == 42);
    delete object;
    delete[] array;
    CHECK(liveObjects == 0);

    alignas(AllocatedObject) unsigned char storage[sizeof(AllocatedObject)];
    auto placed = new (storage) AllocatedObject;
    CHECK(static_cast<void*>(placed) == storage);
    CHECK(placed->value == 42 && liveObjects == 1);
    placed->~AllocatedObject();
    CHECK(liveObjects == 0);

    alignas(AllocatedValue) unsigned char arrayStorage[sizeof(AllocatedValue) * 3];
    auto placedArray = new (arrayStorage) AllocatedValue[3];
    CHECK(static_cast<void*>(placedArray) == arrayStorage);
    CHECK(placedArray[2].value == 42);
    for (unsigned i = 0; i < 3; ++i)
        placedArray[i].~AllocatedValue();

    AllocatedTask* task = new AllocatedDiamond;
    auto tasks = new (std::nothrow) AllocatedDiamond[2];
    CHECK(tasks && liveObjects == 3);
    delete task;
    delete[] tasks;
    CHECK(liveObjects == 0);

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    // Constructor failure must free nothrow allocations, but leave placement storage intact.
    try {
        auto failed = new (std::nothrow) ThrowingObject;
        delete failed;
        CHECK(false);
    } catch (int value) {
        CHECK(value == 42 && liveObjects == 0);
    }
    try {
        auto failed = new (std::nothrow) ThrowingObject[2];
        delete[] failed;
        CHECK(false);
    } catch (int value) {
        CHECK(value == 42 && liveObjects == 0);
    }
    alignas(ThrowingObject) unsigned char throwingStorage[sizeof(ThrowingObject)];
    try {
        auto failed = new (throwingStorage) ThrowingObject;
        failed->~ThrowingObject();
        CHECK(false);
    } catch (int value) {
        CHECK(value == 42 && liveObjects == 0);
    }
#endif

    //Factories and deletion through public base pointers must use the same heap.
    tvg::Paint* shape = tvg::Shape::gen();
    tvg::Fill* gradient = tvg::LinearGradient::gen();
    tvg::Canvas* canvas = tvg::SwCanvas::gen();
    auto animation = tvg::Animation::gen();
    auto accessor = tvg::Accessor::gen();
    auto saver = tvg::Saver::gen();
    CHECK(shape && gradient && canvas && animation && accessor && saver);
    delete shape;
    delete gradient;
    delete canvas;
    delete animation;
    delete accessor;
    delete saver;
    CHECK(allocations == beforeAlloc);
    CHECK(deallocations == beforeFree);

    CHECK(tvg::Initializer::term() == tvg::Result::Success);
    return 0;
}
