#ifndef _BMK_CORE_TYPES_H_
#define _BMK_CORE_TYPES_H_

#ifdef _LP64
typedef long bmk_time_t;
#else
typedef long long bmk_time_t;
#endif

#endif /* _BMK_CORE_TYPES_H_ */
