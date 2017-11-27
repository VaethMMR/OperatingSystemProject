/*
 * Function prototypes from Patrick's phase3 solution. These can be called
 * when in *kernel* mode to get access to phase3 functionality.
 */


#ifndef PROVIDED_PROTOTYPES_H

#define PROVIDED_PROTOTYPES_H

extern int  spawnReal(char *name, int (*func)(char *), char *arg,
                       int stack_size, int priority);
extern int  waitReal(int *status);
extern void terminateReal(int exit_code);
extern int  semCreateReal(int init_value);
extern int  semPReal(int semaphore);
extern int  semVReal(int semaphore);
extern int  semFreeReal(int semaphore);
extern int  getTimeofDayReal(int *time);
extern int  cpuTimeReal(int *time);
extern int  getPIDReal(int *pid);

#endif  /* PROVIDED_PROTOTYPES_H */
