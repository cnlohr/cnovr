/*
	A basic draggable camera as well as an advanced camera for live compositing
*/

#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovropenvr.h>
#include <cnovr.h>
#include <string.h>
#include <cnovrutil.h>
#include <chew.h>
#include <cnovrcanvas.h>

#define CNV4L2_NOSTAT
#include "cntools/cnv4l2/cnv4l2.c"

//#define SECTIONDEBUG

const char *  identifier;
cnovr_shader * shaderrendermodel;
cnovr_shader * shaderlines;
cnovr_shader * shaderblack;
cnovr_shader * previewcomposite;
cnovr_shader * previewbasic;
cnovr_model *  modelcameralines;
cnovr_model *  modelcamerasolid;
cnovrfocus_capture capture;

cnovr_canvas * canvascontrol;
cnovr_canvas * canvaspreview;
cnovr_canvas * canvasvideo;
cnovr_rf_buffer * previewtarget[3]; //Background, foreground, and final

cnv4l2 * v4l2interface;

struct camerastore_t
{
	int initialized;
	cnovr_pose  posecamera;
	int use_paired_object;
	char paired_object_serial_number[64];
	cnovr_pose  paired_relative_offset_pose;
} * store;

int paired_object_id;

int cal_cam_controller;
int cal_cam_stage = 0;
cnovr_point3d cal_cam_points[3];
#define CAM_CAL_EDGE_DIST_RATIO 0.0

//Possible identifier options... "advanced,preview"
int advanced_view;
int preview_view;
float fPreviewForegroundSplitDistance = 10;
int previewW;
int previewH;

int videoW = 1920;
int videoH = 1080;

int pboid;
int dragging_camera = 0;
void * mapptr;
uint8_t * lastdat;
int quit = 0;
int framegrabbed;
og_thread_t videodatathread;
float videoosdvals[4];
//og_thread_t camerabusinessthread;


const cnovr_canvas_canned_gui_element cvp_main_menu[];

char camstatustext[63];
char fovsettext[63];
char notetext[63];
char devlist[MAX_POSES_TO_PULL_FROM_OPENVR][32];

void CalibrateCameraFromTrackingPoints( cnovr_pose * pcamout, cnovr_point3d * cal_cam_points )
{
	//Good: ORIGINAL XFORM: 1.752586 1.646361 1.247994   //  -0.977304 0.043990 -0.204548 -0.033270 // 1.000000


	//Ok, this is hard.
	printf( "ORIGINAL XFORM: %f %f %f   //  %f %f %f %f // %f\n", PFTHREE( pcamout->Pos ), PFFOUR( pcamout->Rot ), pcamout->Scale );
	printf( "POINTS: \n%f %f %f\n%f %f %f\n%f %f %f\n", PFTHREE( (cal_cam_points[0]) ), PFTHREE( (cal_cam_points[1]) ), PFTHREE( (cal_cam_points[2]) ) );
	cnovr_point3d vsa;
	cnovr_point3d vsb;
	sub3d( vsa, cal_cam_points[1], cal_cam_points[0] );
	sub3d( vsb, cal_cam_points[2], cal_cam_points[0] );
	normalize3d( vsa, vsa );
	normalize3d( vsb, vsb );
	cnovr_point3d vec_fwd;
	add3d( vec_fwd, vsa, vsb );
	normalize3d( vec_fwd, vec_fwd );
	printf(" VSA: %f %f %f VSB: %f %f %f  VF: %f %f %f\n", PFTHREE( vsa ), PFTHREE( vsb ), PFTHREE( vec_fwd ) );
	//BELOW is trying to find based on a diagonal.  That is a pain.  Let's try horizontal.

	//Ok, we have an origin, a vector for forward... Now... fov and up
	///also, we know CAM_CAL_EDGE_DIST_RATIO, which is usually about 0.1
#if 0
	float calfov = acos( dot3d( vsa, vsb ) );
	float diagfov = calfov / (1.-CAM_CAL_EDGE_DIST_RATIO);
	float camaspect = videoW/((float)videoH);
	float verticalFOV = 2.0*atan(tan(diagonalFOV)/sqrt(1 + (camAspect*camAspect)));
	cnovrstate->fPreviewFOV = verticalFOV;

	//Now we know forward and a point... how to compute up? 
	//Maybe compute the cross of the diagonals?
	//This should be in-line with the view plane.
#endif
	copy3d( pcamout->Pos, cal_cam_points[0] );

	scale3d( vec_fwd, vec_fwd, -1 );

	cnovr_point3d vec_up;
	cross3d( vec_up, vsb, vsa );
	normalize3d( vec_up, vec_up );
	cnovr_point3d vec_side;
	cross3d( vec_side, vec_up, vec_fwd );
	normalize3d( vec_side, vec_side );

	printf( "Genning Quat from: \n%f %f %f\n%f %f %f\n%f %f %f\n", PFTHREE( vec_side ), PFTHREE( vec_up ), PFTHREE( vec_fwd ) );
	float m44[16];
	copy3d( m44+0, vec_side ); m44[3] = 0;
	copy3d( m44+4, vec_up ); m44[7] = 0;
	copy3d( m44+8, vec_fwd ); m44[11] = 0;
	m44[12] = m44[13] = m44[14] = 0; m44[15] = 1;
	//matrix44transposeself( m44 );
	matrix44print( m44 );

	quatfrommatrix( pcamout->Rot, m44 );
	float f0 = pcamout->Rot[0];
	float f1 = pcamout->Rot[1];
	float f2 = pcamout->Rot[2];
	float f3 = pcamout->Rot[3];
	pcamout->Rot[0] = -f0; //?!?!?!
	pcamout->Rot[1] = f1;
	pcamout->Rot[2] = f2;
	pcamout->Rot[3] = f3;
	pcamout->Scale = 1;
	quatnormalize( pcamout->Rot, pcamout->Rot );

	printf( "QUAT: %f %f %f %f\n", PFFOUR( pcamout->Rot ) );
	printf( "Dot apart: %f\n", dot3d( vsa, vsb ) );
	float calfov = acos( dot3d( vsa, vsb ) ) * 1080./1920.;
	printf( "Calfov: %f\n", calfov );
	cnovrstate->fPreviewFOV = 180.0 * calfov / (1.-CAM_CAL_EDGE_DIST_RATIO) / 3.14159;
	printf( "fPreviewFOV: %f\n", cnovrstate->fPreviewFOV );
}

