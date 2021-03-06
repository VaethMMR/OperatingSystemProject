/* Tests blockMe and unblockProc.
 * XXp1 starts 3 children at a higher priority, passing each child a different
 * argument containing an integer.  Each child executes the XXp2 function,
 * calling blockMe() using the status passed to XXp2 by its parent.
 * on different statuses.
 * XXp1 then unblocks each of the three.
 */

#include <stdio.h>
#include <stdlib.h>
#include <usloss.h>
#include <phase1.h>

int XXp1(char *);
int XXp2(char *);

int start1(char *arg)
{
    int pid1, kid_status;

    printf("start1(): started\n");

    pid1 = fork1("XXp1", XXp1, "XXp1", USLOSS_MIN_STACK, 5);
    pid1 = join(&kid_status);
    printf("start1(): XXp1, pid %d, done; returning...\n", pid1);
    return 0; /* so gcc will not complain about its absence... */
} /* start1 */

int XXp1(char *arg)
{
    int  i, pid[10], result[10];
    char buf[20];

    printf("XXp1(): creating children\n");
    for (i = 0; i < 3; i++) {
        sprintf(buf, "%d", 13 + i); // different blockMe status
        pid[i] = fork1("XXp2", XXp2, buf, USLOSS_MIN_STACK, 3);
    }


    printf("XXp1(): unblocking children\n");
    for (i = 0; i < 3; i++)
        result[i] = unblockProc(pid[i]);

    for (i = 0; i < 3; i++)
        printf("XXp1(): after unblocking %d, result = %d\n", pid[i], result[i]);

    quit(0);
    return 0;
} /* XXp1 */


int XXp2(char *arg)
{
    int result;
    int status = atoi(arg);

    printf("XXp2(): started, pid = %d, calling blockMe\n", getpid());

    result = blockMe(status);
    printf("XXp2(): pid = %d, after blockMe, result = %d\n", getpid(), result);

    quit(0);

    return 0;
} /* XXp2 */

