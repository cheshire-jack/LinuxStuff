#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <unistd.h>
#include <elf.h>

/* Take in a binary (32 or 64 bit) and overwrite the section table
 * with zeroes.  Overwrite the section names with zeroes
 * Fix the ELF header and replace the old binary with new one
 */

/* Check file type and architecture */
int check_file(void *addr, int *arch) 
{
	Elf64_Ehdr* elf_hdr = (Elf64_Ehdr *) addr;
	if(elf_hdr->e_ident[0] != 0x7f || elf_hdr->e_ident[1] != 'E' || elf_hdr->e_ident[2] != 'L' || elf_hdr->e_ident[3] != 'F')
		return 0;

	if(elf_hdr->e_ident[EI_CLASS] == 1)
		*arch = 32;
	else
		*arch = 64;

	return 1;
}
/* 32 bit find section table */

Elf32_Shdr* find_sections32(void* addr, int* section_count, int* string_index)
{
	Elf32_Ehdr* hdr = (Elf32_Ehdr *) addr;
	printf("[+]\t\tZeroing number of sections\n");
	*section_count = hdr->e_shnum;
	hdr->e_shnum = 0;
	
	printf("[+]\t\tZeroing the string index\n");
	*string_index = hdr->e_shstrndx;
	hdr->e_shstrndx = 0;
	
	printf("[+]\t\tZeroing the section header offset\n");
	Elf32_Off section_offset = hdr->e_shoff;
	hdr->e_shoff = 0;


	return (Elf32_Shdr *) (&addr[section_offset]);

}

/* 64 bit find sections table */
Elf64_Shdr* find_sections64(void* addr, int* section_count, int* string_index)
{
	Elf64_Ehdr* hdr = (Elf64_Ehdr *) addr;
	printf("[+]\t\tZeroing number of sections\n");
	*section_count = hdr->e_shnum;
	hdr->e_shnum = 0;
	
	printf("[+]\t\tZeroing the string index\n");
	*string_index = hdr->e_shstrndx;
	hdr->e_shstrndx = 0;
	
	printf("[+]\t\tZeroing the section header offset\n");
	Elf64_Off section_offset = hdr->e_shoff;
	hdr->e_shoff = 0;


	return (Elf64_Shdr *) (&addr[section_offset]);

}

/* 32 bit overwrite function */
int remove_headers32(void *addr, Elf32_Shdr* sections, int* section_count, int* str_index)
{
	Elf32_Shdr* iter = sections;
	
	int count = *section_count;
	for(int i = 0; i < count; ++i, ++iter)
	{
		if(iter->sh_link == (Elf32_Word)*str_index)
		{
			printf("A section is still linked to the str index: %d\n", iter->sh_link);
			return 0;
		}

		if(i == *str_index)
		{
			memset(&addr[iter->sh_offset], 0, iter->sh_size);
		}
	}
	iter->sh_size = 0;
	memset(sections, 0, count * sizeof(Elf32_Shdr));
	return 1;
}

/* 64 bit overwrite function */
int remove_headers64(void *addr, Elf64_Shdr* sections, int* section_count, int* str_index)
{
	Elf64_Shdr* iter = sections;
	
	int count = *section_count;
	for(int i = 0; i < count; ++i, ++iter)
	{
		if(iter->sh_link == (uint64_t)*str_index)
		{
			printf("A section is still linked to the str index: %d\n", iter->sh_link);
			return 0;
		}

		if(i == *str_index)
		{
			memset(&addr[iter->sh_offset], 0, iter->sh_size);
		}
	}
	iter->sh_size = 0;
	memset(sections, 0, count * sizeof(Elf64_Shdr));
	return 1;
}


/* main function */
int main(int argc, char *argv[])
{
	/* variables */
	int fd;
	int* arch = malloc(sizeof(int));


	/* Check args */
	if(argc != 2) {
		printf("Usage: %s <file path>\n", argv[0]);
		return EXIT_FAILURE;
	}

	/* Open file for reading and writing*/
	printf("[+]\tOpening file\n");
	fd = open(argv[1], O_RDWR);
	if(fd < 0)
	{
		printf("[-]\t\tFailed to open file!\n");
		return EXIT_FAILURE;
	}

	struct stat fileStat = {};
	if(fstat(fd, &fileStat))
	{
		printf("[-]\t\tfstat error\n");
		return EXIT_FAILURE;
	}

	/* map the file into memory */
	off_t size = fileStat.st_size;
	char* addr;
	
	printf("[+]\tMapping file into memory\n");
	addr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if(addr == MAP_FAILED)
	{
		printf("[-]\t\tmmap failed\n");
		return EXIT_FAILURE;
	}
	
	printf("[+]\tChecking file type\n");
	if(!check_file(addr, arch))
	{
		printf("[-]\t\tFile not an ELF binary\n");
		return EXIT_FAILURE;
	}
	

	int* section_count = malloc(sizeof(int));
	int* string_index = malloc(sizeof(int));

	if(*arch == 32)
	{
		printf("[+]\t32 bit ELF binary\n");
		printf("[+]\t\tFinding the sections\n");
		Elf32_Shdr* sections = find_sections32(addr, section_count, string_index);
		printf("[+]\t\tRemoving the headers\n");
		if(!remove_headers32(addr, sections, section_count, string_index))
		{
			printf("[-]\t\t\tFailed to remove headers\n");
			return EXIT_FAILURE;
		}
	}
	else if (*arch == 64)
	{
		printf("[+]\t64 bit ELF binary\n");
		printf("[+]\t\tFinding the sections\n");
		Elf64_Shdr* sections = find_sections64(addr, section_count, string_index);
		printf("[+]\t\tRemoving the headers\n");
		if(!remove_headers64(addr, sections, section_count, string_index))
		{
			printf("[-]\t\t\tFailed to remove headers\n");
			return EXIT_FAILURE;
		}
	}

	else 
	{
		printf("[-]\t\tNo architecture found\n");
		return EXIT_FAILURE;
	}

	free(section_count);
	free(string_index);


	printf("[+]\tClosing file and Map\n");
	close(addr);
	close(fd);

	free(addr);

	return 0;
}
	