void UpdateMenuStatuses()
{
	sprintf( camstatustext, "CAM TRK: %s\n", store->paired_object_serial_number );
	sprintf( fovsettext, "FOV: %3.1f", cnovrstate->fPreviewFOV );
	if( canvascontrol && canvascontrol->pCannedGUI )
		CNOVRCanvasApplyCannedGUI( canvascontrol, canvascontrol->pCannedGUI );
	int i;
	for( i = 0; i < MAX_POSES_TO_PULL_FROM_OPENVR; i++ )
	{
		char * stk = cnovrstate->asTrackedDeviceSerialStrings[i];
		if( stk ) stk+= 2;
		int r = snprintf( devlist[i], 26, "%s: %s\n", stk, cnovrstate->asTrackedDeviceModelStrings[i] );
		devlist[i][r] = 0;
	}
}

void coarse_fov( struct cnovr_canvas_t * canvas, cnovr_canvas_canned_gui_element * elem, int rx, int ry, cnovrfocus_event event, int devid ) { 
	cnovrstate->fPreviewFOV = rx + 10;
	UpdateMenuStatuses();
}
void fine_fov( struct cnovr_canvas_t * canvas, cnovr_canvas_canned_gui_element * elem, int rx, int ry, cnovrfocus_event event, int devid ) { 
	cnovrstate->fPreviewFOV += elem->iopaque/10.0;
	UpdateMenuStatuses();
}

void changemenu( struct cnovr_canvas_t * canvas, cnovr_canvas_canned_gui_element * elem, int rx, int ry, cnovrfocus_event event, int devid ) { 
	UpdateMenuStatuses();
	CNOVRCanvasApplyCannedGUI( canvascontrol, elem->vopaque );
}

void resetcam( struct cnovr_canvas_t * canvas, cnovr_canvas_canned_gui_element * elem, int rx, int ry, cnovrfocus_event event, int devid ) { 
	store->use_paired_object = 0;
	paired_object_id = -1;
	pose_make_identity( &store->posecamera );
	memset( store->paired_object_serial_number, 0, sizeof( store->paired_object_serial_number ) );
	sprintf( notetext, "Reset.\n" );
	CNOVRCanvasApplyCannedGUI( canvascontrol, cvp_main_menu );
}

