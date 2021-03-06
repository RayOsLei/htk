/* ----------------------------------------------------------- */
/*                                                             */
/*                          ___                                */
/*                       |_| | |_/   SPEECH                    */
/*                       | | | | \   RECOGNITION               */
/*                       =========   SOFTWARE                  */ 
/*                                                             */
/*                                                             */
/* ----------------------------------------------------------- */
/* developed at:                                               */
/*                                                             */
/*      Machine Intelligence Laboratory                        */
/*      Department of Engineering                              */
/*      University of Cambridge                                */
/*      http://mi.eng.cam.ac.uk/                               */
/*                                                             */
/* ----------------------------------------------------------- */
/*         Copyright:                                          */
/*         2002-2003  Cambridge University                     */
/*                    Engineering Department                   */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*         File: HLVRec-outP.c OutP calculation and caching    */
/* ----------------------------------------------------------- */

/*  *** THIS IS A MODIFIED VERSION OF HTK ***                        */
/* ----------------------------------------------------------------- */
/*           The HMM-Based Speech Synthesis System (HTS)             */
/*           developed by HTS Working Group                          */
/*           http://hts.sp.nitech.ac.jp/                             */
/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2001-2015  Nagoya Institute of Technology          */
/*                           Department of Computer Science          */
/*                                                                   */
/*                2001-2008  Tokyo Institute of Technology           */
/*                           Interdisciplinary Graduate School of    */
/*                           Science and Engineering                 */
/*                                                                   */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/* - Redistributions of source code must retain the above copyright  */
/*   notice, this list of conditions and the following disclaimer.   */
/* - Redistributions in binary form must reproduce the above         */
/*   copyright notice, this list of conditions and the following     */
/*   disclaimer in the documentation and/or other materials provided */
/*   with the distribution.                                          */
/* - Neither the name of the HTS working group nor the names of its  */
/*   contributors may be used to endorse or promote products derived */
/*   from this software without specific prior written permission.   */
/*                                                                   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
/* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
/* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
/* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
/* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
/* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
/* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
/* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
/* POSSIBILITY OF SUCH DAMAGE.                                       */
/* ----------------------------------------------------------------- */

static void ResetOutPCache (OutPCache *cache)
{
   int i;

   
   if (cache->nStates > 0)
      for (i = 0; i <= cache->nStates; ++i)
         cache->stateT[i] = -1000;
   
   if (cache->nMix > 0)
      for (i = 0; i <= cache->nMix; ++i)
         cache->mixT[i] = -1000;

   cache->cacheHit = cache->cacheMiss = 0;
}

static OutPCache *CreateOutPCache (MemHeap *heap, HMMSet *hset, int block)
{
   OutPCache *cache;

   cache = New (heap, sizeof (OutPCache));

   cache->block = block;
   cache->nStates = hset->numSharedStates;
   cache->nMix = hset->numSharedMix;
   /* the sIdx values are 1..numSharedStates, thus the +1 below. Same for mIdx */
   
   cache->stateOutP = cache->mixOutP = NULL;
   if (cache->nStates > 0) {
      cache->stateT = (int *) New (heap, (cache->nStates + 1) * sizeof (int));
      cache->stateOutP = (LogFloat *) New (heap, (cache->nStates + 1) * cache->block * sizeof (LogFloat));
   }
   if (cache->nMix > 0) {
      cache->mixT = (int *) New (heap, (cache->nMix + 1) * sizeof (int));
      cache->mixOutP = (LogFloat *) New (heap, (cache->nMix + 1) * cache->block * sizeof (LogFloat));
   }

   return cache;
}

