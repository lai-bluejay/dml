/* ========================================================
 *   Copyright (C) 2016 All rights reserved.
 *
 *   filename : dtree.c
 *   author   : liuzhiqiangruc@126.com
 *   date     : 2016-02-26
 *   info     : implementation for decision_tree model
 * ======================================================== */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "dtree.h"

#define DBL_MAX 10e11

struct _d_tree {
    int    n;                       /* instance num in this node  */
    int    leaf;                    /* 0:noleaf, 1:leaf, 2:calculating, 3:calculated */
    int    depth;                   /* depth of this node         */
    int    attr;                    /* if not leaf, split attr    */
    double aval;                    /* split val of this attr     */
    double sg;                      /* sum of 1-gradient          */
    double sh;                      /* sum of 2-gradient          */
    double wei;                     /* additive model value       */
    double loss;                    /* loss value of this node    */
    double gain;                    /* gain of best split         */
    struct _d_tree * child[2];      /* splited children nodes     */
};

static DTree * init_root(double * g, double * h, int n, double nr, double wr){
    int i;
    DTree * t = (DTree *)calloc(1, sizeof(DTree));
    t->n     = n;
    t->leaf  = 1;
    t->depth = 0;
    for (i = 0; i < n; i++){
        t->sg += g[i];
        t->sh += h[i];
    }
    t->wei  = -1.0 * t->sg / (t->sh + wr);
    t->loss = -0.5 * t->sg * t->sg / (t->sh + wr) + nr;
    return t;
}

static void init_child(DTree * t){
    t->child[0]  = (DTree*)calloc(1, sizeof(DTree));
    t->child[1]  = (DTree*)calloc(1, sizeof(DTree));
    t->child[0]->leaf  = t->child[1]->leaf  = 2;
    t->child[0]->depth = t->child[1]->depth = t->depth + 1;
}

static void inline update_child(DTree * t, int k, int lc, double lsg, double lsh, double nr, double wr, double v, double lv){
    double l_loss, r_loss, gain;
    DTree *le = t->child[0], *rt = t->child[1];
    l_loss = -0.5 * lsg * lsg / (lsh + wr) + nr;
    r_loss = -0.5 * (t->sg - lsg) * (t->sg - lsg) / (t->sh - lsh + wr) + nr;
    gain   = t->loss - l_loss - r_loss;
    if (gain > t->gain){
        // update t
        t->gain  = gain;
        t->attr  = k;
        t->aval  = (v + lv) / 2.0;
        // update left  child
        le->n    = lc;
        le->sg   = lsg;
        le->sh   = lsh;
        le->wei  = -1.0 * lsg / (lsh + wr);
        le->loss = l_loss;
        // update right child
        rt->n    = t->n - le->n;
        rt->sg   = t->sg - le->sg;
        rt->sh   = t->sh - le->sh;
        rt->wei  = -1.0 * rt->sg / (rt->sh + wr);
        rt->loss = r_loss;
    }
}

static int tree_grow(DTD * ds
                , DTree ** leaf_nodes
                , int    * inst_nodes
                , double * g
                , double * h
                , double nr
                , double wr
                , int l
                , int s
                , int d) {
    int i, j, k, o, r, lc;
    double v = 0.0, lv = DBL_MAX, lsg = 0.0, lsh = 0.0;
    DTree *t = NULL;
    for (i = 0; i < l; i++){
        t = leaf_nodes[i];                  // current leaf node
        if (t->depth >= d)   { continue; }  // max depth, can not split anymore
        if (t->n < (s << 1)) { continue; }  // can not split, have no children
        if (t->child[0] == NULL) { init_child(t); } // can split, generate new children
        if (t->child[0]->leaf == 2) for (j = 0; j < ds->col; j++) { // have children and is new children
            o   = ds->cl[j];
            lsg = lsh = 0.0;
            lc  = 0;
            lv  = DBL_MAX;
            for (k = 0; k < ds->l[j]; k++) {
                if (t->n - lc < s) { break; }
                r = ds->ids[o + k];
                if (inst_nodes[r] == i){
                    if (0 == ds->bin){
                        v = ds->vals[o + k];
                        if (v < lv && lv < DBL_MAX && lc >= s){
                            update_child(t, j, lc, lsg, lsh, nr, wr, v, lv);
                        }
                        lv = v;
                    }
                    lc  += 1;
                    lsg += g[r];
                    lsh += h[r];
                }
            }
            // for column which have missing value and missing cnt >= s
            // and nonmissing cnt also >= s
            if (lc >= s && t->n - lc >= s){
                update_child(t, j, lc, lsg, lsh, nr, wr, v, 1 == ds->bin ? 1.0 : lv);
            }
        }
        // this leaf node has been calculated, no need for any calculation any more
        t->child[0]->leaf = t->child[1]->leaf = 3;
    }
    v = 0.0;
    i = -1;
    for (j = 0; j < l; j++){
        t = leaf_nodes[j];
        if (t && t->gain > v){
            v = t->gain;
            i = j;
        }
    }
    if (i > -1){
        leaf_nodes[i]->leaf = 0;
        leaf_nodes[i]->child[0]->leaf = leaf_nodes[i]->child[1]->leaf = 1;
    }
    return i;
}