void attachtodev( struct cnovr_canvas_t * canvas, cnovr_canvas_canned_gui_element * elem, int rx, int ry, cnovrfocus_event event, int devid ) { 
	int a = elem->iopaque;
	if( a < 0 )
	{
		store->use_paired_object = 0;
		paired_object_id = -1;
		memset( store->paired_object_serial_number, 0, sizeof( store->paired_object_serial_number ) );
		sprintf( notetext, "Disassociated.\n" );
		CNOVRCanvasApplyCannedGUI( canvascontrol, cvp_main_menu );
	}
	else
	{
		//We are attaching to a device.
		//Figure out where we are in the device's coordinate frame based on our camera's location.
		if( cnovrstate->bTrackedPosesValid[a] )
		{
			sprintf( notetext, "Associated.\n" );
			unapply_pose_from_pose( &store->paired_relative_offset_pose, &cnovrstate->pTrackedPoses[a], &store->posecamera );
			store->use_paired_object = 1;
			paired_object_id = a;
			strcpy( store->paired_object_serial_number, cnovrstate->asTrackedDeviceSerialStrings[a] );
			CNOVRCanvasApplyCannedGUI( canvascontrol, cvp_main_menu );
		}
		else
		{
			//Untrackable..
			sprintf( notetext, "Untrackable.\n" );
			CNOVRCanvasApplyCannedGUI( canvascontrol, cvp_main_menu );
		}
	}
	UpdateMenuStatuses();
}

#define MENUH 16
#define MENUY(y) ((MENUH*(y))+2)
#define DEF_WIDTH (160-26)
#define EXTRA_WIDTH (160-4)

const cnovr_canvas_canned_gui_element cvp_tweak[] = {
	{ .x = 2, .y = MENUY(0), .w = 90, .h = 14, .cb = 0, .text = camstatustext },
	{ .x = 2, .y = MENUY(1), .w = EXTRA_WIDTH, .h = 14, .cb = 0, .text = "Main Menu", .cb = changemenu, .vopaque = cvp_main_menu },
	{ .x = 12, .y = MENUY(2), .w = DEF_WIDTH, .h = 14, .cb = 0, .text = fovsettext, .cb = coarse_fov, .allowdrag = 1 },
	{ .x = 2, .y = MENUY(2), .w = 8, .h = 14, .cb = 0, .text = "-", .cb = fine_fov, .iopaque = -1 },
	{ .x = 160-12, .y = MENUY(2), .w = 10, .h = 14, .cb = 0, .text = "+", .cb = fine_fov, .iopaque = 1  },
	{ .x = 2, .y = MENUY(3), .w = EXTRA_WIDTH, .h = 14, .cb = 0, .text = "Reset Cam", .cb = resetcam },
	{ .cb = 0, .w = 0, .h = 0 }
};


const cnovr_canvas_canned_gui_element cvp_dev_menu[] = {
	{ .x = 2, .y = 2, .w = 90, .h = 14, .cb = 0, .text = camstatustext },
	{ .x = 2, .y = MENUY(1), .w = EXTRA_WIDTH, .h = 14, .text = "Main Menu", .cb = changemenu, .vopaque = cvp_main_menu },
	{ .x = 2, .y = MENUY(2), .w = EXTRA_WIDTH, .h = 14, .text = devlist[0], .cb = attachtodev, .iopaque = 0 },
	{ .x = 2, .y = MENUY(3), .w = EXTRA_WIDTH, .h = 14, .text = devlist[1], .cb = attachtodev, .iopaque = 1 },
	{ .x = 2, .y = MENUY(4), .w = EXTRA_WIDTH, .h = 14, .text = devlist[2], .cb = attachtodev, .iopaque = 2 },
	{ .x = 2, .y = MENUY(5), .w = EXTRA_WIDTH, .h = 14, .text = devlist[3], .cb = attachtodev, .iopaque = 3 },
	{ .x = 2, .y = MENUY(6), .w = EXTRA_WIDTH, .h = 14, .text = devlist[4], .cb = attachtodev, .iopaque = 4 },
	{ .x = 2, .y = MENUY(7), .w = EXTRA_WIDTH, .h = 14, .text = devlist[5], .cb = attachtodev, .iopaque = 5 },
	{ .x = 2, .y = MENUY(8), .w = EXTRA_WIDTH, .h = 14, .text = devlist[6], .cb = attachtodev, .iopaque = 6 },
	{ .x = 2, .y = MENUY(9), .w = EXTRA_WIDTH, .h = 14, .text = devlist[7], .cb = attachtodev, .iopaque = 7 },
	{ .x = 2, .y = MENUY(10), .w = EXTRA_WIDTH, .h = 14, .text = devlist[8], .cb = attachtodev, .iopaque = 8 },
	{ .x = 2, .y = MENUY(11), .w = EXTRA_WIDTH, .h = 14, .text = "Detach", .cb = attachtodev, .iopaque = -1 },
	{ .cb = 0, .w = 0, .h = 0 }
};

