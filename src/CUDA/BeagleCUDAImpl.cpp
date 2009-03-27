/*
 *  BeagleCUDAImpl.cpp
 *  BEAGLE
 *
 * @author Marc Suchard
 * @author Andrew Rambaut
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cuda_runtime_api.h>
#include <cuda.h>

#include "BeagleCUDAImpl.h"
#include "CUDASharedFunctions.h"
//#include "beagle.h"

#define CMATRIX_SIZE		2 * PADDED_STATE_COUNT * PADDED_STATE_COUNT + 2 * PADDED_STATE_COUNT // Using memory saving format
#define MATRIX_SIZE     	PADDED_STATE_COUNT * PADDED_STATE_COUNT
#define MATRIX_CACHE_SIZE	PADDED_STATE_COUNT * PADDED_STATE_COUNT * PADDED_STATE_COUNT
#define EVAL_SIZE			PADDED_STATE_COUNT // Change to 2 * PADDED_STATE_COUNT for complex models
#define	RESTORE_VALUE	1
#define STORE_VALUE		2
#define STORE_RESTORE_MAX_LENGTH	2

#define DEVICE_NUMBER	0 // TODO Send info from wrapper
#define INSTANCE	0 // TODO Send info from wrapper

#ifdef LAZY_STORE
#define CHECK_LAZY_STORE(instance)	\
									if (!doStoreRestoreQueue.queueEmpty()) \
										handleStoreRestoreQueue();
#else
#define CHECK_LAZY_STORE
#endif // LAZY_STORE

void checkNativeMemory(void *ptr) {
	if (ptr == NULL) {
		fprintf(stderr, "Unable to allocate some memory!\n");
		exit(-1);
	}
}

void BeagleCUDAImpl::initializeInstanceMemory() {

	cudaSetDevice(device);
	int i;

	dCMatrix = allocateGPURealMemory(MATRIX_CACHE_SIZE);
	dStoredMatrix = allocateGPURealMemory(MATRIX_CACHE_SIZE);
	dEvec = allocateGPURealMemory(MATRIX_SIZE);
	dIevc = allocateGPURealMemory(MATRIX_SIZE);
	dStoredEvec = allocateGPURealMemory(MATRIX_SIZE);
	dStoredIevc = allocateGPURealMemory(MATRIX_SIZE);

	dEigenValues = allocateGPURealMemory(EVAL_SIZE);
	dStoredEigenValues = allocateGPURealMemory(EVAL_SIZE);

	dFrequencies = allocateGPURealMemory(PADDED_STATE_COUNT);
	dStoredFrequencies = allocateGPURealMemory(PADDED_STATE_COUNT);

	dCategoryRates = allocateGPURealMemory(
			matrixCount);
	hCategoryRates = (REAL *) malloc(sizeof(REAL)
			* matrixCount);
	dStoredCategoryRates = allocateGPURealMemory(
			matrixCount);
	hStoredCategoryRates = (REAL *) malloc(sizeof(REAL)
			* matrixCount);

	checkNativeMemory(hCategoryRates);
	checkNativeMemory(hStoredCategoryRates);

	dCategoryProportions = allocateGPURealMemory(
			matrixCount);
	dStoredCategoryProportions = allocateGPURealMemory(
			matrixCount);

	dIntegrationTmp = allocateGPURealMemory(
			patternCount);

	dPartials = (REAL ***) malloc(sizeof(REAL**) * 2);
	dPartials[0] = (REAL **) malloc(sizeof(REAL*)
			* nodeCount);
	dPartials[1] = (REAL **) malloc(sizeof(REAL*)
			* nodeCount);

#ifdef DYNAMIC_SCALING
	dScalingFactors = (REAL ***)malloc(sizeof(REAL**) * 2);
	dScalingFactors[0] = (REAL **)malloc(sizeof(REAL*) * nodeCount);
	dScalingFactors[1] = (REAL **)malloc(sizeof(REAL*) * nodeCount);
	dRootScalingFactors = allocateGPURealMemory(patternCount);
	dStoredRootScalingFactors = allocateGPURealMemory(patternCount);
#endif

	for (i = 0; i < nodeCount; i++) {
		dPartials[0][i] = allocateGPURealMemory(
				partialsSize);
		dPartials[1][i] = allocateGPURealMemory(
				partialsSize);

#ifdef DYNAMIC_SCALING
		dScalingFactors[0][i] = allocateGPURealMemory(patternCount);
		dScalingFactors[1][i] = allocateGPURealMemory(patternCount);
#endif
	}

	hCurrentMatricesIndices = (int *) malloc(sizeof(int)
			* nodeCount);
	hStoredMatricesIndices = (int *) malloc(sizeof(int)
			* nodeCount);
	for (i = 0; i < nodeCount; i++) {
		hCurrentMatricesIndices[i] = 0;
		hStoredMatricesIndices[i] = 0;
	}

	checkNativeMemory(hCurrentMatricesIndices);
	checkNativeMemory(hStoredMatricesIndices);

	hCurrentPartialsIndices = (int *) malloc(sizeof(int)
			* nodeCount);
	hStoredPartialsIndices = (int *) malloc(sizeof(int)
			* nodeCount);

#ifdef DYNAMIC_SCALING
	hCurrentScalingFactorsIndices = (int *)malloc(sizeof(int) * nodeCount);
	hStoredScalingFactorsIndices = (int *)malloc(sizeof(int) * nodeCount);
#endif

	for (i = 0; i < nodeCount; i++) {
		hCurrentPartialsIndices[i] = 0;
		hStoredPartialsIndices[i] = 0;
#ifdef DYNAMIC_SCALING
		hCurrentScalingFactorsIndices[i] = 0;
		hStoredScalingFactorsIndices[i] = 0;
#endif
	}

	checkNativeMemory(hCurrentPartialsIndices);
	checkNativeMemory(hStoredPartialsIndices);

#ifdef DYNAMIC_SCALING
	checkNativeMemory(hCurrentScalingFactorsIndices);
	checkNativeMemory(hStoredScalingFactorsIndices);
#endif

	//	dCurrentMatricesIndices = allocateGPUIntMemory(nodeCount);
	//	dStoredMatricesIndices = allocateGPUIntMemory(nodeCount);
	//	cudaMemcpy(dCurrentMatricesIndices,hCurrentMatricesIndices,sizeof(int)*nodeCount,cudaMemcpyHostToDevice);

	//	dCurrentPartialsIndices = allocateGPUIntMemory(nodeCount);
	//	dStoredPartialsIndices = allocateGPUIntMemory(nodeCount);
	//	cudaMemcpy(dCurrentPartialsIndices,hCurrentPartialsIndices,sizeof(int)*nodeCount,cudaMemcpyHostToDevice);

	dMatrices = (REAL ***) malloc(sizeof(REAL**) * 2);
	dMatrices[0] = (REAL **) malloc(sizeof(REAL*)
			* nodeCount);
	dMatrices[1] = (REAL **) malloc(sizeof(REAL*)
			* nodeCount);

	for (i = 0; i < nodeCount; i++) {
		dMatrices[0][i] = allocateGPURealMemory(MATRIX_SIZE
				* matrixCount);
		dMatrices[1][i] = allocateGPURealMemory(MATRIX_SIZE
				* matrixCount);
	}

	dStates = (INT **) malloc(sizeof(INT*)
			* nodeCount);
	for (i = 0; i < nodeCount; i++) {
		dStates[i] = 0; // Allocate in setStates only if state info is provided
	}

	dNodeIndices = allocateGPUIntMemory(
			nodeCount); // No execution has more no nodeCount events
	hNodeIndices = (int *) malloc(sizeof(int)
			* nodeCount);
	hDependencies = (int *) malloc(sizeof(int)
			* nodeCount);
	dBranchLengths = allocateGPURealMemory(
			nodeCount);

	checkNativeMemory(hNodeIndices);
	checkNativeMemory(hDependencies);

	dDistanceQueue = allocateGPURealMemory(
			nodeCount * matrixCount);
	hDistanceQueue = (REAL *) malloc(sizeof(REAL)
			* nodeCount * matrixCount);

	checkNativeMemory(hDistanceQueue);

	int len = 5;
	if (matrixCount > 5);
		len = matrixCount;

	SAFE_CUDA(cudaMalloc((void**) &dPtrQueue, sizeof(REAL*)*nodeCount*len),dPtrQueue);
	hPtrQueue = (REAL **) malloc(sizeof(REAL*)
			* nodeCount * len);

	checkNativeMemory(hPtrQueue);

}

void BeagleCUDAImpl::initializeDevice(int deviceNumber,
		int inNodeCount, int inStateTipCount, int inPatternCount,
		int inMatrixCount) {

#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering initialize\n");
#endif

	// Increase instance storage
//	numThreads++;
//	thread = (threadVariables*) realloc(thread, numThreads
//			* sizeof(threadVariables));

	int i;
	device = deviceNumber;
	trueStateCount = STATE_COUNT;
	nodeCount = inNodeCount;
	taxaCount = (nodeCount + 1) / 2;
	truePatternCount = inPatternCount;
	matrixCount = inMatrixCount;

	paddedStates = 0;
	paddedPatterns = 0;

#if (PADDED_STATE_COUNT == 4)  // DNA model
	// Make sure that patternCount + paddedPatterns is multiple of 4
	if (truePatternCount % 4 != 0)
	paddedPatterns = 4 - truePatternCount % 4;
	else
	paddedPatterns = 0;
#ifdef DEBUG
	fprintf(stderr,"Padding patterns for 4-state model:\n");
	fprintf(stderr,"\ttruePatternCount = %d\n\tpaddedPatterns = %d\n",truePatternCount,paddedPatterns);
#endif // DEBUG
#endif // DNA model
	patternCount = truePatternCount
			+ paddedPatterns;

	partialsSize = patternCount * PADDED_STATE_COUNT
			* matrixCount;

	hFrequenciesCache = (REAL*)calloc(PADDED_STATE_COUNT, SIZE_REAL);
	hPartialsCache = (REAL*)calloc(partialsSize,SIZE_REAL);
	hMatrixCache = (REAL*)calloc(CMATRIX_SIZE, SIZE_REAL);

//	hNodeCache = NULL;

#ifndef DOUBLE_PRECISION
	hCategoryCache = (REAL*)malloc(matrixCount*SIZE_REAL);
	hLogLikelihoodsCache = (REAL*)malloc(truePatternCount*SIZE_REAL);
#endif

	doRescaling = 1;
	sinceRescaling = 0;

#ifndef PRE_LOAD
	initializeInstanceMemory();
#else
	// initialize temporary storage before likelihood thread exists

	loaded = 0;

	hTmpPartials = (REAL **) malloc(sizeof(REAL*)
			* taxaCount);

	for (i = 0; i < taxaCount; i++) {
		hTmpPartials[i] = (REAL *) malloc(
				partialsSize * SIZE_REAL);
	}
#endif

#ifdef LAZY_STORE
	doStore = 0;
	doRestore = 0;
	doStoreRestoreQueue.initQueue();
#endif // LAZY_STORE

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting initialize\n");
#endif
}

void BeagleCUDAImpl::initialize(int nodeCount,
				int tipCount,
				int stateCount,
				int patternCount,
				int categoryCount,
				int matrixCount) {

	int numDevices = printGPUInfo();
	initializeDevice(DEVICE_NUMBER, nodeCount, tipCount, patternCount, categoryCount);
}

void BeagleCUDAImpl::freeTmpPartials() {
	int i;
	for (i = 0; i < taxaCount; i++) { // TODO divide by 2
		free(hTmpPartials[i]);
	}
}

void BeagleCUDAImpl::freeNativeMemory() {
	int i;
	for (i = 0; i < nodeCount; i++) {
		freeGPUMemory(dPartials[0][i]);
		freeGPUMemory(dPartials[1][i]);
#ifdef DYNAMIC_SCALING
		freeGPUMemory(dScalingFactors[0][i]);
		freeGPUMemory(dScalingFactors[1][i]);
#endif
		freeGPUMemory(dMatrices[0][i]);
		freeGPUMemory(dMatrices[1][i]);
		freeGPUMemory(dStates[i]);
	}

	freeGPUMemory(dCMatrix);
	freeGPUMemory(dStoredMatrix);
	freeGPUMemory(dEvec);
	freeGPUMemory(dIevc);

	free(dPartials[0]);
	free(dPartials[1]);
	free(dPartials);

#ifdef DYNAMIC_SCALING
	free(dScalingFactors[0]);
	free(dScalingFactors[1]);
	free(dScalingFactors);
#endif

	free(dMatrices[0]);
	free(dMatrices[1]);
	free(dMatrices);

	free(dStates);

	free(hCurrentMatricesIndices);
	free(hStoredMatricesIndices);
	free(hCurrentPartialsIndices);
	free(hStoredPartialsIndices);

#ifdef DYNAMIC_SCALING
	free(hCurrentScalingFactorsIndices);
	free(hStoredScalingFactorsIndices);
#endif

	freeGPUMemory(dNodeIndices);
	free(hNodeIndices);
	free(hDependencies);
	freeGPUMemory(dBranchLengths);

	freeGPUMemory(dIntegrationTmp);

	free(hDistanceQueue);
	free(hPtrQueue);
	freeGPUMemory(dDistanceQueue);
	freeGPUMemory(dPtrQueue);
}

REAL *callocBEAGLE(int length, int instance) {
	REAL *ptr = (REAL *) calloc(length, SIZE_REAL);
	if (ptr == NULL) {
		fprintf(stderr,"Unable to allocate native memory!");
		exit(-1);
	}
	return ptr;
}

void BeagleCUDAImpl::finalize() {
	//freeNativeMemory();
}

void BeagleCUDAImpl::setTipStates(int tipIndex,
				  int* inStates) {
#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering setTipStates\n");
#endif

	fprintf(stderr,"Unsupported operation!\n");
	exit(-1);

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting setTipStates\n");
#endif
}

void BeagleCUDAImpl::setTipPartials(int tipIndex,
					double* inPartials) {
#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering setTipPartials\n");
#endif

	double *inPartialsOffset = inPartials;
	int length = patternCount * PADDED_STATE_COUNT;
	REAL *tmpRealArrayOffset = hPartialsCache;

	int s, p;
	for (p = 0; p < truePatternCount; p++) {
#ifdef DOUBLE_PRECISION
		memcpy(tmpRealArrayOffset,inPartialsOffset, SIZE_REAL*STATE_COUNT);
#else
		MEMCPY(tmpRealArrayOffset,inPartialsOffset,STATE_COUNT,REAL);
#endif
		tmpRealArrayOffset += PADDED_STATE_COUNT;
		inPartialsOffset += STATE_COUNT;
	}

	// Replicate 1st copy "times" times
	int i;
	for (i = 1; i < matrixCount; i++) {
		memcpy(hPartialsCache + i * length,
				hPartialsCache, length * SIZE_REAL);
	}

#ifndef PRE_LOAD
	// Copy to CUDA device
	SAFE_CUDA(cudaMemcpy(dPartials[0][tipIndex],
					hPartialsCache,
					SIZE_REAL*length*matrixCount, cudaMemcpyHostToDevice),dPartials[0][tipIndex]);
#else
	memcpy(hTmpPartials[tipIndex],
			hPartialsCache, SIZE_REAL
					* partialsSize);
#endif // PRE_LOAD

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting setTipPartials\n");
#endif
}

void BeagleCUDAImpl::loadTipPartials() {
	int i;
	for (i = 0; i < taxaCount; i++) {
		cudaMemcpy(dPartials[0][i],
				hTmpPartials[i], SIZE_REAL
						* partialsSize, cudaMemcpyHostToDevice);
	}
}

void BeagleCUDAImpl::setStateFrequencies(double* inFrequencies) {

#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering updateRootFreqencies\n");
#endif

	int instance = INSTANCE;

	CHECK_LAZY_STORE(instance);

#ifdef DEBUG_BEAGLE
	printfVectorD(inFrequencies,PADDED_STATE_COUNT);
//	exit(-1);
#endif

#ifdef PRE_LOAD
	if (loaded == 0) {
		initializeInstanceMemory();
		loadTipPartials();
		freeTmpPartials();
		loaded = 1;
	}
#endif // PRE_LOAD

#ifdef DOUBLE_PRECISION
	memcpy(hFrequenciesCache,inFrequencies,STATE_COUNT*SIZE_REAL);
#else
	MEMCPY(hFrequenciesCache,inFrequencies,STATE_COUNT,REAL);
#endif

	cudaMemcpy(dFrequencies,hFrequenciesCache,
			SIZE_REAL*PADDED_STATE_COUNT,cudaMemcpyHostToDevice);

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting updateRootFrequencies\n");
#endif
}

/*
 * Transposes a square matrix in place
 */
