/*
 *  awave.c
 *  Part of Standing Wave 3
 *  Alchemy <-> AS3 Bridge for audio synthesis 
 *
 *  Created by Max Lord on 11/21/09.
 *
 */

/**
 * AlchemicalWave is the Alchemy CLib of StandingWave. It is accessed through the Sample element.
 * Every Sample in Standing Wave has corresponding sample memory allocated by this lib.
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "AS3.h"
 
float pi    = 3.1415926535897932384626433832795029;
float twopi = 6.2831853071795864769252867665590058;
char trace[100];

static inline float interpolate(float sample1, float sample2, float fraction)
{
	return sample1 + fraction * (sample2-sample1);
}
 
 
/**
 * Returns a pointer to the memory allocated for this sample.
 * Every frame value is a float, as Flash's native sound format is a 32bit float
 *  and we don't want to waste time converting back and forth to doubles.
 * The sample is zeroed.
 * Stereo samples are interleaved.
 */ 
static AS3_Val allocateSampleMemory(void* self, AS3_Val args)
{
	int frames;
	int channels;
	int size;
	float *buffer;
	
	AS3_ArrayValue(args, "IntType, IntType", &frames, &channels);
 
	size = frames * channels * sizeof(float);
	// sprintf(trace, "New sample %d bytes", size);
	// sztrace(trace);
	
	buffer = (float *) malloc(size); 
	memset(buffer, 0, size);
	
	// Return the sample pointer
	return AS3_Int((int)buffer);  
}
 
/**
 * Frees the memory associated with this sample pointer
 */ 
static AS3_Val deallocateSampleMemory(void *self, AS3_Val args)
{
	float *buffer;
	int bufferPosition;
	AS3_ArrayValue(args, "IntType", &bufferPosition);
	buffer = (float *) bufferPosition;
	free(buffer);
	buffer = 0;
	return 0;
} 
 
/**
 * Fast sample sample memory copy
 */
static AS3_Val copy(void *self, AS3_Val args) 
{
	int bufferPosition; int channels; int frames;
	float *buffer;
	int sourceBufferPosition;
	float *sourceBuffer;
	
	AS3_ArrayValue(args, "IntType, IntType, IntType, IntType", &bufferPosition, &sourceBufferPosition, &channels, &frames);
	buffer = (float *) bufferPosition;
	sourceBuffer = (float *) sourceBufferPosition;
	
	memcpy(buffer, sourceBuffer, frames * channels * sizeof(float));
	return 0;
} 
 
/**
 * Converts a Sample at a lower rate (22050 Hz) or lower number of channels (mono)
 *  to the standard Flash sound format (44.1k stereo interleaved).
 * The descriptor in this case represents the sourceBuffer, not the targetBuffer, which is stereo/44.1
 */
static AS3_Val standardize(void *self, AS3_Val args) 
{
	int bufferPosition; int rate; int channels; int frames;
	float *buffer;
	int sourceBufferPosition;
	float *sourceBuffer;
	int count;
	
	AS3_ArrayValue(args, "IntType, IntType, IntType, IntType, IntType", &bufferPosition, &sourceBufferPosition, &channels, &frames, &rate);
	buffer = (float *) bufferPosition;
	sourceBuffer = (float *) sourceBufferPosition;

	// TODO: Implement a nice interpolating upsampling algorithm for conversion from 22050 to 44100

	if (rate == 44100 && channels == 2) {
		// We're already standardized. Just copy the memory
		memcpy(buffer, sourceBuffer, frames * channels * sizeof(float));
	} else if (rate == 22050 && channels == 1) {
		// Upsample and stereoize
		count = frames/2;
		while (count--) {
			*buffer++ = *sourceBuffer;
			*buffer++ = *sourceBuffer;
			*buffer++ = *sourceBuffer; 
			*buffer++ = *sourceBuffer++;
		}		
	} else if (rate == 22050 && channels == 2) {
		// Upsample 
		count = frames/2;
		while (count--) {
			*buffer++ = *sourceBuffer;
			*buffer++ = *(sourceBuffer+1);
			*buffer++ = *sourceBuffer;
			*buffer++ = *(sourceBuffer+1);
			sourceBuffer += 2;
		}		
	} else if (rate == 44100 && channels == 1) {
		// Stereoize
		count = frames;
		while (count--) {
			*buffer++ = *sourceBuffer;
			*buffer++ = *sourceBuffer++;
		}
	}
	return 0;
}
 
