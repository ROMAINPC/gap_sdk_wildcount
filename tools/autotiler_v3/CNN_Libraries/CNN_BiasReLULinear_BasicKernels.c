#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpointer-sign"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "Gap.h"
#include "CNN_BasicKernels.h"

static int CoreCountDynamic = 1;
static int ActiveCore = gap_ncore();

static inline unsigned int __attribute__((always_inline)) ChunkSize(unsigned int X)

{
	unsigned int NCore;
	unsigned int Log2Core;
	unsigned int Chunk;

	if (CoreCountDynamic) NCore = ActiveCore; else NCore = gap_ncore();
	Log2Core = gap_fl1(NCore);
	Chunk = (X>>Log2Core) + ((X&(NCore-1))!=0);
	return Chunk;
}

unsigned short int SIGMOID_ACT_LUT_uint16[256] = {
    32768, 33451, 34133, 34813, 35493, 36169, 36843, 37513, 38180, 38841, 39498,
    40149, 40794, 41432, 42064, 42688, 43304, 43912, 44511, 45102, 45683, 46255,
    46817, 47369, 47911, 48443, 48964, 49475, 49975, 50464, 50942, 51409, 51865,
    52311, 52745, 53169, 53581, 53983, 54374, 54755, 55125, 55485, 55834, 56174,
    56503, 56823, 57133, 57433, 57724, 58007, 58280, 58544, 58800, 59048, 59288,
    59519, 59743, 59959, 60168, 60370, 60565, 60753, 60935, 61110, 61279, 61441,
    61599, 61750, 61896, 62036, 62172, 62302, 62428, 62549, 62666, 62778, 62886,
    62990, 63090, 63186, 63279, 63368, 63454, 63536, 63615, 63691, 63765, 63835,
    63903, 63968, 64030, 64090, 64148, 64204, 64257, 64308, 64357, 64405, 64450,
    64494, 64536, 64576, 64614, 64652, 64687, 64721, 64754, 64786, 64816, 64845,
    64873, 64900, 64926, 64950, 64974, 64997, 65019, 65039, 65060, 65079, 65097,
    65115, 65132, 65149, 65164, 65179, 65194, 65208, 65221, 65234, 65246, 65258,
    65269, 65280, 65291, 65301, 65310, 65319, 65328, 65337, 65345, 65352, 65360,
    65367, 65374, 65381, 65387, 65393, 65399, 65404, 65410, 65415, 65420, 65425,
    65429, 65433, 65438, 65442, 65445, 65449, 65453, 65456, 65459, 65462, 65465,
    65468, 65471, 65474, 65476, 65479, 65481, 65483, 65485, 65488, 65489, 65491,
    65493, 65495, 65497, 65498, 65500, 65501, 65503, 65504, 65505, 65507, 65508,
    65509, 65510, 65511, 65512, 65513, 65514, 65515, 65516, 65517, 65517, 65518,
    65519, 65520, 65520, 65521, 65522, 65522, 65523, 65523, 65524, 65524, 65525,
    65525, 65526, 65526, 65526, 65527, 65527, 65528, 65528, 65528, 65529, 65529,
    65529, 65529, 65530, 65530, 65530, 65530, 65531, 65531, 65531, 65531, 65531,
    65532, 65532, 65532, 65532, 65532, 65532, 65533, 65533, 65533, 65533, 65533,
    65533, 65533, 65533, 65534, 65534, 65534, 65534, 65534, 65534, 65534, 65534,
    65534, 65534, 65535};

#ifndef NEAREST
#define NEAREST
#endif

static int SigmoidLUT(int x){
	/* Input x: Q12 [-8:8] range

	   Output y = sig(x) -> Q15
	*/
#ifndef NEAREST
	int result, ua, ub, ut;
	int abs_x = (Abs(x) * 3) >> 9; // * 3/4 (*3 >>2) and clip it to [0:255] (>>7) ot be an index of the LUT
	if (abs_x > 255) {
		result = 0x1FFFC00; // result = 1 in Q25
	} else {
		ua = SIGMOID_ACT_LUT_uint16[abs_x];
		ub = SIGMOID_ACT_LUT_uint16[abs_x+1];
		ut = abs_x & 0xFF;
		result = (ua << 9) + ut * (ub-ua); // LUT in Q16 * ut in Q9 = Q25
	}
	if (x>0) result = result;
	else     result = (1<<25) - result;
	return result >> 10;
#else
	int result;
	int abs_x = (Abs(x) * 3) >> 9; // * 3/4 (*3 >>2) and clip it to [0:255] (>>7) ot be an index of the LUT
	if (abs_x > 255) {
		result = 0xFFFF; // result = 1 in Q16
	} else {
		result = SIGMOID_ACT_LUT_uint16[abs_x]; // LUT in Q16
	}
	if (x>0) result = result;
	else     result = (1<<16) - result;
	return result >> 1;
#endif
}


/* Set output features maps initial bias group
 
   	Assumptions:
		These kernels are meant to prepare for convolution or linear operations (both of them can be single or double precision)

		Input		I	Fixed point format i
		Bias		B	Fixed point format b
		Mul Bias	M	Fixed point format m
		Weight		W	Fixed point format w
		Output		O	Fixed point format o

		NormBias:
			Is the correction factor to be applied to bias prior using it in convolution,
			It should be equal to w+i-b, it is a left shift unless NormBias<0, in this case it is a right shift.

		Norm:
			Is the correction factor to produce an output from a convolution, it should be equal to w+i-o and is a right shift, Norm should alwayw be > 0


		Single precision form is called by KOP_SETBIAS
		Double precision form is called by KOP_SETBIAS_DP

	KerParSetBias_fpd		Bias and out are int, output feature maps are evaluated in parallel (one per core)
	KerParSetBias_fp		Bias and out are short, output feature maps are evaluated in parallel (one per core)
	KerParSetBias_fps		Bias and out are byte, output feature maps are evaluated in parallel (one per core)
	KerParSetBias_fpd_fp		Bias is short, out is int, output feature maps are evaluated in parallel (one per core)
	KerParSetBias_fpd_fps		Bias is byte, out is int, output feature maps are evaluated in parallel (one per core)
	KerParSetBias_fp_fps		Bias is short, out is int, output feature maps are evaluated in parallel (one per core)

	KerParSetBias_DP_fp		Bias is short, out is int in 2*Norm format, output feature maps are evaluated in parallel (one per core)
	KerParSetBias_DP_fps		Bias is byte, out is int in 2*Norm format, output feature maps are evaluated in parallel (one per core)

	KerSetBias_fpd			Bias and out are int, a single output feature map is evaluated in parallel on all cores
	KerSetBias_fp			Bias and out are short, a single output feature map is evaluated in parallel on all cores
	KerSetBias_fps			Bias and out are byte, a single output feature map is evaluated in parallel on all cores
	KerSetBias_fpd_fp		Bias is short, out is int, a single output feature map is evaluated in parallel on all cores
	KerSetBias_fpd_fps		Bias is byte, out is int, a single output feature map is evaluated in parallel on all cores
	KerSetBias_fp_fps		Bias is short, out is int, a single output feature map is evaluated in parallel on all cores

	KerSetBias_DP_fp		Bias is short, out is int in 2*Norm format, a single output feature map is evaluated in parallel on all cores
	KerSetBias_DP_fps		Bias is byte, out is int in 2*Norm format, a single output feature map is evaluated in parallel on all cores
*/

/* Set Bias, all outputs evalauted in parallel */

void KerParSetBias_fpd(KerSetBias_fpd_T *Arg)

{
	/* Bias in non DP context: point position b, no need to adjust */
	int * __restrict__ Out = Arg->Out;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int OutFeatures = Arg->OutFeatures;
	int * __restrict__ Bias = Arg->Bias;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) {
		int *LineOut = (int *) (Out+W*H*of);
		// int B = AT_LSHIFT(Bias[of], NormBias);
		int B = Bias[of];
		for (unsigned int i=0; i<((W*H)/2); i++) {
			LineOut[2*i] = B; LineOut[2*i+1] = B;
		}
		LineOut[W*H-1] = B;
	}
	gap_waitbarrier(0);
}

void KerParSetBias_fp(KerSetBias_fp_T *Arg)

{
	/* Bias in non DP context: point position b, no need to adjust */
	short int * __restrict__ Out = Arg->Out;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int OutFeatures = Arg->OutFeatures;
	short int * __restrict__ Bias = Arg->Bias;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) {
		v2s *LineOut = (v2s *) (Out+W*H*of);
		// int B = AT_LSHIFT(Bias[of], NormBias);
		int B = Bias[of];
		v2s Bv = (v2s) {B, B};
		for (unsigned int i=0; i<((W*H)/4); i++) {
			LineOut[2*i] = Bv; LineOut[2*i+1] = Bv;
		}
		for (unsigned int i=(4*((W*H)/4)); i<(W*H); i++) Out[W*H*of+i] = B;
	}
	gap_waitbarrier(0);
}

void KerParSetBias_fps(KerSetBias_fps_T *Arg)

{
	/* Bias in non DP context: point position b, no need to adjust */
	signed char * __restrict__ Out = Arg->Out;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int OutFeatures = Arg->OutFeatures;
	signed char * __restrict__ Bias = Arg->Bias;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) {
		v4s *LineOut = (v4s *) (Out+W*H*of);
		// int B = AT_LSHIFT(Bias[of], NormBias);
		int B = Bias[of];
		v4s Bv = (v4s) {B, B, B, B};
		for (unsigned int i=0; i<((W*H)/8); i++) {
			LineOut[2*i] = Bv; LineOut[2*i+1] = Bv;
		}
		for (unsigned int i=(8*((W*H)/8)); i<(W*H); i++) Out[W*H*of+i] = B;
	}
	gap_waitbarrier(0);
}

void KerParSetBias_fpd_fp(KerSetBias_fpd_fp_T *Arg)

{
	/* Bias in non DP context: point position b. No need to shift this is just a container size change. */
	int * __restrict__ Out = Arg->Out;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int OutFeatures = Arg->OutFeatures;
	short int * __restrict__ Bias = Arg->Bias;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) {
		int *LineOut = (int *) (Out+W*H*of);
		// int B = AT_LSHIFT(Bias[of], NormBias);
		int B = Bias[of];
		for (unsigned int i=0; i<((W*H)/2); i++) {
			LineOut[2*i] = B; LineOut[2*i+1] = B;
		}
		LineOut[W*H-1] = B;
	}
	gap_waitbarrier(0);
}

void KerParSetBias_fpd_fps(KerSetBias_fpd_fps_T *Arg)

{
	/* Bias in non DP context: point position b. No need to shift this is just a container size change. */
	int * __restrict__ Out = Arg->Out;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int OutFeatures = Arg->OutFeatures;
	signed char * __restrict__ Bias = Arg->Bias;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) {
		int *LineOut = (int *) (Out+W*H*of);
		// int B = AT_LSHIFT(Bias[of], NormBias);
		int B = Bias[of];
		for (unsigned int i=0; i<((W*H)/2); i++) {
			LineOut[2*i] = B; LineOut[2*i+1] = B;
		}
		LineOut[W*H-1] = B;
	}
	gap_waitbarrier(0);
}

void KerParSetBias_fp_fps(KerSetBias_fp_fps_T *Arg)

{
	/* Bias in non DP context: point position b. No need to shift this is just a container size change. */
	short int * __restrict__ Out = Arg->Out;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int OutFeatures = Arg->OutFeatures;
	signed char * __restrict__ Bias = Arg->Bias;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) {
		v2s *LineOut = (v2s *) (Out+W*H*of);
		// int B = AT_LSHIFT(Bias[of], NormBias);
		int B = Bias[of];
		v2s Bv = (v2s) {B, B};
		for (unsigned int i=0; i<((W*H)/4); i++) {
			LineOut[2*i] = Bv; LineOut[2*i+1] = Bv;
		}
		for (unsigned int i=(4*((W*H)/4)); i<(W*H); i++) Out[W*H*of+i] = B;
	}
	gap_waitbarrier(0);
}


void KerParSetBias_DP_fp(KerSetBias_fpd_fp_T *Arg)

{
	/* Bias in DP context: point position is i+w, shift by i+w-b. Usually << but could be >> eventhough should have bias in max i+w bits. Report an error? */
	int * __restrict__ Out = Arg->Out;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int OutFeatures = Arg->OutFeatures;
	short int * __restrict__ Bias = Arg->Bias;
	int NormBias = Arg->NormBias;			/* = i+w-b */

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) {
		int *LineOut = (int *) (Out+W*H*of);
		int B = AT_LSHIFT(Bias[of], NormBias);
		for (unsigned int i=0; i<((W*H)/2); i++) {
			LineOut[2*i] = B; LineOut[2*i+1] = B;
		}
		LineOut[W*H-1] = B;
	}
	gap_waitbarrier(0);
}

void KerParSetBias_DP_fps(KerSetBias_fpd_fps_T *Arg)

