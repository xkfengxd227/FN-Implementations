/******************************************************************************

gaussora (GAUSSian ORAcle) is an oracle for samples generated from a
mixture of Gaussians (see the Development Guide of the Sixth Annual
DIMACS Implementation Challenge: Near Neighbor Searches). Author:
Alfons Juan, May 1998.

Licensing
=========
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

Contact infomation
==================
Alfons Juan i C�scar
Institut Tecnol�gic d'Inform�tica
Universitat Polit�cnica de Val�ncia
Cam� de Vera, s/n
46071 Val�ncia
Spain
e-mail: ajuan@iti.upv.es

Format
======
A gaussian of dimension <d> must be given as:

GAUSS <label> <d> Diag <prior_prob>
<mean_1>     <mean_2>     ... <mean_<d>>
<variance_1> <variance_2> ... <variance_<d>>

or

GAUSS <label> <d> Full <prior_prob>
<mean_1>             <mean_2>             ... <mean_<d>>
<covariance_{1,1}>   <covariance_{1,2}>   ... <covariance_{1,<d>}>
<covariance_{2,1}>   <covariance_{2,2}>   ... <covariance_{2,<d>}>
...
<covariance_{<d>,1}> <covariance_{<d>,2}> ... <covariance_{<d>,<d>}>

Unexpected lines are ignored. A priori probabilities (proportions) can
be omitted for equally probable Gaussians. See GetGauss for more
details.

Version 1.0
===========

******************************************************************************/

#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include "oracle.h"

/*****************************************************************************/

#define MLL 16384                   /* Maximum Line Length */
#define IFS " \t\n"                 /* Input Fields Separators */

#define DEFPORT 5031                /* default port to use */
#define DEFN 100                    /* default number of data points */
#define DEFQ 10                     /* default number of queries */

#define UNKNOWN 0                   /* unknowm type of gaussian */
#define DIAG 1                      /* diagonal covariance matrix */
#define FULL 2                      /* full covariance matrix */

/*****************************************************************************/

char *prog;
int verbose=1;
static double BoxMuller_buffer;     /* see BoxMuller function */
static char BoxMuller_flag=0;       /* see BoxMuller function */

/*****************************************************************************/

typedef struct
{
  int npoints;          /* number of data points (training set size) */
  int nquery;           /* number of queries (test set size) */
  int dim;              /* dimensionality */
  double **sample;      /* training and test sets (npoints+nquery vectors) */ 
  int *color;           /* color (class) of each vector */
} oracle_t;

/*****************************************************************************/

typedef struct
{
  char *label;           /* gaussian name */
  int dim;               /* dimensionality */
  int type;              /* gaussian type */
  double prior_prob;     /* a priori probability */
  double *mean;          /* gaussian mean */
  double *dcovar;        /* diagonal covariance matrix */
  double *dCholesky;     /* Cholesky decomposition of dcovar */
  double **covar;        /* full covariance matrix */
  double **Cholesky;     /* Cholesky decomposition of covar */
} gauss_t;

/*****************************************************************************/

void nonull(void *ptr)
{
  if (ptr==NULL) {
    fprintf(stderr,"%s: insufficient memory\n",__FILE__);
    exit(EXIT_FAILURE);
  }
}

/*****************************************************************************/

int InqNumPoints(Oracle *ora)
{
  oracle_t *o=ora;

  return o->npoints;
}

/*****************************************************************************/

int InqNumQuery(Oracle *ora)
{
  oracle_t *o=ora;

  return o->nquery;
}

/*****************************************************************************/

int InqNumFields(Oracle* ora, int p)
{
  oracle_t *o=ora;

  if ((p<0) || (p>=o->npoints+o->nquery)) return 0;
  else return 1+o->dim;
}

/*****************************************************************************/

char* InqField(Oracle* ora, int p, int f)
{
  oracle_t *o=ora;

  if ((p<0) || (p>=o->npoints+o->nquery)) {
    fprintf(stderr,"%s: undefined point %d\n",prog,p);
    return NULL;
  }
  if ((f<0) || (f>o->dim)) {
    fprintf(stderr,"%s: undefined field %d\n",prog,f);
    return NULL;
  }
  if (f==0) {
    char aux[16];
    if (p>=o->npoints)
      return strdup("?");
    sprintf(aux,"%d",o->color[p]);
    return strdup(aux);
  }
  {
    char aux[64];
    sprintf(aux,"%f",o->sample[p][f-1]);
    return strdup(aux);
  }
}

