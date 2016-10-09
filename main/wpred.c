/* ========================================================
 *   Copyright (C) 2016 All rights reserved.
 *   
 *   filename : wpred.c
 *   author   : liuzhiqiangruc@126.com
 *   date     : 2016-10-09
 *   info     : 
 * ======================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "w2v.h"

int main(int argc, char *argv[]){
    char query[1024] = {'\0'};
    char resul[1024] = {'\0'};
    int r = 0;
    WV * wv = wv_create(argc, argv);
    if (!wv){
        return -1;
    }

    while(!feof(stdin)){
        fgets(query, 1024, stdin);
        r = wv_pred(wv, query, resul);
        if (0 != r) break;
        if(strlen(resul) > 0){
            printf("%s\t%s\n", query, resul);
        }
    }
    wv_free(wv);
    wv = NULL;
    return 0;
}