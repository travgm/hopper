/* hopper.c - patch the interpreter in elf64 binaries
 *
 *  compile:
 *  gcc -o hopper hopper.c
 *
 *  usage: hopper <file> <interp>
 *
 *  (C) Copyright 2024 Travis Montoya "travgm" trav@hexproof.sh
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <elf.h>

int verify_target_binary (Elf64_Ehdr * ehdr);
int check_file_access (const char *file_path);
void print_usage (const char *program_name);
void print_flags (Elf64_Word p_flags);

int
patchInterp (const char *file_name, const char *new_interp)
{
  Elf64_Ehdr ehdr;
  Elf64_Phdr *phdr = NULL;
  Elf64_Off interp_offset = 0;
  Elf64_Xword interp_size = 0;
  int interp_idx = 0;

  FILE *obj = NULL;
  if ((obj = fopen (file_name, "r+b")) == NULL)
    {
      return -1;
    }

  fread (&ehdr, sizeof (ehdr), 1, obj);
  if (verify_target_binary (&ehdr) != 0)
    {
      fprintf (stderr,
	       "errror: not a valid dynamically linked ELF64 binary\n");
      return -1;
    }

  phdr = malloc (ehdr.e_phnum * sizeof (Elf64_Phdr));

  fseek (obj, ehdr.e_phoff, SEEK_SET);
  fread (phdr, sizeof (Elf64_Phdr), ehdr.e_phnum, obj);

  for (int i = 0; i < ehdr.e_phnum; i++)
    {
      if (phdr[i].p_type == PT_INTERP)
	{
	  printf ("Found PT_INTERP segment at 0x%016lx\n", phdr[i].p_vaddr);
	  printf ("Offset: 0x%lx\n", phdr[i].p_offset);
	  printf ("Size: %zu\n", phdr[i].p_filesz);
	  print_flags (phdr[i].p_flags);
	  interp_offset = phdr[i].p_offset;
	  interp_size = (Elf64_Xword) phdr[i].p_filesz;
	  interp_idx = i;
	  break;
	}
    }

  fseek (obj, interp_offset, SEEK_SET);
  char *interp = malloc (interp_size);
  if (interp == NULL)
    {
      free (phdr);
      return -1;
    }

  size_t b_read = fread (interp, 1, interp_size, obj);
  if (b_read != interp_size)
    {
      free (interp);
      free (phdr);
      return -1;
    }

  printf ("OLD 0x%lx:PT_INTERP = %s\n", interp_offset, interp);

  Elf64_Xword new_interp_len = strlen (new_interp) + 1;

  fseek (obj, interp_offset, SEEK_SET);
  fwrite (new_interp, 1, new_interp_len, obj);

  phdr[interp_idx].p_filesz = new_interp_len;
  phdr[interp_idx].p_memsz = new_interp_len;

  fseek (obj, ehdr.e_phoff + interp_idx * sizeof (Elf64_Phdr), SEEK_SET);
  size_t b_written = fwrite (&phdr[interp_idx], sizeof (Elf64_Phdr), 1, obj);
  if (b_written > 0)
    {
      printf ("NEW 0x%lx:PT_INTERP = %s\n", interp_offset, new_interp);
    }

  free (interp);
  free (phdr);

  return 0;

}

void
print_flags (Elf64_Word p_flags)
{
  printf ("Flags (p_flags): 0x%x ( ", p_flags);
  if (p_flags & PF_R)
    printf ("R");
  if (p_flags & PF_W)
    printf ("W");
  if (p_flags & PF_X)
    printf ("E");
  printf (" )\n");

  printf ("  Read:    %s\n", (p_flags & PF_R) ? "Yes" : "No");
  printf ("  Write:   %s\n", (p_flags & PF_W) ? "Yes" : "No");
  printf ("  Execute: %s\n", (p_flags & PF_X) ? "Yes" : "No");
}

int
verify_target_binary (Elf64_Ehdr * ehdr)
{
  if (strncmp (ehdr->e_ident, ELFMAG, 4) != 0 &&
      ehdr->e_ident[EI_CLASS] != ELFCLASS64)
    {
      return -1;
    }

  if (ehdr->e_type != ET_DYN)
    {
      return -1;
    }

  printf ("ELF file is a 64-Bit Shared Object (DYN) file\n\n");
  return 0;

}

int
check_file_access (const char *file_path)
{
  if (access (file_path, F_OK) != 0)
    {
      fprintf (stderr, "error: unable to access '%s': %s\n", file_path,
	       strerror (errno));
      return 0;
    }
  return 1;
}

void
print_usage (const char *program_name)
{
  fprintf (stderr, "usage: %s <file> <interpreter>\n", program_name);
}

int
main (int argc, char *argv[])
{

  if (argc != 3)
    {
      print_usage (argv[0]);
      return 1;
    }

  const char *target_elf = argv[1];
  const char *new_interp = argv[2];

  if (!check_file_access (target_elf) || !check_file_access (new_interp))
    {
      return 1;
    }

  if (patchInterp (target_elf, new_interp) != 0)
    {
      fprintf (stderr, "error patching '%s'\n", target_elf);
      return 1;
    }

  return 0;

}
