#ifndef _CNOVR_CNFA_H
#define _CNOVR_CNFA_H


//This is the data type for 
//	cnovrLAudioPlay
//	cnovrLAudioRec
//If you are playing back, you should increment cb_no.
//This also makes things further down in the pipe aware it has already been written into.
typedef struct cnovr_audiodataset_t
{
	int cb_no;
	int numframes;
	int channels;
	int16_t * frames;
} cnovr_audiodataset;


struct CNFADriver;

extern struct CNFADriver * CNOVRCNFA;

//If already initialized, will not reinit.
void CNOVRCNFAInit();


#endif