void BeagleCUDAImpl::transposeSquareMatrix(REAL *mat, int size) {
	int i, j;
	for (i = 0; i < size - 1; i++) {
		for (j = i + 1; j < size; j++) {
			REAL tmp = mat[i * size + j];
			mat[i * size + j] = mat[j * size + i];
			mat[j * size + i] = tmp;
		}
	}
}

void BeagleCUDAImpl::setEigenDecomposition(int matrixIndex,
						   double** inEigenVectors,
						   double** inInverseEigenVectors,
						   double* inEigenValues) {

#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering updateEigenDecomposition\n");
#endif

	int instance = INSTANCE;

	CHECK_LAZY_STORE(instance);

	// Native memory packing order (length): Ievc (state^2), Evec (state^2), Eval (state), EvalImag (state)

//	int length = 2 * (MATRIX_SIZE + PADDED_STATE_COUNT); // Storing extra space for complex eigenvalues
//
//	if (hMatrixCache == NULL)
//		hMatrixCache = callocBEAGLE(length, instance);

	REAL *Ievc, *tmpIevc, *Evec, *tmpEvec, *Eval, *EvalImag;

	tmpIevc = Ievc = (REAL *) hMatrixCache;
	tmpEvec = Evec = Ievc + MATRIX_SIZE;
	Eval = Evec + MATRIX_SIZE;

	int i, j;
	for (i = 0; i < STATE_COUNT; i++) {
#ifdef DOUBLE_PRECISION
		memcpy(tmpIevc,inInverseEigenVectors[i],SIZE_REAL*STATE_COUNT);
		memcpy(tmpEvec,inEigenVectors[i],SIZE_REAL*STATE_COUNT);
#else
		MEMCPY(tmpIevc,inInverseEigenVectors[i],STATE_COUNT,REAL);
		MEMCPY(tmpEvec,inEigenVectors[i],STATE_COUNT,REAL);
#endif

		tmpIevc += PADDED_STATE_COUNT;
		tmpEvec += PADDED_STATE_COUNT;
	}

	transposeSquareMatrix(Ievc, PADDED_STATE_COUNT); // Transposing matrices avoids incoherent memory read/writes
	transposeSquareMatrix(Evec, PADDED_STATE_COUNT); // TODO Only need to tranpose sub-matrix of trueStateCount

#ifdef DOUBLE_PRECISION
	memcpy(Eval,inEigenValues,SIZE_REAL*STATE_COUNT);
#else
	MEMCPY(Eval,inEigenValues,STATE_COUNT,REAL);
#endif

#ifdef DEBUG_BEAGLE
#ifdef DOUBLE_PRECISION
	printfVectorD(Eval,PADDED_STATE_COUNT);
	printfVectorD(Evec,MATRIX_SIZE);
	printfVectorD(Ievc,PADDED_STATE_COUNT*PADDED_STATE_COUNT);
#else
	printfVectorF(Eval,PADDED_STATE_COUNT);
	printfVectorF(Evec,MATRIX_SIZE);
	printfVectorF(Ievc,PADDED_STATE_COUNT*PADDED_STATE_COUNT);
#endif
#endif

	// Copy to CUDA device
	cudaMemcpy(dIevc,Ievc, SIZE_REAL*MATRIX_SIZE, cudaMemcpyHostToDevice);
	cudaMemcpy(dEvec,Evec, SIZE_REAL*MATRIX_SIZE, cudaMemcpyHostToDevice);
	cudaMemcpy(dEigenValues,Eval, SIZE_REAL*PADDED_STATE_COUNT, cudaMemcpyHostToDevice);

#ifdef DEBUG_BEAGLE
	printfCudaVector(dEigenValues,PADDED_STATE_COUNT);
	printfCudaVector(dEvec,MATRIX_SIZE);
	printfCudaVector(dIevc,PADDED_STATE_COUNT*PADDED_STATE_COUNT);
#endif


#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting updateEigenDecomposition\n");
#endif

}


