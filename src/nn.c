/* Freestanding, original single-head causal Transformer. No libc or model files. */
#include "nn.h"
#define D 8
#define F 16
#define T 14
#define V 12
#define MAX 224
#define E 0
#define P 96
#define WQ 208
#define WK 272
#define WV 336
#define WO 400
#define W1 464
#define W2 592
#define HEAD 720
static float weights[NN_PARAMS],gradient[NN_PARAMS],moment[NN_PARAMS],variance[NN_PARAMS];
typedef struct {float v[MAX],g[MAX];unsigned rows,cols;} Matrix;
static Matrix x,q,k,v,attn,project,h,ffpre,ff,ffout,y,logits;
static float probs[T*T];static unsigned ids[T],step;
static float beta1,beta2;
/* BOS, EOS and unique characters in first-occurrence order. */
static const char tokens[V]={'^','$','H','e','l','o',',',' ','w','r','d','!'};
static const unsigned targets[T]={2,3,4,4,5,6,7,8,5,9,4,10,11,1};
static float absolute(float a){return a<0?-a:a;}
static float exponential(float a){if(a< -30)return 0;if(a>30)a=30;int n=(int)(a*1.4426950408889634f);float r=a-(float)n*.6931471805599453f,term=1,sum=1;for(unsigned i=1;i<=10;++i){term*=r/(float)i;sum+=term;}union {unsigned u;float f;}scale;scale.u=(unsigned)(n+127)<<23;return sum*scale.f;}
static float logarithm(float a){union {unsigned u;float f;}m;m.f=a;int e=(int)(m.u>>23)-127;m.u=(m.u&0x7fffffu)|(127u<<23);float z=(m.f-1)/(m.f+1),power=z,sum=0;for(unsigned j=1;j<20;j+=2){sum+=power/(float)j;power*=z*z;}return 2*sum+(float)e*.6931471805599453f;}
static float root(float a){if(a<=0)return 0;union {unsigned u;float f;}r;r.f=a;r.u=(r.u>>1)+0x1fc00000u;for(unsigned i=0;i<5;++i)r.f=.5f*(r.f+a/r.f);return r.f;}
static float hyperbolic(float a){float e=exponential(-2*absolute(a)),t=(1-e)/(1+e);return a<0?-t:t;}
static void reset(Matrix *m,unsigned rows,unsigned cols){m->rows=rows;m->cols=cols;for(unsigned i=0;i<MAX;++i)m->v[i]=m->g[i]=0;}
static void linear(Matrix *a,unsigned offset,unsigned width,Matrix *b){reset(b,a->rows,width);for(unsigned i=0;i<a->rows;++i)for(unsigned j=0;j<width;++j)for(unsigned d=0;d<a->cols;++d)b->v[i*width+j]+=a->v[i*a->cols+d]*weights[offset+d*width+j];}
static void linear_back(Matrix *a,unsigned offset,Matrix *b){for(unsigned i=0;i<a->rows;++i)for(unsigned j=0;j<b->cols;++j)for(unsigned d=0;d<a->cols;++d){float g=b->g[i*b->cols+j];a->g[i*a->cols+d]+=g*weights[offset+d*b->cols+j];gradient[offset+d*b->cols+j]+=g*a->v[i*a->cols+d];}}
static void add(Matrix *a,Matrix *b,Matrix *c){reset(c,a->rows,a->cols);for(unsigned i=0;i<a->rows*a->cols;++i)c->v[i]=a->v[i]+b->v[i];}
static void add_back(Matrix *a,Matrix *b,Matrix *c){for(unsigned i=0;i<c->rows*c->cols;++i){a->g[i]+=c->g[i];b->g[i]+=c->g[i];}}
static void forward(unsigned n){reset(&x,n,D);for(unsigned i=0;i<n;++i)for(unsigned d=0;d<D;++d)x.v[i*D+d]=weights[E+ids[i]*D+d]+weights[P+i*D+d];linear(&x,WQ,D,&q);linear(&x,WK,D,&k);linear(&x,WV,D,&v);reset(&attn,n,D);
 for(unsigned i=0;i<n;++i){float maximum=-1e30f,sum=0;for(unsigned j=0;j<=i;++j){float s=0;for(unsigned d=0;d<D;++d)s+=q.v[i*D+d]*k.v[j*D+d];s*=.3535533905932738f;probs[i*T+j]=s;if(s>maximum)maximum=s;}for(unsigned j=0;j<=i;++j)sum+=(probs[i*T+j]=exponential(probs[i*T+j]-maximum));for(unsigned j=0;j<=i;++j){probs[i*T+j]/=sum;for(unsigned d=0;d<D;++d)attn.v[i*D+d]+=probs[i*T+j]*v.v[j*D+d];}}
 linear(&attn,WO,D,&project);add(&x,&project,&h);linear(&h,W1,F,&ffpre);reset(&ff,n,F);for(unsigned i=0;i<n*F;++i)ff.v[i]=hyperbolic(ffpre.v[i]);linear(&ff,W2,D,&ffout);add(&h,&ffout,&y);linear(&y,HEAD,V,&logits);
}
static void backward(void){linear_back(&y,HEAD,&logits);add_back(&h,&ffout,&y);linear_back(&ff,W2,&ffout);for(unsigned i=0;i<T*F;++i)ffpre.g[i]=ff.g[i]*(1-ff.v[i]*ff.v[i]);linear_back(&h,W1,&ffpre);add_back(&x,&project,&h);linear_back(&attn,WO,&project);
 for(unsigned i=0;i<T;++i){float dp[T]={0},dot=0;for(unsigned j=0;j<=i;++j){for(unsigned d=0;d<D;++d){dp[j]+=attn.g[i*D+d]*v.v[j*D+d];v.g[j*D+d]+=probs[i*T+j]*attn.g[i*D+d];}dot+=probs[i*T+j]*dp[j];}for(unsigned j=0;j<=i;++j){float ds=probs[i*T+j]*(dp[j]-dot)*.3535533905932738f;for(unsigned d=0;d<D;++d){q.g[i*D+d]+=ds*k.v[j*D+d];k.g[j*D+d]+=ds*q.v[i*D+d];}}}
 linear_back(&x,WQ,&q);linear_back(&x,WK,&k);linear_back(&x,WV,&v);for(unsigned i=0;i<T;++i)for(unsigned d=0;d<D;++d){gradient[E+ids[i]*D+d]+=x.g[i*D+d];gradient[P+i*D+d]+=x.g[i*D+d];}
}
void nn_init(unsigned seed){step=0;beta1=beta2=1;for(unsigned i=0;i<NN_PARAMS;++i){seed=seed*1664525u+1013904223u;float scale=i<WQ?.15f:.3f;weights[i]=((float)(seed>>8)/16777216.0f*2-1)*scale;gradient[i]=moment[i]=variance[i]=0;}}
float nn_loss(int back){ids[0]=0;for(unsigned i=1;i<T;++i)ids[i]=targets[i-1];forward(T);float loss=0;for(unsigned i=0;i<T;++i){float maximum=-1e30f,e[V],sum=0;for(unsigned j=0;j<V;++j)if(logits.v[i*V+j]>maximum)maximum=logits.v[i*V+j];for(unsigned j=0;j<V;++j)sum+=(e[j]=exponential(logits.v[i*V+j]-maximum));loss+=logarithm(sum)+maximum-logits.v[i*V+targets[i]];if(back)for(unsigned j=0;j<V;++j)logits.g[i*V+j]=(e[j]/sum-(j==targets[i]?1:0))/(float)T;}if(back)backward();return loss/(float)T;}
float nn_train_step(void){for(unsigned i=0;i<NN_PARAMS;++i)gradient[i]=0;float loss=nn_loss(1),norm=0;for(unsigned i=0;i<NN_PARAMS;++i)norm+=gradient[i]*gradient[i];float clip=norm>25?5/root(norm):1;++step;beta1*=.9f;beta2*=.999f;for(unsigned i=0;i<NN_PARAMS;++i){float g=gradient[i]*clip;moment[i]=.9f*moment[i]+.1f*g;variance[i]=.999f*variance[i]+.001f*g*g;weights[i]-=.02f*(moment[i]/(1-beta1))/(root(variance[i]/(1-beta2))+1e-8f);}return loss;}
void nn_generate(char output[16]){ids[0]=0;unsigned n=0;for(unsigned i=0;i<T;++i){forward(i+1);unsigned best=0;for(unsigned j=1;j<V;++j)if(logits.v[i*V+j]>logits.v[i*V+best])best=j;if(best==1)break;output[n++]=tokens[best];if(i+1<T)ids[i+1]=best;}output[n]=0;}
unsigned nn_steps(void){return step;}
unsigned nn_fingerprint(void){unsigned hash=2166136261u;for(unsigned i=0;i<NN_PARAMS;++i){union{float f;unsigned u;}bits;bits.f=weights[i];hash=(hash^bits.u)*16777619u;}return hash;}
float nn_gradient_check(void){nn_init(9);nn_loss(1);static const unsigned offsets[]={0,95,96,207,208,271,272,335,336,399,400,463,464,591,592,719,720,815};float maximum=0;for(unsigned i=0;i<18;++i){unsigned index=offsets[i];float actual=gradient[index],value=weights[index],eps=.01f;weights[index]=value+eps;float a=nn_loss(0);weights[index]=value-eps;float b=nn_loss(0);weights[index]=value;float error=absolute(actual-(a-b)/(2*eps));if(error>maximum)maximum=error;}return maximum;}