/**
 * Set every sample in the range to a fixed value.
 * Useful for function generators of different types, or erasing audio.
 */ 
static AS3_Val setSamples(void *self, AS3_Val args)
{
	float *buffer; int bufferPosition; int channels; int frames;
	double valueArg; float value;	
	int count;
	
	AS3_ArrayValue(args, "IntType, IntType, IntType, DoubleType", &bufferPosition, &channels, &frames, &valueArg);
	buffer = (float *) bufferPosition;
	value = (float) valueArg;
	
	count = frames * channels;
	while (count--) {
		*buffer++ = value;
	}

	return 0;
} 

// Scale all samples
static AS3_Val changeGain(void* self, AS3_Val args)
{
	int bufferPosition; int channels; int frames;
	float *buffer;
	double leftGainArg; double rightGainArg;
	float leftGain; float rightGain;
	int count;
		
	AS3_ArrayValue(args, "IntType, IntType, IntType, DoubleType, DoubleType", &bufferPosition, &channels, &frames, &leftGainArg, &rightGainArg);
	buffer = (float *) bufferPosition;
	leftGain = (float) leftGainArg;
	rightGain = (float) rightGainArg;

	count = frames;
	if (channels == 1) {
		while (count--) {
			*buffer++ *= leftGain; 
		}
	} else if (channels == 2) {
		while (count--) {
			*buffer++ *= leftGain; 
			*buffer++ *= rightGain;
		}
	}
	return 0;
} 

// Mix one buffer into another
static AS3_Val mixIn(void *self, AS3_Val args)
{
	int bufferPosition; int channels; int frames;
	float *buffer;
	int sourceBufferPosition;
	float *sourceBuffer;
	double leftGainArg;
	double rightGainArg;
	float leftGain;
	float rightGain;
	int count;
	
	AS3_ArrayValue(args, "IntType, IntType, IntType, IntType, DoubleType, DoubleType", 
		&bufferPosition, &sourceBufferPosition, &channels, &frames, &leftGainArg, &rightGainArg);
	buffer = (float *) bufferPosition;
	sourceBuffer = (float *) sourceBufferPosition; // this can be passed with an offset to easily mix offset slices of samples
	leftGain = (float) leftGainArg;
	rightGain = (float) rightGainArg;	
	
	// sprintf(trace, "channels=%d, frames=%d, leftGain=%f, rightGain=%f", channels, frames, leftGain, rightGain);
	// sztrace(trace);
	
	count = frames;
	if (channels == 1) {
		while (count--) {
			*buffer++ += *sourceBuffer++ * leftGain; 
		}
	} else if (channels == 2) {
		while (count--) {
			*buffer++ += *sourceBuffer++ * leftGain; 		
			*buffer++ += *sourceBuffer++ * rightGain;
		}
	}
	return 0;
}

/**
 * Mix a mono sample into a stereo sample.
 * Buffer is stereo, and source buffer is mono.
 */
static AS3_Val mixInPan(void *self, AS3_Val args)
{
	int bufferPosition;  int frames;
	float *buffer;
	int sourceBufferPosition;
	float *sourceBuffer;
	double leftGainArg;
	double rightGainArg;
	float leftGain;
	float rightGain;
	int count;
	
	AS3_ArrayValue(args, "IntType, IntType, IntType, DoubleType, DoubleType", 
		&bufferPosition, &sourceBufferPosition, &frames, &leftGainArg, &rightGainArg);
	buffer = (float *) bufferPosition;
	sourceBuffer = (float *) sourceBufferPosition; 
	leftGain = (float) leftGainArg;
	rightGain = (float) rightGainArg;	
	count = frames;
	
	while (count--) {
		*buffer++ += *sourceBuffer * leftGain; 		
		*buffer++ += *sourceBuffer++ * rightGain;
	}

	return 0;
}


/**
 * Multiply (Amplitude modulate) one buffer against another
 */
