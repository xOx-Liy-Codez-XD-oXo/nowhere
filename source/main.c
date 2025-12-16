#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <wiiuse/wiiuse.h>
#include <asndlib.h>

#include "oggplayer.h"

#include "nowhereToGoShortMix_ogg.h"

#include "album_tpl.h"
#include "album.h"

#include "mdl/lakehairvp.h"
#include "mdl/lakehairvc.h"
#include "mdl/lakeheadvp.h"
#include "mdl/lakeheadvc.h"
#include "mdl/lakeeyevp.h"    //all white so no col
#include "mdl/lakeeyebrow.h"  //combined
#include "mdl/lakebodyvc.h"
#include "mdl/lakebodyvp.h"
#include "mdl/stage.h"        //combined
#include "mdl/lakehairhalves.h"  //left one malformed
#include "mdl/scissor/scissor1vp.h"
#include "mdl/scissor/scissor1vc.h"
#include "mdl/scissor/scissor2vp.h"
#include "mdl/scissor/scissor2vc.h"
#include "mdl/stringvp.h"     // no col

#include "mdl/mouth/eh.h"
#include "mdl/mouth/teeth.h" 
#include "mdl/mouth/close.h" //monochrome
#include "mdl/mouth/o.h" //monochrome
#include "mdl/mouth/m.h" //monochrome

#include "mdl/credit/text1vp.h"
#include "mdl/credit/text2vp.h"
#include "mdl/credit/text3vp.h"

// #include "anim/lakeHairRegularAnim.h"
#include "anim/lakeHeadAnim.h"
#include "anim/lakeEyeAnim.h"
#include "anim/lakePupilAnim.h"
#include "anim/mouthAnims.h"
#include "anim/lakeEyebrowAnim.h"
#include "anim/lakeTorsoAnim.h"
#include "anim/lakeHairHalvesAnims.h"
#include "anim/scissoranims.h"
#include "anim/stringAnimShort.h"
#include "anim/lakeHairClipAnim.h"

#include "anim/camAnim.h"
#include "anim/camFovAnim.h"

#define DEFAULT_FIFO_SIZE	(256*1024)

static GXRModeObj *rmode = NULL;
static void *frameBuffer[2] = { NULL, NULL};

int frame = 0;

