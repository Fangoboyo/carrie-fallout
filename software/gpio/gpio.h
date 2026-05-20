#ifndef GPIO_H
#define GPIO_H

/* Exports a GPIO pin to sysfs so it can be controlled */
int gpio_export(int gpio);

/* Unexports a GPIO pin from sysfs */
void gpio_unexport(int gpio);

/* Sets the direction of a GPIO pin to input */
int gpio_set_input(int gpio);

/* Reads the value of a GPIO pin (0 or 1). Returns -1 on error. */
int gpio_read(int gpio);

#endif // GPIO_H
