#include "nn.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
int main(void){const unsigned seeds[]={7,42,123};for(unsigned si=0;si<3;++si){unsigned seed=seeds[si];nn_init(seed);float initial=nn_loss(0),loss=initial;char before[16],after[16];nn_generate(before);unsigned old=nn_fingerprint();for(unsigned i=0;i<NN_STEPS;++i)loss=nn_train_step();nn_generate(after);printf("seed=%u initial=%.8f final=%.8f before='%s' after='%s' weights=%u->%u\n",seed,initial,loss,before,after,old,nn_fingerprint());assert(isfinite(loss)&&loss<.02f);assert(strcmp(after,"Hello, world!")==0);assert(strcmp(before,after));assert(old!=nn_fingerprint());}
float error=nn_gradient_check();printf("18 float32 gradient samples max error %.9f\n",error);assert(error<.0001f);puts("PASS guest C Transformer host validation");}
