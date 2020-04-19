#ifndef MKTORRENT_MSG_H
#define MKTORRENT_MSG_H

void fatal(const char *format, ...);


#define FATAL_IF(cond, format, ...) do { if (cond) fatal(format, __VA_ARGS__); } while(0)
#define FATAL_IF0(cond, format) do { if (cond) fatal(format); } while(0)

#endif /* MKTORRENT_MSG_H */