void BeagleCUDAImpl::setCategoryRates(double* inCategoryRates) {
#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering updateCategoryRates\n");
#endif

	int instance = INSTANCE;

	CHECK_LAZY_STORE(instance);

#ifdef DOUBLE_PRECISION
	double* categoryRates = inCategoryRates;
#else
	REAL* categoryRates = hCategoryCache;
	MEMCPY(categoryRates,inCategoryRates,matrixCount,REAL);
#endif

	cudaMemcpy(dCategoryRates, categoryRates,
			SIZE_REAL*matrixCount, cudaMemcpyHostToDevice); // TODO Are these used on the GPU?

#ifdef DEBUG_BEAGLE
	printfCudaVector(dCategoryRates,matrixCount);
#endif

	memcpy(hCategoryRates, categoryRates,
			SIZE_REAL*matrixCount);

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting updateCategoryRates\n");
#endif
}

void BeagleCUDAImpl::setCategoryProportions(double* inCategoryProportions) {
#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering updateCategoryProportions\n");
#endif

	int instance = INSTANCE;

	CHECK_LAZY_STORE(instance);

#ifdef DOUBLE_PRECISION
	REAL* categoryProportions = inCategoryProportions;
#else
	REAL* categoryProportions = hCategoryCache;
	MEMCPY(categoryProportions,inCategoryProportions,matrixCount,REAL);
#endif

	cudaMemcpy(dCategoryProportions, categoryProportions,
			SIZE_REAL*matrixCount, cudaMemcpyHostToDevice);

#ifdef DEBUG_BEAGLE
	printfCudaVector(dCategoryProportions,matrixCount);
#endif

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting updateCategoryProportions\n");
#endif
}

