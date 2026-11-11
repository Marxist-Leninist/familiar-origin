#ifndef ORIGIN_NN_H
#define ORIGIN_NN_H
#include "core.h"
#define NN_PARAMS 816
#define NN_STEPS 250
void nn_init(unsigned seed);
float nn_loss(int backward);
float nn_train_step(void);
void nn_generate(char output[16]);
unsigned nn_steps(void);
unsigned nn_fingerprint(void);
float nn_gradient_check(void);
#endif
