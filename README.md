libmalelf
=========

The libmalelf is an evil library that *SHOULD* be used for good!
It was developed with the intent to assist in the process of infecting binaries
and provide a safe way to analyze malwares.

With the library you can:

     * infect/backdoorize ELF binaries;
     * dissect ELF data structure;
     * Add segments/sections content and headers to the binary;
     * Modify any ELF information;
     * Find holes/gaps to insert code;
     * Create your own binary from scratch;
     * any other?

The libmalelf is a project under active development and not yet complete. If
you need more information or want to help, contact us at:
    $ echo "rb.moc.sulpces@letan" | rev
    or subscribe to the mailing-list:
    https://groups.google.com/forum/#!forum/malelficus


Building and Installing
=======================

The library is written in pure C, so the build process is very simple.
If you want help the project, you can enable and run the unit tests after
compilation.

First, get the source:

    $ git clone https://github.com/SecPlus/libmalelf.git

It has few dependencies to be installed, which are:

* NASM
* libxml2-dev
* libcunit1-dev (optional)

On debian based machines that dependencies can be resolved with:

    $ sudo apt-get install nasm libxml2-dev libcunit1-dev

Ok? Then build:

    $ ./configure --prefix=/usr --enable-tests
    $ make

If you chose to run the tests (The tests are platform dependent and should pass
for gcc version 4.7.2 (Debian 4.7.2-5)):

    $ make check

Install:

    $ sudo make install

See the directory examples to more information about the usage of the library.

:-)

Examples
==========

With libmalelf you can dissect complexes binaries generated by gcc, clang or
nasm as well as create your own ELF executable from scratch using your
shellcode/payload/virus as text segments. See below:

```c
/**
 * Simple example to create a RAW ELF Executable file.
 * Very useful if you need create a tiny ELF to send in a exploit or
 * simply to understand the ELF format.
 *
 * This example could be used to create a tiny ELF binary that execute your
 * shellcode/payload.
 *
 * ;; shellcode.asm
 *
 * BITS 32
 *
 * _start:
 *     push dword 0x37333331
 *     mov ecx, esp
 *     mov edx, 4
 *     mov ebx, 1
 *     mov eax, 4      ; write(stdout, "1337", 4)
 *     int 0x80
 *
 *     xor eax, eax
 *     xor ebx, ebx
 *     inc eax         ; exit(0)
 *     int 0x80
 *
 * ;; cut here ;;
 *
 * $ nasm -f bin ./shellcode.asm -o shellcode.bin
 *
 * $ ./creating_elf ./shellcode.bin ./tiny_elf
 * $ ./tiny_elf
 * 1337
 * $
 *
 * by i4k
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <elf.h>

#include <malelf/defines.h>
#include <malelf/binary.h>
#include <malelf/error.h>
#include <malelf/util.h>

int main(int argc, char **argv)
{
	MalelfBinary bin;
	Elf32_Phdr phdr;
	_u32 error;
	int fd;
	struct stat st_info;
	unsigned char *text_data;

	if (argc < 3) {
		printf("%s <text-segment-file> <output-elf>\n", *argv);
		return 1;
	}

	/* open text segment (your payload) */
	fd = open(argv[1], O_RDONLY);

	if (fd == -1) {
		fprintf(stderr, "Failed to open %s...\n", argv[1]);
		return 1;
	}

	if (stat(argv[1], &st_info) == -1) {
		perror("Failed to stat file.\n");
		return 1;
	}


	text_data = mmap(NULL, st_info.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (!text_data) {
		perror("Failed to mmap...\n");
		return 1;
	}

	malelf_binary_init(&bin);
	error = malelf_binary_create_elf_exec(&bin, MALELF_ELF32);

	if (MALELF_SUCCESS != error) {
		MALELF_PERROR(error);
		return 1;
	}

	phdr.p_type = PT_LOAD;
	phdr.p_offset = sizeof (Elf32_Ehdr) + sizeof (Elf32_Phdr) * 2;
	phdr.p_vaddr = 0x08048000 + phdr.p_offset;
	phdr.p_paddr = phdr.p_vaddr;
	phdr.p_filesz = st_info.st_size;
	phdr.p_memsz = phdr.p_filesz;
	phdr.p_flags = PF_X;
	phdr.p_align = 0;

	error = malelf_binary_add_phdr32(&bin, &phdr);

	if (MALELF_SUCCESS != error) {
		MALELF_PERROR(error);
		malelf_binary_close(&bin);
		return 1;
	}

	bin.mem = malelf_realloc(bin.mem, phdr.p_offset + st_info.st_size);

	memcpy(bin.mem + phdr.p_offset, text_data, st_info.st_size);

	malelf_ehdr_set_entry(&bin.ehdr, phdr.p_vaddr);

	error = malelf_binary_write(&bin, argv[2]);

	if (MALELF_SUCCESS != error) {
		MALELF_PERROR(error);
		return 1;
	}

	munmap(text_data, st_info.st_size);
	malelf_binary_close(&bin);
	return 0;
}

```