void stopcamcal( struct cnovr_canvas_t * canvas, cnovr_canvas_canned_gui_element * elem, int rx, int ry, cnovrfocus_event event, int devid ) { 
	cal_cam_stage = 0;
	CNOVRCanvasApplyCannedGUI( canvascontrol, cvp_main_menu );
}

const cnovr_canvas_canned_gui_element cvp_cal1[] = {
	{ .x = 2, .y = 2, .w = 90, .h = 14, .cb = 0, .text = camstatustext },
	{ .x = 2, .y = MENUY(1), .w = EXTRA_WIDTH, .h = 14, .cb = 0, .text = "Main Menu", .cb = stopcamcal },
	{ .x = 2, .y = MENUY(2), .w = EXTRA_WIDTH, .h = 14, .cb = 0, .text = "Use OSD. Click to Go.", .iopaque = 0 },
	{ .x = 2, .y = MENUY(11), .w = 90, .h = 14, .cb = 0, .text = notetext },
	{ .cb = 0, .w = 0, .h = 0 }
};

void startcamcal( struct cnovr_canvas_t * canvas, cnovr_canvas_canned_gui_element * elem, int rx, int ry, cnovrfocus_event event, int devid ) { 
	cal_cam_stage = 1;
	cal_cam_controller = devid;
	CNOVRCanvasApplyCannedGUI( canvascontrol, cvp_cal1 );
}


const cnovr_canvas_canned_gui_element cvp_main_menu[] = {
	{ .x = 2, .y = 2, .w = 90, .h = 14, .cb = 0, .text = camstatustext },
	{ .x = 2, .y = MENUY(1), .w = EXTRA_WIDTH, .h = 14, .text = "Tweak Cam", .cb = changemenu, .vopaque = cvp_tweak },
	{ .x = 2, .y = MENUY(2), .w = EXTRA_WIDTH, .h = 14, .text = "Attach To Dev", .cb = changemenu, .vopaque = cvp_dev_menu },
	{ .x = 2, .y = MENUY(3), .w = EXTRA_WIDTH, .h = 14, .text = "Cal Cam", .cb = startcamcal, .vopaque = cvp_cal1 },
	{ .x = 2, .y = MENUY(11), .w = 90, .h = 14, .cb = 0, .text = notetext },
	{ .cb = 0, .w = 0, .h = 0 }
};



void init( const char * identifier )
{
}

void v4l2framecb( struct cnv4l2_t * match, uint8_t * payload, int payloadlen )
{
	//Stream new data to GPU
	//lastdat = payload;
	if( framegrabbed )
	{
		ovrprintf( "Pending camera frame transfer.  Frame dropped\n" );
		return;
	}
	if( mapptr ) memcpy( mapptr, payload, payloadlen );
	framegrabbed = 1;
}

void * videodatathreadfunction( void * v )
{
	if( !v4l2interface ) return 0;
	while( !quit )
	{
		int r = cnv4l2_read( v4l2interface, 100 );
		if( r < 0 )
		{
			printf( "SERIOUS CAMERA FAULT\n" );
			return 0;
		}
	}
	return 0;
}