static AS3_Val multiplyIn(void *self, AS3_Val args)
{
	int bufferPosition; int channels; int frames;
	float *buffer;
	int sourceBufferPosition;
	float *sourceBuffer;
	double gainArg;
	float gain;
	int count;
	
	AS3_ArrayValue(args, "IntType, IntType, IntType, IntType, DoubleType", &bufferPosition, &sourceBufferPosition, &channels, &frames, &gainArg);
	buffer = (float *) bufferPosition;
	sourceBuffer = (float *) sourceBufferPosition; 
	gain = (float) gainArg;
	count = frames;
	
	if (channels == 1) {
		while (count--) {
			*buffer++ *= *sourceBuffer++ * gain; 
		}
	} else if (channels == 2) {
		while (count--) {
			*buffer++ *= *sourceBuffer++ * gain; 		
			*buffer++ *= *sourceBuffer++ * gain;
		}
	}
	return 0;
}

/**
 * Scan in a wavetable. Wavetable should be at least one longer than the table size.
 */
static AS3_Val wavetableIn(void *self, AS3_Val args)
{
	AS3_Val settings;
	int bufferPosition; int channels; int frames;
	float *buffer; 
	int sourceBufferPosition;
	float *sourceBuffer;
	int pitchTablePosition;
	float *pitchTable;
	double phaseArg; float phase;
	double phaseAddArg; float phaseAdd;
	double phaseResetArg; float phaseReset;
	int tableSize;
	int count; 
	int intPhase;
	float *wavetablePosition;
	
	AS3_ArrayValue(args, "IntType, IntType, IntType, IntType, AS3ValType", 
		&bufferPosition, &sourceBufferPosition, &channels, &frames, &settings);
	AS3_ObjectValue(settings, "tableSize:IntType, phase:DoubleType, phaseAdd:DoubleType, phaseReset:DoubleType, pitchTable:IntType",
		&tableSize, &phaseArg, &phaseAddArg, &phaseResetArg, &pitchTablePosition);

	buffer = (float *) bufferPosition;
	sourceBuffer = (float *) sourceBufferPosition; 
	pitchTable = (float *) pitchTablePosition;
	phaseAdd = (float) phaseAddArg * tableSize; // num source frames to add per output frames
	phase = (float) phaseArg * tableSize; // translate into a frame count into the table
	phaseReset = (float) phaseResetArg * tableSize;
		
	// pitch table based manipulation is still unimplemented	
		
	// Make sure we got everything right
	// sprintf(trace, "Wavetable size=%d phase=%f phaseAdd=%f", tableSize, phase, phaseAdd);
	// sztrace(trace);	
				
	// phase goes from 0 to tableSize
	count=frames;
	if (channels == 1) {
		while (count--) {
			while (phase >= tableSize) {
				phase -= tableSize; // wrap phase to the loop point
				phase += phaseReset;
			}
			intPhase = floor(phase); // int phase
			wavetablePosition = sourceBuffer + intPhase;
			*buffer++ = interpolate(*wavetablePosition, *(wavetablePosition+1), phase - intPhase);
			// sprintf(trace, "out=%f", *(buffer-1));
			// sztrace(trace);
			phase += phaseAdd; 
		}
	} else if (channels == 2 ) {
		while (count--) {
			while (phase >= tableSize) {
				phase -= tableSize; // wrap phase to the loop point
				phase += phaseReset;
			}
			intPhase = floor(phase*0.5)*2; // int phase, round to even frames, for each stereo frame pair
			wavetablePosition = sourceBuffer + intPhase;
			*buffer++ = interpolate(*wavetablePosition, *(wavetablePosition+2), phase - intPhase);
			*buffer++ = interpolate(*(wavetablePosition+1), *(wavetablePosition+3), phase - intPhase);
			phase += phaseAdd;
		}
	}
	
	return 0;
}