void BeagleCUDAImpl::calculateProbabilityTransitionMatrices(
                                            int* nodeIndices,
                                            double* branchLengths,
                                            int count) {
#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering updateMatrices\n");
#endif

	int instance = INSTANCE;

	CHECK_LAZY_STORE(instance);

	int x, total = 0;
	for (x = 0; x < count; x++) {
		int nodeIndex = nodeIndices[x];

		hCurrentMatricesIndices[nodeIndex] = 1
				- hCurrentMatricesIndices[nodeIndex];

		int l;
		for (l = 0; l < matrixCount; l++) {
			hPtrQueue[total]
					= dMatrices[hCurrentMatricesIndices[nodeIndex]][nodeIndex]
							+ l * MATRIX_SIZE;
			hDistanceQueue[total] = ((REAL)branchLengths[x])
					* hCategoryRates[l];
			total++;
		}
	}

	cudaMemcpy(dDistanceQueue,
			hDistanceQueue, SIZE_REAL*total,
			cudaMemcpyHostToDevice);
	cudaMemcpy(dPtrQueue, hPtrQueue,
			sizeof(REAL*) * total, cudaMemcpyHostToDevice);

//#ifdef DEBUG_BEAGLE
//	printf("bl[0] = %1.5e\n",branchLengths[0]);
//	printf("matrixCount = %d\n",matrixCount);
//	printf("cat rates = ");
//	printfVector(hCategoryRates,matrixCount);
//	printf("branch lengths = \n");
//	printfVector(hDistanceQueue,total);
//	printf("\n\n");
//	printfCudaVector(dDistanceQueue,total);
//#endif

	nativeGPUGetTransitionProbabilitiesSquare(dPtrQueue,
			dEvec, dIevc,
			dEigenValues, dDistanceQueue,
			total);

#ifdef DEBUG_BEAGLE
	printfCudaVector(hPtrQueue[0],MATRIX_SIZE);
	//exit(-1);
#endif

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting updateMatrices\n");
#endif
}