/* SOutP_ID_mix_Block: returns log prob of stream s of observation x */
LogFloat SOutP_ID_mix_Block(HMMSet *hset, int s, Observation *x, StreamInfo *sti)
{
   int vSize;
   LogDouble px;
   MixtureElem *me;
   MixPDF *mp;
   Vector v;
   LogFloat wt;

   assert (hset->hsKind == PLAINHS && hset->hsKind == SHAREDHS);
   
   v = x->fv[s];
   if (hset->msdflag[s]) {
      vSize = SpaceOrder(v);
   } else {
   vSize = VectorSize(v);
   assert (vSize == hset->swidth[s]);
   }
   me = sti->spdf.cpdf+1;
   if (sti->nMix == 1){     /* Single Mixture Case */
      mp = me->mpdf; 
      assert (mp->ckind == INVDIAGC);
      /*       px = IDOutP(v,vSize,mp); */
      {
         int i;
         float sum;
         float *mean, *ivar;

         mean = mp->mean;
         ivar = mp->cov.var;

         sum = mp->gConst;
         for (i=1 ; i <= vSize; i++) {
            sum += (v[i] - mean[i]) * (v[i] - mean[i]) * ivar[i];
         }
         px = -0.5*sum;
      }


      return px;
   } else {             /* Multi Mixture Case */
      LogDouble bx = LZERO;                   
      int m;

      for (m=1; m<=sti->nMix; m++,me++) {
         wt = MixLogWeight(hset,me->weight);
         if (wt>LMINMIX) {  
            mp = me->mpdf; 
            if (!hset->msdflag[s] || vSize == VectorSize(mp->mean))
            /*       px = IDOutP(v,vSize,mp);   */
            {
               int i;
               float sum,xmm;
               
               sum = mp->gConst;
               for (i=1;i<=vSize;i++) {
                  xmm = v[i] - mp->mean[i];
                  sum += xmm*xmm*mp->cov.var[i];
               }
               px = -0.5*sum;
            }
            
            bx = LAdd(bx,wt+px);
         }
      }
      return bx;
   }
   return LZERO;;
}

#if 0           /* old OutPBlock()  copes with streams and non-diag outp's */
static void OutPBlock (DecoderInst *dec, Observation **obsBlock, 
                       int n, HLink hmm, int state, float acScale, LogFloat *outP)
{
   int i;
   
#if 0
   for (i = 0; i < n; ++i) {
      outP[i] = OutP (obsBlock[i], hmm, state);
   }

#else
   StateInfo *si;
   StreamElem *se;
   int s, S = obsBlock[0]->swidth[0];
   
   si = (hmm->svec+state)->info;
   se = si->pdf+1;
   
   if (S == 1 && !si->weights) {
      
      for (i = 0; i < n; ++i) {
         outP[i] = OutP_lv (dec->si, hmm->svec[state].info->sIdx, &obsBlock[i]->fv[1][1]);
#if 1   /* sanity checking */
         {
            LogFloat soutp;
            soutp = SOutP (hmm->owner, 1, obsBlock[i], se);
            assert (fabs (outP[i] - soutp) < 0.01);
         }
#endif
      }
   }
   else {       /* multi stream */
      Vector w;

      for (i = 0; i < n; ++i)
         outP[i] = 0.0;
      
      w = si->weights;
      for (s = 1; s <= S; s++, se++)
         for (i = 0; i < n; ++i)
            outP[i] += w[s] * SOutP (hmm->owner, s, obsBlock[i], se);
   }

   /* acoustic scaling */
   if (acScale != 1.0)
      for (i = 0; i < n; ++i)
         outP[i] *= acScale;
#endif
}

#endif


/* cOutP

     caching version of OutP from HModel. This only caches only on a state 
     level, not on a mixture level. 
*/
static LogFloat cOutP (DecoderInst *dec, Observation *x, HLink hmm, int state)
{
   int sIdx, n;
   LogFloat outP;
   OutPCache *cache;

   assert (x == dec->obsBlock[0]);

   cache = dec->outPCache;
   sIdx = hmm->svec[state].info->sIdx;

   assert (sIdx >= 0);
   assert (sIdx < cache->nStates);
   
   n = dec->frame - cache->stateT[sIdx];

   assert (n >= 0);

   if (n < cache->block) {
      outP = cache->stateOutP[sIdx * cache->block + n];
      ++cache->cacheHit;
#if 0
      /* the following is *very* expensive, it effectively disables the cache,
         use only for sanity checking! */
      assert (outP == dec->acScale * OutP (x, hmm, state));
#endif
   }
   else {
      ++cache->cacheMiss;
      if (!cache->mixOutP) {     /* don't bother caching mixtures */
         /* #### handle boundary case where we don't have cache->block obs left */

         if (!dec->si->useHModel) 
            OutPBlock (dec->si, &dec->obsBlock[0], cache->block,
                       sIdx, dec->acScale, &cache->stateOutP[sIdx * cache->block]);
         else
            OutPBlock_HMod (dec->si, &dec->obsBlock[0], cache->block,
                            sIdx, dec->acScale, &cache->stateOutP[sIdx * cache->block],
                            dec->frame);
            
         cache->stateT[sIdx] = dec->frame;
         outP = cache->stateOutP[sIdx * cache->block];
#if 0   /* sanity checking for OutPBlock */
         {
            LogFloat safe_outP;
            safe_outP = dec->acScale * OutP (x, hmm, state);
            assert (fabs (outP - safe_outP) < 0.01);
         }
#endif
      }
      else {            /* cache mixtures (e.g. for soft-tied systems) */
         abort ();
         /*
x      outP = OutP (x, hmm, state);
x      dec->cacheOutP[sIdx] = outP;
x      CACHE_FLAG_SET(dec, sIdx);
         */
      }
   }      

   return outP;
}