/*****************************************************************************/

double InqDist(Oracle* ora, int p1, int p2)
{
  oracle_t *o=ora;
  int dim=o->dim,k;
  double *x=o->sample[p1];
  double *y=o->sample[p2];
  double sum=0.0,aux;
  
  if ((p1<0) || (p1>=o->npoints+o->nquery)) {
    fprintf(stderr,"%s: undefined point %d\n",prog,p1);
    return -1.0;
  }
  if ((p2<0) || (p2>=o->npoints+o->nquery)) {
    fprintf(stderr,"%s: undefined point %d\n",prog,p2);
    return -1.0;
  }
  for (k=0;k<dim;k++) {
    aux=*x++-*y++;
    aux*=aux;
    sum+=aux;
  }
  return sqrt(sum);
}

/*****************************************************************************/

double *GetVector(FILE *fp, int d, double *v)
{
  char line[MLL],*np,*ep;
  int i;
  double x;

  while (fgets(line,MLL,fp)!=NULL) {
    for (i=0,strtod(np=line,&ep);i<d && np!=ep;i++,strtod(np=ep,&ep)) ;
    if (i!=d) continue;
    for (i=0,x=strtod(np=line,&ep);i<d;i++,x=strtod(np=ep,&ep)) v[i]=x;
    return v;
  }
  return NULL;
}

/*****************************************************************************/

void Cholesky(int d, double **A)
{
  int i,j,k;
  double aux;

  /*
  for (i=0;i<d;i++) {
    for (j=0;j<=i;j++)
      printf(" %f",A[i][j]);
    printf("\n");
  }
  */
  for (i=0;i<d;i++) {
    for (j=i;j<d;j++) {
      aux=A[j][i];
      for (k=i-1;k>=0;k--) aux-=A[i][k]*A[j][k];
      if (i==j)
	if (aux<0.0) {
	  fprintf(stderr,"%s: unable to compute Cholesky\n",prog);
	  exit(EXIT_FAILURE);
	}
	else A[i][i]=sqrt(aux);
      else A[j][i]=aux/A[i][i];
    }
  }
  /*
  for (i=0;i<d;i++) {
    for (j=0;j<=i;j++) {
      aux=0.0;
      for (k=0;k<=j;k++)
	aux+=A[i][k]*A[j][k];
      printf(" %f",aux);
    }
    printf("\n");
  }
  */
}

/*****************************************************************************/

