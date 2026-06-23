#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} ByteBuffer;

void buf_init(ByteBuffer *buf) {
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

void buf_free(ByteBuffer *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

static void buf_ensure_capacity(ByteBuffer *buf, size_t needed) {
    if (buf->size + needed > buf->capacity) {
        // x2
        buf->capacity = buf->capacity == 0 ? 128 : buf->capacity * 2;
        
        // In case the x2 pitch is still smaller than it should be
        while (buf->size + needed > buf->capacity) {
            buf->capacity *= 2;
        }

        uint8_t *temp = realloc(buf->data, buf->capacity);
        if (temp == NULL) {
            perror("Failed to allocate memory for the buffer.");
            exit(1);
        }
        buf->data = temp;
    }
}

void buf_write_byte(ByteBuffer *buf, uint8_t byte) {
    buf_ensure_capacity(buf, 1);
    buf->data[buf->size++] = byte;
}

void buf_write_bytes(ByteBuffer *buf, const uint8_t *bytes, size_t count) {
    if (count == 0) return;
    buf_ensure_capacity(buf, count);
    memcpy(&buf->data[buf->size], bytes, count);
    buf->size += count;
}

#define HALT 0x00
#define RESET 0x01
#define CALL 0x02
#define LOAD_CONST 0x03
#define PRINT 0x04

int save_bytecode(const char *filepath, const uint8_t *bytecode, size_t size) {
    if (filepath == NULL || bytecode == NULL || size == 0) {
        return 0;
    }

    FILE *file = fopen(filepath, "wb");
    if (file == NULL) {
        perror("Error opening file for recording.");
        return 0;
    }

    size_t bytes_written = fwrite(bytecode, 1, size, file);

    fclose(file);

    return (bytes_written == size);
}

int main(void) {
    printf("Hello, rawсс!\n");
    printf("Init compiler...\n");

    ByteBuffer code;
    buf_init(&code);

    uint8_t base_header[] = { 0x52, 0x41, 0x57, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    buf_write_bytes(&code, base_header, sizeof(base_header));

    if (save_bytecode("resourses.rawb", code.data, sizeof(code.size))) {
        printf("COMPLETED without errors.\n");
    } else {
        fprintf(stderr, "Write ERROR.\n");
        return 1;
    }

    buf_free(&code);
    return 0;
}