static void scan_tree(DTD * ts, DTree * t, DTree ** inst_nodes, int n, int m){
    int i, id, rowid, l_c, r_c;
    l_c = 0;
    r_c = m;
    for (i = 0; i < ts->l[t->attr]; i++){
        id = i + ts->cl[t->attr];
        rowid = ts->ids[id];
        if (inst_nodes[rowid] == t) {
            if (ts->bin == 1 || (ts->bin == 0 && ts->vals[id] >= t->aval)){
                l_c += 1;
                r_c -= 1;
                inst_nodes[rowid] = t->child[0];
            }
        }
    }
    for (i = 0; i < n; i++){
        if (inst_nodes[i] == t){
            inst_nodes[i] = t->child[1];
        }
    }
    if (l_c > 0 && t->child[0]->leaf == 0) {
        scan_tree(ts, t->child[0], inst_nodes, n, l_c);
    }
    if (r_c > 0 && t->child[1]->leaf == 0) {
        scan_tree(ts, t->child[1], inst_nodes, n, r_c);
    }
}

DTree * generate_dtree(DTD * ds      /* dataset for build tree */
                     , double * F    /* current f vector       */
                     , double * g    /* current gradient vec   */
                     , double * h    /* current hessian  vec   */
                     , double nr     /* node regulizatin       */
                     , double wr     /* weight regulization    */
                     , int n         /* number of instances    */
                     , int s         /* min instance each node */
                     , int d         /* max depth of tree      */
                     , int m) {      /* max leaf nodes         */
    int i, k, o, l;
    if (m < 2)
        return NULL;
    DTree ** leaf_nodes = (DTree**)calloc(m, sizeof(DTree*));
    int   *  inst_nodes = (int*)calloc(n, sizeof(int));
    DTree *  t = init_root(g, h, n, nr, wr);
    l = 0;
    for (i = 0; i < n; i++){
        inst_nodes[i] = l;
    }
    leaf_nodes[l++] = t;
    while (l < m){
        if(-1 == (k = tree_grow(ds, leaf_nodes, inst_nodes, g, h, nr, wr, l, s, d))){
            break;
        }
        DTree * tmp = leaf_nodes[k];
        o = ds->cl[tmp->attr];
        for (i = 0; i < ds->l[tmp->attr]; i++){
            if (inst_nodes[ds->ids[o + i]] == k){
                if ((1 == ds->bin) || (0 == ds->bin && ds->vals[o + i] >= tmp->aval)){
                    inst_nodes[ds->ids[o + i]] = l;
                }
            }
        }
        leaf_nodes[l++] = tmp->child[0];
        leaf_nodes[k]   = tmp->child[1];
    }
    for (i = 0; i < l; i++) if (leaf_nodes[i]->child[0]) {
        free(leaf_nodes[i]->child[0]);
        free(leaf_nodes[i]->child[1]);
        leaf_nodes[i]->child[0] = leaf_nodes[i]->child[1] = NULL;
    }
    if (t->leaf == 0){
        for (i = 0; i < n; i++){
            F[i] = leaf_nodes[inst_nodes[i]]->wei;
        }
    }
    else{
        free_dtree(t); t = NULL;
    }
    free(leaf_nodes);    leaf_nodes = NULL;
    free(inst_nodes);    inst_nodes = NULL;
    return t;
}

void free_dtree(DTree * t){
    if (t){
        if(t->child[0]){
            free_dtree(t->child[0]);
            t->child[0] = NULL;
        }
        if (t->child[1]){
            free_dtree(t->child[1]);
            t->child[1] = NULL;
        }
        free(t);
    }
}

double * eval_tree(DTD * ts, DTree * t, double * F, int n){
    int i;
    DTree ** inst_nodes = NULL;
    if (ts && F && t && t->leaf == 0 && n > 0) {
        inst_nodes = (DTree**)malloc(sizeof(DTree *) * n);
        for (i = 0; i < n; i++){
            inst_nodes[i] = t;
        }
        scan_tree(ts, t, inst_nodes, n, n);
        for (i = 0; i < n; i++){
            F[i] = inst_nodes[i]->wei;
        }
        free(inst_nodes); inst_nodes = NULL;
    }
    return F;
}

void save_dtree(DTree * t, char * out_file, char (*id_map)[FKL]){
    if (!t){
        return;
    }
    FILE * fp = NULL;
    if (NULL == (fp = fopen(out_file, "w"))){
        fprintf(stderr, "save out file \"%s\"\n", out_file);
        return;
    }
    // max 1000 leaf_nodes and 999 non_leaf nodes in tree
    DTree ** ts = (DTree **)malloc(sizeof(void *) * 1999);
    memset(ts, 0, sizeof(void*) * 1999);
    int i, l, c1, c2;
    i = l = 0;
    ts[l++] = t;
    fprintf(fp, "n\tleaf\tAttr\taval\tNode_wei\tNode_loss\tleft\tright\n");
    do {
        DTree * ct = ts[i];
        c1 = c2 = 0;
        // if is not leaf, push two children into ts
        if (ct->leaf == 0){
            c1 = l; ts[l++] = ct->child[0];
            c2 = l; ts[l++] = ct->child[1];
            fprintf(fp, "%d\t%d\t%s\t%.3f\t%.3f\t%.3f\t%d\t%d\n"          \
                      , ct->n,   ct->leaf, id_map[ct->attr], ct->aval \
                      , ct->wei, ct->loss, c1, c2);
        }
        else{
            fprintf(fp, "%d\t%d\tNone\tNone\t%.3f\t%.3f\t%d\t%d\n"    \
                      , ct->n, ct->leaf, ct->wei, ct->loss, c1, c2);
        }
        i += 1;
    } while (i < l && l <= 1997);
    fclose(fp);
    free(ts);
    ts = NULL;
}