/* outP caclulation for USEHMODEL=T case  */


/*******************************************************************************/
/*  outP calculation from HModel.c and extended for new adapt code */


static LogFloat SOutP_HMod (HMMSet *hset, int s, Observation *x, StreamInfo *sti,
                            int id)
{
   int m;
   LogFloat bx,px,wt,det;
   MixtureElem *me;
   MixPDF *mp;
   Vector v,otvs;
   
   /* Note hset->kind == SHAREDHS */
   assert (hset->hsKind == SHAREDHS);

   v=x->fv[s];
   me=sti->spdf.cpdf+1;
   if (sti->nMix==1){     /* Single Mixture Case */
      bx= MOutP(ApplyCompFXForm(me->mpdf,v,inXForm,&det,id),me->mpdf);
      bx += det;
   } else if (!pde) {
      bx=LZERO;                   /* Multi Mixture Case */
      for (m=1; m<=sti->nMix; m++,me++) {
         wt = MixLogWeight(hset,me->weight);
         if (wt>LMINMIX) {   
            px= MOutP(ApplyCompFXForm(me->mpdf,v,inXForm,&det,id),me->mpdf);
            px += det;
            bx=LAdd(bx,wt+px);
         }
      }
   } else {   /* Partial distance elimination */
      wt = MixLogWeight(hset,me->weight);
      mp = me->mpdf;
      if (!hset->msdflag[s] || SpaceOrder(v)==VectorSize(mp->mean)) {
      otvs = ApplyCompFXForm(mp,v,inXForm,&det,id);
      px = IDOutP(otvs,VectorSize(otvs),mp);
      } else {
         px = LZERO;
         det = 0.0;
      }
      bx = wt+px+det;
      for (m=2,me=sti->spdf.cpdf+2;m<=sti->nMix;m++,me++) {
         wt = MixLogWeight(hset,me->weight);
	 if (wt>LMINMIX){
	    mp = me->mpdf;
	    otvs = ApplyCompFXForm(mp,v,inXForm,&det,id);
	    if (PDEMOutP(otvs,mp,&px,bx-wt-det) == TRUE)
	      bx = LAdd(bx,wt+px+det);
	 }
      }
   }
   return bx;
}

LogFloat POutP_HModel (HMMSet *hset,Observation *x, StateInfo *si, int id)
{
   LogFloat bx;
   StreamElem *se;
   Vector w;
   int s,S = x->swidth[0];
   
   if (S==1 && si->weights==NULL)
      return SOutP_HMod(hset,1,x,si->pdf[1].info, id);
   bx=0.0; se=si->pdf+1; w = si->weights;
   for (s=1;s<=S;s++,se++)
      bx += w[s]*SOutP_HMod(hset,s,x,se->info, id);
   return bx;
}

void OutPBlock_HMod (StateInfo_lv *si, Observation **obsBlock, 
                int n, int sIdx, float acScale, LogFloat *outP, int id)
{
   int i;

   assert  (si->useHModel);
   
   for (i = 0; i < n; ++i) {
      outP[i] = POutP_HModel (si->hset, obsBlock[i], si->si[sIdx], id);
   }
   
   /* acoustic scaling */
   if (acScale != 1.0)
      for (i = 0; i < n; ++i)
         outP[i] *= acScale;
}