{
	/* Bias in DP context: point position is i+w, shift by i+w-b. Usually << but could be >> eventhough should have bias in max i+w bits. Report an error? */
	int * __restrict__ Out = Arg->Out;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int OutFeatures = Arg->OutFeatures;
	signed char * __restrict__ Bias = Arg->Bias;
	int NormBias = Arg->NormBias;			/* = i+w-b */

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) {
		int *LineOut = (int *) (Out+W*H*of);
		int B = AT_LSHIFT(Bias[of], NormBias);
		for (unsigned int i=0; i<((W*H)/2); i++) {
			LineOut[2*i] = B; LineOut[2*i+1] = B;
		}
		LineOut[W*H-1] = B;
	}
	gap_waitbarrier(0);
}

/* Set bias, one output evaluated in parallel */

void KerSetBias_fpd(KerSetBias_fpd_T *Arg)

{
	/* Bias in non DP context: point position b, No need to adjust bias in non DP Context */
	int * __restrict__ Out = Arg->Out;
	int W = Arg->W;
	int H = Arg->H;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(W*H);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Minu(First+ChunkCell, W*H);
	int Iter = Max(0, Last-First);
	int i;

	// int Bias = AT_LSHIFT(*Arg->Bias, NormBias);
	int Bias = *Arg->Bias;
	for (i=0; i<(Iter); i++) Out[First+i] = Bias;
	gap_waitbarrier(0);
}

void KerSetBias_fp(KerSetBias_fp_T *Arg)

{
	/* Bias in non DP context: point position b. No need to shift this is just a container size change. */
	short int * __restrict__ Out = Arg->Out;
	int W = Arg->W;
	int H = Arg->H;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(W*H);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Minu(First+ChunkCell, W*H);
	v2s * __restrict__ VectOut = (v2s *) (Out+First);
	int Iter = Max(0, Last-First);
	int i;

	// int Bias = AT_LSHIFT(*Arg->Bias, NormBias);
	int Bias = *Arg->Bias;
	v2s Vb = (v2s) {Bias, Bias};
	for (i=0; i<(Iter/2); i++) VectOut[i] = Vb;
	if (Iter&0x1) Out[Last-1] = Bias;
	gap_waitbarrier(0);
}

void KerSetBias_fps(KerSetBias_fps_T *Arg)

{
	/* Bias in non DP context: point position b. No need to shift this is just a container size change. */
	signed char * __restrict__ Out = Arg->Out;
	int W = Arg->W;
	int H = Arg->H;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(W*H);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Minu(First+ChunkCell, W*H);
	v4s * __restrict__ VectOut = (v4s *) (Out+First);
	int Iter = Max(0, Last-First);
	int i;

	// int Bias = AT_LSHIFT(*Arg->Bias, NormBias);
	int Bias = *Arg->Bias;
	v4s Vb = (v4s) {Bias, Bias, Bias, Bias};
	for (i=0; i<(Iter/4); i++) VectOut[i] = Vb;
	for (i=(Iter/4)*4; i<Iter; i++) Out[i] = Bias;
	gap_waitbarrier(0);
}

void KerSetBias_fpd_fp(KerSetBias_fpd_fp_T *Arg)

{
	/* Bias in non DP context: point position b. No need to shift this is just a container size change. */
	int * __restrict__ Out = Arg->Out;
	int W = Arg->W;
	int H = Arg->H;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(W*H);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Minu(First+ChunkCell, W*H);
	int Iter = Max(0, Last-First);
	int i;

	// int Bias = AT_LSHIFT(*Arg->Bias, NormBias);
	int Bias = *Arg->Bias;
	for (i=0; i<(Iter); i++) Out[First+i] = Bias;
	gap_waitbarrier(0);
}

void KerSetBias_fpd_fps(KerSetBias_fpd_fps_T *Arg)

{
	/* Bias in non DP context: point position b. No need to shift this is just a container size change. */
	int * __restrict__ Out = Arg->Out;
	int W = Arg->W;
	int H = Arg->H;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(W*H);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Minu(First+ChunkCell, W*H);
	int Iter = Max(0, Last-First);
	int i;

	// int Bias = AT_LSHIFT(*Arg->Bias, NormBias);
	int Bias = *Arg->Bias;
	for (i=0; i<(Iter); i++) Out[First+i] = Bias;
	gap_waitbarrier(0);
}

void KerSetBias_fp_fps(KerSetBias_fp_fps_T *Arg)

{
	/* Bias in non DP context: point position b. No need to shift this is just a container size change. */
	short int * __restrict__ Out = Arg->Out;
	int W = Arg->W;
	int H = Arg->H;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(W*H);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Minu(First+ChunkCell, W*H);
	v2s * __restrict__ VectOut = (v2s *) (Out+First);
	int Iter = Max(0, Last-First);
	int i;

	// int Bias = AT_LSHIFT(*Arg->Bias, NormBias);
	int Bias = *Arg->Bias;
	v2s Vb = (v2s) {Bias, Bias};
	for (i=0; i<(Iter/2); i++) VectOut[i] = Vb;
	if (Iter&0x1) Out[Last-1] = Bias;
	gap_waitbarrier(0);
}

void KerSetBias_DP_fp(KerSetBias_fpd_fp_T *Arg)

{
	/* Bias in DP context: point position is i+w, shift by i+w-b. Usually << but could be >> eventhough should have bias in max i+w bits. Report an error? */
	int * __restrict__ Out = Arg->Out;
	int W = Arg->W;
	int H = Arg->H;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(W*H);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Minu(First+ChunkCell, W*H);
	int Iter = Max(0, Last-First);
	int i;

	int Bias = AT_LSHIFT(*Arg->Bias, NormBias);
	for (i=0; i<(Iter); i++) Out[First+i] = Bias;
	gap_waitbarrier(0);
}

void KerSetBias_DP_fps(KerSetBias_fpd_fps_T *Arg)

{
	/* Bias in DP context: point position is i+w, shift by i+w-b. Usually << but could be >> eventhough should have bias in max i+w bits. Report an error? */
	int * __restrict__ Out = Arg->Out;
	int W = Arg->W;
	int H = Arg->H;
	int NormBias = Arg->NormBias;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(W*H);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Minu(First+ChunkCell, W*H);
	int Iter = Max(0, Last-First);
	int i;

	int Bias = AT_LSHIFT(*Arg->Bias, NormBias);
	for (i=0; i<(Iter); i++) Out[First+i] = Bias;
	gap_waitbarrier(0);
}

/* Linear rectification group:

	Feature maps in and out are bytes:
		KerParReLU_fps				Feature maps are evaluated in parallel, each core process one feature map
			|------	KerParDoReLU_fps
		KerReLU_fps				A single feature map is evaluated on all cores in parallel

	Feature maps in and out are short words:
		KerParReLU_fp				Feature maps are evaluated in parallel, each core process one feature map
			|------	KerParDoReLU_fp
		KerReLU_fp				A single feature map is evaluated on all cores in parallel
*/

static void KerParDoReLU_fp(
	short int * __restrict__ In,
	unsigned int W,
	unsigned int H,
	short int * __restrict__ Out,
	int LB,
	int UB
	)

{
	v2s * VectIn  = (v2s *) In;
	v2s * VectOut = (v2s *) Out;

	for (unsigned int i=0; i<((W*H)/4); i++) {
		v2s X = gap_min2(gap_max2(VectIn[2*i], ((v2s) {LB, LB})), ((v2s) {UB, UB}));
		v2s Y = gap_min2(gap_max2(VectIn[2*i+1], ((v2s) {LB, LB})), ((v2s) {UB, UB}));
		VectOut[2*i] = X; VectOut[2*i+1] = Y;
	}
	for (unsigned int i=4*((W*H)/4); i<(W*H); i++) Out[i] = Min(Max(In[i], LB), UB);
}

static void KerParDoReLU_fps(
	signed char * __restrict__ In,
	unsigned int W,
	unsigned int H,
	signed char * __restrict__ Out,
	int LB,
	int UB
	)

{
	v4s * VectIn  = (v4s *) In;
	v4s * VectOut = (v4s *) Out;

	for (unsigned int i=0; i<((W*H)/4); i++) {
		v4s X = gap_min4(gap_max4(VectIn[i], ((v4s) {LB, LB, LB, LB})), ((v4s) {UB, UB, UB, UB}));
		VectOut[i] = X;
	}
	for (unsigned int i=4*((W*H)/4); i<(W*H); i++) Out[i] = Min(Max(In[i], LB), UB);
}


static void KerParDoHswish_fp(
	short int * __restrict__ In,
	unsigned int W,
	unsigned int H,
	short int * __restrict__ Out,
	int Norm
	)

{
	/* Here norm should be = o 
	*/
	int C1 = 3<<(Norm);
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<Norm;
	for (unsigned int i=0; i<((W*H)/2); i++) {
		int Acc0 = In[2*i], Acc1 = In[2*i+1];
		Out[2*i]   = AT_NORM(AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * Acc0, Norm) * C2, 15);
		Out[2*i+1] = AT_NORM(AT_NORM(AT_CLIP_POS(Acc1 + C1, UB) * Acc1, Norm) * C2, 15);
	}
	if ((W*H)&0x1) {
		int Acc = In[W*H-1];
		Out[W*H-1] = AT_NORM(AT_NORM(AT_CLIP_POS(Acc + C1, UB) * Acc, Norm) * C2, 15);
	}
}

static void KerParDoHswish_fps(
	signed char * __restrict__ In,
	unsigned int W,
	unsigned int H,
	signed char * __restrict__ Out,
	int Norm
	)

{
	/* Here norm should be = o,
	*/
	int C1 = 3<<(Norm);
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<Norm;
	for (unsigned int i=0; i<((W*H)/2); i++) {
		int Acc0 = In[2*i], Acc1 = In[2*i+1];
		Out[2*i]   = AT_NORM(AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * Acc0, Norm) * C2, 15);
		Out[2*i+1] = AT_NORM(AT_NORM(AT_CLIP_POS(Acc1 + C1, UB) * Acc1, Norm) * C2, 15);
	}
	if ((W*H)&0x1) {
		int Acc = In[W*H-1];
		Out[W*H-1] = AT_NORM(AT_NORM(AT_CLIP_POS(Acc + C1, UB) * Acc, Norm) * C2, 15);
	}
}

static void KerParDoHsigmoid_fp(
	short int * __restrict__ In,
	unsigned int W,
	unsigned int H,
	short int * __restrict__ Out,
	int Norm
	)

{
	/* Here norm should be = o,
	   Note i=o is assumed here
	   I = Relu6(I + 3) / 6
	*/
	int C1 = 3<<(Norm);
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<Norm;

	for (unsigned int i=0; i<((W*H)/2); i++) {
		int Acc0 = In[2*i], Acc1 = In[2*i+1];
		Out[2*i]   = AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * C2, 15);
		Out[2*i+1] = AT_NORM(AT_CLIP_POS(Acc1 + C1, UB) * C2, 15);
	}
	if ((W*H)&0x1) {
		int Acc = In[W*H-1];
		Out[W*H-1] = AT_NORM(AT_CLIP_POS(Acc + C1, UB) * C2, 15);
	}
}

static void KerParDoHsigmoid_fps(
	signed char * __restrict__ In,
	unsigned int W,
	unsigned int H,
	signed char * __restrict__ Out,
	int Norm
	)

{
	/* Here norm should be = o,
	  I = Relu6(I + 3) / 6
	*/
	int C1 = 3<<(Norm); 
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<Norm;
	for (unsigned int i=0; i<((W*H)/2); i++) {
		int Acc0 = In[2*i], Acc1 = In[2*i+1];
		Out[2*i]   = AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * C2, 15);
		Out[2*i+1] = AT_NORM(AT_CLIP_POS(Acc1 + C1, UB) * C2, 15);
	}
	if ((W*H)&0x1) {
		int Acc = In[W*H-1];
		Out[W*H-1] = AT_NORM(AT_CLIP_POS(Acc + C1, UB) * C2, 15);
	}
}

void KerParReLU_fp(KerReLUPool_fp_T *Arg)

{
	/* LB, UB are assumed to FP o */
	short int * __restrict__ In = Arg->In;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int Wo = W;
	unsigned int Ho = H;
	unsigned int OutFeatures = Arg->OutFeatures;
	short int * __restrict__ Out = Arg->Out;
	int LB = Arg->LB, UB = Arg->UB;

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) KerParDoReLU_fp(In+of*W*H, W, H, Out+of*Wo*Ho, LB, UB);

	gap_waitbarrier(0);
}

void KerParReLU_fps(KerReLUPool_fps_T *Arg)

{
	/* LB, UB are assumed to FP o */
	signed char * __restrict__ In = Arg->In;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int Wo = W;
	unsigned int Ho = H;
	unsigned int OutFeatures = Arg->OutFeatures;
	signed char * __restrict__ Out = Arg->Out;
	int LB = Arg->LB;
	char *UB = (char *) Arg->UB;

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) KerParDoReLU_fps(In+of*W*H, W, H, Out+of*Wo*Ho, LB, UB[of]);

	gap_waitbarrier(0);
}

void KerParHswish_fp(KerReLUPool_fp_T *Arg)

{
	/* Norm = o,
	   WARNING UB contains Norm
	   Note i=o
	*/
	short int * __restrict__ In = Arg->In;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int Wo = W;
	unsigned int Ho = H;
	unsigned int OutFeatures = Arg->OutFeatures;
	short int * __restrict__ Out = Arg->Out;
	int Norm = Arg->UB;

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) KerParDoHswish_fp(In+of*W*H, W, H, Out+of*Wo*Ho, Norm);

	gap_waitbarrier(0);
}