int main(int argc,char **argv)
{
	SYS_STDIO_Report(true);

	CON_EnableGecko(1, 0);
	
	f32 yscale;

	u32 xfbHeight;

	Mtx44 perspective;
	Mtx model, modelview, view;
	Mtx model2;
	Mtx mrx, mry, mrz;

	GXTexObj albumtex;
	TPLFile albumTPL;
	
	u32 fb = 0;
	GXColor background = {86,147,213, 0xff}; //blender col to wii: (sqrt(x))*255
	
	void *gpfifo = NULL;

	VIDEO_Init(); //In the devkitted pro. straight up "GX_Init()". and by "it()", haha, well. let's just say.   My graphits
	WPAD_Init();
	PAD_Init();
	ASND_Init();

	short advance = 1;

	rmode = VIDEO_GetPreferredMode(NULL);
	rmode->viWidth = 704;
	rmode->viXOrigin = 8;

	// allocate the fifo buffer
	gpfifo = memalign(32,DEFAULT_FIFO_SIZE);
	memset(gpfifo,0,DEFAULT_FIFO_SIZE);

	// allocate 2 framebuffers for double buffering
	frameBuffer[0] = SYS_AllocateFramebuffer(rmode);
	frameBuffer[1] = SYS_AllocateFramebuffer(rmode);

	// configure video
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(frameBuffer[fb]);
	VIDEO_SetBlack(true);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	fb ^= 1;

	// init the flipper
	GX_Init(gpfifo,DEFAULT_FIFO_SIZE);

	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_NEAR);

	// clears the bg to color and clears the z buffer
	GX_SetCopyClear(background, 0x00ffffff);

	// other gx setup
	GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
	yscale = GX_GetYScaleFactor(rmode->efbHeight,rmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering,((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(frameBuffer[fb],GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	//set number of textures to generate
	GX_SetNumTexGens(1); 

	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetNumTevStages(1);

	// setup the vertex attribute table
	// describes the data
	// args: vat location 0-7, type of data, data format, size, scale
	// so for ex. in the first call we are sending position data with
	// 3 values X,Y,Z of size F32. scale sets the number of fractional
	// bits for non float data.
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGB8, 0);

	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	// set number of rasterized color channels
	GX_SetNumChans(1);

	GX_InvVtxCache();

	TPL_OpenTPLFromMemory(&albumTPL, (void *)album_tpl,album_tpl_size);
	TPL_GetTexture(&albumTPL,album,&albumtex);

	GX_LoadTexObj(&albumtex, GX_TEXMAP0);
	
	GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0); 

	GX_InvalidateTexAll();

	//this is the part of the coed where i put a cool  unicode braille block  art picture of lake

	// ⣿⣽⣿⣻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⣿⣿⡿⣿⣿⢿⣿⢿⣯⡿⣽⣽⣽⣽⣽⡽⣽⢞
	// ⢟⣷⢿⣽⣾⣿⣾⣿⣾⣿⣾⣿⣾⣿⣾⣿⣾⣿⣾⣿⣾⡿⣿⣿⣷⣿⣿⡿⣿⣿⡿⣿⢿⣿⣞⣷⣷⣷⢿⣽⠃⠀⠀⠀⠀⠀⠀⠀
	// ⣤⣾⣿⣿⣿⣻⣽⣿⣽⣿⣽⣿⣽⣿⣽⣿⣽⣿⣽⣿⣷⣿⣿⣿⣾⣿⣟⣿⣿⣷⣿⣿⣿⣿⣿⣿⣿⣿⣿⡏
	// ⣿⣿⣿⣯⣿⣿⣿⣿⣿⡿⣿⣿⢿⣿⡿⣿⣿⢿⣿⣿⣽⣿⣿⣽⣿⣽⣿⣿⢿⣻⣿⣟⣯⣿⣿⣿⣿⣿⣯⠃
	// ⣿⣿⣯⣿⣿⣿⣯⣷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣟⣿⣿⣻⣿⣿⣿⡿⣿⣿⣿⣿⣿⣿⣿⣷⣿⡿⣟⡗
	// ⣿⣿⣿⡿⣟⣯⣛⣿⠿⣿⣿⣻⣿⣻⣽⣿⣿⣽⣿⣿⣿⣿⣿⡿⣿⡾⢿⣟⣻⣿⣟⣿⣟⣿⣭⡓⠛⠿⠧
	// ⣿⡿⣷⣿⡏⠉⠉⠛⠛⠛⠽⢖⡶⡻⣟⢿⢽⡻⡻⣞⢿⡲⡶⡞⠟⠛⠛⠋⠉⠉⣿⡿⣿⢿⣿⢿⣶⣄
	// ⣿⢿⣻⣟⡇⠀⠀⠀⠀⠀⠀⠀⠈⠇⢇⠇⡇⢝⠜⡜⢜⠜⠈⠀⠀⠀⠀⠀⠀⠀⣿⡿⣿⢿⣻⣿⣯⣿⢿⣤
	// ⣿⣟⣿⣻⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠌⠐⠁⠌⠠⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣻⣟⣿⣻⡷⣿⡾⣿⣻⣽⣶⣄
	// ⣿⢾⣻⣽⣧⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⡿⣯⣿⣽⣯⡿⣯⠿⠻⠽⠿⠞⠻⠛⠂
	// ⣟⣿⣽⢷⣟⣿⢶⣄⡀⠀⠀⠀⠀⠀⠀⠀⠂⠀⠂⠁⠀⠀⠀⠀⠀⠀⣀⣤⣾⣻⢿⣽⢾⣷⣻⣽⣟⣷⡀
	// ⣟⣷⣟⣿⡽⣯⡿⣯⣿⣻⢷⣦⣤⣰⣀⣐⣂⣀⣂⣀⣠⣰⣤⡶⣷⣟⣿⣽⢾⣻⣯⢿⣻⣾⣻⡾⣯⡷⣷⡀
	// ⣟⣷⣻⢷⣻⣯⢿⣻⣞⡿⣯⣷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣿⣻⡾⣷⣻⢿⣽⢾⣟⡿⣞⣯⣿⡽⣿⡽⣶
	// ⡿⣽⡽⣿⢽⣾⣻⡽⣷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣯⣟⣯⣟⣯⡿⣽⣯⢷⠋⠻⢷⣻⣯⢷
	// ⠙⣯⢿⣽⣻⣞⣷⣻⡽⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⣞⡷⣯⢷⡯⣟⣷⣻⠁⠀⠀⠀⠀⠈⠙⠓
	// ⠀⠈⢿⣺⣳⣻⣺⣳⢿⢽⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢿⢽⢯⢿⢽⢯⣟⣾⠃
	// doesnt tha tlook totally sick

	// this concludes the part ofthe code where i put a cool  unicode braille block  art picture of lake


	
	//Mess up color data
	for (int i = 0; i < sizeof(lakeHairVertCol) / sizeof(f32); i++) {
		lakeHairVertCol[i] = lakeHairVertCol[i] * 0.12f;
		lakeHairVertCol[i] = sqrt(lakeHairVertCol[i]);
	}
	
	for (int i = 0; i < sizeof(lakeHeadVertCol) / sizeof(f32); i++) {
		lakeHeadVertCol[i] = lakeHeadVertCol[i] * 0.95f;
		lakeHeadVertCol[i] = sqrt(lakeHeadVertCol[i]);
	}

	for (int i = 0; i < sizeof(lakeBodyVertCol) / sizeof(f32); i++) {
		lakeBodyVertCol[i] = lakeBodyVertCol[i] * 0.12f;
		lakeBodyVertCol[i] = sqrt(lakeBodyVertCol[i]);
	}

	for (int i = 0; i < sizeof(lakeHairRipRightVertCol) / sizeof(f32); i++) {
		lakeHairRipRightVertCol[i] = lakeHairRipRightVertCol[i] * 0.2f;
		lakeHairRipRightVertCol[i] = sqrt(lakeHairRipRightVertCol[i]);
	}

	for (int i = 0; i < sizeof(lakeHairRipLeftVertCol) / sizeof(f32); i++) {
		lakeHairRipLeftVertCol[i] = lakeHairRipLeftVertCol[i] * 0.2f;
		lakeHairRipLeftVertCol[i] = sqrt(lakeHairRipLeftVertCol[i]);
	}

	PlayOgg(nowhereToGoShortMix_ogg, nowhereToGoShortMix_ogg_size, 0, OGG_ONE_TIME);

	//previously aspect ratio was derived from resolution
	//which meant that 640x480 would be 4:3. but 640x528 wouldnt be.
	//do uk tvs have non-square pixels ? i dont know . i hope so

	f32 aspect;
	//aspect = 21.0f/9.0f;
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9) {
		aspect = 16.0f/9.0f;
	} else {
		aspect = 4.0f/3.0f;
	}

	VIDEO_SetBlack(false);

	while(1) {
		WPAD_ScanPads();
		PAD_ScanPads();
		if ( WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME || PAD_ButtonsDown(0) & PAD_BUTTON_START) {
			StopOgg();
			exit(0);
		}
		/*
		if ( WPAD_ButtonsDown(0) & WPAD_BUTTON_B) {
			StopOgg();
			PlayOgg(nowhereToGoShortMix_ogg, nowhereToGoShortMix_ogg_size, 0, OGG_ONE_TIME);
			frame = 1;
			
		}		
		if ( WPAD_ButtonsDown(0) & WPAD_BUTTON_A) {
			if (advance == 1) {
				advance = 0;
				printf("%d\n", frame);
				StopOgg();
			} else {
				advance = 1;
			}
		}
		if (advance != 1) {
			if ( WPAD_ButtonsDown(0) & WPAD_BUTTON_RIGHT) { 
				frame++;
				printf("%d\n", frame);
			}
			if ( WPAD_ButtonsDown(0) & WPAD_BUTTON_LEFT) { 
				frame = frame - 1;
				printf("%d\n", frame);
			}
		}*/

		switch(frame) {
			case 1:
				printf("\n");
				break;
			case 188:
				printf("I don't remember a damn thing from last week\n");
				break;
			case 344:
				printf("I'm sorry I could never be the one you thought I was\n");	
				break;
			case 518:
				printf("I try my best but everybody's looking past me\n");
				break;
			case 689:
				printf("I blacked out on the pavement and I think it's because\n");
				break;
			case 856:
				printf("She told me I was a waste of time, and I hated life\n");
				break;
			case 1080:
				printf("I was stuck and I had nowhere to go\n");
				break;
			case 1200:
				printf("and I know you're not a friend of mine, I'm not on your mind\n");
				break;
			case 1404:
				printf("so I said fuck it, I guess I'll be alone\n");
				break;
			case 1598: printf("\n");break;
			case 1600: printf("                       #                       #\n");break;
			case 1602: printf("                      #                       #\n");break;
			case 1604: printf("   ###   ##  #     # ###   ##  ###   ##     ####  ##      ###  ##\n");break;
			case 1606: printf("  #  # #  #  # # #  #  # #### #  # ####     #   #  #    #  # #  #\n");break;
			case 1608: printf(" #  #  ##    # #   #  #  ### #     ###      ##  ##      ###  ##\n");break;
			case 1610: printf("******************************************************** # ***\n");break;
			case 1612: printf(" (feat. jane remover) by juno     animation by lliy   ##\n");break;
			case 1614: printf("\n");break;
		}

		//This function applies a tev config
		GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
		//This one sets inputs for it
		GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);

		//We'll need these a bunch
		int frame12 = frame * 12;
		int frame9 = frame * 9;

		//View matrix per frame is pregenerated
		//... sorry
		view[0][0] = cameraAnim[frame12];
		view[0][1] = cameraAnim[frame12 + 1];
		view[0][2] = cameraAnim[frame12 + 2];
		view[0][3] = cameraAnim[frame12 + 3];	
		view[1][0] = cameraAnim[frame12 + 4];		
		view[1][1] = cameraAnim[frame12 + 5];		
		view[1][2] = cameraAnim[frame12 + 6];		
		view[1][3] = cameraAnim[frame12 + 7];		
		view[2][0] = cameraAnim[frame12 + 8];		
		view[2][1] = cameraAnim[frame12 + 9];		
		view[2][2] = cameraAnim[frame12 + 10];		
		view[2][3] = cameraAnim[frame12 + 11];
		
		guPerspective(perspective, cameraFovAnim[frame] * 1.6f, aspect, 0.5F, 30.0F);
		GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);

		GX_ClearVtxDesc();
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

		//This is a pointer to the 9 floats required to build the matrices
		float * modeltransform;
	
		if (frame < 1203) {                                                //His hair isnt used after this frame
			modeltransform = &lakeHeadAnim[frame9];
			//Now modeltransform can be indexed into for this frame's transform

			//These gu functions, the ones without "apply" in the name, return the same matrix depending on the input values
			//regardless of the contents of the matrix that you pass to them.
			guMtxRotRad(mrx, 'x', modeltransform[3]);
			guMtxRotRad(mry, 'y', modeltransform[4]);
			guMtxRotRad(mrz, 'z', modeltransform[5]);
			guMtxConcat(mrz,mry,mry); //Matrix multiply A and B and put it in C
			guMtxConcat(mry,mrx,model);

			//This function applies its translation to what you pass to it
			guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
			guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
			guMtxConcat(model,model2,model);
			guMtxConcat(view,model,modelview);
			// load the modelview matrix into matrix memory
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 681); //amount of faces data were sending ie faces times three
			for (int i = 0; i < 681; i++) {  //put same number here too
				int i3 = i*3;
				GX_Position3f32(lakehairVertPos[i3], lakehairVertPos[i3+1], lakehairVertPos[i3+2]);
				GX_Color3f32(lakeHairVertCol[i3], lakeHairVertCol[i3+1], lakeHairVertCol[i3+2]);
			}
			//GX_End(); //This function is actually empty, the GPU knows its time to stop when you've given it enough verts	
		}		
		
		modeltransform = &lakeHairRipRightAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);

		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 579);
		for (int i = 0; i < 579; i++) {
			int i3 = i * 3;
			GX_Position3f32(lakeHairRipRightVertPos[i3], lakeHairRipRightVertPos[i3+1], lakeHairRipRightVertPos[i3+2]);
			GX_Color3f32(lakeHairRipRightVertCol[i3], lakeHairRipRightVertCol[i3+1], lakeHairRipRightVertCol[i3+2]);
		}

		modeltransform = &lakeHairRipLeftAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);

		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 162);
		for (int i = 0; i < 162; i++) {
			int i3 = i * 3;
			GX_Position3f32(lakeHairRipLeftVertPos[i3], lakeHairRipLeftVertPos[i3+1], lakeHairRipLeftVertPos[i3+2]);
			GX_Color3f32(lakeHairRipLeftVertCol[i3], lakeHairRipLeftVertCol[i3+1], lakeHairRipLeftVertCol[i3+2]);
		}	

		modeltransform = &lakeHeadAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);

		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 102);
		for (int i = 0; i < 102; i++) {
			int i3 = i * 3;
			GX_Position3f32(lakeHeadVertPos[i3], lakeHeadVertPos[i3+1], lakeHeadVertPos[i3+2]);
			GX_Color3f32(lakeHeadVertCol[i3], lakeHeadVertCol[i3+1], lakeHeadVertCol[i3+2]);
		}	

		modeltransform = &lakeEyeAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 180);
		for (int i = 0; i < 180; i++) {
			int i3 = i * 3;
			GX_Position3f32(lakeEyeVertPos[i3], lakeEyeVertPos[i3+1], lakeEyeVertPos[i3+2]);
			GX_Color3f32(1.0f,1.0f,1.0f);
		}	

		modeltransform = &lakePupilLAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 6); //amount of faces data were sending ie faces times three
		GX_Position3f32(1.0f, -1.0f, 0.0f);
		GX_Color3f32(0.0f,0.0f,0.0f);
		GX_Position3f32(-1.0f, 1.0f, 0.0f);
		GX_Color3f32(0.0f,0.0f,0.0f);
		GX_Position3f32(-1.0f, -1.0f, 0.0f);
		GX_Color3f32(0.0f,0.0f,0.0f);

		GX_Position3f32(1.0f, -1.0f, 0.0f);
		GX_Color3f32(0.0f,0.0f,0.0f);
		GX_Position3f32(1.0f, 1.0f, 0.0f);
		GX_Color3f32(0.0f,0.0f,0.0f);
		GX_Position3f32(-1.0f, 1.0f, 0.0f);
		GX_Color3f32(0.0f,0.0f,0.0f);	

		modeltransform = &lakePupilRAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 6); //amount of faces data were sending ie faces times three
		GX_Position3f32(1.0f, -1.0f, 0.0f);
		GX_Color3f32(0.0f,0.0f,0.0f);
		GX_Position3f32(-1.0f, 1.0f, 0.0f);
		GX_Color3f32(0.0f,0.0f,0.0f);
		GX_Position3f32(-1.0f, -1.0f, 0.0f);
		GX_Color3f32(0.0f,0.0f,0.0f);

		GX_Position3f32(1.0f, -1.0f, 0.0f);
		GX_Color3f32(0.0f,0.0f,0.0f);
		GX_Position3f32(1.0f, 1.0f, 0.0f);
		GX_Color3f32(0.0f,0.0f,0.0f);
		GX_Position3f32(-1.0f, 1.0f, 0.0f);
		GX_Color3f32(0.0f,0.0f,0.0f);

		modeltransform = &lakeEyebrowAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 384);
		for (int i = 0; i < 384; i++) {
			int i3 = i * 3;
			GX_Position3f32(lakeEyebrowVertPos[i3], lakeEyebrowVertPos[i3+1], lakeEyebrowVertPos[i3+2]);
			GX_Color3f32(1.0f,1.0f,1.0f);
		}

		modeltransform = &lakeMouthEhAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 474);
		for (int i = 0; i < 474; i++) {
			int i3 = i * 3;
			GX_Position3f32(lakeMouthEhVertPos[i3], lakeMouthEhVertPos[i3+1], lakeMouthEhVertPos[i3+2]);
			GX_Color3f32(lakeMouthEhVertCol[i3], lakeMouthEhVertCol[i3+1], lakeMouthEhVertCol[i3+2]);
		}	

		modeltransform = &lakeMouthClosejawShoteefAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 24);
		for (int i = 0; i < 24; i++) {
			int i3 = i * 3;
			GX_Position3f32(lakeMouthTeethVertPos[i3], lakeMouthTeethVertPos[i3+1], lakeMouthTeethVertPos[i3+2]);
			GX_Color3f32(lakeMouthTeethVertCol[i3], lakeMouthTeethVertCol[i3+1], lakeMouthTeethVertCol[i3+2]);
		}

		modeltransform = &lakeMouthCloseAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 12);
		for (int i = 0; i < 12; i++) {
			int i3 = i * 3;
			GX_Position3f32(lakeMouthCloseVertPos[i3], lakeMouthCloseVertPos[i3+1], lakeMouthCloseVertPos[i3+2]);
			GX_Color3f32(0.027f, 0.027f, 0.027f);
		}	

		modeltransform = &lakeMouthMAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 24);
		for (int i = 0; i < 24; i++) {
			int i3 = i * 3;
			GX_Position3f32(lakeMouthMVertPos[i3], lakeMouthMVertPos[i3+1], lakeMouthMVertPos[i3+2]);
			GX_Color3f32(0.027f, 0.027f, 0.027f);
		}

		modeltransform = &lakeMouthOAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 36);
		for (int i = 0; i < 36; i++) {
			int i3 = i * 3;
			GX_Position3f32(lakeMouthOVertPos[i3], lakeMouthOVertPos[i3+1], lakeMouthOVertPos[i3+2]);
			GX_Color3f32(0.027f, 0.027f, 0.027f);
		}	

		modeltransform = &lakeTorsoAnim[frame9];

		guMtxRotRad(mrx, 'x', modeltransform[3]);
		guMtxRotRad(mry, 'y', modeltransform[4]);
		guMtxRotRad(mrz, 'z', modeltransform[5]);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
		guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 69);
		for (int i = 0; i < 69; i++) {
			int i3 = i * 3;
			GX_Position3f32(lakeBodyVertPos[i3], lakeBodyVertPos[i3+1], lakeBodyVertPos[i3+2]);
			GX_Color3f32(lakeBodyVertCol[i3], lakeBodyVertCol[i3+1], lakeBodyVertCol[i3+2]);
		}	

		struct stage {
			f32 x;
			f32 y;
			f32 z;
			f32 rx;
			f32 ry;
			f32 rz;
			f32 sx;
			f32 sy;
			f32 sz;
		};

		struct stage stage;

		if (frame >= 1538) {
			stage.x = 0.0f;
			stage.y = 2.00455;
			stage.z = 0.640983;
			stage.rx = -1.5707963705062866f;
			stage.ry = 0.0f;
			stage.rz = 3.141592502593994f;
			stage.sx = 2.0f;
			stage.sy = 2.0f;
			stage.sz = 8.0f;
		} else {
			stage.x = 0.0f;
			stage.y = -0.001766f;
			stage.z = -1.35364f;
			stage.rx = 0.0f;
			stage.ry = 0.0f;
			stage.rz = 0.0f;
			stage.sx = 2.0f;
			stage.sy = 2.0f;
			stage.sz = 2.0f;
		}

		guMtxRotRad(mrx, 'x', stage.rx);
		guMtxRotRad(mry, 'y', stage.ry);
		guMtxRotRad(mrz, 'z', stage.rz);
		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);
		guMtxTransApply(model, model, stage.x, stage.y, stage.z);
		guMtxScale(model2, stage.sx, stage.sy, stage.sz);
		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 18);
		for (int i = 0; i < 18; i++) {
			int i3 = i * 3;
			GX_Position3f32(stageVertPos[i3], stageVertPos[i3+1], stageVertPos[i3+2]);
			GX_Color3f32(stageVertCol[i3], stageVertCol[i3+1], stageVertCol[i3+2]);
		}	
	
		if (frame > 165 && frame < 340) {
			modeltransform = &scissor1Anim[frame9];

			guMtxRotRad(mrx, 'x', modeltransform[3]);
			guMtxRotRad(mry, 'y', modeltransform[4]);
			guMtxRotRad(mrz, 'z', modeltransform[5]);
			guMtxConcat(mrz,mry,mry); 
			guMtxConcat(mry,mrx,model);
			guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
			guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
			guMtxConcat(model,model2,model);
			guMtxConcat(view,model,modelview);
			
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 2514);
			for (int i = 0; i < 2514; i++) {
				int i3 = i * 3;
				GX_Position3f32(scissor1VertPos[i3], scissor1VertPos[i3+1], scissor1VertPos[i3+2]);
				GX_Color3f32(scissor1VertCol[i3], scissor1VertCol[i3+1], scissor1VertCol[i3+2]);
			}	

			modeltransform = &scissor2Anim[frame9];

			guMtxRotRad(mrx, 'x', modeltransform[3]);
			guMtxRotRad(mry, 'y', modeltransform[4]);
			guMtxRotRad(mrz, 'z', modeltransform[5]);
			guMtxConcat(mrz,mry,mry); 
			guMtxConcat(mry,mrx,model);
			guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
			guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
			guMtxConcat(model,model2,model);
			guMtxConcat(view,model,modelview);
			
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 1452);
			for (int i = 0; i < 1452; i++) {
				int i3 = i * 3;
				GX_Position3f32(scissor2VertPos[i3], scissor2VertPos[i3+1], scissor2VertPos[i3+2]);
				GX_Color3f32(scissor2VertCol[i3], scissor2VertCol[i3+1], scissor2VertCol[i3+2]);
			}	
		}

		if (frame > 842 && frame < 1204) {
			modeltransform = &stringAnimShort[frame9 - 7551]; //7551 is 839 * 9, and 839 is the when the anim for the string starts

			guMtxRotRad(mrx, 'x', modeltransform[3]);
			guMtxRotRad(mry, 'y', modeltransform[4]);
			guMtxRotRad(mrz, 'z', modeltransform[5]);
			guMtxConcat(mrz,mry,mry); 
			guMtxConcat(mry,mrx,model);
			guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
			guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
			guMtxConcat(model,model2,model);
			guMtxConcat(view,model,modelview);
			
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 600);
			for (int i = 0; i < 600; i++) {
				int i3 = i * 3;
				GX_Position3f32((f32)stringVertPos[i3], (f32)stringVertPos[i3+1], (f32)stringVertPos[i3+2]);
				GX_Color3f32(0.672f, 0.672f, 0.672f);
			}		
		}

		if (frame > 171 && frame < 349) {
			modeltransform = &lakeHairCutBitAnim[frame9 - 1521];

			guMtxRotRad(mrx, 'x', modeltransform[3]);
			guMtxRotRad(mry, 'y', modeltransform[4]);
			guMtxRotRad(mrz, 'z', modeltransform[5]);
			guMtxConcat(mrz,mry,mry); 
			guMtxConcat(mry,mrx,model);
			guMtxTransApply(model, model, modeltransform[0], modeltransform[1], modeltransform[2]);
			guMtxScale(model2, modeltransform[6], modeltransform[7], modeltransform[8]);
			guMtxConcat(model,model2,model);
			guMtxConcat(view,model,modelview);
			
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 3); //amount of faces data were sending ie faces times three

			{
				GX_Position3f32(-0.099130f, -0.000000f, 0.067562f);
				GX_Color3f32(0.1270934f, 0.1270934f, 0.1270934f);
				GX_Position3f32(0.016780f, 0.000000f, -0.148370f);
				GX_Color3f32(0.1496859f, 0.1496859f, 0.1496859f);
				GX_Position3f32(0.082355f, -0.000000f, 0.080809f);
				GX_Color3f32(0.1244663f, 0.1244663f, 0.1244663f);
			}		
		}

		if (frame > 1688 && frame < 1830) {
			guMtxIdentity(model);                                                  //text3

			guMtxConcat(view,model,modelview);
			
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 3789);
			for (int i = 0; i < 3789; i++) {
				int i3 = i * 3;
				GX_Position3f32(text3VertPos[i3], text3VertPos[i3+1], text3VertPos[i3+2]);
				GX_Color3f32(1.0f, 1.0f, 1.0f);
			}	
		}

		if (frame > 1538 && frame < 1689) {
			guMtxIdentity(model);                                                  //text1

			guMtxConcat(view,model,modelview);
			
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 5820);
			for (int i = 0; i < 5820; i++) {
				int i3 = i * 3;
				GX_Position3f32(text1VertPos[i3], text1VertPos[i3+1], text1VertPos[i3+2]);
				GX_Color3f32(1.0f, 1.0f, 1.0f);
			}	

			guMtxIdentity(model);                                                  //text2

			guMtxConcat(view,model,modelview);
			
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 822);
			for (int i = 0; i < 822; i++) {
				int i3 = i * 3;
				GX_Position3f32(text2VertPos[i3], text2VertPos[i3+1], text2VertPos[i3+2]);
				GX_Color3f32(0.8f, 0.8f, 0.8f);
			}		
	


			GX_SetTevOp(GX_TEVSTAGE0,GX_REPLACE);                                 //change settings for the first tev stage to handle textures
			GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0); 

			GX_ClearVtxDesc();
			GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);                                  //change vertex descriptors to do texture coords instead of vertex colors
			GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	
			guMtxIdentity(model);                                                  //album art

			guMtxConcat(view,model,modelview);
			
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT1, 6); //amount of faces data were sending ie faces times three
			GX_Position3f32(-1.0414f, -0.000001f, 3.10628f);
			GX_TexCoord2f32(0.0f, 0.0f);
			GX_Position3f32(-1.0414f, -0.000001f, 1.261f);
			GX_TexCoord2f32(0.0f, 1.0f);
			GX_Position3f32(0.803878f, -0.000001f, 1.261f);
			GX_TexCoord2f32(1.0f, 1.0f);
			GX_Position3f32(0.803878f, -0.000001f, 1.261f);
			GX_TexCoord2f32(1.0f, 1.0f);
			GX_Position3f32(0.803878f, -0.000001f, 3.10628f);
			GX_TexCoord2f32(1.0f, 0.0f);
			GX_Position3f32(-1.0414f, -0.000001f, 3.10628f);
			GX_TexCoord2f32(0.0f, 0.0f);
		}
	
		GX_DrawDone();

		//stop drawig stuf NOW
		
		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE); 
		GX_SetColorUpdate(GX_TRUE);
		GX_CopyDisp(frameBuffer[fb],GX_TRUE);

		VIDEO_SetNextFramebuffer(frameBuffer[fb]);

		VIDEO_Flush();
		VIDEO_WaitVSync();
		fb ^= 1;

		if (rmode->viHeight == 576) {
			if (frame == 1936) {
				StopOgg();
				exit(0);
			}
			if ((frame % 6) == 0) {
				frame++;
			} 
		}		

		if (frame == 1937) {
			StopOgg();
			exit(0);
		} else {
			frame = frame + advance;
		}
	}
}
