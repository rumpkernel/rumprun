
#include <xen/io/xs_wire.h>

struct rumpxenbus_data_dev;
struct rumpxenbus_data_user;

#define BUFFER_SIZE (XENSTORE_PAYLOAD_MAX+sizeof(struct xsd_sockmsg))

struct rumpxenbus_data_common {
	/* Partially written request(s). */
	unsigned int wbuf_used;
	union {
		struct xsd_sockmsg msg;
		unsigned char buffer[BUFFER_SIZE];
	} wbuf;

	_Bool queued_enomem;

	struct rumpxenbus_data_user *du;
};

/* void __NORETURN__ WTROUBLE(const char *details_without_newline);
 * assumes:   int err;
 *            end: */
#define WTROUBLE(dc,s) do{ rumpxenbus_write_trouble((dc),s); err = EINVAL; goto end; }while(0)

void
rumpxenbus_write_trouble(struct rumpxenbus_data_common *dc, const char *what);

int
rumpxenbus_process_request(struct rumpxenbus_data_common *dc);

struct xsd_sockmsg*
rumpxenbus_next_event_msg(struct rumpxenbus_data_common *dc,
			 _Bool block,
			 void (**mfree_r)(void*));

void rumpxenbus_block_before(struct rumpxenbus_data_common *dc);
void rumpxenbus_block_after(struct rumpxenbus_data_common *dc);

void rumpxenbus_dev_xb_wakeup(struct rumpxenbus_data_common *dc);
void rumpxenbus_dev_restart_wakeup(struct rumpxenbus_data_common *dc);

void
rumpxenbus_dev_user_shutdown(struct rumpxenbus_data_common *dc);
int
rumpxenbus_dev_user_open(struct rumpxenbus_data_common *dc);

/* nicked from NetBSD sys/dev/pci/cxgb/cxgb_adapter.h */
#ifndef container_of
#define container_of(p, stype, field) ((stype *)(((uint8_t *)(p)) - offsetof(stype, field)))
#endif