void KerParHswish_fps(KerReLUPool_fps_T *Arg)

{
	/* Norm = o,
	   WARNING UB contains Norm
	   Note: i=o
	*/
	signed char * __restrict__ In = Arg->In;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int Wo = W;
	unsigned int Ho = H;
	unsigned int OutFeatures = Arg->OutFeatures;
	signed char * __restrict__ Out = Arg->Out;
	int Norm = Arg->UB;

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) KerParDoHswish_fps(In+of*W*H, W, H, Out+of*Wo*Ho, Norm);

	gap_waitbarrier(0);
}

void KerParHsigmoid_fp(KerReLUPool_fp_T *Arg)

{
	/* Norm = o, 
	   WARNING UB contains Norm
	   I = Relu6(I + 3) / 6
	*/
	short int * __restrict__ In = Arg->In;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int Wo = W;
	unsigned int Ho = H;
	unsigned int OutFeatures = Arg->OutFeatures;
	short int * __restrict__ Out = Arg->Out;
	int Norm = Arg->UB;

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) KerParDoHsigmoid_fp(In+of*W*H, W, H, Out+of*Wo*Ho, Norm);

	gap_waitbarrier(0);
}

void KerParHsigmoid_fps(KerReLUPool_fps_T *Arg)

{
	/* Norm = o,
	   WARNING UB contains Norm
	   I = Relu6(I + 3) / 6
	*/
	signed char * __restrict__ In = Arg->In;
	unsigned int W = Arg->W;
	unsigned int H = Arg->H;
	unsigned int Wo = W;
	unsigned int Ho = H;
	unsigned int OutFeatures = Arg->OutFeatures;
	signed char * __restrict__ Out = Arg->Out;
	int Norm = Arg->UB;

	unsigned int CoreId = gap_coreid();
	unsigned int Chunk = ChunkSize(OutFeatures);
	unsigned int First = Chunk*CoreId;
	unsigned int Last = Min(First+Chunk, OutFeatures);

	for (unsigned int of=First; of<Last; of++) KerParDoHsigmoid_fps(In+of*W*H, W, H, Out+of*Wo*Ho, Norm);

	gap_waitbarrier(0);
}

void KerReLU_fp(KerReLUPool_fp_T *Arg)

{
	/* LB, UB are in FP o */
	short int * __restrict__ In = Arg->In;
	int W = Arg->W;
	int H = Arg->H;
	short int * __restrict__ Out = Arg->Out;
	int LB = Arg->LB, UB = Arg->UB;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize((W*H)/2);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Minu(First+ChunkCell, (W*H)/2);
	v2s * VectIn  = (v2s *) In;
	v2s * VectOut = (v2s *) Out;
	int i, j;

	for (i=First; i<Last; i++) VectOut[i] = gap_min2(gap_max2(VectIn[i], ((v2s) {LB, LB})), ((v2s) {UB, UB}));
	if ((W*H)&0x1) Out[W*H-1] = Min(Max(In[W*H-1], LB), UB);
	gap_waitbarrier(0);
}

void KerReLU_fps(KerReLUPool_fps_T *Arg)

{
	/* LB, UB are in FP o */
	signed char * __restrict__ In = Arg->In;
	int W = Arg->W;
	int H = Arg->H;
	signed char * __restrict__ Out = Arg->Out;
	int LB = Arg->LB, UB = Arg->UB;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize((W*H)/4);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Minu(First+ChunkCell, (W*H)/4);
	v4s * VectIn  = (v4s *) In;
	v4s * VectOut = (v4s *) Out;
	int i, j;

	for (i=First; i<Last; i++) VectOut[i] = gap_min4(gap_max4(VectIn[i], ((v4s) {LB, LB, LB, LB})), ((v4s) {UB, UB, UB, UB}));
	for (i=((W*H)/4)*4; i<(W*H); i++) Out[i] = Min(Max(In[i], LB), UB);
	gap_waitbarrier(0);
}

/**************************************************************************************************************************************
	Linear Layer followed by optional linear rectification group
**************************************************************************************************************************************/

void KerLinearLayerReLU_fp(KerLinearLayerReLU_fp_T *Arg)

{
	/* Bias in DP context due to reduction: Bias point position is i+w, shift by i+w-b. Usually << but could be >> eventhough should have bias in max i+w bits. Report an error?
	   Norm = i+w-o
	   NormBias = i+w-b
	   LB, UB: o
	*/
	short int * __restrict__ In = Arg->In;
	int InSize = Arg->InSize;
	const short int * __restrict__ Filter = Arg->Filter;
	const short int * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	short int * __restrict__ Out = Arg->Out;
	int OutSize = Arg->OutSize;
	int LB = Arg->LB, UB = Arg->UB;
	static L1_CL_MEM int Reduct[8];

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(InSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, InSize);
	int Iter = Max(0, Last-First);

	for (int i=0; i<OutSize; i++) {
		v2s * __restrict__ Filt = (v2s *) (&Filter[i*InSize+First]);
		v2s * __restrict__ VectIn = (v2s *) (&In[First]);
		int Acc = 0;
		for (int j=0; j<(Iter/2); j++) Acc = gap_sumdotp2(VectIn[j], Filt[j], Acc);
		if (Iter%2) Acc += In[Last-1]*Filter[i*InSize+Last-1];
		Reduct[CoreId] = Acc;
		gap_waitbarrier(0);
		if (CoreId==0) {
			Acc = AT_LSHIFT(Bias[i], NormBias);
			for (int j=0;j<gap_ncore();j++) Acc += Reduct[j];
			Out[i] = Min(Max(AT_NORM(Acc, Norm), LB), UB);
		}
		gap_waitbarrier(0);
	}
       	gap_waitbarrier(0);
}

void KerLinearLayerReLU_fps(KerLinearLayerReLU_fps_T *Arg)

{
	/* Bias in DP context due to reduction: Bias point position is i+w, shift by i+w-b. Usually << but could be >> eventhough should have bias in max i+w bits. Report an error?
	   Norm = i+w-o
	   Bias = i+w-b
	   LB, UB: o
	*/
	signed char * __restrict__ In = Arg->In;
	int InSize = Arg->InSize;
	const signed char * __restrict__ Filter = Arg->Filter;
	const signed char * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	signed char * __restrict__ Out = Arg->Out;
	int OutSize = Arg->OutSize;
	int LB = Arg->LB, UB = Arg->UB;
	static L1_CL_MEM int Reduct[8];

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(InSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, InSize);
	int Iter = Max(0, Last-First);

	for (int i=0; i<OutSize; i++) {
		v4s * __restrict__ Filt = (v4s *) (&Filter[i*InSize+First]);
		v4s * __restrict__ VectIn = (v4s *) (&In[First]);
		int Acc = 0;
		for (int j=0; j<(Iter/4); j++) Acc = gap_sumdotp4(VectIn[j], Filt[j], Acc);
		for (int j=4*(Iter/4); j<Iter; j++) Acc += In[First+j]*Filter[i*InSize+First+j];
		Reduct[CoreId] = Acc;
		gap_waitbarrier(0);
		if (CoreId==0) {
			Acc = AT_LSHIFT(Bias[i], NormBias);
			for (int j=0;j<gap_ncore();j++) Acc += Reduct[j];
			Out[i] = Min(Max(AT_NORM(Acc, Norm), LB), UB);
		}
		gap_waitbarrier(0);
	}
       	gap_waitbarrier(0);
}

void KerLinearLayerReLU_fp_fps_fp(KerLinearLayerReLU_fp_fps_fp_T *Arg)

{
	/* Bias in DP context due to reduction: Bias point position is i+w, shift by i+w-b. Usually << but could be >> eventhough should have bias in max i+w bits. Report an error?
	   Norm = i+w-o
	   Bias = i+w-b
	   LB, UB: o
	*/
	short int * __restrict__ In = Arg->In;
	int InSize = Arg->InSize;
	const signed char * __restrict__ Filter = Arg->Filter;
	const short int * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	short int * __restrict__ Out = Arg->Out;
	int OutSize = Arg->OutSize;
	int LB = Arg->LB, UB = Arg->UB;
	static L1_CL_MEM int Reduct[8];

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(InSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, InSize);
	int Iter = Max(0, Last-First);

	for (int i=0; i<OutSize; i++) {
		v2s * __restrict__ VectIn = (v2s *) (&In[First]);
		int Acc = 0;
		for (int j=0; j<(Iter/2); j++) {
			v2s F = gap_pack2(Filter[i*InSize+First+2*j], Filter[i*InSize+First+2*j+1]);
			Acc = gap_sumdotp2(VectIn[j], F, Acc);
		}
		if (Iter%2) Acc += In[Last-1]*Filter[i*InSize+Last-1];
		Reduct[CoreId] = Acc;
		gap_waitbarrier(0);
		if (CoreId==0) {
			Acc = AT_LSHIFT(Bias[i], NormBias);
			for (int j=0;j<gap_ncore();j++) Acc += Reduct[j];
			Out[i] = Min(Max(AT_NORM(Acc, Norm), LB), UB);
		}
		gap_waitbarrier(0);
	}
       	gap_waitbarrier(0);
}

void KerLinearLayerReLU_fps_fps_fp(KerLinearLayerReLU_fps_fps_fp_T *Arg)

{
	/* Bias in DP context due to reduction: Bias point position is i+w, shift by i+w-b. Usually << but could be >> eventhough should have bias in max i+w bits. Report an error?
	   Norm = i+w-o
	   Bias = i+w-b
	   LB, UB: o
	*/
	signed char * __restrict__ In = Arg->In;
	int InSize = Arg->InSize;
	const signed char * __restrict__ Filter = Arg->Filter;
	const short int * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	short int * __restrict__ Out = Arg->Out;
	int OutSize = Arg->OutSize;
	int LB = Arg->LB, UB = Arg->UB;
	static L1_CL_MEM int Reduct[8];

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(InSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, InSize);
	int Iter = Max(0, Last-First);

	for (int i=0; i<OutSize; i++) {
		v4s * __restrict__ Filt = (v4s *) (&Filter[i*InSize+First]);
		v4s * __restrict__ VectIn = (v4s *) (&In[First]);
		int Acc = 0;
		for (int j=0; j<(Iter/4); j++) Acc = gap_sumdotp4(VectIn[j], Filt[j], Acc);
		for (int j=4*(Iter/4); j<Iter; j++) Acc += In[First+j]*Filter[i*InSize+First+j];
		Reduct[CoreId] = Acc;
		gap_waitbarrier(0);
		if (CoreId==0) {
			Acc = AT_LSHIFT(Bias[i], NormBias);
			for (int j=0;j<gap_ncore();j++) Acc += Reduct[j];
			Out[i] = Min(Max(AT_NORM(Acc, Norm), LB), UB);
		}
		gap_waitbarrier(0);

	}
	gap_waitbarrier(0);
}

void KerLinearLayerReLU_fp_fp_fpd(KerLinearLayerReLU_fp_fp_fpd_T *Arg)

{
	/* Bias in DP context due to reduction: Bias point position is i+w, shift by i+w-b. Usually << but could be >> eventhough should have bias in max i+w bits. Report an error?
	   Norm = i+w-o
	   Bias = i+w-b
	   LB, UB: o
	*/
	short int * __restrict__ In = Arg->In;
	int InSize = Arg->InSize;
	const short int * __restrict__ Filter = Arg->Filter;
	const short int * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	int * __restrict__ Out = Arg->Out;
	int OutSize = Arg->OutSize;
	int LB = Arg->LB, UB = Arg->UB;
	static L1_CL_MEM int Reduct[8];

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(InSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, InSize);
	int Iter = Max(0, Last-First);

	for (int i=0; i<OutSize; i++) {
		v2s * __restrict__ Filt = (v2s *) (&Filter[i*InSize+First]);
		v2s * __restrict__ VectIn = (v2s *) (&In[First]);
		int Acc = 0;
		for (int j=0; j<(Iter/2); j++) Acc = gap_sumdotp2(VectIn[j], Filt[j], Acc);
		if (Iter%2) Acc += In[Last-1]*Filter[i*InSize+Last-1];
		Reduct[CoreId] = Acc;
		gap_waitbarrier(0);
		if (CoreId==0) {
			Acc = AT_LSHIFT(Bias[i], NormBias);
			for (int j=0;j<gap_ncore();j++) Acc += Reduct[j];
			Out[i] = Min(Max(AT_NORM(Acc, Norm), LB), UB);
		}
		gap_waitbarrier(0);
	}
       	gap_waitbarrier(0);
}

/* Double precision Linear Layer */
void KerDPLinearLayer_fp(KerDPLinearLayer_fp_T *Arg)

