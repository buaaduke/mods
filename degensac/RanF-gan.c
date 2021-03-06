#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "ranF.h"
#include "Ftools.h"
#include "rtools.h"
#include <matutls/matutl.h>

#include <gandalf/linalg/mat_gen.h>
#include <gandalf/linalg/mat_square.h>
#include <gandalf/linalg/mat_qr.h>

int iterF(double *u, int len, int *inliers, double th, double ths,
          int steps, double *F, double **errs, double *buffer)
{
  double *d = errs[1], *w;
  double f[9], dth;
  int it, I, Is, maxI;

  w = (double *) malloc(len * sizeof(double));
  dth = (ths - th) / (steps); 

  /* F from the sample inliers by th */

  maxI = inlidxs(errs[4], len, th, inliers);
  u2f(u, inliers, maxI, f, buffer);

  /*iterate */

  for (it = 0; it < steps; it ++)
    {
      exFDs (u, f, d, w, len); 

      Is = inlidxs(d, len, th, inliers);
      I = inlidxs (d, len, ths, inliers);

      if (Is > maxI)
	{
	  maxI = Is;
	  errs[1] = errs[0];
	  errs[0] = d;
	  d = errs[1];
          memcpy(F,f,9*sizeof(double)); /*!!!*/
	} 

      if (I < 8)
	{
         free(w); 
         return maxI;
	}
      u2fw(u, inliers, w, I, f, buffer);
      ths -= dth;
    } 
 
  FDs (u, f, d, len);
  I = inlidxs (d, len, th, inliers);
  if (I > maxI)
    {
      maxI = I;
      errs[1] = errs[0];
      errs[0] = d;
      memcpy(F,f,9*sizeof(double)); /*!!!*/
    }

  free(w);
  return maxI;
}

int inFran (double *u, int len, int *inliers, int ninl,
            double th, double **errs, double *buffer, 
            double *F, int rep)
{
  int I, maxI, ssiz, i, Is;
  double *d = errs[1], f[9];
  int *sample;
  int * inls;

  if (ninl < 16) return 0;
  ssiz = ninl /2;
  /*  if (ssiz > 14) ssiz = 14; */

  inls = (int *) malloc (sizeof(int) * len);
  maxI = ninl;

  for (i = 0; i < rep; i++)
    {
      sample = randsubset(inliers, ninl, ssiz);
      u2f(u, sample, ssiz, f, buffer);
      FDs (u, f, d, len);
      Is = inlidxs (d, len, th, inls);
      I = inlidxs (d, len, TC*th, inls);
      if (Is > maxI)
	{
	  maxI = Is;
	  errs[1] = errs[0];
	  errs[0] = d;
	  d = errs[1];
          memcpy(F,f,9*sizeof(double)); /*!!!*/
	}
      if (I < 8) continue;
      u2f(u, inls, I, f, buffer);
      FDs (u, f, d, len);
      I = inlidxs (d, len, th, inls);
      if (I > maxI)
	{
	  maxI = I;
	  errs[1] = errs[0];
	  errs[0] = d;
	  d = errs[1];
          memcpy(F,f,9*sizeof(double)); /*!!!*/
	}
    }

  free(inls);
  return maxI;
}

int inFrani (double *u, int len, int *inliers, int psz, int ninl,
            double th, double **errs, double *buffer, 
            double *F, int rep)
{
  int I, maxI, ssiz, i;
  double *d, f[9];
  int *sample;
  int *intbuff;

  intbuff = (int *) malloc(sizeof(int) * len);

  if (psz < 16) return 0;
  ssiz = psz /2;
  //  if (ssiz > 14) ssiz = 14;
  ssiz = 9;

   maxI = ninl;

  d = errs[2];
  errs[2] = errs[0];
  errs[0] = d;

  for (i = 0; i < rep; i++)
    {
      sample = randsubset(inliers, psz, ssiz);
      u2f(u, sample, ssiz, f, buffer);
      FDs (u, f, errs[0], len);
      errs[4] = errs[0];

      I = iterF(u, len, intbuff, th, TC*th, 4, f, errs, buffer);

      if (I > maxI)
	{
	  maxI = I;
          d = errs[2];
	  errs[2] = errs[0];
	  errs[0] = d;
          memcpy(F,f,9*sizeof(double)); /*!!!*/
	}
    }

   errs[4] = errs[2];
   I = iterF(u, len, intbuff, th, 16*TC*th, 10, f, errs, buffer);
   if (I > maxI)
	{
	  //          printf("!");
	  maxI = I;
          d = errs[2];
	  errs[2] = errs[0];
	  errs[0] = d;
          memcpy(F,f,9*sizeof(double)); /*!!!*/
	}

  d = errs[2];
  errs[2] = errs[0];
  errs[0] = d;

  free(intbuff);
  return maxI;
}