void BeagleCUDAImpl::calculatePartials(
					   int* operations,
					   int* dependencies,
					   int count,
					   int rescale) {
#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering updatePartials\n");
#endif

	int instance = INSTANCE;

	CHECK_LAZY_STORE(instance);

#ifdef DYNAMIC_SCALING
	if (doRescaling == 0) // Forces rescaling on first computation
		doRescaling = rescale;
#endif

	// Serial version
	int op, x = 0, y = 0;
	for (op = 0; op < count; op++) {
		int nodeIndex1 = operations[x];
		x++;
		int nodeIndex2 = operations[x];
		x++;
		int nodeIndex3 = operations[x];
		x++;

		REAL *matrices1 = dMatrices[hCurrentMatricesIndices[nodeIndex1]][nodeIndex1];
		REAL *matrices2 = dMatrices[hCurrentMatricesIndices[nodeIndex2]][nodeIndex2];

		REAL *partials1 = dPartials[hCurrentPartialsIndices[nodeIndex1]][nodeIndex1];
		REAL *partials2 = dPartials[hCurrentPartialsIndices[nodeIndex2]][nodeIndex2];

		hCurrentPartialsIndices[nodeIndex3] = 1
				- hCurrentPartialsIndices[nodeIndex3];
		REAL *partials3 = dPartials[hCurrentPartialsIndices[nodeIndex3]][nodeIndex3];

#ifdef DYNAMIC_SCALING

		if( doRescaling )
			hCurrentScalingFactorsIndices[nodeIndex3] = 1 - hCurrentScalingFactorsIndices[nodeIndex3];

		REAL* scalingFactors = dScalingFactors[hCurrentScalingFactorsIndices[nodeIndex3]][nodeIndex3];

		nativeGPUPartialsPartialsPruningDynamicScaling(
				partials1,partials2, partials3,
				matrices1,matrices2,
				scalingFactors,
				patternCount, matrixCount, doRescaling);
#else
		nativeGPUPartialsPartialsPruning(partials1, partials2, partials3,
				matrices1, matrices2, patternCount,
				matrixCount);

#ifdef DEBUG_BEAGLE
		fprintf(stderr,"patternCount = %d\n",patternCount);
		fprintf(stderr,"truePatternCount = %d\n",truePatternCount);
		fprintf(stderr,"matrixCount  = %d\n",matrixCount);
		fprintf(stderr,"partialSize = %d\n",partialsSize);//		printfCudaVector(partials1,partialsSize);
		printfCudaVector(partials1,partialsSize);
		printfCudaVector(partials2,partialsSize);
		printfCudaVector(partials3,partialsSize);
//		exit(-1);
#endif
#endif // DYNAMIC_SCALING

	}

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting updatePartials\n");
#endif

}