{
	short int * __restrict__ In = (short int *__restrict__) Arg->In;
	int InSize = Arg->InSize;
	short int * __restrict__ Filter = (short int *__restrict__) Arg->Filter;
	int * __restrict__ Out = (int *__restrict__) Arg->Out;
	int Tile = Arg->Tile;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(InSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, InSize);
	int Iter = Max(0, Last-First);

	v2s * __restrict__ Filt = (v2s *) (&Filter[First]);
	v2s * __restrict__ VectIn = (v2s *) (&In[First]);
	int Acc = 0;
	if (Tile==0) Out[CoreId] = 0;
	for (int i=0; i<(Iter/4); i++) {
		Acc = gap_sumdotp2(VectIn[2*i],   Filt[2*i],   Acc);
		Acc = gap_sumdotp2(VectIn[2*i+1], Filt[2*i+1], Acc);
	}
	for (int i=(Iter/4)*4; i<Iter; i++) Acc += In[First+i]*Filter[First+i];
	Out[CoreId] += Acc;
       	gap_waitbarrier(0);
}

void KerDPLinearLayer_fp_fps(KerDPLinearLayer_fp_fps_T *Arg)

{
	short int * __restrict__ In = (short int *__restrict__) Arg->In;
	int InSize = Arg->InSize;
	signed char * __restrict__ Filter = Arg->Filter;
	int * __restrict__ Out = (int *__restrict__) Arg->Out;
	int Tile = Arg->Tile;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(InSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, InSize);
	int Iter = Max(0, Last-First);

	signed char * __restrict__ F1 = &Filter[First];
	signed char * __restrict__ F2 = &Filter[First+1];
	v2s * __restrict__ VectIn = (v2s *) (&In[First]);
	int Acc = 0;
	if (Tile==0) Out[CoreId] = 0;

	for (int i=0; i<(Iter/2); i++) {
		int f1, f2;
	       	f1 = *F1; F1 += 2; f2 = *F2; F2 += 2;
		Acc = gap_sumdotp2(VectIn[i], gap_pack2(f1, f2), Acc);
	}
	if (Iter&0x1) Acc += In[Last-1]*Filter[Last-1];
	/*
	for (int i=0; i<(Iter/4); i++) {
		v2s F1 = gap_pack2(Filt[4*i], Filt[4*i+1]), F2 = gap_pack2(Filt[4*i+2], Filt[4*i+3]);
		Acc = gap_sumdotp2(VectIn[2*i],   F1, Acc);
		Acc = gap_sumdotp2(VectIn[2*i+1], F2, Acc);
	}
	for (int i=(Iter/4)*4; i<Iter; i++) Acc += In[First+i]*Filter[First+i];
	*/
	Out[CoreId] += Acc;
       	gap_waitbarrier(0);
}

typedef enum {
        KACT_NONE = 0,
        KACT_RELU,
        KACT_RELUN,
        KACT_HSIGMOID,
        KACT_SIGMOID,
        KACT_HSWISH,
        KACT_LEAKY,
} CNN_ActivationOper_T;

void KerDPLinearLayerReduct_fp(KerDPLinearLayerReduct_fp_T *Arg)

{
	/* In is in w+i Norm: w+i-o
	   Bias is in b, NormBias: w+i-b
	   We need to know the value of o to set C1 and C2. o is passed into Arg->UB when needed
	*/
	int C1, C2;
	int * __restrict__ In = (int *__restrict__) Arg->In;
	int Norm = Arg->Norm;
	int NormOut = (int) Arg->UB;
	int Acc = Arg->Bias[0]<<Arg->NormBias;

	for (int i=0; i<gap_ncore(); i++) Acc += In[i];
	switch (Arg->Oper) {
		case KACT_HSIGMOID:
			/* Out = ReLU6(In + 3) / 6 */
			C1 = 3<<NormOut;
			C2 = (1<<15)/6; // 1/6 in Q15
			Acc = AT_NORM(AT_CLIP_POS(AT_NORM(Acc, Norm) + C1, 6<<NormOut) * C2, 15);
			break;
		case KACT_HSWISH:
			/* Out = (In * ReLU6(In + 3)) / 6 */
			C1 = 3<<NormOut;
			C2 = (1<<15)/6; // 1/6 in Q15
			Acc = AT_NORM(AT_NORM(AT_CLIP_POS(AT_NORM(Acc, Norm) + C1, 6<<NormOut) * Acc, NormOut) * C2, 15);
			break;
		case KACT_LEAKY:
			/* Out = (In<0) ? In*Const : In */
			{
				int Input = AT_NORM(Acc, Norm);
				int Neg = (Input<0), Pos = (Input>=0);
				int Input1 = AT_NORM(Input*LEAK_CONSTANT_FIX, LEAK_CONSTANT_FORMAT);
				Acc = gap_clip(Neg*Input1+Pos*Input, 15);
			}
			break;
		default:
			/* No Activation LB=-32768, UB=32767, ReLU: LB=0,UB=32767, ReLUN: LB=0,UB=N<<Norm */
			Acc = Min(Max(AT_NORM(Acc, Norm), Arg->LB), Arg->UB);
	}
	*((short int *)Arg->Out) = Acc;
}

void KerDPLinearLayer_fps(KerDPLinearLayer_fps_T *Arg)

{
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	int InSize = Arg->InSize;
	signed char * __restrict__ Filter = (signed char *__restrict__) Arg->Filter;
	int * __restrict__ Out = (int *__restrict__) Arg->Out;
	int Tile = Arg->Tile;

	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(InSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, InSize);
	int Iter = Max(0, Last-First);

	v4s * __restrict__ Filt = (v4s *) (&Filter[First]);
	v4s * __restrict__ VectIn = (v4s *) (&In[First]);
	int Acc = 0;
	if (Tile==0) Out[CoreId] = 0;
	for (int i=0; i<(Iter/8); i++) {
		Acc = gap_sumdotp4(VectIn[2*i],   Filt[2*i],   Acc);
		Acc = gap_sumdotp4(VectIn[2*i+1], Filt[2*i+1], Acc);
	}
	for (int i=(Iter/8)*8; i<Iter; i++) Acc += In[First+i]*Filter[First+i];
	Out[CoreId] += Acc;
       	gap_waitbarrier(0);
}

void KerDPLinearLayerReduct_fps(KerDPLinearLayerReduct_fps_T *Arg)

{
	/* Norm: w+i-o
	   NormBias: w+i-b
	   We need to know the value of o to set C1 and C2. o is passed in Arg->UB when needed
	   LB, UB: in o format.
	*/
	int C1, C2;
	int * __restrict__ In = (int *__restrict__) Arg->In;
	int Norm = Arg->Norm;
	int NormOut = (int) Arg->UB;
	int Acc = AT_LSHIFT(Arg->Bias[0], Arg->NormBias);
	for (int i=0; i<gap_ncore(); i++) Acc += In[i];
	switch (Arg->Oper) {
		case KACT_HSIGMOID:	/* Here we need point position of Acc, in general w+i */
			/* Out = ReLU6(In + 3) / 6 */
			C1 = 3<<NormOut;
			C2 = (1<<15)/6; // 1/6 in Q15
			Acc = AT_NORM(AT_CLIP_POS(AT_NORM(Acc, Norm) + C1, 6<<NormOut) * C2, 15);
			break;
		case KACT_HSWISH:	/* Here we need point position of Acc */
			/* Out = (In * ReLU6(In + 3)) / 6 */
			C1 = 3<<NormOut;
			C2 = (1<<15)/6; // 1/6 in Q15
			Acc = AT_NORM(AT_NORM(AT_CLIP_POS(AT_NORM(Acc, Norm) + C1, 6<<NormOut) * Acc, NormOut) * C2, 15);
			break;
		case KACT_LEAKY:
			/* Out = (In<0) ? In*Const : In */
			{
				int Input = AT_NORM(Acc, Norm);
				int Neg = (Input<0), Pos = (Input>=0);
				int Input1 = AT_NORM(Input*LEAK_CONSTANT_FIX, LEAK_CONSTANT_FORMAT);
				Acc = gap_clip(Neg*Input1+Pos*Input, 7);
			}
			break;
		default:
			/* No Activation LB=-128, UB=127, ReLU: LB=0,UB=127, ReLUN: LB=0,UB=N<<Norm */
			Acc = Min(Max(AT_NORM(Acc, Norm), Arg->LB), Arg->UB);
	}
	*((signed char *)Arg->Out) = Acc;
}

/* All the KerParLinearLayer family evaluates output channels in parallel
 * For proper operation user needs to make sure that a given output can be evaluated in a single pass
 * This is inforced by the related generator
 */

void KerParLinearLayerReLU_fp(KerLinearLayerReLU_fp_T *Arg)

{
	/*
	 	NormBias: w+i-b
	 	Norm: w+i-o
		LB, UB: in o format
	*/
	short int * __restrict__ In = Arg->In;
	int TotalInSize = Arg->TotalInSize;
	int InSize = Arg->InSize;
	const short int * __restrict__ Filter = Arg->Filter;
	const short int * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	short int * __restrict__ Out = Arg->Out;
	int LB = Arg->LB, UB = Arg->UB;

	int OutSize = Arg->OutSize;
	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(OutSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, OutSize);
	int i,j;
	v2s * __restrict__ VectIn = (v2s *) In;

	/* Don't use this kernel for partial evaluation of an output */
	for (i=First; i<Last; i++) {
		v2s * __restrict__ Filt = (v2s *) (&Filter[i*TotalInSize]);
		int Acc = AT_LSHIFT(Bias[i], NormBias);
		for (j = 0; j<(InSize/(2*2)); j++) {
			Acc = gap_sumdotp2(VectIn[2*j], Filt[2*j], Acc);
			Acc = gap_sumdotp2(VectIn[2*j+1], Filt[2*j+1], Acc);
		}
		if (InSize&0x2) Acc = gap_sumdotp2(VectIn[InSize/2-1], Filt[InSize/2-1], Acc);
		if (InSize&0x1) Acc += In[InSize-1]*Filter[i*InSize+InSize-1];
		Out[i] = Min(Max(AT_NORM(Acc, Norm), LB), UB);
	}
	gap_waitbarrier(0);
}

void KerParLinearLayerHswish_fp(KerLinearLayerReLU_fp_T *Arg)

{
	/* Norm: w+i-o
	   Bias is in b, NormBias: w+i-b
	   We need to know the value of o to set C1 and C2. o is passed in Arg->UB

	   Swish(I) =  (I * ReLU6(I + 3)) / 6
	*/
	short int * __restrict__ In = Arg->In;
	int TotalInSize = Arg->TotalInSize;
	int InSize = Arg->InSize;
	const short int * __restrict__ Filter = Arg->Filter;
	const short int * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	int NormOut = Arg->UB;
	short int * __restrict__ Out = Arg->Out;
	int C1 = 3<<NormOut;
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<NormOut;

	int OutSize = Arg->OutSize;
	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(OutSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, OutSize);
	int i,j;
	v2s * __restrict__ VectIn = (v2s *) In;

	/* Don't use this kernel for partial evaluation of an output */
	for (i=First; i<Last; i++) {
		v2s * __restrict__ Filt = (v2s *) (&Filter[i*TotalInSize]);
		int Acc = AT_LSHIFT(Bias[i], NormBias);
		for (j = 0; j<(InSize/(2*2)); j++) {
			Acc = gap_sumdotp2(VectIn[2*j], Filt[2*j], Acc);
			Acc = gap_sumdotp2(VectIn[2*j+1], Filt[2*j+1], Acc);
		}
		if (InSize&0x2) Acc = gap_sumdotp2(VectIn[InSize/2-1], Filt[InSize/2-1], Acc);
		if (InSize&0x1) Acc += In[InSize-1]*Filter[i*InSize+InSize-1];
		/* Out = (In * ReLU6(In + 3)) / 6 */
		Acc = AT_NORM(Acc, Norm);
		Out[i] = AT_NORM(AT_NORM(AT_CLIP_POS(Acc + C1, UB) * Acc, NormOut) * C2, 15);
	}
	gap_waitbarrier(0);
}

void KerParLinearLayerHsigmoid_fp(KerLinearLayerReLU_fp_T *Arg)

{
	/* Norm: w+i-o
	   Bias is in b, NormBias: w+i-b
	   We need to know the value of o to set C1 and C2. o is passed in Arg->UB

	   I = Relu6(I + 3) / 6
	*/
	short int * __restrict__ In = Arg->In;
	int TotalInSize = Arg->TotalInSize;
	int InSize = Arg->InSize;
	const short int * __restrict__ Filter = Arg->Filter;
	const short int * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	int NormOut = Arg->UB;
	short int * __restrict__ Out = Arg->Out;
	int C1 = 3<<NormOut;
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<NormOut;

	int OutSize = Arg->OutSize;
	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(OutSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, OutSize);
	int i,j;
	v2s * __restrict__ VectIn = (v2s *) In;

	/* Don't use this kernel for partial evaluation of an output */
	for (i=First; i<Last; i++) {
		v2s * __restrict__ Filt = (v2s *) (&Filter[i*TotalInSize]);
		int Acc = Bias[i]<<NormBias;
		for (j = 0; j<(InSize/(2*2)); j++) {
			Acc = gap_sumdotp2(VectIn[2*j], Filt[2*j], Acc);
			Acc = gap_sumdotp2(VectIn[2*j+1], Filt[2*j+1], Acc);
		}
		if (InSize&0x2) Acc = gap_sumdotp2(VectIn[InSize/2-1], Filt[InSize/2-1], Acc);
		if (InSize&0x1) Acc += In[InSize-1]*Filter[i*InSize+InSize-1];

		Acc = gap_max(0, gap_min(AT_NORM(Acc, Norm)+C1, UB));
		Out[i] = AT_NORM(Acc*C2, 15);
	}
	gap_waitbarrier(0);
}