void checkerrs(double ** errs)
{
  if ((errs[0] == errs[1])||(errs[0] == errs[2])||(errs[2] == errs[1]))
    printf("************ ERROR !!!!! *********************");
}

/*********************   RANSAC   ************************/

#define wspacesize (4*9*9)

int ransacF(double *u, int len, double th, double conf,
             double *F, unsigned char * inl,
             int * data_out)
{
  int *pool, no_sam, max_sam, new_sam;
  double *Z, *M, *buffer;
  double *f1, *f2;

  double poly[4], roots[3], f[9], *err, *d;
  double *errs[5];
  int nsol, i, j, *inliers, new_max, do_iterate;
  int maxI, maxIs, I;
  int *samidx;

  Gan_Matrix mA, mQ;
  double *adWorkspace; 

  /* to eliminate */
  int iter_cnt = 0, LmaxI;

  /* allocations */

  pool = (int *)malloc(len * sizeof(int));
  for (i = 0; i < len; i ++)
    pool[i] = i;
  samidx = pool + len - 7;

  Z = (double *) malloc(len * 9 * sizeof(double));
  lin_fm(u, Z, pool, len);
 
  buffer = (double *) malloc(len * 9 * sizeof(double));

  err = (double *) malloc(len * 4 * sizeof(double));
  for (i=0; i<4; i++)
    errs[i] = err + i * len;

  inliers = (int *) malloc(sizeof(int) * len);

  maxI  = 8;
  maxIs = 8;
  max_sam = MAX_SAMPLES;
  no_sam = 0;

  /* Gandalf */
  gan_mat_form (&mA, 9, 9); 
  gan_mat_form (&mQ, 9, 9); 
  adWorkspace = (double*) malloc(wspacesize * sizeof(double));
  f1 = mQ.data + 7*9;
  f2 = mQ.data + 8*9;
  M = mA.data;
  for (i=7*9; i<9*9; i++)
     M[i] = 0.0;

  /*  srand(RAND_SEED++); */
  while(no_sam < max_sam)
    {
      no_sam ++;
      checkerrs(errs);

      rsampleT(Z, 9, pool, 7, len, M);

      /* QR */
     if ( gan_mat_qr(&mA, &mQ, NULL, adWorkspace, wspacesize) == GAN_FALSE)
       {
        printf("Gndalf routine gan_mat_qr() failed.");
	exit(-1);
       }
      slcm (f1, f2, poly);  
      nsol = rroots3(poly, roots);

      new_max = 0; do_iterate = 0;
      LmaxI = 0;
      for (i = 0; i < nsol; i++)
        {
          for (j = 0; j < 9; j++)
	    f[j] = f1[j] * roots[i] + f2[j] * (1 -roots[i]);

          /* orient. constr. */
	  if (!all_ori_valid(f, u, samidx, 7))  continue;  

          d = errs[i];
          FDs(u, f, d, len);

          I = 0;
          for (j = 0; j < len; j++)
             if (d[j] <= th) I++;

          if (I > LmaxI) LmaxI = I;

          if(I > maxI)
	    {
	      errs[i] = errs[3];
	      errs[3] = d;
	      maxI = I;
	      memcpy(F,f,9*sizeof(double)); /*!!!*/
	      new_max = 1;
	    }
          if(I > maxIs)
	    {
	      do_iterate = no_sam > ITER_SAM;
	      maxIs = I;
	      errs[4] = d;
	    }
        }

      data_out[LmaxI+2] ++;

      if ((no_sam == ITER_SAM) && (maxIs > 8))
	do_iterate = 1;

      if (do_iterate)
	{
	  iter_cnt ++;

 	  d = errs[0];
	  I = inlidxs(errs[4], len, TC*th, inliers);
	  u2f(u, inliers, I, f, buffer);
          FDs(u, f, d, len);
	  I = inlidxs(d, len, th, inliers);
          I = inFrani (u, len, inliers, I, maxI, th, errs, buffer, f, RAN_REP);

          if(I > maxI)
            {
   	       d = errs[0];
	       errs[0] = errs[3];
	       errs[3] = d;
	       maxI = I;
	       memcpy(F,f,9*sizeof(double)); /*!!!*/
               new_max = 1;
            }
	}

      if (new_max)
	{
           new_sam = nsamples(maxI+1, len, 7, conf); 
	   if (new_sam < max_sam)
	     max_sam = new_sam;
	}
    }

   d = errs[3];
  for (j = 0; j < len; j++)   
    if (d[j] <= th) inl[j] = 1; 
    else inl[j] = 0; 

  /* deallocations */

  free(pool);
  free(Z);
  free(err);
  free(inliers);
  free(buffer);

  /* Gandalf */
  gan_mat_free(&mA); 
  gan_mat_free(&mQ); 
  free(adWorkspace);

  xprintf("Samples done %d\n", no_sam);
  *data_out = no_sam; 
  data_out[1] = iter_cnt;
  return maxI;
}


