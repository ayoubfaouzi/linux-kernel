/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2025.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU General Public License as published by the   *
* Free Software Foundation, either version 3 or (at your option) any      *
* later version. This program is distributed without any warranty.  See   *
* the file COPYING.gpl-v3 for details.                                    *
\*************************************************************************/

/* Supplementary program for Chapter 41 */

/* foo2.c

*/
#include <stdlib.h>
#include <stdio.h>

void
xyz(void)
{
    printf("        foo2-xyz\n");
}

void
foo2(int x)
{
    printf("Called foo2\n");
    xyz();

    void foo3(void);
    foo3();
}