void BeagleCUDAImpl::calculateLogLikelihoods(int rootNodeIndex,
							 double* outLogLikelihoods) {
#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering calculateLogLikelihoods\n");
#endif
	int instance = INSTANCE;

	CHECK_LAZY_STORE(instance);

#ifdef DYNAMIC_SCALING

	if (doRescaling) {
		// Construct node-list for scalingFactors
		int n;
		int length = nodeCount - taxaCount;
		for(n=0; n<length; n++)
			hPtrQueue[n] = dScalingFactors[hCurrentScalingFactorsIndices[n+taxaCount]][n+taxaCount];

		cudaMemcpy(dPtrQueue,hPtrQueue,sizeof(REAL*)*length, cudaMemcpyHostToDevice);

		// Computer scaling factors at the root
		nativeGPUComputeRootDynamicScaling(dPtrQueue,dRootScalingFactors,length,patternCount);
	}

	doRescaling = 0;

	nativeGPUIntegrateLikelihoodsDynamicScaling(dIntegrationTmp, dPartials[hCurrentPartialsIndices[rootNodeIndex]][rootNodeIndex],
			dCategoryProportions, dFrequencies,
			dRootScalingFactors,
			patternCount, matrixCount, nodeCount);

#else
	nativeGPUIntegrateLikelihoods(
			dIntegrationTmp,
			dPartials[hCurrentPartialsIndices[rootNodeIndex]][rootNodeIndex],
			dCategoryProportions,
			dFrequencies, patternCount,
			matrixCount);
#endif // DYNAMIC_SCALING

#ifdef DOUBLE_PRECISION
	cudaMemcpy(outLogLikelihoods,dIntegrationTmp,
			SIZE_REAL*truePatternCount, cudaMemcpyDeviceToHost);
#else
	cudaMemcpy(hLogLikelihoodsCache,dIntegrationTmp,
			SIZE_REAL*truePatternCount, cudaMemcpyDeviceToHost);
	MEMCPY(outLogLikelihoods,hLogLikelihoodsCache,truePatternCount,double);
#endif

#ifdef DEBUG
	printf("logLike = ");
	printfVectorD(outLogLikelihoods,truePatternCount);
	exit(-1);
#endif

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting calculateLogLikelihoods\n");
#endif
}