void PrerenderFunction( void * tag, void * opaquev )
{
	#ifdef SECTIONDEBUG
		printf( "PrerenderFunction start\n" );
	#endif

	static int did_set_aspect_ratio;

	if( cal_cam_stage )
	{
		cnovrfocus_properties * prop = CNOVRFocusGetPropsForDev( cal_cam_controller );
		switch( cal_cam_stage )
		{
			case 1:
			case 2:
			case 3:
				videoosdvals[0] = 1;
				videoosdvals[1] = 0.5;
				videoosdvals[2] = 0.5;
				videoosdvals[3] = 0;
				if( !( prop->buttonmask[0] & 1 ) && cal_cam_stage == 1 )
					cal_cam_stage = 2;
				if( ( prop->buttonmask[0] & 1 ) && cal_cam_stage == 2 )
					cal_cam_stage = 3;
				if( !( prop->buttonmask[0] & 1 ) && cal_cam_stage == 3 )
				{
					copy3d( cal_cam_points[0], prop->poseTip.Pos );
					cal_cam_stage = 4;
				}
				break;
			case 4:
			case 5:
				videoosdvals[0] = 1;
				videoosdvals[1] = CAM_CAL_EDGE_DIST_RATIO;
				videoosdvals[2] = 0.5;
				videoosdvals[3] = 0;
				if( ( prop->buttonmask[0] & 1 ) && cal_cam_stage == 4 )
					cal_cam_stage = 5;
				if( !( prop->buttonmask[0] & 1 ) && cal_cam_stage == 5 )
				{
					copy3d( cal_cam_points[1], prop->poseTip.Pos );
					cal_cam_stage = 6;
				}
				break;
			case 6:
			case 7:
				videoosdvals[0] = 1;
				videoosdvals[1] = 1.0-CAM_CAL_EDGE_DIST_RATIO;
				videoosdvals[2] = 0.5;
				videoosdvals[3] = 0;
				if( ( prop->buttonmask[0] & 1 ) && cal_cam_stage == 6 )
					cal_cam_stage = 7;
				if( !( prop->buttonmask[0] & 1 ) && cal_cam_stage == 7 )
				{
					copy3d( cal_cam_points[2], prop->poseTip.Pos );
					//Actually perform calibration.
					CalibrateCameraFromTrackingPoints( &store->posecamera, cal_cam_points );
					unapply_pose_from_pose( &store->paired_relative_offset_pose, &cnovrstate->pTrackedPoses[paired_object_id], &store->posecamera );

					cal_cam_stage = 0;
				}
				break;
		}
	}
	else
	{
		videoosdvals[0] = 0;
		videoosdvals[1] = 0;
		videoosdvals[2] = 0;
		videoosdvals[3] = 0;
	}

	//Need to figure out what our paired object is.
	if( store->use_paired_object && paired_object_id < 0 )
	{
		int i;
		for( i = 0; i < MAX_POSES_TO_PULL_FROM_OPENVR; i++ )
		{
			const char * sernum = cnovrstate->asTrackedDeviceSerialStrings[i];
			if( sernum )
			{
				if( strcmp( sernum, store->paired_object_serial_number ) == 0 )
				{
					paired_object_id = i;
					break;
				}
			}
		}
		printf( "PAIR ATTEMPT: %d\n", paired_object_id );
	}

	if( paired_object_id >= 0 && !dragging_camera )
	{
		apply_pose_to_pose( &store->posecamera, &cnovrstate->pTrackedPoses[paired_object_id], &store->paired_relative_offset_pose );
		pose_invert( &cnovrstate->pPreviewPose, &store->posecamera );
	}

	if( v4l2interface )
	{
		uint32_t textureid;
		if( !canvasvideo || !canvasvideo->model->pTextures[0] ||
			!(textureid = canvasvideo->model->pTextures[0]->nTextureId) ) return;
		glBindTexture( GL_TEXTURE_2D, textureid );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

		if( framegrabbed )
		{
			if( !did_set_aspect_ratio )
			{
				did_set_aspect_ratio = 1;
				CNOVRCanvasSetPhysicalSize( canvasvideo, videoW/(float)videoH, 1.0 );
			}
			if( !pboid )
			{
				glGenBuffers( 1, &pboid );
				glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pboid );
				glBufferData(GL_PIXEL_UNPACK_BUFFER, videoW*videoH*2, NULL, GL_STREAM_DRAW);
				mapptr = glMapBufferRange( GL_PIXEL_UNPACK_BUFFER, 0, videoW*videoH*4/2, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
				glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
			}
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pboid ); 
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER );
			glBindTexture( GL_TEXTURE_2D, textureid );
			glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, videoW/2, videoH, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glBindTexture( GL_TEXTURE_2D, 0 );
			mapptr = glMapBufferRange( GL_PIXEL_UNPACK_BUFFER, 0, videoW*videoH*4/2, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 ); 
			framegrabbed = 0;
		}
	}
	#ifdef SECTIONDEBUG
		printf( "PrerenderFunction end\n" );
	#endif

}

