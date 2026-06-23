#include "func-space.h"

#include <stdio.h>

void CALL_handle(int32_t call_byte) {
	switch (call_byte) {
		case 0:
			printf("PRINTF BY CALL\n");
			break;
	}
}