gauss_t *GetGauss(FILE *fp)
{
  char line[MLL],*cp;
  int i,j;
  gauss_t *mp;

  if (verbose) fprintf(stderr,"Reading Gaussian...\n");
  nonull(mp=malloc(sizeof(gauss_t)));
  mp->dim=1;
  mp->type=FULL;
  mp->prior_prob=0.0;
  while (1) {
    if (fgets(line,MLL,fp)==NULL) {mp->type=UNKNOWN; break;}
    if ((cp=strtok(line,IFS))==NULL) continue;
    if (strcmp(cp,"GAUSS")!=0) continue;
    if ((cp=strtok(NULL,IFS))==NULL) continue;
    nonull(mp->label=malloc((strlen(cp)+1)*sizeof(char)));
    strcpy(mp->label,cp);
    if ((cp=strtok(NULL,IFS))==NULL) break;
    if ((mp->dim=atoi(cp))<1) {free(mp->label); continue;}
    if ((cp=strtok(NULL,IFS))==NULL) break;
    if (strncmp(cp,"Diagonal",1)==0) mp->type=DIAG;
    if ((cp=strtok(NULL,IFS))!=NULL) mp->prior_prob=atof(cp);
    if (verbose) {
      fprintf(stderr,"label=%s dim=%d ",mp->label,mp->dim);
      if (mp->type==DIAG) fprintf(stderr,"type=Diag\n");
      else fprintf(stderr,"type=Full\n");
    }
    break;
  }
  if (mp->type==UNKNOWN) {
    if (verbose) fprintf(stderr,"no more gaussians.\n");
    return NULL;
  }
  if (verbose) fprintf(stderr,"mean...\n");
  nonull(mp->mean=malloc(mp->dim*sizeof(double)));
  nonull(GetVector(fp,mp->dim,mp->mean));
  if (verbose) fprintf(stderr,"covariance matrix...\n");
  if (mp->type==DIAG) {
    mp->covar=mp->Cholesky=NULL;
    nonull(mp->dcovar=malloc(mp->dim*sizeof(double)));
    nonull(GetVector(fp,mp->dim,mp->dcovar));
    nonull(mp->dCholesky=malloc(mp->dim*sizeof(double)));
    for (i=0;i<mp->dim;i++) mp->dCholesky[i]=sqrt(mp->dcovar[i]);
  }
  else {
    mp->dcovar=mp->dCholesky=NULL;
    nonull(mp->covar=malloc(mp->dim*sizeof(double *)));
    nonull(mp->Cholesky=malloc(mp->dim*sizeof(double *)));
    for (i=0;i<mp->dim;i++) {
      nonull(mp->covar[i]=malloc((i+1)*sizeof(double)));
      nonull(GetVector(fp,i+1,mp->covar[i]));
      nonull(mp->Cholesky[i]=malloc((i+1)*sizeof(double)));
      for (j=0;j<=i;j++) mp->Cholesky[i][j]=mp->covar[i][j];
    }
    Cholesky(mp->dim,mp->Cholesky);
  }
  if (verbose) fprintf(stderr,"end GAUSS.\n");
  return mp;
}

/*****************************************************************************/

void FreeGauss(gauss_t *mp)
{
  int i;

  free(mp->label);
  free(mp->mean);
  free(mp->dcovar);
  if (mp->covar!=NULL)
    for (i=0;i<mp->dim;i++)
      free(mp->covar[i]);
  free(mp->covar);
  free(mp);
}

/*****************************************************************************/

void PutGauss(FILE *fp, gauss_t *mp)
{
  int i,j;

  fprintf(fp,"GAUSS %s %d ",mp->label,mp->dim);
  if (mp->type==DIAG) fprintf(stderr,"Diag\n");
  else fprintf(stderr,"Full\n");
  for (i=0;i<mp->dim;i++)
    fprintf(fp," %10f",mp->mean[i]);
  fprintf(fp,"\n");
  if (mp->dcovar!=NULL) {
    for (i=0;i<mp->dim;i++) {
       if (i!=0)
	 fputc(' ',fp);
      fprintf(fp,"%10f",mp->dcovar[i]);
    }
    fprintf(fp,"\n");
  }
  else {
    for (i=0;i<mp->dim;i++) {
      for (j=0;j<=mp->dim;j++) {
       if (j!=0)
	 fputc(' ',fp);
	fprintf(fp,"%10f",mp->covar[i][j]);
      }
      fprintf(fp,"\n");
    }
  }
}

/*****************************************************************************/
/* G.E.P Box, M.E. Muller, "A note on the generation of random normal
   deviates", Annals Math. Stat. 29, 610-611, 1958. */

double BoxMuller(void)
{
  double v1,v2,fac,r;

  if (BoxMuller_flag) {BoxMuller_flag=0; return BoxMuller_buffer;}
  do {
    v1=2.0*rand()/(RAND_MAX+1.0)-1.0;
    v2=2.0*rand()/(RAND_MAX+1.0)-1.0;
    r=v1*v1+v2*v2;
  } while ((r<=0.0) || (r>=1.0));
  fac=sqrt(-2.0*log(r)/r);
  BoxMuller_buffer=v1*fac;
  BoxMuller_flag=1;
  return v2*fac;
}

/*****************************************************************************/

double *SampleGauss(gauss_t *mp)
{
  int i,j;
  double *v,*aux;

  nonull(v=malloc(mp->dim*sizeof(double)));
  nonull(aux=malloc(mp->dim*sizeof(double)));
  for (i=0;i<mp->dim;i++) aux[i]=BoxMuller();
  for (i=0;i<mp->dim;i++) v[i]=mp->mean[i];
  if (mp->type==DIAG)
    for (i=0;i<mp->dim;i++) v[i]+=mp->dCholesky[i]*aux[i];
  else {
    for (i=0;i<mp->dim;i++)
      for (j=0;j<=i;j++)
	v[i]+=mp->Cholesky[i][j]*aux[j];
  }
  free(aux);
  return v;
}

