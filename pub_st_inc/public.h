/*
 * public.h
 *
 *  Created on: 2014-9-3
 *      Author: liwei
 */

#ifndef PUBLIC_H_
#define PUBLIC_H_

#define CONFIG_SMP 1

#ifndef likely
# define likely(x)  __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
# define unlikely(x)    __builtin_expect(!!(x), 0)
#endif

#ifdef CONFIG_SMP
#define LOCK_PREFIX_HERE \
		".pushsection .smp_locks,\"a\"\n"	\
		".balign 4\n"				\
		".long 671f - .\n" /* offset */		\
		".popsection\n"				\
		"671:"

#define LOCK_PREFIX LOCK_PREFIX_HERE "\n\tlock; "

#else /* ! CONFIG_SMP */
#define LOCK_PREFIX_HERE ""
#define LOCK_PREFIX ""
#endif
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

/*
static __always_inline  void rep_nop(void)
{
        asm volatile ( "rep;nop" : : : "memory" );
}*/
#define rep_nop asm volatile ( "rep;nop" : : : "memory" )
#define cpu_relax() rep_nop
#define barrier() __asm__ __volatile__("": : :"memory")
#define show_memory(pos,len) for(void *_tmp_pos=pos;_tmp_pos<pos+len;_tmp_pos++)

#define max2(a,b) ((a)>(b)?(a):(b))
#define ALIGN(x, a)	(((x) + (a) - 1) & ~((a) - 1))
#endif /* PUBLIC_H_ */