void KerParLinearLayerReLU_fps(KerLinearLayerReLU_fps_T *Arg)

{
	/* Norm: w+i-o
	   Bias is in b, NormBias: w+i-b
	   LB, UB: in o format
	*/
	signed char * __restrict__ In = Arg->In;
	int TotalInSize = Arg->TotalInSize;
	int InSize = Arg->InSize;
	const signed char * __restrict__ Filter = Arg->Filter;
	const signed char * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	signed char * __restrict__ Out = Arg->Out;
	int LB = Arg->LB, UB = Arg->UB;

	int OutSize = Arg->OutSize;
	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(OutSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, OutSize);
	int i,j;
	v4s * __restrict__ VectIn = (v4s *) In;

	/* Don't use this kernel for partial evaluation of an output */
	for (i=First; i<Last; i++) {
		v4s * __restrict__ Filt = (v4s *) (&Filter[i*TotalInSize]);
		int Acc = Bias[i]<<NormBias;
		for (j=0; j<(InSize/(4*2)); j++) {
			Acc = gap_sumdotp4(VectIn[2*j], Filt[2*j], Acc);
			Acc = gap_sumdotp4(VectIn[2*j+1], Filt[2*j+1], Acc);
		}
		if (InSize&0x4) Acc = gap_sumdotp4(VectIn[InSize/4-1], Filt[InSize/4-1], Acc);
		for (j=4*(InSize/4); j<InSize; j++) Acc += In[j]*Filter[i*TotalInSize+j];
		Out[i] = Min(Max(AT_NORM(Acc, Norm), LB), UB);
	}
	gap_waitbarrier(0);
}

void KerParLinearLayerHswish_fps(KerLinearLayerReLU_fps_T *Arg)

{
	/* Norm: w+i-o
	   Bias is in b, NormBias: w+i-b
	   We need to know the value of o to set C1 and C2. o passed in Arg->UB
	*/
	signed char * __restrict__ In = Arg->In;
	int TotalInSize = Arg->TotalInSize;
	int InSize = Arg->InSize;
	const signed char * __restrict__ Filter = Arg->Filter;
	const signed char * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormOut = Arg->UB;
	int NormBias = Arg->NormBias;
	signed char * __restrict__ Out = Arg->Out;
	int C1 = 3<<NormOut;
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<NormOut;

	int OutSize = Arg->OutSize;
	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(OutSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, OutSize);
	int i,j;
	v4s * __restrict__ VectIn = (v4s *) In;

	/* Don't use this kernel for partial evaluation of an output */
	for (i=First; i<Last; i++) {
		v4s * __restrict__ Filt = (v4s *) (&Filter[i*TotalInSize]);
		int Acc = Bias[i]<<NormBias;
		for (j=0; j<(InSize/(4*2)); j++) {
			Acc = gap_sumdotp4(VectIn[2*j], Filt[2*j], Acc);
			Acc = gap_sumdotp4(VectIn[2*j+1], Filt[2*j+1], Acc);
		}
		if (InSize&0x4) Acc = gap_sumdotp4(VectIn[InSize/4-1], Filt[InSize/4-1], Acc);
		for (j=4*(InSize/4); j<InSize; j++) Acc += In[j]*Filter[i*TotalInSize+j];
		Acc = AT_NORM(Acc, Norm);
		Out[i] = AT_NORM(AT_NORM(AT_CLIP_POS(Acc + C1, UB) * Acc, NormOut) * C2, 15);
	}
	gap_waitbarrier(0);
}

void KerParLinearLayerHsigmoid_fps(KerLinearLayerReLU_fps_T *Arg)

{
	/* Norm: w+i-o
	   Bias is in b, NormBias: w+i-b
	   We need to know the value of o to set C1 and C2. o passed in Arg->UB
	   I = Relu6(I + 3) / 6
	*/
	signed char * __restrict__ In = Arg->In;
	int TotalInSize = Arg->TotalInSize;
	int InSize = Arg->InSize;
	const signed char * __restrict__ Filter = Arg->Filter;
	const signed char * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	int NormOut = Arg->UB;
	signed char * __restrict__ Out = Arg->Out;
	int C1 = 3<<NormOut;
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<NormOut;

	int OutSize = Arg->OutSize;
	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(OutSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, OutSize);
	int i,j;
	v4s * __restrict__ VectIn = (v4s *) In;

	/* Don't use this kernel for partial evaluation of an output */
	for (i=First; i<Last; i++) {
		v4s * __restrict__ Filt = (v4s *) (&Filter[i*TotalInSize]);
		int Acc = Bias[i]<<NormBias;
		for (j=0; j<(InSize/(4*2)); j++) {
			Acc = gap_sumdotp4(VectIn[2*j], Filt[2*j], Acc);
			Acc = gap_sumdotp4(VectIn[2*j+1], Filt[2*j+1], Acc);
		}
		if (InSize&0x4) Acc = gap_sumdotp4(VectIn[InSize/4-1], Filt[InSize/4-1], Acc);
		for (j=4*(InSize/4); j<InSize; j++) Acc += In[j]*Filter[i*TotalInSize+j];
	   	/* Out = Relu6(In + 3) / 6 */
		Out[i] = AT_NORM(AT_CLIP_POS(AT_NORM(Acc, Norm) + C1, UB) * C2, 15);
	}
	gap_waitbarrier(0);
}

void KerParLinearLayerReLU_fps_fps_fp(KerLinearLayerReLU_fps_fps_fp_T *Arg)

{
	/* Norm: w+i-o
	   Bias is in b, NormBias: w+i-b
	   LB, UB: in o format
	*/
	signed char * __restrict__ In = Arg->In;
	int TotalInSize = Arg->TotalInSize;
	int InSize = Arg->InSize;
	const signed char * __restrict__ Filter = Arg->Filter;
	short int * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	short int * __restrict__ Out = Arg->Out;
	int LB = Arg->LB, UB = Arg->UB;

	int OutSize = Arg->OutSize;
	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(OutSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, OutSize);
	int i,j;
	v4s * __restrict__ VectIn = (v4s *) In;

	for (i=First; i<Last; i++) {
		int Acc = Bias[i]<<NormBias;
		v4s * __restrict__ VectFilter = (v4s *) (&Filter[i*TotalInSize]);
		for (j=0; j<(InSize/(4*2)); j++) {
			Acc = gap_sumdotp4(VectIn[2*j], VectFilter[2*j], Acc);
			Acc = gap_sumdotp4(VectIn[2*j+1], VectFilter[2*j+1], Acc);
		}
		if (InSize&0x4) Acc = gap_sumdotp4(VectIn[InSize/4-1], VectFilter[InSize/4-1], Acc);
		for (j=((InSize/4)*4); j<InSize; j++) Acc += In[j]*Filter[j];
		Out[i] = Min(Max(AT_NORM(Acc, Norm), LB), UB);
	}
	gap_waitbarrier(0);
}

void KerParLinearLayerReLU_fps_fps_fpd(KerLinearLayerReLU_fps_fps_fpd_T *Arg)

{
	/* Norm: w+i-o
	   Bias is in b, NormBias: w+i-b
	   LB, UB: in o format
	*/
	signed char * __restrict__ In = Arg->In;
	int TotalInSize = Arg->TotalInSize;
	int InSize = Arg->InSize;
	const signed char * __restrict__ Filter = Arg->Filter;
	const signed char * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	int * __restrict__ Out = Arg->Out;
	int LB = Arg->LB, UB = Arg->UB;

	int OutSize = Arg->OutSize;
	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(OutSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, OutSize);
	int i,j;
	v4s * __restrict__ VectIn = (v4s *) In;

	for (i=First; i<Last; i++) {
		int Acc = Bias[i]<<NormBias;
		v4s * __restrict__ VectFilter = (v4s *) (&Filter[i*TotalInSize]);
		for (j=0; j<(InSize/(4*2)); j++) {
			Acc = gap_sumdotp4(VectIn[2*j], VectFilter[2*j], Acc);
			Acc = gap_sumdotp4(VectIn[2*j+1], VectFilter[2*j+1], Acc);
		}
		if (InSize&0x4) Acc = gap_sumdotp4(VectIn[InSize/4-1], VectFilter[InSize/4-1], Acc);
		for (j=((InSize/4)*4); j<InSize; j++) Acc += In[j]*Filter[j];
		Out[i] = Min(Max(AT_NORM(Acc, Norm), LB), UB);
	}
	gap_waitbarrier(0);
}

void KerParLinearLayerReLU_fp_fps_fp(KerLinearLayerReLU_fp_fps_fp_T *Arg)

{
	/* Norm: w+i-o
	   Bias is in b, NormBias: w+i-b
	   LB, UB: in o format
	*/
	short int * __restrict__ In = Arg->In;
	int TotalInSize = Arg->TotalInSize;
	int InSize = Arg->InSize;
	const signed char * __restrict__ Filter = Arg->Filter;
	const short int * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	short int * __restrict__ Out = Arg->Out;
	int LB = Arg->LB, UB = Arg->UB;

	int OutSize = Arg->OutSize;
	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(OutSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, OutSize);
	int i,j;
	v2s * __restrict__ VectIn = (v2s *) In;

	for (i=First; i<Last; i++) {
		int Acc = Bias[i]<<NormBias;
		for (j=0; j<(InSize/2); j++) {
			v2s F = gap_pack2(Filter[i*TotalInSize+2*j], Filter[i*TotalInSize+2*j+1]);
			Acc = gap_sumdotp2(VectIn[j], F, Acc);
		}
		if (InSize%2) Acc += In[InSize-1]*Filter[i*InSize+InSize-1];
		Out[i] = Min(Max(AT_NORM(Acc, Norm), LB), UB);
	}
	gap_waitbarrier(0);
}

void KerParLinearLayerReLU_fp_fp_fpd(KerLinearLayerReLU_fp_fp_fpd_T *Arg)

{
	/* Norm: w+i-o
	   Bias is in b, NormBias: w+i-b
	   LB, UB: in o format
	*/
	short int * __restrict__ In = Arg->In;
	int TotalInSize = Arg->TotalInSize;
	int InSize = Arg->InSize;
	const short int * __restrict__ Filter = Arg->Filter;
	const short int * __restrict__ Bias = Arg->Bias;
	unsigned int Norm = Arg->Norm;
	int NormBias = Arg->NormBias;
	int * __restrict__ Out = Arg->Out;
	int LB = Arg->LB, UB = Arg->UB;

	int OutSize = Arg->OutSize;
	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(OutSize);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, OutSize);
	int i,j;
	v2s * __restrict__ VectIn = (v2s *) In;

	for (i=First; i<Last; i++) {
		v2s * __restrict__ Filt = (v2s *) (&Filter[i*TotalInSize]);
		int Acc = Bias[i]<<NormBias;
		for (j = 0; j<(InSize/2); j++) Acc = gap_sumdotp2(VectIn[j], Filt[j], Acc);
		if (InSize%2) Acc += In[InSize-1]*Filter[i*InSize+InSize-1];
		Out[i] = Min(Max(AT_NORM(Acc, Norm), LB), UB);
	}
	gap_waitbarrier(0);
}

/**************************************************************************************************************************************
	Double precision to single precision reduction
**************************************************************************************************************************************/

void KerDP_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	   LB, UB: in o format
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	int LB = Arg->LB, UB = Arg->UB;
	unsigned int Norm = Arg->Norm;
	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	int * __restrict__ I = &In[First];
	v2s * __restrict__ O = (v2s *)(&Out[First]);

	for (i=0; i<(Size/2); i++) {
		int Acc0 = I[2*i], Acc1 = I[2*i+1];
		Acc0 = Min(Max(AT_NORM(Acc0, Norm), LB), UB); Acc1 = Min(Max(AT_NORM(Acc1, Norm), LB), UB);
		O[i] = gap_pack2(Acc0, Acc1);
	}
	if (Size&0x1) Out[Last-1] = Min(Max(AT_NORM(In[Last-1], Norm), LB), UB);
	gap_waitbarrier(0);
}

void KerDPMulBiasScalar_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	   NormBias: m
	   LB, UB: in o format
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	unsigned int Norm = Arg->Norm;
	unsigned int NormBias = Arg->NormBias;
	int LB = Arg->LB, UB = Arg->UB;
	int M = *Arg->MulBias;
	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	int * __restrict__ I = &In[First];
	v2s * __restrict__ O = (v2s *)(&Out[First]);

	for (i=0; i<(Size/2); i++) {
		int Acc0 = I[2*i], Acc1 = I[2*i+1];
		Acc0 = Min(Max(AT_NORM(AT_NORM(Acc0, Norm)*M, NormBias), LB), UB);
		Acc1 = Min(Max(AT_NORM(AT_NORM(Acc1, Norm)*M, NormBias), LB), UB);
		O[i] = gap_pack2(Acc0, Acc1);
	}
	if (Size&0x1) Out[Last-1] = Min(Max(AT_NORM(AT_NORM(In[Last-1], Norm)*M, NormBias), LB), UB);
	gap_waitbarrier(0);
}

