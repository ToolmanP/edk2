#ifndef COMMON_ASM_H_
#define COMMON_ASM_H_

#define BEGIN_FUNC(_name) \
  .global _name; \
  .type _name, %function; \
  _name:

#define END_FUNC(_name) .size _name, .- _name

#endif