void AdvancedPreviewRender( void * tag, void * opaquev )
{
	#ifdef SECTIONDEBUG
		printf( "AdvancedPreviewRender start\n" );
	#endif

	if( cnovrstate->iPreviewHeight == 0 || cnovrstate->iPreviewWidth == 0 ) return;
	if( advanced_view )
	{
		if( cnovrstate->iPreviewHeight != previewH || cnovrstate->iPreviewWidth != previewW || !previewtarget[0] || !previewtarget[1] || !previewtarget[2] )
		{
			previewW = cnovrstate->iPreviewWidth;
			previewH = cnovrstate->iPreviewHeight;
			if( previewtarget[0] ) CNOVRDelete( previewtarget[0] );
			if( previewtarget[1] ) CNOVRDelete( previewtarget[1] );
			if( previewtarget[2] ) CNOVRDelete( previewtarget[2] );
			previewtarget[0] = CNOVRRFBufferCreate( previewW, previewH, cnovrstate->multisample );
			previewtarget[1] = CNOVRRFBufferCreate( previewW, previewH, cnovrstate->multisample );
			previewtarget[2] = CNOVRRFBufferCreate( previewW, previewH, 0 );
			CNOVRCanvasSetPhysicalSize( canvaspreview, previewW/(float)previewH, -1.0 );
			CNOVRCanvasYFlip( canvaspreview, 1 );
		}

		//Find cutting distance.
		cnovr_point3d fvcamdist;
		apply_pose_to_point(fvcamdist, &cnovrstate->pPreviewPose, cnovrstate->pTrackedPoses[0].Pos);
		fPreviewForegroundSplitDistance = -fvcamdist[2];

		int width = cnovrstate->iRTWidth = previewW;
		int height = cnovrstate->iRTHeight = previewH;
		cnovrstate->eyeTarget = 2;
		matrix44identity( cnovrstate->mModel );
		pose_to_matrix44( cnovrstate->mView, &cnovrstate->pPreviewPose );
		int i;
		float distancecut = fPreviewForegroundSplitDistance;
		for( i = 0; i < 2; i++ )
		{
			CNOVRFBufferActivate( previewtarget[i] );
			matrix44perspective( cnovrstate->mPerspective, cnovrstate->fPreviewFOV, width/(float)height, i?cnovrstate->fNear:distancecut, i?distancecut:cnovrstate->fFar );
			glViewport(0, 0, width, height );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			CNOVRListCall( cnovrLRender0, 0, 0); 
			CNOVRListCall( cnovrLRender1, 0, 0); 
			CNOVRListCall( cnovrLRender2, 0, 0); 
			CNOVRListCall( cnovrLRender3, 0, 0); 
			CNOVRListCall( cnovrLRender4, 0, 0); 
			CNOVRFBufferBlitResolve( previewtarget[i] );
		}
		glDisable( GL_DEPTH_TEST );

		CNOVRFBufferActivate( previewtarget[2] );
	//	glClearColor( 1., 0., 0., 1. );
		glViewport(0, 0, width, height );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		glEnable( GL_TEXTURE_2D );
		glActiveTextureCHEW( GL_TEXTURE0 + 0 );
		glBindTexture( GL_TEXTURE_2D, previewtarget[0]->nResolveTextureId );
		glActiveTextureCHEW( GL_TEXTURE0 + 1 );
		glBindTexture( GL_TEXTURE_2D, previewtarget[1]->nResolveTextureId );
		CNOVRRender( previewcomposite );
		CNOVRRender( cnovrstate->fullscreengeo );
		CNOVRFBufferBlitResolve( previewtarget[2] );

		CNOVRRender( previewbasic );
		glEnable( GL_TEXTURE_2D );
		glActiveTextureCHEW( GL_TEXTURE0 + 0 );
		glBindTexture( GL_TEXTURE_2D, previewtarget[2]->nResolveTextureId );
		CNOVRRender( cnovrstate->fullscreengeo );

		//glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	}
	else
	{
		int width = cnovrstate->iPreviewWidth;
		int height = cnovrstate->iPreviewHeight;
		cnovrstate->eyeTarget = 2;
		matrix44identity( cnovrstate->mModel );
		pose_to_matrix44( cnovrstate->mView, &cnovrstate->pPreviewPose );
		matrix44perspective( cnovrstate->mPerspective, cnovrstate->fPreviewFOV, width/(float)height, cnovrstate->fNear, cnovrstate->fFar );
		glViewport(0, 0, width, height );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		CNOVRListCall( cnovrLRender0, 0, 0); 
		CNOVRListCall( cnovrLRender1, 0, 0); 
		CNOVRListCall( cnovrLRender2, 0, 0); 
		CNOVRListCall( cnovrLRender3, 0, 0); 
		CNOVRListCall( cnovrLRender4, 0, 0); 
	}

	#ifdef SECTIONDEBUG
		printf( "AdvancedPreviewRender end\n" );
	#endif
}

void RenderFunction( void * tag, void * opaquev )
{
	#ifdef SECTIONDEBUG
		printf( "RenderFunction start\n" );
	#endif


	int i;
	CNOVRRender( shaderlines );
	CNOVRModelRenderWithPose( modelcameralines, &store->posecamera );
	CNOVRRender( shaderrendermodel );

	if( canvascontrol ) CNOVRRender( canvascontrol );
	if( canvaspreview && previewtarget[2] )
	{
		if( canvaspreview->model )
			canvaspreview->model->pTextures[0]->nTextureId = previewtarget[2]->nResolveTextureId;
		CNOVRRender( canvaspreview->model );
	}

	if( canvasvideo )
	{
		CNOVRRender( canvasvideo->overrideshd );
		glUniform4fv( 9, 1, videoosdvals );
		CNOVRRender( canvasvideo );
	}

	#ifdef SECTIONDEBUG
		printf( "RenderFunction end\n" );
	#endif
}

void UpdateCamera()
{
	#ifdef SECTIONDEBUG
		printf( "UpdateCamera start\n" );
	#endif

	cnovr_pose pin;
	cnovr_pose * posecam = &store->posecamera;
	memcpy( &pin, posecam, sizeof( cnovr_pose ) );
	pin.Scale = 1;
	cnovr_pose pinvert;
	pose_invert( &pinvert, &pin );
	pinvert.Scale = 1;
	memcpy( &cnovrstate->pPreviewPose, &pinvert, sizeof( cnovr_pose ) );

	UpdateMenuStatuses();

	if( canvascontrol )
	{
		const cnovr_pose relative_pose_control = { .Pos = { 0 ,.6 ,0 }, .Rot = { 1, 0, 0, 0 }, .Scale = 1 };
		apply_pose_to_pose( canvascontrol->pose, posecam, &relative_pose_control );
		canvascontrol->pose->Scale *= .4;
	}
	if( canvasvideo )
	{
		const cnovr_pose relative_pose_video = { .Pos = { .5 ,0 ,0 }, .Rot = { 1, 0, 0, 0 }, .Scale = 1 };
		apply_pose_to_pose( canvasvideo->pose, posecam, &relative_pose_video );
		canvasvideo->pose->Scale *= .5;
	}
	if( canvaspreview ) 
	{
		const cnovr_pose relative_pose_preview = { .Pos = { -.5 ,0 ,0 }, .Rot = { 0, 0, 0, 1 }, .Scale = 1 };
		apply_pose_to_pose( canvaspreview->pose, posecam, &relative_pose_preview );
		canvaspreview->pose->Scale *= .5;
	}
	#ifdef SECTIONDEBUG
		printf( "UpdateCamera end\n" );
	#endif
}

int CameraFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	#ifdef SECTIONDEBUG
		printf( "CameraFocusEvent start\n" );
	#endif
	
	//void * c = (void*)cap->opaque;
	//cnovr_model * m = c->model;
	//int id = m->iOpaque;
	if( event == CNOVRF_LOSTFOCUS )
	{
		CNOVRNamedPtrSave( "camerastore" );
	}

	CNOVRGeneralHandleFocusEvent( modelcamerasolid->focuscontrol, modelcamerasolid->pose, prop, event, buttoninfo, CTRLA_PINCHBTN );

/*
	CNOVRF_IN = 0,
	CNOVRF_OUT,
	CNOVRF_DOWNFOCUS,
	CNOVRF_DOWNNOFOCUS,
	CNOVRF_UPFOCUS,
	CNOVRF_UPNOFOCUS,
	CNOVRF_MOTION,
	CNOVRF_DRAG,
	CNOVRF_ACQUIREDFOCUS,
	CNOVRF_LOSTFOCUS,
	CNOVRF_MAX_EVENTS,
*/
	if( event == CNOVRF_ACQUIREDFOCUS )
	{
		dragging_camera = 1;
	}
	else if( event == CNOVRF_DRAG )
	{
		UpdateCamera();
	}
	else if( event == CNOVRF_LOSTFOCUS )
	{
		//Recompute attach point.
		if( paired_object_id >= 0 )
			unapply_pose_from_pose( &store->paired_relative_offset_pose, &cnovrstate->pTrackedPoses[paired_object_id], &store->posecamera );
		dragging_camera = 0;
	}

	#ifdef SECTIONDEBUG
		printf( "CameraFocusEvent end\n" );
	#endif

	return 0;
}