void KerDPMulBias_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	   NormBias: m
	   LB, UB: in o format
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->InFeatures;
	int Size = Arg->W*Arg->H;
	unsigned int Norm = Arg->Norm;
	unsigned int NormBias = Arg->NormBias;
	int LB = Arg->LB, UB = Arg->UB;
	short int * __restrict__ MB = Arg->MulBias;
	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);

	for (j=First; j<Last; j++) {
		int * __restrict__ I = &In[Size*j];
		v2s * __restrict__ O = (v2s *)(&Out[Size*j]);
		int M = MB[j];
		for (i=0; i<(Size/2); i++) {
			int Acc0 = I[2*i], Acc1 = I[2*i+1];
			Acc0 = Min(Max(AT_NORM(AT_NORM(Acc0, Norm)*M, NormBias), LB), UB);
			Acc1 = Min(Max(AT_NORM(AT_NORM(Acc1, Norm)*M, NormBias), LB), UB);
			O[i] = gap_pack2(Acc0, Acc1);
		}
		if (Size&0x1) Out[Size*j+Size-1] = Min(Max(AT_NORM(AT_NORM(I[Size-1], Norm)*M, NormBias), LB), UB);
	}
	gap_waitbarrier(0);
}

void KerDP_hswish_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	   We need to know o to build C1,C2, o passed in Arg->UB
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	unsigned int Norm = Arg->Norm;
	int NormOut = Arg->UB;
	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	int * __restrict__ I = &In[First];
	v2s * __restrict__ O = (v2s *)(&Out[First]);

	int C1 = 3<<NormOut;
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<NormOut;

	for (i=0; i<(Size/2); i++) {
		int Acc0 = AT_NORM(I[2*i], Norm), Acc1 = AT_NORM(I[2*i+1], Norm);
		Acc0 = AT_NORM(AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * Acc0, NormOut) * C2, 15);
		Acc1 = AT_NORM(AT_NORM(AT_CLIP_POS(Acc1 + C1, UB) * Acc1, NormOut) * C2, 15);
		O[i] = gap_pack2(Acc0, Acc1);
	}
	if (Size&0x1) {
		int Acc0 = AT_NORM(In[Last-1], Norm);
		Out[Last-1] = AT_NORM(AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * Acc0, NormOut) * C2, 15);
	}
	gap_waitbarrier(0);
}

void KerDP_sigmoid_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
           o must be Q15
	   o passed in Arg->UB
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->InFeatures*Arg->W*Arg->H;
        int Norm12 = Arg->Norm + 3;

	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	int * __restrict__ I = &In[First];
	v2s * __restrict__ O = (v2s *)(&Out[First]);

	for (i=0; i<(Size/2); i++) {
		int Acc0 = AT_NORM(I[2*i], Norm12), Acc1 = AT_NORM(I[2*i+1], Norm12);
		Acc0 = AT_CLIP(SigmoidLUT(Acc0), 15);
		Acc1 = AT_CLIP(SigmoidLUT(Acc1), 15);
		O[i] = (v2s) gap_pack2((short int)Acc0, (short int)Acc1);
	}
	if (Size&0x1) {
		int Acc0 = AT_NORM(In[Last-1], Norm12);
		Out[Last-1] = (short int) AT_CLIP(SigmoidLUT(Acc0), 15);
	}
	gap_waitbarrier(0);
}

void KerDP_hsigmoid_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	   We need to know o to build C1, C2, o passed in Arg->UB
	   I = Relu6(I + 3) / 6
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->InFeatures*Arg->W*Arg->H;
	int Norm = Arg->Norm;
	int NormOut = Arg->UB;
	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	int * __restrict__ I = &In[First];
	v2s * __restrict__ O = (v2s *)(&Out[First]);

	int C1 = 3<<NormOut;
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<NormOut;

	for (i=0; i<(Size/2); i++) {
		int Acc0 = AT_NORM(I[2*i], Norm), Acc1 = AT_NORM(I[2*i+1], Norm);
		Acc0 = AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * C2, 15);
		Acc1 = AT_NORM(AT_CLIP_POS(Acc1 + C1, UB) * C2, 15);
		O[i] = gap_pack2(Acc0, Acc1);
	}
	if (Size&0x1) {
		int Acc0 = AT_NORM(In[Last-1], Norm);
		Out[Last-1] = AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * C2, 15);
	}
	gap_waitbarrier(0);
}

void KerDP_leakyrelu_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->InFeatures*Arg->W*Arg->H;
	int Norm = Arg->Norm;
	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	int * __restrict__ I = &In[First];
	short int * __restrict__ Os = (short int *)(&Out[First]);

	for (i=0; i<Size; i++) {
		int Input = AT_NORM(I[i], Norm);
		int Neg = (Input<0);
		int Pos = (Input>=0);
		int Input1 = AT_NORM(Input*LEAK_CONSTANT_FIX, LEAK_CONSTANT_FORMAT);
		int Acc0 = gap_clip(Neg*Input1+Pos*Input, 15);
		Os[i] = Acc0;
	}
	gap_waitbarrier(0);
}

#define B_CLR(x, bits)  ((x)&(~((1<<(bits))-1)))
static void KerReductIO_Compact_fps(char *__restrict__ To, char *__restrict__ From, int Size, int TotalSize)

{
        unsigned int CoreId = gap_coreid(), Chunk = ChunkSize(Size), First = Chunk*CoreId, Last = Min(First+Chunk, Size);
        unsigned int Iter = Max(0, Last-First);

        for (int i=Size; i<TotalSize; i+=Size) {
                From += Size*4; To += Size;

                int *pFrom = (int *) (From+First), *pTo = (int *) (To+First);
                for (int j=0; j<Iter/8; j++) {
                        int V0 = pFrom[2*j], V1 = pFrom[2*j+1];
                        pTo[2*j] = V0; pTo[2*j+1] = V1;
                }
                if (Iter & 0x4) *((int *) (To + First + B_CLR(Iter, 3))) = *((int *) (From + First + B_CLR(Iter, 3)));
                if (Iter & 0x2) *((short int *) (To + First + B_CLR(Iter, 2))) = *((short int *) (From + First + B_CLR(Iter, 2)));
                if (Iter & 0x1) *((signed char *) (To + First + Iter - 1)) = *((signed char *) (From + First + Iter - 1));
                gap_waitbarrier(0);
        }
}

static void KerReductIO_Compact_fp(short int *__restrict__ To, short int *__restrict__ From, int Size, int TotalSize)

{
        unsigned int CoreId = gap_coreid(), Chunk = ChunkSize(Size), First = Chunk*CoreId, Last = Min(First+Chunk, Size);
        unsigned int Iter = Max(0, Last-First);

        for (int i=Size; i<TotalSize; i+=Size) {
                From += Size*2; To += Size;

                int *pFrom = (int *) (From+First), *pTo = (int *) (To+First);
                for (int j=0; j<Iter/4; j++) {
                        int V0 = pFrom[2*j], V1 = pFrom[2*j+1];
                        pTo[2*j] = V0; pTo[2*j+1] = V1;
                }
                if (Iter & 0x2) *((int *) (To + First + B_CLR(Iter, 3))) = *((int *) (From + First + B_CLR(Iter, 3)));
                if (Iter & 0x1) *((short int *) (To + First + B_CLR(Iter, 2))) = *((short int *) (From + First + B_CLR(Iter, 2)));
                //if (Iter & 0x1) *((signed char *) (To + First + Iter - 1)) = *((signed char *) (From + First + Iter - 1));
                gap_waitbarrier(0);
        }
}

void KerDP_IO_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	   NormBias: m
	   LB, UB: in o format
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	int LB = Arg->LB, UB = Arg->UB;
	unsigned int Norm = Arg->Norm;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	int *I = &In[First];
	short int *Is = (short int *)In + First;
	v2s *O = (v2s *)(&In[First]);

	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */
	for (i=0; i<(Size/2); i++) {
		int Acc0 = I[2*i], Acc1 = I[2*i+1];
		Acc0 = Min(Max(AT_NORM(Acc0, Norm), LB), UB); Acc1 = Min(Max(AT_NORM(Acc1, Norm), LB), UB);
		O[i] = gap_pack2(Acc0, Acc1);
	}
	if (Size&0x1) ((short int *)I)[Size-1] = Min(Max(AT_NORM(I[Size-1], Norm), LB), UB);
	gap_waitbarrier(0);
	/* Now this is the reduction phase */

	KerReductIO_Compact_fp((short int *__restrict__)In, (short int *__restrict__)In, ChunkCell, S);
}

void KerDP_IO_hsigmoid_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	   We need to know o to build C1, C2, o is passed in Arg->UB
	   I = Relu6(I + 3) / 6
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	unsigned int Norm = Arg->Norm;
	int NormOut = Arg->UB;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	int *I = &In[First];
	v2s *O = (v2s *)(&In[First]);

	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */

	int C1 = 3<<NormOut;
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<NormOut;

	for (i=0; i<(Size/2); i++) {
		int Acc0 = AT_NORM(I[2*i], Norm), Acc1 = AT_NORM(I[2*i+1], Norm);
		Acc0 = AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * C2, 15);
		Acc1 = AT_NORM(AT_CLIP_POS(Acc1 + C1, UB) * C2, 15);
		O[i] = gap_pack2(Acc0, Acc1);
	}
	if (Size&0x1) {
		int Acc0 = AT_NORM(In[Last-1], Norm);
		Out[Last-1] = AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * C2, 15);
	}
	gap_waitbarrier(0);
	/* Now this is the reduction phase */

	KerReductIO_Compact_fp((short int *__restrict__)In, (short int *__restrict__)In, ChunkCell, S);
}

void KerDP_IO_sigmoid_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	   Out norm always indicates norm to Q15
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	unsigned int Norm12 = Arg->Norm + 3;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	int *I = &In[First];
	v2s *O = (v2s *)(&In[First]);

	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */

	for (i=0; i<(Size/2); i++) {
		int Acc0 = AT_NORM(I[2*i], Norm12), Acc1 = AT_NORM(I[2*i+1], Norm12);
		Acc0 = AT_CLIP(SigmoidLUT(Acc0), 15);
		Acc1 = AT_CLIP(SigmoidLUT(Acc1), 15);
		O[i] = (v2s) gap_pack2(Acc0, Acc1);
	}
	if (Size&0x1) {
		int Acc0 = AT_NORM(In[Last-1], Norm12);
		Out[Last-1] = AT_CLIP(SigmoidLUT(Acc0), 15);
	}
	gap_waitbarrier(0);
	/* Now this is the reduction phase */

	KerReductIO_Compact_fp((short int *__restrict__)In, (short int *__restrict__)In, ChunkCell, S);
#if 0
	U = gap_ncore()/2; Log2Core = gap_fl1(gap_ncore()); A = 2; B = 1;
	for (k=0; k<Log2Core; k++) {
		if (CoreId<U) {
			short int *__restrict__ OOs = ((short int *)In+(A*CoreId+B)*ChunkCell);
			short int *__restrict__ IIs = ((short int *)In+(2*(A*CoreId+B))*ChunkCell);
			int *__restrict__ II = (int *) IIs;
			int *__restrict__ OO = (int *) OOs;
			for (i=0;i<(Size/4);i++) {
				int V0 = II[2*i], V1 = II[2*i+1];
				OO[2*i] = V0; OO[2*i+1] = V1;
			}
			for (i=((Size/4)*4); i<Size; i++) OOs[i] = IIs[i];
		}
		U = U/2; A = A*2; B = B*2;
		gap_waitbarrier(0);
	}
#endif
}

void KerDP_IO_hswish_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	   We need to know o to build C1, o is passed in Arg->UB
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	unsigned int Norm = Arg->Norm;
	int NormOut = Arg->UB;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	int *I = &In[First];
	v2s *O = (v2s *)(&In[First]);

	int C1 = 3<<NormOut;
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<NormOut;

	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */

	for (i=0; i<(Size/2); i++) {
		int Acc0 = AT_NORM(I[2*i], Norm), Acc1 = AT_NORM(I[2*i+1], Norm);
		Acc0 = AT_NORM(AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * Acc0, NormOut) * C2, 15);
		Acc1 = AT_NORM(AT_NORM(AT_CLIP_POS(Acc1 + C1, UB) * Acc1, NormOut) * C2, 15);
		O[i] = gap_pack2(Acc0, Acc1);
	}
	if (Size&0x1) {
		int Acc0 = AT_NORM(In[Last-1], Norm);
		Out[Last-1] = AT_NORM(AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * Acc0, NormOut) * C2, 15);
	}
	gap_waitbarrier(0);
	/* Now this is the reduction phase */

	KerReductIO_Compact_fp((short int *__restrict__)In, (short int *__restrict__)In, ChunkCell, S);
}