static AS3_Val delay(void *self, AS3_Val args)
{
	int bufferPosition; int channels; int frames; int count; 
	float *buffer; 
	int ringBufferPosition;
	float *ringBuffer;
	AS3_Val settings;
	int length;
	double feedbackArg; float feedback;
	double dryMixArg; float dryMix;
	double wetMixArg; float wetMix;
	int offset = 0;
	float echo;
	float *echoPointer;
	
	// Extract	args
	AS3_ArrayValue(args, "IntType, IntType, IntType, IntType, AS3ValType", 
	    &bufferPosition, &ringBufferPosition, &channels, &frames,  &settings);
	AS3_ObjectValue(settings, "length:IntType, dryMix:DoubleType, wetMix:DoubleType, feedback:DoubleType",
		&length, &dryMixArg, &wetMixArg, &feedbackArg);
	// Cast arguments to the needed types
	buffer = (float *) bufferPosition;
	ringBuffer = (float *) ringBufferPosition;
	dryMix = (float) dryMixArg;
	wetMix = (float) wetMixArg;
	feedback = (float) feedbackArg;
	
	// Show params
	// sprintf(trace, "Echo length=%d, dry=%f, wet=%f, fb=%f", length, dryMix, wetMix, feedback); 
	// sztrace(trace);
	
	if (channels == 1) {
		count = frames;
		while (count--) {
			if (offset > length) { 
				offset = 0; 
			}
			echoPointer = ringBuffer + offset;
			echo = *echoPointer;
			*echoPointer = *buffer + echo*feedback;
			*buffer = *buffer * dryMix + echo * wetMix;
			buffer++; offset++;
		}		
	} else if (channels == 2) {
		count = frames*2;
		while (count--) {
			if (offset > length) { 
				offset = 0; 
			}
			echoPointer = ringBuffer + offset;
			echo = *echoPointer;
			*echoPointer = *buffer + (echo * feedback);
			*buffer = *buffer * dryMix + echo * wetMix;
			buffer++; offset++;
		}		
	}
	
	// Shift the memory so that the echo pointer offset is the start of the buffer
	
	int ringSize = length * channels * sizeof(float);
	int firstChunkSize = offset * channels * sizeof(float);
	int secondChunkSize = ringSize - firstChunkSize;
	float *temp = (float *) malloc(ringSize);
	
	// copy offset-end -> start of temp buffer	
	memcpy(temp, ringBuffer + offset, secondChunkSize);

	// copy start-offset -> second half of temp buffer
	memcpy(temp + offset, ringBuffer, firstChunkSize);
	
	// copy temp buffer back to ringbuffer
	memcpy(ringBuffer, temp, ringSize); 
	free(temp);
	
	return 0;
	
}

/* biquad(samplePointer, stateBuffer, coefficients, rate, channels, frames) */ 

static AS3_Val biquad(void *self, AS3_Val args)
{
	int bufferPosition;  int channels; int frames; int count; 
	float *buffer; 
	int stateBufferPosition;
	float *stateBuffer;
	
	AS3_Val coeffs; // coefficients object
	double a0d, a1d, a2d, b0d, b1d, b2d; // doubles from object
	float a0, a1, a2, b0, b1, b2; // filter coefficients
	float lx, ly, lx1, lx2, ly1, ly2; // left delay line 
	float rx, ry, rx1, rx2, ry1, ry2; // right delay line 
	
	// Extract args
	AS3_ArrayValue(args, "IntType, IntType, IntType, IntType, AS3ValType", 
		&bufferPosition, &stateBufferPosition, &channels, &frames, &coeffs);
	buffer = (float *) bufferPosition;
	stateBuffer = (float *) stateBufferPosition;	
		
	// Extract filter coefficients from object	
	AS3_ObjectValue(coeffs, "a0:DoubleType, a1:DoubleType, a2:DoubleType, b0:DoubleType, b1:DoubleType, b2:DoubleType",
		&a0d, &a1d, &a2d, &b0d, &b1d, &b2d);
	// Cast to floats
	a0 = (float) a0d; a1 = (float) a1d; a2 = (float) a2d; 	
	b0 = (float) b0d; b1 = (float) b1d; b2 = (float) b2d; 	

	// Make sure we recieved all the correct coefficients 
	// sprintf(trace, "Biquad a0=%f a1=%f a2=%f b0=%f b1=%f b2=%f", a0, a1, a2, b0, b1, b2);
	// sztrace(trace);
		
	count = frames;

	if (channels == 1) {
		lx1 = *stateBuffer;
		lx2 = *(stateBuffer+1);
		ly1 = *(stateBuffer+2);
		ly2 = *(stateBuffer+3);
		while (count--) {
			lx = *buffer; // input
            ly = lx*b0 + lx1*b1 + lx2*b2 - ly1*a1 - ly2*a2;
			lx2 = lx1;
			lx1 = lx;
			ly2 = ly1;
			ly1 = ly;
            *buffer++ = ly; // output
		}
		*stateBuffer = lx1;
		*(stateBuffer+1) = lx2;
		*(stateBuffer+2) = ly1;
		*(stateBuffer+3) = ly2;
	} else if (channels == 2) {
		lx1 = *stateBuffer;
		rx1 = *(stateBuffer+1);
		lx2 = *(stateBuffer+2);
		rx2 = *(stateBuffer+3);
		ly1 = *(stateBuffer+4);
		ry1 = *(stateBuffer+5);
		ly2 = *(stateBuffer+6);
		ry2 = *(stateBuffer+7);
		while (count--) {
			lx = *buffer; // left input
            ly = lx*b0 + lx1*b1 + lx2*b2 - ly1*a1 - ly2*a2;
			lx2 = lx1;
			lx1 = lx;
			ly2 = ly1;
			ly1 = ly;
            *buffer++ = ly; // left output
			rx = *buffer; // right input
            ry = rx*b0 + rx1*b1 + rx2*b2 - ry1*a1 - ry2*a2;
			rx2 = rx1;
			rx1 = rx;
			ry2 = ry1;
			ry1 = ry;
            *buffer++ = ry; // right output
		}
		*stateBuffer = lx1;
		*(stateBuffer+1) = rx1;
		*(stateBuffer+2) = lx2;
		*(stateBuffer+3) = rx2;
		*(stateBuffer+4) = ly1;
		*(stateBuffer+5) = ry1;
		*(stateBuffer+6) = ly2;
		*(stateBuffer+7) = ry2;
	}

	return 0;
}

