#ifndef _BMKCORE_BMK_OPS_H_
#define _BMKCORE_BMK_OPS_H_

struct bmk_ops {
	void *(*bmk_allocpg2)(int);
	void (*bmk_halt)(void) __attribute__((noreturn));
};

extern const struct bmk_ops *bmk_ops;
extern long bmk_stacksize, bmk_pagesize;

#endif /* _BMKCORE_BMK_OPS_H_ */
