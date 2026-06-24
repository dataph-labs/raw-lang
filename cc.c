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
        buf->capacity = buf->capacity == 0 ? 128 : buf->capacity * 2;
        
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

char *read_source_file(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        perror("The source file could not be opened.");
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    char *source_buffer = (char *)malloc(size + 1);
    if (source_buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(source_buffer, 1, size, file);
    fclose(file);

    source_buffer[bytes_read] = '\0';
    return source_buffer;
}

char *custom_strdup(const char *s) {
    size_t len = strlen(s);
    char *d = malloc(len + 1);
    if (d) {
        memcpy(d, s, len + 1);
    }
    return d;
}

/* --- Struct --- */

typedef enum {
    CONST_INT,
    CONST_STRING
} ConstType;

typedef struct {
    ConstType type;
    union {
        int int_val;
        char *str_val;
    } val;
} Constant;

#define MAX_CONSTANTS 1024
Constant constant_pool[MAX_CONSTANTS];
size_t constant_pool_count = 0;

int add_string_constant(const char *str) {
    for (size_t i = 0; i < constant_pool_count; i++) {
        if (constant_pool[i].type == CONST_STRING && strcmp(constant_pool[i].val.str_val, str) == 0) {
            return (int)i;
        }
    }
    if (constant_pool_count >= MAX_CONSTANTS) {
        fprintf(stderr, "Error: Constant pool overflow\n");
        exit(1);
    }
    constant_pool[constant_pool_count].type = CONST_STRING;
    constant_pool[constant_pool_count].val.str_val = custom_strdup(str);
    return (int)constant_pool_count++;
}

int add_int_constant(int val) {
    for (size_t i = 0; i < constant_pool_count; i++) {
        if (constant_pool[i].type == CONST_INT && constant_pool[i].val.int_val == val) {
            return (int)i;
        }
    }
    if (constant_pool_count >= MAX_CONSTANTS) {
        fprintf(stderr, "Error: Constant pool overflow\n");
        exit(1);
    }
    constant_pool[constant_pool_count].type = CONST_INT;
    constant_pool[constant_pool_count].val.int_val = val;
    return (int)constant_pool_count++;
}

/* --- Lexer --- */

typedef struct {
    const char *source;
    size_t cursor;
} Lexer;

void skip_whitespace(Lexer *lexer) {
    while (lexer->source[lexer->cursor] != '\0' && 
          (lexer->source[lexer->cursor] == ' ' || 
           lexer->source[lexer->cursor] == '\t' || 
           lexer->source[lexer->cursor] == '\r' || 
           lexer->source[lexer->cursor] == '\n')) {
        lexer->cursor++;
    }
}

char *parse_identifier(Lexer *lexer) {
    size_t start = lexer->cursor;
    while ((lexer->source[lexer->cursor] >= 'a' && lexer->source[lexer->cursor] <= 'z') ||
           (lexer->source[lexer->cursor] >= 'A' && lexer->source[lexer->cursor] <= 'Z') ||
           lexer->source[lexer->cursor] == '_') {
        lexer->cursor++;
    }
    size_t length = lexer->cursor - start;
    if (length == 0) return NULL;
    char *id = malloc(length + 1);
    memcpy(id, &lexer->source[start], length);
    id[length] = '\0';
    return id;
}

char *parse_string(Lexer *lexer) {
    if (lexer->source[lexer->cursor] != '"') return NULL;
    lexer->cursor++; // Pass
    size_t start = lexer->cursor;
    while (lexer->source[lexer->cursor] != '"' && lexer->source[lexer->cursor] != '\0') {
        lexer->cursor++;
    }
    if (lexer->source[lexer->cursor] == '\0') {
        fprintf(stderr, "Syntax Error: Unterminated string literal\n");
        exit(1);
    }
    size_t length = lexer->cursor - start;
    lexer->cursor++; // Pass
    char *str = malloc(length + 1);
    memcpy(str, &lexer->source[start], length);
    str[length] = '\0';
    return str;
}

int parse_number(Lexer *lexer) {
    int val = 0;
    while (lexer->source[lexer->cursor] >= '0' && lexer->source[lexer->cursor] <= '9') {
        val = val * 10 + (lexer->source[lexer->cursor] - '0');
        lexer->cursor++;
    }
    return val;
}

void parse_and_compile(Lexer *lexer, ByteBuffer *code_buf) {
    while (1) {
        skip_whitespace(lexer);
        if (lexer->source[lexer->cursor] == '\0') {
            break;
        }

        char *id = parse_identifier(lexer);
        if (id == NULL) {
            fprintf(stderr, "Syntax Error: Expected identifier at cursor position %zu\n", lexer->cursor);
            exit(1);
        }

        skip_whitespace(lexer);
        if (lexer->source[lexer->cursor] != '(') {
            fprintf(stderr, "Syntax Error: Expected '(' after '%s'\n", id);
            free(id);
            exit(1);
        }
        lexer->cursor++; // Pass '('

        skip_whitespace(lexer);

        if (strcmp(id, "print") == 0) {
            char *str = parse_string(lexer);
            if (str == NULL) {
                fprintf(stderr, "Syntax Error: Expected string literal inside print()\n");
                free(id);
                exit(1);
            }

            skip_whitespace(lexer);
            if (lexer->source[lexer->cursor] != ')') {
                fprintf(stderr, "Syntax Error: Expected ')' after string in print()\n");
                free(str); free(id);
                exit(1);
            }
            lexer->cursor++; // Pass ')'

            skip_whitespace(lexer);
            if (lexer->source[lexer->cursor] != ';') {
                fprintf(stderr, "Syntax Error: Expected ';' after print(...)\n");
                free(str); free(id);
                exit(1);
            }
            lexer->cursor++; // Pass ';'

            int idx = add_string_constant(str);
            free(str);

            // Codegen:
            // LOAD_CONST R0 idx
            buf_write_byte(code_buf, LOAD_CONST);
            buf_write_byte(code_buf, 0x00); // R0
            uint8_t idx_bytes[4] = {
                idx & 0xFF,
                (idx >> 8) & 0xFF,
                (idx >> 16) & 0xFF,
                (idx >> 24) & 0xFF
            };
            buf_write_bytes(code_buf, idx_bytes, 4);

            // 2. PRINT R0
            buf_write_byte(code_buf, PRINT);
            buf_write_byte(code_buf, 0x00); // R0

        } else if (strcmp(id, "call") == 0) {
            if (lexer->source[lexer->cursor] < '0' || lexer->source[lexer->cursor] > '9') {
                fprintf(stderr, "Syntax Error: Expected integer literal in call()\n");
                free(id);
                exit(1);
            }
            int val = parse_number(lexer);

            skip_whitespace(lexer);
            if (lexer->source[lexer->cursor] != ')') {
                fprintf(stderr, "Syntax Error: Expected ')' after number in call()\n");
                free(id);
                exit(1);
            }
            lexer->cursor++;

            skip_whitespace(lexer);
            if (lexer->source[lexer->cursor] != ';') {
                fprintf(stderr, "Syntax Error: Expected ';' after call(...)\n");
                free(id);
                exit(1);
            }
            lexer->cursor++;

            int idx = add_int_constant(val);

            // Codegen:
            // LOAD_CONST R0 idx
            buf_write_byte(code_buf, LOAD_CONST);
            buf_write_byte(code_buf, 0x00); // R0
            uint8_t idx_bytes[4] = {
                idx & 0xFF,
                (idx >> 8) & 0xFF,
                (idx >> 16) & 0xFF,
                (idx >> 24) & 0xFF
            };
            buf_write_bytes(code_buf, idx_bytes, 4);

            // CALL R0
            buf_write_byte(code_buf, CALL);
            buf_write_byte(code_buf, 0x00); // R0

        } else {
            fprintf(stderr, "Syntax Error: Unknown instruction '%s'\n", id);
            free(id);
            exit(1);
        }

        free(id);
    }

    buf_write_byte(code_buf, HALT);
}

void write_constant_pool(ByteBuffer *buf) {
    for (size_t i = 0; i < constant_pool_count; i++) {
        if (constant_pool[i].type == CONST_INT) {
            buf_write_byte(buf, 0x01);
            uint32_t val = (uint32_t)constant_pool[i].val.int_val;
            uint8_t bytes[4] = {
                val & 0xFF,
                (val >> 8) & 0xFF,
                (val >> 16) & 0xFF,
                (val >> 24) & 0xFF
            };
            buf_write_bytes(buf, bytes, 4);
        } else if (constant_pool[i].type == CONST_STRING) {
            buf_write_byte(buf, 0x07);
            size_t len = strlen(constant_pool[i].val.str_val);
            uint16_t len_u16 = (uint16_t)len;
            uint8_t len_bytes[2] = {
                len_u16 & 0xFF,
                (len_u16 >> 8) & 0xFF
            };
            buf_write_bytes(buf, len_bytes, 2);
            buf_write_bytes(buf, (const uint8_t *)constant_pool[i].val.str_val, len);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source_file>\n", argv[0]);
        return 1;
    }

    printf("Hello, rawсс!\n");
    printf("Init compiler...\n");

    char *source = read_source_file(argv[1]);
    if (source == NULL) {
        return 1;
    }

    Lexer lexer;
    lexer.source = source;
    lexer.cursor = 0;

    ByteBuffer code_instructions;
    buf_init(&code_instructions);

    parse_and_compile(&lexer, &code_instructions);

    ByteBuffer final_code;
    buf_init(&final_code);

    printf("Init done.\n");

    uint16_t const_size_le = (uint16_t)constant_pool_count;
    uint8_t base_header[] = {
        0x52, 0x41, 0x57, 0x00,                 // MAGIC         RAW\0    0,1,2,3,
        0x01, 0x00,                             // Version       1        4,5,
        (uint8_t)(const_size_le & 0xFF),        // const_size    LE       6,7,
        (uint8_t)((const_size_le >> 8) & 0xFF), 
        0x00, 0x00,                             // type_count    0        8,9,
        0x00, 0x00,                             // method_count  0        10,11.
    };

    buf_write_bytes(&final_code, base_header, sizeof(base_header));

    write_constant_pool(&final_code);

    buf_write_bytes(&final_code, code_instructions.data, code_instructions.size);

    if (save_bytecode(argv[2], final_code.data, final_code.size)) {
        printf("COMPILED and COMPLETED without errors.\n");
    } else {
        fprintf(stderr, "COMPILED with write ERROR.\n");
        buf_free(&code_instructions);
        buf_free(&final_code);
        free(source);
        return 1;
    }

    for (size_t i = 0; i < constant_pool_count; i++) {
        if (constant_pool[i].type == CONST_STRING) {
            free(constant_pool[i].val.str_val);
        }
    }
    buf_free(&code_instructions);
    buf_free(&final_code);
    free(source);
    return 0;
}