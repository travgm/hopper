/* hopper.c - patch the interpreter in elf64 binaries
 *
 *  compile:
 *  gcc -o hopper hopper.c
 *
 *  usage: ./hopper [target] [interpreter]
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
#include <stdbool.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <elf.h>

/* Options for patching/displaying information */
bool verbose = false;
bool show_symbols = false;
bool disp_interp = false;

int verify_target_binary (Elf64_Ehdr * ehdr);
int check_file_access (const char *file_path);
void print_usage (const char *program_name);
void print_flags (Elf64_Word p_flags);
void print_symbols (FILE * obj, Elf64_Shdr * shdr, long table_offset);
void print_interps ();

int
patchInterp (const char *file_name, const char *new_interp)
{
  Elf64_Ehdr ehdr;
  Elf64_Phdr *phdr = NULL;
  Elf64_Shdr *shdr = NULL;
  Elf64_Off table_offset = 0;
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
  if (phdr == NULL)
    {
      fprintf (stderr, "error: unable to allocate memory for phdr\n");
      fclose (obj);
      return -1;
    }

  fseek (obj, ehdr.e_phoff, SEEK_SET);
  fread (phdr, sizeof (Elf64_Phdr), ehdr.e_phnum, obj);

  for (int i = 0; i < ehdr.e_phnum; i++)
    {
      if (phdr[i].p_type == PT_INTERP)
	{
	  if (verbose)
	    {
	      printf ("Found PT_INTERP segment at 0x%016lx\n",
		      phdr[i].p_vaddr);
	      printf ("Offset: 0x%lx\n", phdr[i].p_offset);
	      printf ("Size: %zu\n", phdr[i].p_filesz);
	      print_flags (phdr[i].p_flags);
	    }
	  interp_offset = phdr[i].p_offset;
	  interp_size = (Elf64_Xword) phdr[i].p_filesz;
	  interp_idx = i;
	  break;
	}
    }

  if (interp_offset == 0)
    {
      fprintf (stderr, "error: no PT_INTERP segment found\n");
      free (phdr);
      return -1;
    }

  shdr = malloc (ehdr.e_shnum * sizeof (Elf64_Shdr));
  if (shdr == NULL)
    {
      fprintf (stderr, "error: unable to allocate memory for shdr\n");
      free (phdr);
      fclose (obj);
      return -1;
    }

  fseek (obj, ehdr.e_shoff, SEEK_SET);
  fread (shdr, sizeof (Elf64_Shdr), ehdr.e_shnum, obj);

  table_offset = shdr[ehdr.e_shstrndx].sh_offset;

  char *shstrtab = malloc (shdr[ehdr.e_shstrndx].sh_size);
  if (shstrtab == NULL)
    {
      fprintf (stderr, "error: unable to allocate memory for string table\n");
      free (phdr);
      free (shdr);
      fclose (obj);
      return -1;
    }

  fseek (obj, shdr[ehdr.e_shstrndx].sh_offset, SEEK_SET);
  fread (shstrtab, shdr[ehdr.e_shstrndx].sh_size, 1, obj);

  if (show_symbols)
    {
      for (int i = 0; i < ehdr.e_shnum; i++)
	{
	  if (shdr[i].sh_type == SHT_SYMTAB || shdr[i].sh_type == SHT_DYNSYM)
	    {

	      const char *symtab_name = shstrtab + shdr[i].sh_name;
	      printf ("Symbol Table '%s' (STT_FUNC):\n", symtab_name);
	      print_symbols (obj, &shdr[i], shdr[shdr[i].sh_link].sh_offset);
	    }
	}
    }

  free (shdr);
  free (shstrtab);

  fseek (obj, interp_offset, SEEK_SET);
  char *interp = malloc (interp_size);
  if (interp == NULL)
    {
      free (phdr);
      free (shdr);
      fclose (obj);
      return -1;
    }

  size_t b_read = fread (interp, 1, interp_size, obj);
  if (b_read != interp_size)
    {
      free (interp);
      free (phdr);
      return -1;
    }

  if (disp_interp)
    {
      printf ("0x%lx:PT_INTERP = %s\n", interp_offset, interp);
    }

  if (new_interp)
    {
      printf ("Patching Binary:\n");
      printf ("  OLD 0x%lx:PT_INTERP = %s\n", interp_offset, interp);

      Elf64_Xword new_interp_len = strlen (new_interp) + 1;

      fseek (obj, interp_offset, SEEK_SET);
      fwrite (new_interp, 1, new_interp_len, obj);

      phdr[interp_idx].p_filesz = new_interp_len;
      phdr[interp_idx].p_memsz = new_interp_len;

      fseek (obj, ehdr.e_phoff + interp_idx * sizeof (Elf64_Phdr), SEEK_SET);
      size_t b_written =
	fwrite (&phdr[interp_idx], sizeof (Elf64_Phdr), 1, obj);
      if (b_written > 0)
	{
	  printf ("  NEW 0x%lx:PT_INTERP = %s\n", interp_offset, new_interp);
	}
    }

  free (interp);
  free (phdr);
  fclose (obj);

  return 0;

}

