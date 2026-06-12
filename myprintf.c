#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

int Strlen(char *s)
{
	int n = 0;
	while (*s++ != '\0') n++;
	return n;
}

uint8_t *Sprintf(uint8_t *output, uint8_t *format, ...)
{
	va_list ap;
	union {
		struct {
			uint16_t ctx:1;
			uint16_t period:1;
			uint16_t zero:1;
			uint16_t sign:1;
			uint16_t minus:1;
			uint16_t :3;
			uint16_t col:4;
			uint16_t subcol:4;
		} bit;
		uint16_t clear;
	} flags;

	uint16_t u;
	uint8_t buf[16], *s;
	int i, v;

	if (output == NULL) return NULL;
	flags.clear = 0;
	va_start(ap, format);

	while (*format != '\0') {
		if (flags.bit.ctx) {
			switch (*format) {
				case '.':
					flags.bit.period = 1;
					break;
				case '0':
					if (flags.bit.col == 0 && !flags.bit.period) {
						flags.bit.zero = 1;
						break;
					}
				case '1':	case '2':	case '3':	case '4':
				case '5':	case '6':	case '7':	case '8':	case '9':
					if (flags.bit.period) {
						flags.bit.subcol *= 10;
						flags.bit.subcol += (*format-'0');
					} else {
						flags.bit.col *= 10;
						flags.bit.col += (*format-'0');
					}
					break;

				case '-':
					flags.bit.sign = 1;
					break;
				case ' ':
					flags.bit.zero = 0;
					break;

				default:
					i = 0;
					switch (*format) {
						case '%':
							buf[i++] = '%';
							break;
						case 'b':
							u = (uint16_t)va_arg(ap, unsigned int);
							if (u == 0) {
								buf[i++] = '0';
							} else {
								int cl = (int)((sizeof(buf) < flags.bit.col)? sizeof(buf):flags.bit.col);
								for (i = 0; u != 0 && i < cl; i++) {
									buf[i] = (u & 1)? '1':'0';
									u >>= 1;
								}
							}
							break;
						case 'd':
							v = va_arg(ap, int);
							if (v < 0) {
								v *= -1;
								flags.bit.minus = 1;
							}
							u = (uint16_t)v;
						case 'u':
							if (*format == 'u') {
								u = (uint16_t)va_arg(ap, unsigned int);
							}
							i = 0;
							while (i < sizeof(buf)-1) {
								if (flags.bit.period) {
									if (flags.bit.subcol == 0) {
										buf[i++] = '.';
										flags.bit.period = 0;
									} else {
										buf[i++] = u % 10 +'0';
										u /= 10;
										flags.bit.subcol--;
									}
									continue;
								}

								buf[i++] = u % 10 +'0';
								u /=10;
								if (u == 0) break;
							}
							if (flags.bit.minus && (!flags.bit.zero || flags.bit.sign)) {
								buf[i++] = '-';
							}
							break;
						case 's':
							s = va_arg(ap, char *);
							if (!flags.bit.sign) {
								// 右寄せ
								int len = Strlen(s);
								if (flags.bit.period) {
									if ((int)flags.bit.subcol < len) {
										len = (int)flags.bit.subcol;
									}
								}
								for (i = 0; i < (int)flags.bit.col - len; i++) {
									*output++ = ' ';
								}
								for (i = 0; *s != '\0' && i < len; i++) *output++ = *s++;
							} else {
								i = 0;
								while (*s != '\0') {
									*output++ = *s++;
									i++;
								}
								for (; i < flags.bit.col; i++) {
									*output++ = ' '; 
								}
							}
							i = -1;
							break;
						case 'c':
							*output++ = (uint8_t)(va_arg(ap, unsigned int) & 0xff);
							i = -1;
							break;
						case 'x':
						case 'X':
							u = (uint16_t)va_arg(ap, unsigned int);
							if (u == 0) {
								buf[i++] = '0';
							} else {
								for (i = 0; u != 0 && i < sizeof(buf); i++) {
									uint16_t u0 = u & 0x0f;
									if (u0 > 9) {
										u0 = ((*format=='X')? 'A':'a')+(u0-10);
									} else {
										u0 += '0';
									}
									buf[i] = u0;
									u >>= 4;
								}
							}
							break;
						default:
							va_arg(ap, int);
							i = -1;
							break;
					}
					if (i > 0) {
						if (flags.bit.sign) {
							// 左寄せ
							int pos = (int)flags.bit.col - i;
							for (i--; i >= 0; i--) {
								*output++ = buf[i]; 
							}
							for (; pos > 0; pos--) {
								*output++ = ' '; 
							}
						} else {
							// 右寄せ あるいは 桁数指定のない左寄せ
							int pos = (flags.bit.col == 0)? i-1:
								((int)((sizeof(buf) < flags.bit.col)? sizeof(buf):flags.bit.col)-1);
							for (; i <= pos;i++) {
								buf[i] = (flags.bit.zero)? '0':' ';
							}
							if (flags.bit.minus && flags.bit.zero) buf[pos] = '-';
							for (; pos >= 0; pos--) {
								*output++ = buf[pos]; 
							}
						}
					}
					flags.clear = 0;
			}
			format++;
			continue;
		}
		
		if (*format == '%') {
			flags.bit.ctx = 1;
			format++;
			continue;
		}

		*output++ = *format++; 
	}
	va_end(ap);
	*output = '\0';
	return output;
}