void BeagleCUDAImpl::handleStoreRestoreQueue() {

#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering handleStoreRestoreQueue: ");
	doStoreRestoreQueue.printQueue();
#endif

	while (!doStoreRestoreQueue.queueEmpty()) {
		int command = doStoreRestoreQueue.deQueue();
		switch (command) {
		case STORE_VALUE:
			doStoreState();
			break;
		case RESTORE_VALUE:
			doRestoreState();
			break;
		default:
			fprintf(stderr,"Illegal command in Store/Restore queue!");
			exit(-1);
		}
	}

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting handleStoreRestoreQueue\n");
#endif
}

void BeagleCUDAImpl::doStoreState() {

#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering doStore\n");
#endif

	storeGPURealMemoryArray(dStoredEigenValues,
			dEigenValues, EVAL_SIZE);
	storeGPURealMemoryArray(dStoredEvec,
			dEvec, MATRIX_SIZE);
	storeGPURealMemoryArray(dStoredIevc,
			dIevc, MATRIX_SIZE);

	storeGPURealMemoryArray(dStoredFrequencies,
			dFrequencies, PADDED_STATE_COUNT);
	storeGPURealMemoryArray(dStoredCategoryRates,
			dCategoryRates, matrixCount);
	memcpy(hStoredCategoryRates,
			hCategoryRates, matrixCount
					* sizeof(REAL));
	storeGPURealMemoryArray(dStoredCategoryProportions,
			dCategoryProportions, matrixCount);

	memcpy(hStoredMatricesIndices,
			hCurrentMatricesIndices, sizeof(int)
					* nodeCount);
	memcpy(hStoredPartialsIndices,
			hCurrentPartialsIndices, sizeof(int)
					* nodeCount);

#ifdef DYNAMIC_SCALING
	memcpy(hStoredScalingFactorsIndices, hCurrentScalingFactorsIndices, sizeof(int) * nodeCount);
	storeGPURealMemoryArray(dStoredRootScalingFactors, dRootScalingFactors, patternCount);
#endif

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting doStore\n");
#endif
}