void
print_symbols (FILE * obj, Elf64_Shdr * shdr, long table_offset)
{
  Elf64_Sym *sym = NULL;
  char symbol_name[256];
  int symbol_cnt = shdr->sh_size / sizeof (Elf64_Sym);
  int func_cnt = 0;

  sym = malloc (shdr->sh_size);

  fseek (obj, shdr->sh_offset, SEEK_SET);
  fread (sym, shdr->sh_size, 1, obj);
  for (int i = 0; i < symbol_cnt; i++)
    {
      if (ELF64_ST_TYPE (sym[i].st_info) == STT_FUNC)
	{
	  fseek (obj, table_offset + sym[i].st_name, SEEK_SET);
	  fgets (symbol_name, sizeof (symbol_name), obj);

	  printf ("  %d: %016lx %s\n", func_cnt++, sym[i].st_value,
		  symbol_name);
	}
    }

  free (sym);
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
  if (memcmp (ehdr->e_ident, ELFMAG, SELFMAG) != 0 ||
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
print_interps ()
{
  const char *dirs[] = { "/lib", "/lib64", "/usr/lib", "/usr/lib64" };
  DIR *dir;
  struct dirent *entry;
  char full_path[1024];
  struct stat file_stat;
  int count = 0;

  printf ("Searching for interpreters on local system...\n\n");
  for (int i = 0; i < sizeof (dirs) / sizeof (dirs[0]); i++)
    {
      dir = opendir (dirs[i]);
      if (!dir)
	{
	  continue;
	}

      while ((entry = readdir (dir)) != NULL)
	{
	  if (strcmp (entry->d_name, ".") == 0
	      || strcmp (entry->d_name, "..") == 0)
	    {
	      continue;
	    }

	  snprintf (full_path, sizeof (full_path), "%s/%s", dirs[i],
		    entry->d_name);

	  if (stat (full_path, &file_stat) == 0
	      && S_ISREG (file_stat.st_mode))
	    {
	      if (strstr (entry->d_name, "ld-linux") != NULL)
		{
		  printf ("%s\n", full_path);
		  count++;
		}
	    }
	}
      closedir (dir);
    }
  printf ("\nFound (%d) interpreters\n", count);
}

void
print_usage (const char *program_name)
{
  fprintf (stderr,
	   "Hopper the ELF64 PT_INTERP tool by Travis Montoya <trav@hexproof.sh>\n");
  fprintf (stderr, "usage: %s [option(s)] [target]\n", program_name);
  fprintf (stderr, "  -v                 show verbose output\n");
  fprintf (stderr,
	   "  -s                 display symbol information (STT_FUNC)\n");
  fprintf (stderr, "  -d                 display interpreter\n");
  fprintf (stderr, "  -p [interpreter]   patch interpreter\n");
  fprintf (stderr,
	   "\nYou can run '%s -search' to list common interpreters on your system\n",
	   program_name);
}

int
main (int argc, char *argv[])
{
  int opt;
  char *interp = NULL;
  char *target_elf = NULL;

  if (argc < 2)
    {
      print_usage (argv[0]);
      return 1;
    }

  /* If we are just searching do nothing further */
  if (argc == 2 && strncmp (argv[1], "-search", 7) == 0)
    {
      print_interps ();
      return 0;
    }

  while ((opt = getopt (argc, argv, "vsdp:")) != -1)
    {
      switch (opt)
	{
	case 'v':
	  verbose = true;
	  break;
	case 's':
	  show_symbols = true;
	  break;
	case 'd':
	  disp_interp = true;
	  break;
	case 'p':
	  interp = optarg;
	  break;
	default:
	  print_usage (argv[0]);
	  return 1;
	}
    }

  if (disp_interp && interp)
    {
      fprintf (stderr, "error: You cant patch and display the interpreter\n");
      return 1;
    }

  if (!disp_interp && !interp)
    {
      fprintf (stderr,
	       "error: you must either display interpreter or patch interpreter\n");
      return 1;
    }


  if (optind < argc)
    {
      target_elf = argv[optind];
    }
  else
    {
      fprintf (stderr, "error: No target ELF file specified\n");
      return 1;
    }

  if (!check_file_access (target_elf))
    {
      return 1;
    }

  if (interp && !check_file_access (interp))
    {
      return 1;
    }

  /* patchInterp handles displaying symbols, displaying interpreter and
   * patching the file
   */
  if (patchInterp (target_elf, interp) != 0)
    {
      fprintf (stderr, "error patching '%s'\n", target_elf);
      return 1;
    }

  return 0;

}
