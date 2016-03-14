/* ========================================================
 *   Copyright (C) 2016 All rights reserved.
 *   
 *   filename : dtree.c
 *   author   : liuzhiqiangruc@126.com
 *   date     : 2016-02-26
 *   info     : implementation for decision_tree model
 * ======================================================== */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "dtree.h"

struct _t_node {
    int n;                          /* instance num in this node */
    int leaf;                       /* is leaf or not, 0 | 1     */
    struct _t_node * child[2];      /* splited children nodes    */
    int attr;                       /* if not leaf, split attr   */
    double attr_val;                /* split val of this attr    */
    double sg;                      /* sum of 1-gradient         */
    double sh;                      /* sum of 2-gradient         */
    double wei;                     /* additive model value      */
};

static void tree_grow(DTree * t, DTrain * ds, unsigned long * bit_map, int len, double * g, double * h, int n){
    int offs = 0;                    
    int row = -1;                   
    int counter = 0;
    int l_count = 0;
    int b_attr  = -1;            
    double val = 0.0;             
    double attr_val = 0.0;        
    double gain = 0.0;
    double max_gain = 0.0;
    double l_sg = 0.0;
    double l_sh = 0.0;
    double r_sg = 0.0;
    double r_sh = 0.0;
    double l_sg_b = 0.0;
    double l_sh_b = 0.0;
    double r_sg_b = 0.0;
    double r_sh_b = 0.0;
    double last_value = 0.123456789;
    double l_weight = 0.0;
    double r_weight = 0.0;
    unsigned long * l_bit = (unsigned long *)malloc(len << 3);
    unsigned long * r_bit = (unsigned long *)malloc(len << 3);

    // scan all the attributes
    for (int i = 0, offs = 0; i < ds->col; i++){
        memset(l_bit, 0, len << 3);
        memmove(r_bit, bit_map, len << 3);
        l_sg = l_sh = 0.0;
        r_sg = t->sg;
        r_sh = t->sh;
        last_value = 0.123456789;
        counter = 0;

        // scan the non missing instance and value in this feature
        for (int j = 0; j < ds->l[i]; j++){
            row = ds->vals[offs + j].id;
            val = ds->vals[offs + j].val;

            // feature value changes
            if ((fabs(val - last_value) > 1e-6) && (fabs(last_value - 0.12345678) > 1e-6)){
                gain = (l_sg * l_sg / l_sh + r_sg * r_sg / r_sh - t->sg * t->sg / t->sh) / 2.0;
                if (gain > max_gain){
                    max_gain = gain;
                    b_attr = i;
                    attr_val = (val + last_value) / 2.0;
                    l_sg_b = l_sg;
                    l_sh_b = l_sh;
                    r_sg_b = r_sg;
                    r_sh_b = r_sh;
                    l_count = counter;
                }
            }
            unsigned long f = bit_map[row >> 6];
            if((f & (1UL << (row & 63))) > 0){
                l_bit[row >> 6] |= (1UL << (row & 63));
                r_bit[row >> 6] ^= (1UL << (row & 63));
                l_sg += g[row]; l_sh += h[row];
                r_sg -= g[row]; r_sh -= h[row];
                counter += 1;
            }
            last_value = val;
        }
        // this feature has missing instance
        if (counter < t->n){
            gain = (l_sg * l_sg / l_sh + r_sg * r_sg / r_sh - t->sg * t->sg / t->sh) / 2.0;
            if (gain > max_gain){
                max_gain = gain;
                b_attr = i;
                attr_val = val;
                l_sg_b = l_sg;
                l_sh_b = l_sh;
                r_sg_b = r_sg;
                r_sh_b = r_sh;
                l_count = counter;
            }
        }
        offs += ds->l[i];
    }

    // grow or not 
    if (max_gain > 0.0){
        // t is not a leaf node
        t->leaf = 0;
        t->attr = b_attr;
        t->attr_val = attr_val;

        // children nodes init
        DTree * ln = (DTree *)malloc(sizeof(DTree));
        DTree * rn = (DTree *)malloc(sizeof(DTree));
        memset(ln, 0, sizeof(DTree));
        memset(rn, 0, sizeof(DTree));
        ln->n = l_count;
        rn->n = t->n - ln->n;
        ln->leaf = rn->leaf = 0;
        ln->sg = l_sg_b;
        ln->sh = l_sh_b;
        rn->sg = r_sg_b;
        rn->sh = r_sh_b;
        t->child[0] = ln;
        t->child[1] = rn;

        // grow tree from ln and rn
        tree_grow(ln, ds, l_bit, len, g, h, n);
        tree_grow(rn, ds, r_bit, len, g, h, n);
    }
    else{
        // t is a leaf node
        t->leaf = 1;
        t->wei = -1.0 * t->sg / t->sh;
    }
    free(l_bit);  l_bit = NULL;
    free(r_bit);  r_bit = NULL;
}

DTree * generate_dtree(DTrain * ds, double *F, double * g, double * h, int n){
    int i ;
    // root of tree
    DTree * t = (DTree*)malloc(sizeof(DTree));
    if (!t){
        return NULL;
    }
    memset(t, 0, sizeof(DTree));
    // init the tree node
    t->n = ds->row;
    t->leaf = 0;
    for (i = 0; i < n; i++){
        t->sg += g[i];
        t->sh += h[i];
    }
    // node instance bit map : root contains all instances
    unsigned long * bit_map = NULL;
    int row = ds->row;
    int len = row >> 6;
    if ((row & ((1UL << 6) - 1)) > 0){
        len += 1;
    }
    bit_map = (unsigned long *)malloc(len << 3);
    memset(bit_map, -1, len << 3);

    // tree grow from root t
    tree_grow(t, ds, bit_map, len, g, h, n);
    free(bit_map); bit_map = NULL;

    if (t->leaf == 1){
        free(t);
        t = NULL;
    }

    return t;
}
