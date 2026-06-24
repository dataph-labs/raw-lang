#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reg-memory.h"
#include "func-space.h"

uint8_t *load_bytecode(const char *filepath, size_t *out_size) {
    if (out_size != NULL) {
        *out_size = 0;
    } else {
        return NULL;
    }

    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    
    long file_size = ftell(file);
    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }
    
    // Return the reading pointer to the beginning of the file.
    rewind(file);

    uint8_t *bytecode = (uint8_t *)malloc(file_size);
    if (bytecode == NULL) {
        // Не удалось выделить память
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(bytecode, 1, file_size, file);
    fclose(file);

    if (bytes_read == 0) {
        free(bytecode);
        return NULL;
    }

    *out_size = bytes_read;
    return bytecode;
}

int main(int argc, char *argv[]) {
    printf("Hello, raw-runtime!\n");
    printf("Init runtime...\n");

    Val registers[REG_NUMBER] = {0};;
    printf("Init registers done.\n");

    bool halt = false;
    int pc = 0;

	uint8_t default_bytecode[] = {
    // Header
    0x52, 0x41, 0x57, 0x00, // "RAW\0"
    0x01, 0x00,             // version 	    1
    0x03, 0x00,             // const_size   3
    0x00, 0x00,             // type_count   0
    0x00, 0x00,             // method_count 0
    
    // Constant Pool
    0x01,
    0x2A, 0x00, 0x00, 0x00,
    0x07,
    0x0D, 0x00,
    0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x77, 0x6F, 0x72, 0x6C, 0x64, 0x21,
    0x01,
    0x00, 0x00, 0x00, 0x00,

    // Type Table

    // Method Table

    // Code
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x00,

    0x03, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x04, 0x01,

    0x03, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x02, 0x00,
    0x04, 0x00,

    0x00,
	};

    // In the future, there is code that reads ".braw" files.
	const char *path = NULL; 

    if (argc < 2) {
        printf("No paths specified, trying the standard path.\n");
        path = "resources.rawb";
    } else {
    	path = argv[1];
    }

    size_t size = 0;
    uint8_t *bytecode = load_bytecode(path, &size);

    uint16_t const_size = 0;

    if (bytecode == NULL) {
        printf("ERROR!!! The binary is empty or does not exist. Launch the program by default.\n");
        bytecode = default_bytecode;
    }

	if (bytecode[0] != 0x52 || bytecode[1] != 0x41 || 
		bytecode[2] != 0x57 || bytecode[3] != 0x00) {
		printf("Format not supported!\n");
		exit(1);
	}

	const_size = (bytecode[7] << 8) | bytecode[6];
	printf("Const size: %d\n", const_size);

	pc = 12;

    Val *const_pool = NULL;

    const_pool = malloc(const_size * sizeof(Val));

    for (uint16_t i = 0; i <= const_size-1; i++) {
		printf("Parsing constant %d at pc: %d\n", i, pc);

        uint8_t tag = bytecode[pc];
        pc += 1;

        switch (tag) {
			case 0x01: // i32
				const_pool[i].type = TYPE_INT32;
				const_pool[i].as.int32 = *(int32_t *)&bytecode[pc];
				pc += 4;
				break;

			case 0x02: // i64
				const_pool[i].type = TYPE_INT64;
				const_pool[i].as.int64 = *(int64_t *)&bytecode[pc];
				pc += 8;
				break;

			case 0x03: // u32
				const_pool[i].type = TYPE_UNS32;
				const_pool[i].as.uns32 = *(uint32_t *)&bytecode[pc];
				pc += 4;
				break;

			case 0x04: // u64
				const_pool[i].type = TYPE_UNS64;
				const_pool[i].as.uns64 = *(uint64_t *)&bytecode[pc];
				pc += 8;
				break;

			case 0x05: // f32
				const_pool[i].type = TYPE_FLOAT32;
				const_pool[i].as.float32 = *(float *)&bytecode[pc];
				pc += 4;
				break;

			case 0x06: // f64
				const_pool[i].type = TYPE_FLOAT64;
				const_pool[i].as.float64 = *(double *)&bytecode[pc];
				pc += 8;
				break;

			case 0x07: { // string
				// read len
				uint16_t len = (bytecode[pc + 1] << 8) | bytecode[pc];
				pc += 2;

				const_pool[i].type = TYPE_STRING;

				// In C, lines must end with a zero (\0)
				const_pool[i].as.string = malloc(len + 1);
				if (const_pool[i].as.string == NULL) {
					printf("Failed to allocate memory for a string in the constant pool.");
					exit(1);
				}

				// copy bytes
				memcpy(const_pool[i].as.string, &bytecode[pc], len);
				const_pool[i].as.string[len] = '\0'; // zero

				pc += len;
				break;
        }

        case 0x08: // i8
            const_pool[i].type = TYPE_INT8;
            const_pool[i].as.int8 = *(int8_t *)&bytecode[pc];
            pc += 1;
            break;

        case 0x09: // u8
            const_pool[i].type = TYPE_UNS8;
            const_pool[i].as.uns8 = *(uint8_t *)&bytecode[pc];
            pc += 1;
            break;

        case 0x0A: // i16
            const_pool[i].type = TYPE_INT16;
            const_pool[i].as.int16 = *(int16_t *)&bytecode[pc];
            pc += 2;
            break;

        case 0x0B: // u16
            const_pool[i].type = TYPE_UNS16;
            const_pool[i].as.uns16 = *(uint16_t *)&bytecode[pc];
            pc += 2;
            break;

        default:
            printf("Unknown constant tag 0x%02X at pc %d", tag, pc - 1);
            exit(1);
        }

		printf("Parsed...\n");
    }

	printf("Started read code in while loop...\n");

    while (halt == false) {
		printf("pc: %d\n", pc);

        uint8_t op = bytecode[pc];
        pc++;

        switch (op) {
			case 0x00: { // HALT
				halt = true;
				printf("HALT by 0x00...\n");
				break;
			}

			case 0x01: { // RESET
				pc = 0;
				break;
			}

			case 0x02: { // CALL
				uint8_t reg_idx = bytecode[pc++];
				Val *v = &registers[reg_idx];

				switch (v->type) {
				case TYPE_INT32:
					CALL_handle(v->as.int32);
					break;

				default:
					printf("Unknown type. CALL support only call_bytes.\n");
				}	
				break;
			}

			case 0x03: { // LOAD_CONST
				printf("LOAD CONST\n");

				uint8_t reg_idx = bytecode[pc++];
				uint32_t const_idx = *(uint32_t *)&bytecode[pc];
				pc += 4;

				if (const_idx >= const_size) {
					printf("Constant Index %d beyond borders.", const_idx);
				}

				registers[reg_idx] = const_pool[const_idx];
				printf("Loaded...\n");
				break;
			}

			case 0x04: { // PRINT
				uint8_t reg_idx = bytecode[pc++];
				Val *v = &registers[reg_idx];

				switch (v->type) {
				case TYPE_INT32:
					printf("%d\n", v->as.int32);
					break;
				case TYPE_INT64:
					printf("%ld\n", v->as.int64);
					break;
				case TYPE_UNS32:
					printf("%u\n", v->as.uns32);
					break;
				case TYPE_UNS64:
					printf("%lu\n", v->as.uns64);
					break;
				case TYPE_FLOAT32:
					printf("%f\n", v->as.float32);
					break;
				case TYPE_FLOAT64:
					printf("%f\n", v->as.float64);
					break;
				case TYPE_STRING:
					printf("%s\n", v->as.string);
					break;
				case TYPE_INT8:
					printf("%d\n", v->as.int8);
					break;
				case TYPE_UNS8:
					printf("%u\n", v->as.uns8);
					break;
				case TYPE_INT16:
					printf("%d\n", v->as.int16);
					break;
				case TYPE_UNS16:
					printf("%u\n", v->as.uns16);
					break;
				case TYPE_BOOL:
					printf("%s\n", v->as.boolean ? "true" : "false");
					break;
				default:
					printf("Unknown type\n");
				}
				break;
			}

			case 0x05: { // ADD

			}

        }
    }

    free(const_pool);
}