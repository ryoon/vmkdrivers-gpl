#ifndef _ASM_X8664_MODULE_H
#define _ASM_X8664_MODULE_H

struct mod_arch_specific {}; 

#define MODULES_ARE_ELF64
#define Elf_Shdr Elf64_Shdr
#define Elf_Sym Elf64_Sym
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Rel Elf64_Rel
#define Elf_Rela Elf64_Rela
#define ELF_R_TYPE(X)	ELF64_R_TYPE(X)
#define ELF_R_SYM(X)	ELF64_R_SYM(X)

#endif 