const char * usecamtex = 0;



void example_scene_setup( void * tag, void * opaquev )
{
	shaderrendermodel = CNOVRShaderCreate( "rendermodel" );
	shaderlines = CNOVRShaderCreate( "assets/basic" );
	shaderblack = CNOVRShaderCreate( "assets/black" );
	previewcomposite = CNOVRShaderCreate( "modules/camera/previewwindow" );
	previewbasic = CNOVRShaderCreate( "previewbasic" );

	modelcameralines = CNOVRModelCreate( 0, GL_LINES );
	modelcamerasolid = CNOVRModelCreate( 0, GL_TRIANGLES );
	modelcameralines->pose = &store->posecamera;
	modelcamerasolid->pose = &store->posecamera;
	CNOVRModelLoadFromFileAsync( modelcameralines, "modules/camera/camera.obj:lineify" );
	CNOVRModelLoadFromFileAsync( modelcamerasolid, "modules/camera/camera.obj" );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );
	CNOVRListAdd( cnovrLPrerender, 0, PrerenderFunction );
	CNOVRListAdd( cnovrLPreviewRender, 0, AdvancedPreviewRender );

	capture.tag = 0;
	capture.tcctag = GetTCCTag();
	capture.opaque = 0;
	capture.cb = CameraFocusEvent;
	CNOVRModelSetInteractable( modelcamerasolid, &capture );

	if( !store->initialized )
	{
		printf( "initializing camera\n" );
		pose_make_identity( &store->posecamera );
		store->initialized = 1;
	}
	UpdateCamera();

	if( advanced_view )
	{
		canvascontrol = CNOVRCanvasCreate( "VideoSetupControl", 160, 192 );
		CNOVRCanvasApplyCannedGUI( canvascontrol, cvp_main_menu );
		canvasvideo = CNOVRCanvasCreate( "VideoSetupVideoView", videoW/2, videoH );
		CNOVRCanvasSwapBuffers( canvasvideo );
		canvasvideo->overrideshd = CNOVRShaderCreate( "modules/camera/previewyuyv" );
	}

	if( preview_view )
	{
		canvaspreview = CNOVRCanvasCreate( "PreviewCameraView", 0, 0 );
	}

	//if( advanced_view )
	//{
	//	camerabusinessthread = OGCreateThread( camerabusinessthreadfunction, 0 );
	//}

	if( usecamtex )
	{
		v4l2interface = cnv4l2_open( usecamtex, videoW, videoH, CNV4L2_YUYV, CNV4L2_MMAP, v4l2framecb );
		printf( "Opening cam interface %s got %p\n", usecamtex, v4l2interface );
		if( v4l2interface )
		{
			cnv4l2_set_framerate( v4l2interface, 1, 60 );
			cnv4l2_start_capture( v4l2interface );
			videodatathread = OGCreateThread( videodatathreadfunction, 0 );
		}
	}

	printf( "Is setup\n" );
}


void camcnv4l2enumcb( void * opaque, const char * dev, const char * name, const char * bus )
{
	if( strstr( name, "MiraBox" ) != 0 ) usecamtex = strdup( dev );
}


void start( const char * identifier )
{
	paired_object_id = -1;
	store = CNOVRNamedPtrData( "camerastore", "camerastore", sizeof( *store ) + 128 );
	printf( "=== Initializing %p\n", store );

	advanced_view = strstr( identifier, "advanced" )?1:0;
	preview_view = strstr( identifier, "preview" )?1:0;

	if( advanced_view )
	{
		cnv4l2_enumerate( 0, camcnv4l2enumcb );
	}

	//cnovrstate->fPreviewFOV = 45;

	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", identifier, identifier );

}

void stop( const char * identifier )
{
	quit = 1;
	if( v4l2interface )
	{
		cnv4l2_close( v4l2interface );
	}
	OGJoinThread( videodatathread );
	//OGJoinThread( camerabusinessthread );

	printf( "=== End Example stop\n" );
}