void KerDP_IO_leakyrelu_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	int UB = Arg->UB;
	unsigned int Norm = Arg->Norm;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	int *I = &In[First];
	short int *Os = (short int *)(&In[First]);

	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */

	for (i=0; i<Size; i++) {
		int Input = AT_NORM(I[i], Norm);
		int Neg = (Input<0);
		int Pos = (Input>=0);
		int Input1 = AT_NORM(Input*LEAK_CONSTANT_FIX, LEAK_CONSTANT_FORMAT);
		int Acc0 = gap_clip(Neg*Input1+Pos*Input, 15);
		Os[i] = Acc0;
	}
	gap_waitbarrier(0);
	/* Now this is the reduction phase */

	KerReductIO_Compact_fp((short int *__restrict__)In, (short int *__restrict__)In, ChunkCell, S);
}

void KerDPMulBiasScalar_IO_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	   NormBias: m
	   LB, UB: in o format
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	int LB = Arg->LB, UB = Arg->UB;
	int M = *Arg->MulBias;
	unsigned int Norm = Arg->Norm;
	unsigned int NormBias = Arg->NormBias;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	int *I = &In[First];
	short int *Is = (short int *)In + First;
	v2s *O = (v2s *)(&In[First]);

	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */
	for (i=0; i<(Size/2); i++) {
		int Acc0 = I[2*i], Acc1 = I[2*i+1];
		Acc0 = Min(Max(AT_NORM(AT_NORM(Acc0, Norm)*M, NormBias), LB), UB);
		Acc1 = Min(Max(AT_NORM(AT_NORM(Acc1, Norm)*M, NormBias), LB), UB);
		O[i] = gap_pack2(Acc0, Acc1);
	}
	if (Size&0x1) ((short int *)I)[Size-1] = Min(Max(AT_NORM(AT_NORM(I[Size-1], Norm)*M, NormBias), LB), UB);
	gap_waitbarrier(0);
	/* Now this is the reduction phase */

	KerReductIO_Compact_fp((short int *__restrict__)In, (short int *__restrict__)In, ChunkCell, S);
}

void KerDPMulBias_IO_fp(KerDP_fp_T *Arg)

{
	/* Norm: w+i-o
	   NormBias: m
	   LB, UB: in o format
	*/
	int * __restrict__ In = Arg->In;
	short int * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H;
	int Feat = Arg->InFeatures;
	int LB = Arg->LB, UB = Arg->UB;
	short int * __restrict__ MB = Arg->MulBias;
	unsigned int Norm = Arg->Norm;
	unsigned int NormBias = Arg->NormBias;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(Feat);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, Feat);
	int Size = S*Max(0, Last-First);

	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */
	for (int f=First; f<Last; f++) {
		int *I = &In[S*f];
		v2s *O = (v2s *)(&In[S*f]);
		int M = MB[f];
		for (i=0; i<(S/2); i++) {
			int Acc0 = I[2*i], Acc1 = I[2*i+1];
			Acc0 = Min(Max(AT_NORM(AT_NORM(Acc0, Norm)*M, NormBias), LB), UB);
			Acc1 = Min(Max(AT_NORM(AT_NORM(Acc1, Norm)*M, NormBias), LB), UB);
			O[i] = gap_pack2(Acc0, Acc1);
		}
		if (S&0x1) ((short int *)I)[S-1] = Min(Max(AT_NORM(AT_NORM(I[S-1], Norm)*M, NormBias), LB), UB);
	}
	gap_waitbarrier(0);
	/* Now this is the reduction phase */

	KerReductIO_Compact_fps((signed char *__restrict__)In, (signed char *__restrict__)In, S*ChunkCell, S*Feat);
}

void KerDP_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   LB, UB: in o format
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	int LB = Arg->LB, UB = Arg->UB;
	unsigned int Norm = Arg->Norm;
	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	DP_fps_T * __restrict__ I = &In[First];
	v4s * __restrict__ O = (v4s *)(&Out[First]);
	signed char *__restrict__ Os = &Out[First];

	for (i=0; i<(Size/4); i++) {
		int Acc0 = I[4*i  ], Acc1 = I[4*i+1], Acc2 = I[4*i+2], Acc3 = I[4*i+3];
		Acc0 = Min(Max(AT_NORM(Acc0, Norm), LB), UB); Acc1 = Min(Max(AT_NORM(Acc1, Norm), LB), UB);
		Acc2 = Min(Max(AT_NORM(Acc2, Norm), LB), UB); Acc3 = Min(Max(AT_NORM(Acc3, Norm), LB), UB);
		O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
	}
	for (i=((Size/4)*4); i<Size; i++) Os[i] = Min(Max(AT_NORM(I[i], Norm), LB), UB);

	gap_waitbarrier(0);
}

void KerDP_hswish_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   We need to know o to build C1, o is passed in Arg->UB
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	unsigned int Norm = Arg->Norm;
	int NormOut = Arg->UB;
	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	DP_fps_T * __restrict__ I = &In[First];
	v4s * __restrict__ O = (v4s *)(&Out[First]);
	signed char *__restrict__ Os = &Out[First];

	int C1 = 3<<NormOut;
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<NormOut;

	for (i=0; i<(Size/4); i++) {
		int Acc0 = AT_NORM(I[4*i], Norm), Acc1 = AT_NORM(I[4*i+1], Norm), Acc2 = AT_NORM(I[4*i+2], Norm), Acc3 = AT_NORM(I[4*i+3], Norm);
		Acc0 = AT_NORM(AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * Acc0, NormOut) * C2, 15);
		Acc1 = AT_NORM(AT_NORM(AT_CLIP_POS(Acc1 + C1, UB) * Acc1, NormOut) * C2, 15);
		Acc2 = AT_NORM(AT_NORM(AT_CLIP_POS(Acc2 + C1, UB) * Acc2, NormOut) * C2, 15);
		Acc3 = AT_NORM(AT_NORM(AT_CLIP_POS(Acc3 + C1, UB) * Acc3, NormOut) * C2, 15);
		O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
	}
	for (i=((Size/4)*4); i<Size; i++) {
		int Acc0 = AT_NORM(I[i], Norm);
		Os[i] = AT_NORM(AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * Acc0, NormOut) * C2, 15);
	}
	gap_waitbarrier(0);
}

void KerDP_hsigmoid_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   We need to know o to build C1, o is passed in Arg->UB
	   I = Relu6(I + 3) / 6
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	unsigned int Norm = Arg->Norm;
	int NormOut = Arg->UB;
	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	DP_fps_T * __restrict__ I = &In[First];
	v4s * __restrict__ O = (v4s *)(&Out[First]);
	signed char *__restrict__ Os = &Out[First];

	int C1 = 3<<NormOut;
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<NormOut;

	for (i=0; i<(Size/4); i++) {
		int Acc0 = AT_NORM(I[4*i], Norm), Acc1 = AT_NORM(I[4*i+1], Norm), Acc2 = AT_NORM(I[4*i+2], Norm), Acc3 = AT_NORM(I[4*i+3], Norm);
		Acc0 = AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * C2, 15);
		Acc1 = AT_NORM(AT_CLIP_POS(Acc1 + C1, UB) * C2, 15);
		Acc2 = AT_NORM(AT_CLIP_POS(Acc2 + C1, UB) * C2, 15);
		Acc3 = AT_NORM(AT_CLIP_POS(Acc3 + C1, UB) * C2, 15);
		O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
	}
	for (i=((Size/4)*4); i<Size; i++) {
		int Acc0 = AT_NORM(I[i], Norm);
		Os[i] = AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * C2, 15);
	}
	gap_waitbarrier(0);
}

void KerDP_sigmoid_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   o is always Q7
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	int Norm12 = Arg->Norm - 5; // Norm to Q7 - 5 to get to Q12
	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	DP_fps_T * __restrict__ I = &In[First];
	v4s * __restrict__ O = (v4s *)(&Out[First]);
	signed char *__restrict__ Os = &Out[First];
	if (Norm12<0) {
		Norm12 = -Norm12;
		for (i=0; i<(Size/4); i++) {
			int Acc0 = I[4*i] << Norm12, Acc1 = I[4*i+1] << Norm12, Acc2 = I[4*i+2] << Norm12, Acc3 = I[4*i+3] << Norm12;
			Acc0 = AT_CLIP(AT_NORM(SigmoidLUT(Acc0), 8), 7);
			Acc1 = AT_CLIP(AT_NORM(SigmoidLUT(Acc1), 8), 7);
			Acc2 = AT_CLIP(AT_NORM(SigmoidLUT(Acc2), 8), 7);
			Acc3 = AT_CLIP(AT_NORM(SigmoidLUT(Acc3), 8), 7);
			O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
		}
		for (i=((Size/4)*4); i<Size; i++) {
			int Acc0 = I[i] << Norm12;
			Os[i] = AT_CLIP(AT_NORM(SigmoidLUT(Acc0), 8), 7);
		}
	} else {
		for (i=0; i<(Size/4); i++) {
			int Acc0 = AT_NORM(I[4*i], Norm12), Acc1 = AT_NORM(I[4*i+1], Norm12), Acc2 = AT_NORM(I[4*i+2], Norm12), Acc3 = AT_NORM(I[4*i+3], Norm12);
			Acc0 = AT_CLIP(AT_NORM(SigmoidLUT(Acc0), 8), 7);
			Acc1 = AT_CLIP(AT_NORM(SigmoidLUT(Acc1), 8), 7);
			Acc2 = AT_CLIP(AT_NORM(SigmoidLUT(Acc2), 8), 7);
			Acc3 = AT_CLIP(AT_NORM(SigmoidLUT(Acc3), 8), 7);
			O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
		}
		for (i=((Size/4)*4); i<Size; i++) {
			int Acc0 = AT_NORM(I[i], Norm12);
			Os[i] = AT_CLIP(AT_NORM(SigmoidLUT(Acc0), 8), 7);
		}
	}
	gap_waitbarrier(0);
}

void KerDPMulBiasScalar_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   NormBias: m
	   LB, UB: in o format
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	int LB = Arg->LB, UB = Arg->UB;
	int M = *Arg->MulBias;
	unsigned int Norm = Arg->Norm+Arg->NormBias;
	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	DP_fps_T * __restrict__ I = &In[First];
	v4s * __restrict__ O = (v4s *)(&Out[First]);
	signed char *__restrict__ Os = &Out[First];

	for (i=0; i<(Size/4); i++) {
		int Acc0 = I[4*i  ], Acc1 = I[4*i+1], Acc2 = I[4*i+2], Acc3 = I[4*i+3];
		Acc0 = Min(Max(AT_NORM(Acc0*M, Norm), LB), UB); Acc1 = Min(Max(AT_NORM(Acc1*M, Norm), LB), UB);
		Acc2 = Min(Max(AT_NORM(Acc2*M, Norm), LB), UB); Acc3 = Min(Max(AT_NORM(Acc3*M, Norm), LB), UB);
		O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
	}
	for (i=((Size/4)*4); i<Size; i++) Os[i] = Min(Max(AT_NORM(I[i]*M, Norm), LB), UB);

	gap_waitbarrier(0);
}

void KerDPMulBias_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   NormBias: m
	   LB, UB: in o format
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	unsigned int S = Arg->W*Arg->H;
	int Feat = Arg->InFeatures;
	int LB = Arg->LB, UB = Arg->UB;
	signed char * __restrict__ MB = Arg->MulBias;
	unsigned int Norm = Arg->Norm+Arg->NormBias;
	int i, j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(Feat);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, Feat);
	int Size = S*Max(0, Last-First);

	for (int f=First; f<Last; f++) {
		DP_fps_T * __restrict__ I = &In[S*f];
		v4s * __restrict__ O = (v4s *)(&Out[S*f]);
		signed char *__restrict__ Os = &Out[S*f];
		int M = MB[f];
		for (i=0; i<(S/4); i++) {
			int Acc0 = I[4*i  ], Acc1 = I[4*i+1],
			    Acc2 = I[4*i+2], Acc3 = I[4*i+3];
			Acc0 = Min(Max(AT_NORM(Acc0*M, Norm), LB), UB); Acc1 = Min(Max(AT_NORM(Acc1*M, Norm), LB), UB);
			Acc2 = Min(Max(AT_NORM(Acc2*M, Norm), LB), UB); Acc3 = Min(Max(AT_NORM(Acc3*M, Norm), LB), UB);
			O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
		}
		for (i=((S/4)*4); i<S; i++) Os[i] = Min(Max(AT_NORM(I[i]*M, Norm), LB), UB);
	}
	gap_waitbarrier(0);
}

void KerDP_leakyrelu_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   LB, UB: Do we want to clip ????
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	unsigned int Norm = Arg->Norm;
	int i,j;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	DP_fps_T *I = &In[First];
	signed char *Os = (signed char *) (In + First);

	for (i=0; i<Size; i++) {
		int Input = AT_NORM(I[i], Norm);
		int Neg = (Input<0);
		int Pos = (Input>=0);
		int Input1 = AT_NORM(Input*LEAK_CONSTANT_FIX, LEAK_CONSTANT_FORMAT);
		int Acc0 = gap_clip(Neg*Input1+Pos*Input, 7);
		Os[i] = Acc0;
	}
	gap_waitbarrier(0);
}

