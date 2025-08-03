#ifndef SERIAL_H
#define SERIAL_H

int init_serial(void);
void write_serial(const char *str);
void serial_printf(const char *format, ...);
void write_serial_char(char a);

#endif // SERIAL_H