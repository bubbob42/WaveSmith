/*
 * boopsi.h -- the register glue a class dispatcher needs on both compilers
 * (as PrismaControl's classes have it): a0 the class, a2 the object, a1
 * the message.
 */
#ifndef PRISMAREC_BOOPSI_H
#define PRISMAREC_BOOPSI_H

#if defined(__SASC)
# define DISP_QUAL __saveds __asm
# define REG(r, a) register __##r a
#elif defined(__GNUC__)
# define DISP_QUAL
# define REG(r, a) a __asm(#r)
#else
# error "a dispatcher needs register arguments (SAS/C or GCC)"
#endif

#endif
