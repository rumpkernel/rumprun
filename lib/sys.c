/*
 * POSIX-compatible libc layer
 *
 * Samuel Thibault <Samuel.Thibault@eu.citrix.net>, October 2007
 *
 * Provides the UNIXish part of the standard libc function.
 *
 * Relatively straight-forward: just multiplex the file descriptor operations
 * among the various file types (console, FS, network, ...)
 */

//#define LIBC_VERBOSE
//#define LIBC_DEBUG

#ifdef LIBC_DEBUG
#define DEBUG(fmt,...) printk(fmt, ##__VA_ARGS__)
#else
#define DEBUG(fmt,...)
#endif