/*****************************************************************************/

void usage(void)
{
  fprintf(stderr,"\n\
usage: %s\n\n\
  [-help]           this message\n\
  [-trace]          have server dump trace of messages\n\
  [-port] <port>    port number to use (default %d)\n\
  [-seed] <seed>    select a specific seed for any randomization\n\
  [-n] <num>        number of training (data) samples (default %d)\n\
  [-q] <num>        number of test samples (queries) (default %d)\n\
  [-gauss] <file>   filename of Gaussians (default stdin)\n",prog,DEFPORT,
	  DEFN,DEFQ);
  exit(EXIT_FAILURE);
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
  int port,quiet,nmodels,nsamples,i,j;
  double p,sum;
  gauss_t **gauss=NULL,*mp;
  FILE *fp;
  oracle_t ora;

  prog=argv[0]; 
  quiet=1;
  port=DEFPORT; 
  ora.npoints=DEFN;
  ora.nquery=DEFQ;
  fp=stdin;
  for (i=1;i<argc;i++)
    if (strncmp(argv[i],"-trace",2)==0) quiet=0;
    else if (strncmp(argv[i],"-help",3)==0) usage();
    else if (strncmp(argv[i],"-port",2)==0) port=atoi(argv[++i]);
    else if (strncmp(argv[i],"-seed",2)==0) srand(atoi(argv[++i]));
    else if (strncmp(argv[i],"-n",2)==0) ora.npoints=atoi(argv[++i]);
    else if (strncmp(argv[i],"-q",2)==0) ora.nquery=atoi(argv[++i]);
    else if (strncmp(argv[i],"-gauss",2)==0) {
      if ((fp=fopen(argv[++i],"r"))==NULL) {
	fprintf(stderr,"%s: couldn't open %s\n",prog,argv[i]);
	exit(EXIT_FAILURE);
      }
    }
    else {
      fprintf(stderr,"%s: unknown option %s\n",prog,argv[i]);
      usage();
    }
  for (nmodels=0;(mp=GetGauss(fp))!=NULL;gauss[nmodels++]=mp)
    nonull(gauss=realloc(gauss,(nmodels+1)*sizeof(gauss_t *)));
  if (!nmodels) {
    fprintf(stderr,"%s: couldn't read any model\n",prog);
    exit(EXIT_FAILURE);
  }
  if (verbose) fprintf(stderr,"Data generation...\n");
  nsamples=ora.npoints+ora.nquery;
  ora.dim=gauss[0]->dim;
  for (i=1;i<nmodels;i++)
    if (gauss[i]->dim!=ora.dim) {
      fprintf(stderr,"%s: incompatible gaussian %s\n",prog,gauss[i]->label);
      exit(EXIT_FAILURE);
    }
  nonull(ora.sample=malloc(nsamples*sizeof(double *)));
  nonull(ora.color=malloc(nsamples*sizeof(int)));
  sum=0.0; for (i=0;i<nmodels;i++) sum+=gauss[i]->prior_prob;
  sum=(1.0-sum)/nmodels; for (i=0;i<nmodels;i++) gauss[i]->prior_prob+=sum;
  for (i=0;i<nsamples;i++) {
    p=rand()/(RAND_MAX+1.0);
    sum=0.0; 
    for (j=0;;j++) {
      sum+=gauss[j]->prior_prob;
      if (sum>p || j==nmodels-1) break;
    }
    ora.color[i]=j;
    ora.sample[i]=SampleGauss(gauss[ora.color[i]]);
    if (verbose) {
      for (j=0;j<ora.dim;j++) {
	 if (j!=0)
	   fputc(' ',stdout);
	 fprintf(stdout," %f",ora.sample[i][j]);
      }
      fprintf(stdout,"\n");
    }
  }
  for (i=0;i<nmodels;i++) FreeGauss(gauss[i]);
  exit(EXIT_SUCCESS);
}

/*****************************************************************************/


