#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys\stat.h>
#include <fcntl.h>
#include <io.h>

#define FALSE               0
#define TRUE                1

typedef int                 BOOL;

void die(char* str)
{
	fprintf(stderr, "%s\n", str);
	exit(1);
}

void usage()
{
	die("Usage: build bootsect setup system [> image]");
}

BOOL fill(int c)
{
	char *buf = malloc(c);
	BOOL result = (write(fileno(stdout), buf, c) == c);
	free(buf);

	return result;
}

BOOL exec(char* buf)
{
	char *mz = buf;
	if (*((unsigned short*)mz) != 0x5A4D)
		return FALSE;
	char *pe = mz + *((unsigned int*)(mz + 0x3C));
	if (*((unsigned short*)pe) != 0x4550)
		return FALSE;
	unsigned short len = *((unsigned short*)(pe + 0x06));
	char *sh = pe + 0xF8;
	unsigned int last = 0;
	for (int i = 0; i < len; i++)
	{
		char *base = sh + i * 0x28;
		unsigned int addr = *((unsigned int*)(base + 0x0C));
		unsigned int size = *((unsigned int*)(base + 0x10));
		unsigned int data = *((unsigned int*)(base + 0x14));
		fprintf(stderr, "%s\t0x%p\t0x%p\t0x%p\n", base, addr, data, size);
		if (len > 1)
		{
			if (!fill(addr - last))
				return FALSE;
			last = addr + size;
		}
		if (write(fileno(stdout), buf + data, size) != size)
			return FALSE;
	}

	return TRUE;
}

BOOL merge(char *filename)
{
	BOOL result = FALSE;
	int id, len;
	char *buf;

	if ((id = open(filename, O_RDONLY | O_BINARY)) > 0)
	{
		len = filelength(id);
		fprintf(stderr, "%s\t%d\n", filename, len);
		buf = malloc(len);
		read(id, buf, len);
		if (exec(buf))
			result = TRUE;
		free(buf);
		close(id);
	}

	return result;
}

int main(int argc, char **argv)
{
	if (argc != 4)
		usage();

	setmode(fileno(stdout), O_BINARY);
	if (!merge(argv[1]))
		die("Merge 'bootsect' fail.");
	if (!merge(argv[2]))
		die("Merge 'setup' fail.");
	if (!merge(argv[3]))
		die("Merge 'system' fail.");

	return 0;
}