int main()
{
	// Create method objects
	AS3_Val allocateSampleMemoryMethod = AS3_Function(NULL, allocateSampleMemory);
	AS3_Val deallocateSampleMemoryMethod = AS3_FunctionAsync(NULL, deallocateSampleMemory); //  deallocate asynchronously seems to work better!
	AS3_Val setSamplesMethod = AS3_Function(NULL, setSamples);
	AS3_Val changeGainMethod = AS3_Function(NULL, changeGain);
	AS3_Val copyMethod = AS3_Function(NULL, copy);
	AS3_Val mixInPanMethod = AS3_Function(NULL, mixInPan);
	AS3_Val mixInMethod = AS3_Function(NULL, mixIn);
	AS3_Val multiplyInMethod = AS3_Function(NULL, multiplyIn);
	AS3_Val standardizeMethod = AS3_Function(NULL, standardize);
	AS3_Val wavetableInMethod = AS3_Function(NULL, wavetableIn);
	AS3_Val delayMethod = AS3_Function(NULL, delay);
	AS3_Val biquadMethod = AS3_Function(NULL, biquad);
  
	// Make a ginormous object with all the lib's methods
	AS3_Val result = AS3_Object("allocateSampleMemory:AS3ValType, setSamples:AS3ValType, deallocateSampleMemory:AS3ValType, copy:AS3ValType, changeGain:AS3ValType, mixIn:AS3ValType, mixInPan:AS3ValType, multiplyIn:AS3ValType, standardize:AS3ValType, wavetableIn:AS3ValType, delay:AS3ValType, biquad:AS3ValType", 
		allocateSampleMemoryMethod, setSamplesMethod, deallocateSampleMemoryMethod, copyMethod, changeGainMethod, mixInMethod, mixInPanMethod, multiplyInMethod, standardizeMethod, wavetableInMethod, delayMethod, biquadMethod);
	 
	// Free memory
	AS3_Release(allocateSampleMemoryMethod);
	AS3_Release(deallocateSampleMemoryMethod);
	AS3_Release(setSamplesMethod);
	AS3_Release(copyMethod);
	AS3_Release(changeGainMethod);
	AS3_Release(mixInMethod);
	AS3_Release(mixInPanMethod);
	AS3_Release(multiplyInMethod);
	AS3_Release(standardizeMethod);
	AS3_Release(wavetableInMethod);
	AS3_Release(delayMethod);
	AS3_Release(biquadMethod);
	
	// notify that we initialized -- THIS DOES NOT RETURN!
	AS3_LibInit(result);
 
	return 0;
}