void KerDP_IO_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   LB, UB: in o format
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	int LB = Arg->LB, UB = Arg->UB;
	unsigned int Norm = Arg->Norm;
	int i,j,k,U,A,B,Log2Core;
	unsigned int CoreId = gap_coreid();
	unsigned int ChunkCell = ChunkSize(S);
	unsigned int First = CoreId*ChunkCell;
	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	DP_fps_T *I = &In[First];
	v4s *O = (v4s *)(&In[First]);
	signed char *Os = (signed char *) (In + First);

	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */
	for (i=0; i<(Size/4); i++) {
		int Acc0 = I[4*i], Acc1 = I[4*i+1],
		    Acc2 = I[4*i+2], Acc3 = I[4*i+3];
		Acc0 = Min(Max(AT_NORM(Acc0, Norm), LB), UB); Acc1 = Min(Max(AT_NORM(Acc1, Norm), LB), UB);
		Acc2 = Min(Max(AT_NORM(Acc2, Norm), LB), UB); Acc3 = Min(Max(AT_NORM(Acc3, Norm), LB), UB);
		O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
	}
	for (i=((Size/4)*4); i<Size; i++) Os[i] = Min(Max(AT_NORM(I[i], Norm), LB), UB);
	gap_waitbarrier(0);

	KerReductIO_Compact_fps((signed char *__restrict__)In, (signed char *__restrict__)In, ChunkCell, S);
#if 0
	U = gap_ncore()/2; Log2Core = gap_fl1(gap_ncore()); A = 2; B = 1;
	for (k=0; k<Log2Core; k++) {
		if (CoreId<U) {
			signed char *__restrict__ OOs = ((signed char *)In+(A*CoreId+B)*ChunkCell);
			signed char *__restrict__ IIs = ((signed char *)In+((sizeof(DP_fps_T)/sizeof(signed char))*(A*CoreId+B))*ChunkCell);
			int *__restrict__ II = (int *) IIs;
			int *__restrict__ OO = (int *) OOs;
			for (i=0;i<Size/8;i++) {
				int V0 = II[2*i], V1 = II[2*i+1];
				OO[2*i] = V0; OO[2*i+1] = V1;
			}
			for (i=((Size/8)*8); i<Size; i++) OOs[i] = IIs[i];
		}
		U = U/2; A = A*2; B = B*2;
		gap_waitbarrier(0);
	}
#endif
}

void KerDP_IO_hswish_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   We need to know o to build C1, o passed in Arg->UB
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	unsigned int Norm = Arg->Norm;
	int NormOut = Arg->UB;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	DP_fps_T *I = &In[First];
	v4s *O = (v4s *)(&In[First]);
	signed char *Os = (signed char *) (In + First);

	int C1 = 3<<NormOut;
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<NormOut;
	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */

	for (i=0; i<(Size/4); i++) {
		int Acc0 = AT_NORM(I[4*i], Norm), Acc1 = AT_NORM(I[4*i+1], Norm), Acc2 = AT_NORM(I[4*i+2], Norm), Acc3 = AT_NORM(I[4*i+3], Norm);
		Acc0 = AT_NORM(AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * Acc0, NormOut) * C2, 15);
		Acc1 = AT_NORM(AT_NORM(AT_CLIP_POS(Acc1 + C1, UB) * Acc1, NormOut) * C2, 15);
		Acc2 = AT_NORM(AT_NORM(AT_CLIP_POS(Acc2 + C1, UB) * Acc2, NormOut) * C2, 15);
		Acc3 = AT_NORM(AT_NORM(AT_CLIP_POS(Acc3 + C1, UB) * Acc3, NormOut) * C2, 15);
		O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
	}
	for (i=((Size/4)*4); i<Size; i++) {
		int Acc0 = AT_NORM(I[i], Norm);
		Os[i] = AT_NORM(AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * Acc0, NormOut) * C2, 15);
	}
	gap_waitbarrier(0);

	KerReductIO_Compact_fps((signed char *__restrict__)In, (signed char *__restrict__)In, ChunkCell, S);
#if 0
	U = gap_ncore()/2; Log2Core = gap_fl1(gap_ncore()); A = 2; B = 1;
	for (k=0; k<Log2Core; k++) {
		if (CoreId<U) {
			signed char *__restrict__ OOs = ((signed char *)In+(A*CoreId+B)*ChunkCell);
			signed char *__restrict__ IIs = ((signed char *)In+((sizeof(DP_fps_T)/sizeof(signed char))*(A*CoreId+B))*ChunkCell);
			int *__restrict__ II = (int *) IIs;
			int *__restrict__ OO = (int *) OOs;
			for (i=0;i<Size/8;i++) {
				int V0 = II[2*i], V1 = II[2*i+1];
				OO[2*i] = V0; OO[2*i+1] = V1;
			}
			for (i=((Size/8)*8); i<Size; i++) OOs[i] = IIs[i];
		}
		U = U/2; A = A*2; B = B*2;
		gap_waitbarrier(0);
	}
#endif
}

void KerDP_IO_hsigmoid_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   We need to know o to build C1, o passed in Arg->UB
	   I = Relu6(I + 3) / 6
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	int Norm = Arg->Norm;
	int NormOut = Arg->UB;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	DP_fps_T *I = &In[First];
	v4s *O = (v4s *)(&In[First]);
	signed char *Os = (signed char *) (In + First);

	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */

	int C1 = 3<<NormOut;
	int C2 = (1<<15)/6; // 1/6 in Q15
	int UB = 6<<NormOut;

	for (i=0; i<(Size/4); i++) {
		int Acc0 = AT_NORM(I[4*i], Norm), Acc1 = AT_NORM(I[4*i+1], Norm), Acc2 = AT_NORM(I[4*i+2], Norm), Acc3 = AT_NORM(I[4*i+3], Norm);
		Acc0 = AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * C2, 15);
		Acc1 = AT_NORM(AT_CLIP_POS(Acc1 + C1, UB) * C2, 15);
		Acc2 = AT_NORM(AT_CLIP_POS(Acc2 + C1, UB) * C2, 15);
		Acc3 = AT_NORM(AT_CLIP_POS(Acc3 + C1, UB) * C2, 15);
		O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
	}
	for (i=((Size/4)*4); i<Size; i++) {
		int Acc0 = AT_NORM(I[i], Norm);
		Os[i] = AT_NORM(AT_CLIP_POS(Acc0 + C1, UB) * C2, 15);
	}
	gap_waitbarrier(0);

	KerReductIO_Compact_fps((signed char *__restrict__)In, (signed char *__restrict__)In, ChunkCell, S);
}

void KerDP_IO_sigmoid_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   o is always Q7
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	int Norm12 = Arg->Norm - 5;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	DP_fps_T *I = &In[First];
	v4s *O = (v4s *)(&In[First]);
	signed char *Os = (signed char *) (In + First);

	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */

	if (Norm12 < 0) {
		for (i=0; i<(Size/4); i++) {
			int Acc0 = I[4*i] << Norm12, Acc1 = I[4*i+1] << Norm12, Acc2 = I[4*i+2] << Norm12, Acc3 = I[4*i+3] << Norm12;
			Acc0 = AT_CLIP(AT_NORM(SigmoidLUT(Acc0), 8), 7);
			Acc1 = AT_CLIP(AT_NORM(SigmoidLUT(Acc1), 8), 7);
			Acc2 = AT_CLIP(AT_NORM(SigmoidLUT(Acc2), 8), 7);
			Acc3 = AT_CLIP(AT_NORM(SigmoidLUT(Acc3), 8), 7);
			O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
		}
		for (i=((Size/4)*4); i<Size; i++) {
			int Acc0 = I[i] << Norm12;
			Os[i] = AT_CLIP(AT_NORM(SigmoidLUT(Acc0), 8), 7);
		}
	} else {
		for (i=0; i<(Size/4); i++) {
			int Acc0 = AT_NORM(I[4*i], Norm12), Acc1 = AT_NORM(I[4*i+1], Norm12), Acc2 = AT_NORM(I[4*i+2], Norm12), Acc3 = AT_NORM(I[4*i+3], Norm12);
			Acc0 = AT_CLIP(AT_NORM(SigmoidLUT(Acc0), 8), 7);
			Acc1 = AT_CLIP(AT_NORM(SigmoidLUT(Acc1), 8), 7);
			Acc2 = AT_CLIP(AT_NORM(SigmoidLUT(Acc2), 8), 7);
			Acc3 = AT_CLIP(AT_NORM(SigmoidLUT(Acc3), 8), 7);
			O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
		}
		for (i=((Size/4)*4); i<Size; i++) {
			int Acc0 = AT_NORM(I[i], Norm12);
			Os[i] = AT_CLIP(AT_NORM(SigmoidLUT(Acc0), 8), 7);
		}
	}
	gap_waitbarrier(0);

	KerReductIO_Compact_fps((signed char *__restrict__)In, (signed char *__restrict__)In, ChunkCell, S);
}

void KerDP_IO_leakyrelu_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   LB, UB: Do we want to clip ????
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	unsigned int Norm = Arg->Norm;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	DP_fps_T *I = &In[First];
	signed char *Os = (signed char *) (In + First);

	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */

	for (i=0; i<Size; i++) {
		int Input = AT_NORM(I[i], Norm);
		int Neg = (Input<0);
		int Pos = (Input>=0);
		int Input1 = AT_NORM(Input*LEAK_CONSTANT_FIX, LEAK_CONSTANT_FORMAT);
		int Acc0 = gap_clip(Neg*Input1+Pos*Input, 7);
		Os[i] = Acc0;
	}
	gap_waitbarrier(0);

	KerReductIO_Compact_fps((signed char *__restrict__)In, (signed char *__restrict__)In, ChunkCell, S);
}

void KerDPMulBiasScalar_IO_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   NormBias: m
	   LB, UB: in o format
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->W*Arg->H*Arg->InFeatures;
	unsigned int Norm = Arg->Norm+Arg->NormBias;
	int LB = Arg->LB, UB = Arg->UB;
	int M = *Arg->MulBias;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Max(0, Last-First);

	DP_fps_T *I = &In[First];
	v4s *O = (v4s *)(&In[First]);
	signed char *Os = (signed char *) (In + First);

	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */
	for (i=0; i<(Size/4); i++) {
		int Acc0 = I[4*i], Acc1 = I[4*i+1],
		    Acc2 = I[4*i+2], Acc3 = I[4*i+3];
		Acc0 = Min(Max(AT_NORM(Acc0*M, Norm), LB), UB); Acc1 = Min(Max(AT_NORM(Acc1*M, Norm), LB), UB);
		Acc2 = Min(Max(AT_NORM(Acc2*M, Norm), LB), UB); Acc3 = Min(Max(AT_NORM(Acc3*M, Norm), LB), UB);
		O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
	}
	for (i=((Size/4)*4); i<Size; i++) Os[i] = Min(Max(AT_NORM(I[i]*M, Norm), LB), UB);

	gap_waitbarrier(0);

	KerReductIO_Compact_fps((signed char *__restrict__)In, (signed char *__restrict__)In, ChunkCell, S);
}

void KerDPMulBias_IO_fps(KerDP_fps_T *Arg)

{
	/* Norm: w+i-o
	   NormBias: m
	   LB, UB: in o format
	*/
	DP_fps_T * __restrict__ In = Arg->In;
	signed char * __restrict__ Out = Arg->Out;
	int S = Arg->InFeatures;
	unsigned int Norm = Arg->Norm+Arg->NormBias;
	int LB = Arg->LB, UB = Arg->UB;
	signed char * __restrict__ MB = Arg->MulBias;
	int i,j,k,U,A,B,Log2Core;
       	unsigned int CoreId = gap_coreid();
       	unsigned int ChunkCell = ChunkSize(S);
       	unsigned int First = CoreId*ChunkCell;
       	unsigned int Last  = Min(First+ChunkCell, S);
	int Size = Arg->W*Arg->H;


	/* First normalize In, each parallel chunk overwrites it's own input
	   After we are done In contains groups of contiguous normalized values
	   each group beeing followed by an empty group of exactly the same size, these
	   one need to be supressed, second step is taking care of this reduction */
	for (j=First; j<Last; j++) {
		DP_fps_T *I = &In[Size*j];
		v4s *O = (v4s *)(&In[Size*j]);
		signed char *Os = (signed char *) (In + Size*j);
		int M = MB[j];
		for (i=0; i<(Size/4); i++) {
			int Acc0 = I[4*i], Acc1 = I[4*i+1],
		    	    Acc2 = I[4*i+2], Acc3 = I[4*i+3];
			Acc0 = Min(Max(AT_NORM(Acc0*M, Norm), LB), UB); Acc1 = Min(Max(AT_NORM(Acc1*M, Norm), LB), UB);
			Acc2 = Min(Max(AT_NORM(Acc2*M, Norm), LB), UB); Acc3 = Min(Max(AT_NORM(Acc3*M, Norm), LB), UB);
			O[i] = gap_pack4(Acc0, Acc1, Acc2, Acc3);
		}
		for (i=((Size/4)*4); i<Size; i++) Os[i] = Min(Max(AT_NORM(I[i]*M, Norm), LB), UB);
	}

	gap_waitbarrier(0);

	KerReductIO_Compact_fps((signed char *__restrict__)In, (signed char *__restrict__)In, Size*ChunkCell, Size*S);
}
#pragma GCC diagnostic pop
