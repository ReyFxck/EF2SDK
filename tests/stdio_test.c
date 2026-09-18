#include <ef2/heap.h>
#include <ef2/stdio.h>

typedef struct {
    char data[1024];
    ef2_size_t size;
    ef2_size_t max_chunk;
} test_sink;

static ef2_s32 test_write(
    void *context,
    const char *data,
    ef2_size_t size)
{
    test_sink *sink =
        (test_sink *)context;
    ef2_size_t amount = size;
    ef2_size_t i;

    if (sink->max_chunk != 0u &&
        amount > sink->max_chunk)
        amount = sink->max_chunk;

    if (amount >
        sizeof(sink->data) - sink->size)
        amount =
            sizeof(sink->data) -
            sink->size;

    if (amount == 0u)
        return -1;

    for (i = 0; i < amount; ++i)
        sink->data[sink->size + i] =
            data[i];

    sink->size += amount;
    return (ef2_s32)amount;
}

static int same_text(
    const char *left,
    const char *right,
    ef2_size_t size)
{
    ef2_size_t i;

    for (i = 0; i < size; ++i) {
        if (left[i] != right[i])
            return 0;
    }

    return 1;
}

int main(void)
{
    static unsigned char heap_memory[8192];
    test_sink sink = {{0}, 0, 3};
    ef2_FILE stream = {
        test_write,
        &sink
    };
    int result;

    if (ef2_heap_init(
            heap_memory,
            sizeof(heap_memory)) != 0)
        return 1;

    result = ef2_fprintf(
        &stream,
        "%s:%08x:%d",
        "EF2",
        0x2Au,
        -7);

    if (result != 15)
        return 2;

    if (sink.size != 15u ||
        !same_text(
            sink.data,
            "EF2:0000002a:-7",
            15u))
        return 3;

    sink.size = 0;
    sink.max_chunk = 17;

    ef2_stdio_set_stdout(
        test_write,
        &sink);

    result = ef2_printf(
        "%s-%u",
        "stream",
        36u);

    if (result != 9 ||
        sink.size != 9u ||
        !same_text(
            sink.data,
            "stream-36",
            9u))
        return 4;

    sink.size = 0;

    result = ef2_puts("hello");

    if (result != 6 ||
        sink.size != 6u ||
        !same_text(
            sink.data,
            "hello\n",
            6u))
        return 5;

    /*
     * Force the heap-backed formatting path (>255 bytes).
     * Width is already supported by the formatter core.
     */
    sink.size = 0;
    sink.max_chunk = 31;

    result = ef2_printf("%0300u", 1u);

    if (result != 300 ||
        sink.size != 300u ||
        sink.data[299] != '1')
        return 6;

    return 0;
}