void BeagleCUDAImpl::storeState() {
#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering storeState\n");
#endif

	int instance = INSTANCE;

#ifdef LAZY_STORE
	doStoreRestoreQueue.enQueue(STORE_VALUE);
#else
	doStoreState();
#endif

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting storeState\n");
#endif
}


// restore the stored state after a rejected move
void BeagleCUDAImpl::doRestoreState() {
#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering doRestore\n");
#endif
	// Rather than copying the stored stuff back, just swap the pointers...
	REAL* tmp = dCMatrix;
	dCMatrix = dStoredMatrix;
	dStoredMatrix = tmp;

	tmp = dEvec;
	dEvec = dStoredEvec;
	dStoredEvec = tmp;

	tmp = dIevc;
	dIevc = dStoredIevc;
	dStoredIevc = tmp;

	tmp = dEigenValues;
	dEigenValues = dStoredEigenValues;
	dStoredEigenValues = tmp;

	tmp = dFrequencies;
	dFrequencies = dStoredFrequencies;
	dStoredFrequencies = tmp;

	tmp = dCategoryRates;
	dCategoryRates = dStoredCategoryRates;
	dStoredCategoryRates = tmp;

	tmp = hCategoryRates;
	hCategoryRates = hStoredCategoryRates;
	hStoredCategoryRates = tmp;

	tmp = dCategoryProportions;
	dCategoryProportions
			= dStoredCategoryProportions;
	dStoredCategoryProportions = tmp;

	int* tmp2 = hCurrentMatricesIndices;
	hCurrentMatricesIndices
			= hStoredMatricesIndices;
	hStoredMatricesIndices = tmp2;

	tmp2 = hCurrentPartialsIndices;
	hCurrentPartialsIndices
			= hStoredPartialsIndices;
	hStoredPartialsIndices = tmp2;

#ifdef DYNAMIC_SCALING
	tmp2 = hCurrentScalingFactorsIndices;
	hCurrentScalingFactorsIndices = hStoredScalingFactorsIndices;
	hStoredScalingFactorsIndices = tmp2;
	tmp = dRootScalingFactors;
	dRootScalingFactors = dStoredRootScalingFactors;
	dStoredRootScalingFactors = tmp;
#endif

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting doRestore\n");
#endif

}

// restore the stored state after a rejected move
void BeagleCUDAImpl::restoreState() {
#ifdef DEBUG_FLOW
	fprintf(stderr,"Entering restoreState\n");
#endif

	int instance = INSTANCE;

#ifdef LAZY_STORE
	doStoreRestoreQueue.enQueue(RESTORE_VALUE);
#else
	doRestoreState();
#endif

#ifdef DEBUG_FLOW
	fprintf(stderr,"Exiting restoreState\n");
#endif

}

int BeagleCUDAImpl::printGPUInfo() {
	char* nativeName = "*** Marc is too lazy to write this function!";

	int cDevices;
	CUresult status;
	status = cuInit(0);
	if (CUDA_SUCCESS != status)
		return 0;
	status = cuDeviceGetCount(&cDevices);
	if (CUDA_SUCCESS != status)
		return 0;
	if (cDevices == 0) {
		return 0;
	}

	fprintf(stderr,"GPU Device Information:");

	int iDevice;
	for (iDevice = 0; iDevice < cDevices; iDevice++) {

		char name[256];
		int totalGlobalMemory = 0;
		int clockSpeed = 0;

		// New CUDA functions in cutil.h do not work in JNI files
		getGPUInfo(iDevice, name, &totalGlobalMemory, &clockSpeed);
		fprintf(stderr,"\nDevice #%d: %s\n",(iDevice+1),name);
		double mem = totalGlobalMemory / 1024.0 / 1024.0;
		double clo = clockSpeed / 1000000.0;
		fprintf(stderr,"\tGlobal Memory (MB) : %1.2f\n",mem);
		fprintf(stderr,"\tClock Speed (Ghz)  : %1.2f\n",clo);
	}

	if (cDevices == 0)
		fprintf(stderr,"None found.\n");

	return cDevices;
}

void BeagleCUDAImpl::getGPUInfo(int iDevice, char *name, int *memory, int *speed) {
	cudaDeviceProp deviceProp;
	memset(&deviceProp, 0, sizeof(deviceProp));
	cudaGetDeviceProperties(&deviceProp, iDevice);
	*memory = deviceProp.totalGlobalMem;
	*speed = deviceProp.clockRate;
	strcpy(name, deviceProp.name);
}
