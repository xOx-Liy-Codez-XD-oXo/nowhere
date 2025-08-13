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

	Mtx view;
	Mtx44 perspective;
	Mtx model, modelview;
	Mtx model2;
	Mtx mrx, mry, mrz;

	GXTexObj albumtex;
	TPLFile albumTPL;
	
	u32 fb = 0;
	GXColor background = {86,147,213, 0xff}; //blender col to wii: (sqrt(x))*255
	
	void *gpfifo = NULL;

	VIDEO_Init(); //In the devkitted pro. straight up "GX_Init()". and by "it()", haha, well. let's just say.   My graphits
	WPAD_Init();
	ASND_Init();

	short advance = 1;

	rmode = VIDEO_GetPreferredMode(NULL);

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

	guVector xaxis = {1,0,0};
	guVector yaxis = {0,1,0};
	guVector zaxis = {0,0,1};

	guVector cam = {0.0F, -20.0F, 1.0F},
		    up = {0.0F, 1.0F, 0.0F},
		  look = {0.0F, 0.0F, 0.0F};
	guLookAt(view, &cam, &up, &look);


	// list of models and corresponding original arrays and copied arrays
	//  _______________________________________________________________
	// |       name       |   original arrays      |    copy arrays    |
	// |__________________|________________________|___________________|
	// |  the stage -v    |   stageVertPos         |   stageVertPosCop |
	// |  the stage -c    |   stageVertCol         |   stageVertColCop |
	// |     lake's hair-v|   lakehairVertPos      |    mdl1VertWork   |  
	// |     lake's hair-c|   lakeHairVertCol      |    mdl1VertColk   |
	// |     lake's head-v|   lakeHeadVertPos      |    mdl2VertWork   |
	// |     lake's head-c|   lakeHeadVertCol      |    mdl2VertColk   |
	// |  lake's eyes     |   lakeEyeVertPos       |    mdl3VertWork   |
	// | lake's eyebrows v|  lakeEyebrowVertPos    |    mdl4VertWork   |
	// | lake's body - v  |  lakeBodyVertPos       |    mdl5VertWork   |
	// | lake's body - c  |  lakeBodyVertCol       |    mdl5VertColk   |
	// |lake hair right v |lakeHairRipRightVertPos |    mdl6VertWork   |
	// |lake hair right c |lakeHairRipRightVertCol |    mdl6VertColk   |
	// |lake hair left v  | lakeHairRipLeftVertPos |    mdl7VertWork   |
	// |lake hair left c  | lakeHairRipLeftVertCol |    mdl7VertWork   |
	// |                  |                        |                   |
	// |   mouth eh vp    |lakeMouthEhVertPos      |    mouthEhWork    |
	// |   mouth eh vc    |lakeMouthEhVertCol      |    mouthEhColk    |
	// |  mouth teeth vp  |lakeMouthTeethVertPos   |    mouthTeWork    |
	// |  mouth teeth vc  |lakeMouthTeethVertCol   |    mouthTeColk    |
	// |  mouth close vp  |lakeMouthCloseVertPos   |    mouthClWork    |
	// |  mouth o vp      |lakeMouthOVertPos       |    mouthOWork     |
	// |  mouth m vp      |lakeMouthMVertPos       |    mouthMWork     |
	// |                  |                        |                   |
	// | scissor 1 v      |scissor1VertPos         |    sc1vpcop       |
	// | scissor 1 c      |scissor1VertCol         |    sc1vccop       |
	// | scissor 2 v      |scissor2VertPos         |    sc2vpcop       |
	// | scissor 2 c      |scissor2VertCol         |    sc2vccop       |
	// | string           |stringVertPos           |stringVertPosCop   |
	// |                  |                        |                   |
	// |credit text 1     |text1VertPos            |text1VertPosCop    |
	// |credit text 2     |text2VertPos            |text2VertPosCop    |
	// |credit text 3     |text3VertPos            |text3VertPosCop    |

	// we'd want to copy verts into ram every frame if we were doing vertex deformation
	// but we're just doing transformation on these models so we dont need to

	f32 *stageVertPosCop;
	stageVertPosCop = malloc(sizeof(f32) * (sizeof(stageVertPos) / sizeof(f32)));

	f32 *stageVertColCop;
	stageVertColCop = malloc(sizeof(f32) * (sizeof(stageVertCol) / sizeof(f32)));

	f32 *mdl1VertWork;
	mdl1VertWork = malloc(sizeof(f32) * (sizeof(lakehairVertPos) / sizeof(f32)));

	f32 *mdl1VertColk;
	mdl1VertColk = malloc(sizeof(f32) * (sizeof(lakeHairVertCol) / sizeof(f32)));

	f32 *mdl2VertWork;
	mdl2VertWork = malloc(sizeof(f32) * (sizeof(lakeHeadVertPos) / sizeof(f32)));

	f32 *mdl2VertColk;
	mdl2VertColk = malloc(sizeof(f32) * (sizeof(lakeHeadVertCol) / sizeof(f32)));

	f32 *mdl3VertWork;
	mdl3VertWork = malloc(sizeof(f32) * (sizeof(lakeEyeVertPos) / sizeof(f32)));

	f32 *mdl4VertWork;
	mdl4VertWork = malloc(sizeof(f32) * (sizeof(lakeEyebrowVertPos) / sizeof(f32)));

	f32 *mdl5VertWork;
	mdl5VertWork = malloc(sizeof(f32) * (sizeof(lakeBodyVertPos) / sizeof(f32)));
	
	f32 *mdl5VertColk;
	mdl5VertColk = malloc(sizeof(f32) * (sizeof(lakeBodyVertCol) / sizeof(f32)));

	f32 *mdl6VertWork;
	mdl6VertWork = malloc(sizeof(f32) * (sizeof(lakeHairRipRightVertPos) / sizeof(f32)));
	
	f32 *mdl6VertColk;
	mdl6VertColk = malloc(sizeof(f32) * (sizeof(lakeHairRipRightVertCol) / sizeof(f32)));

	f32 *mdl7VertWork;
	mdl7VertWork = malloc(sizeof(f32) * (sizeof(lakeHairRipLeftVertPos) / sizeof(f32)));
	
	f32 *mdl7VertColk;
	mdl7VertColk = malloc(sizeof(f32) * (sizeof(lakeHairRipLeftVertCol) / sizeof(f32)));


	f32 *mouthEhWork;
	mouthEhWork = malloc(sizeof(f32) * (sizeof(lakeMouthEhVertPos) / sizeof(f32)));

	f32 *mouthEhColk;
	mouthEhColk = malloc(sizeof(f32) * (sizeof(lakeMouthEhVertCol) / sizeof(f32)));

	f32 *mouthTeWork;
	mouthTeWork = malloc(sizeof(f32) * (sizeof(lakeMouthTeethVertPos) / sizeof(f32)));

	f32 *mouthTeColk;
	mouthTeColk = malloc(sizeof(f32) * (sizeof(lakeMouthTeethVertCol) / sizeof(f32)));

	f32 *mouthClWork;
	mouthClWork = malloc(sizeof(f32) * (sizeof(lakeMouthCloseVertPos) / sizeof(f32)));

	f32 *mouthOWork;
	mouthOWork = malloc(sizeof(f32) * (sizeof(lakeMouthOVertPos) / sizeof(f32)));
	
	f32 *mouthMWork;
	mouthMWork = malloc(sizeof(f32) * (sizeof(lakeMouthMVertPos) / sizeof(f32)));


	f32 *sc1vpcop;
	sc1vpcop = malloc(sizeof(f32) * (sizeof(scissor1VertPos) / sizeof(f32)));
	
	f32 *sc1vccop;
	sc1vccop = malloc(sizeof(f32) * (sizeof(scissor1VertCol) / sizeof(f32)));

	f32 *sc2vpcop;
	sc2vpcop = malloc(sizeof(f32) * (sizeof(scissor2VertPos) / sizeof(f32)));
	
	f32 *sc2vccop;
	sc2vccop = malloc(sizeof(f32) * (sizeof(scissor2VertCol) / sizeof(f32)));

	f32 *stringVertPosCop;
	stringVertPosCop = malloc(sizeof(f32) * (sizeof(stringVertPos) / sizeof(f32)));


	f32 *text1VertPosCop;
	text1VertPosCop = malloc(sizeof(f32) * (sizeof(text1VertPos) / sizeof(f32)));

	f32 *text2VertPosCop;
	text2VertPosCop = malloc(sizeof(f32) * (sizeof(text2VertPos) / sizeof(f32)));

	f32 *text3VertPosCop;
	text3VertPosCop = malloc(sizeof(f32) * (sizeof(text3VertPos) / sizeof(f32)));


	for (int k = 0; k < sizeof(stageVertPos) / sizeof(f32); k++) { 
		stageVertPosCop[k] = stageVertPos[k];
	}

	for (int k = 0; k < sizeof(stageVertCol) / sizeof(f32); k++) {
		stageVertColCop[k] = stageVertCol[k];
	}

	for (int k = 0; k < sizeof(lakehairVertPos) / sizeof(f32); k++) { 
		mdl1VertWork[k] = lakehairVertPos[k];
	}

	for (int k = 0; k < sizeof(lakeHairVertCol) / sizeof(f32); k++) {
		mdl1VertColk[k] = lakeHairVertCol[k];
	}

	for (int k = 0; k < sizeof(lakeHeadVertPos) / sizeof(f32); k++) { 
		mdl2VertWork[k] = lakeHeadVertPos[k];
	}
	
	for (int k = 0; k < sizeof(lakeHeadVertCol) / sizeof(f32); k++) { 
		mdl2VertColk[k] = lakeHeadVertCol[k];
	}

	for (int k = 0; k < sizeof(lakeEyeVertPos) / sizeof(f32); k++) { 
		mdl3VertWork[k] = lakeEyeVertPos[k];
	}

	for (int k = 0; k < sizeof(lakeBodyVertPos) / sizeof(f32); k++) { 
		mdl5VertWork[k] = lakeBodyVertPos[k];
	}
	
	for (int k = 0; k < sizeof(lakeBodyVertCol) / sizeof(f32); k++) { 
		mdl5VertColk[k] = lakeBodyVertCol[k];
	}

	for (int k = 0; k < sizeof(lakeHairRipRightVertPos) / sizeof(f32); k++) { 
		mdl6VertWork[k] = lakeHairRipRightVertPos[k];
	}
	
	for (int k = 0; k < sizeof(lakeHairRipRightVertCol) / sizeof(f32); k++) { 
		mdl6VertColk[k] = lakeHairRipRightVertCol[k];
	}

	for (int k = 0; k < sizeof(lakeHairRipLeftVertPos) / sizeof(f32); k++) { 
		mdl7VertWork[k] = lakeHairRipLeftVertPos[k];
	}
	
	for (int k = 0; k < sizeof(lakeHairRipLeftVertCol) / sizeof(f32); k++) { 
		mdl7VertColk[k] = lakeHairRipLeftVertCol[k];
	}

	for (int k = 0; k < sizeof(lakeEyebrowVertPos) / sizeof(f32); k++) { 
		mdl4VertWork[k] = lakeEyebrowVertPos[k];
	}



	for (int k = 0; k < sizeof(lakeMouthEhVertPos) / sizeof(f32); k++) { 
		mouthEhWork[k] = lakeMouthEhVertPos[k];
	}

	for (int k = 0; k < sizeof(lakeMouthEhVertCol) / sizeof(f32); k++) { 
		mouthEhColk[k] = lakeMouthEhVertCol[k];
	}

	for (int k = 0; k < sizeof(lakeMouthTeethVertPos) / sizeof(f32); k++) { 
		mouthTeWork[k] = lakeMouthTeethVertPos[k];
	}

	for (int k = 0; k < sizeof(lakeMouthTeethVertCol) / sizeof(f32); k++) { 
		mouthTeColk[k] = lakeMouthTeethVertCol[k];
	}

	for (int k = 0; k < sizeof(lakeMouthCloseVertPos) / sizeof(f32); k++) { 
		mouthClWork[k] = lakeMouthCloseVertPos[k];
	}

	for (int k = 0; k < sizeof(lakeMouthOVertPos) / sizeof(f32); k++) { 
		mouthOWork[k] = lakeMouthOVertPos[k];
	}

	for (int k = 0; k < sizeof(lakeMouthMVertPos) / sizeof(f32); k++) { 
		mouthMWork[k] = lakeMouthMVertPos[k];
	}
	

	for (int k = 0; k < sizeof(scissor1VertPos) / sizeof(f32); k++) { 
		sc1vpcop[k] = scissor1VertPos[k];
	}

	for (int k = 0; k < sizeof(scissor1VertCol) / sizeof(f32); k++) { 
		sc1vccop[k] = scissor1VertCol[k];
	}

	for (int k = 0; k < sizeof(scissor2VertPos) / sizeof(f32); k++) { 
		sc2vpcop[k] = scissor2VertPos[k];
	}

	for (int k = 0; k < sizeof(scissor2VertCol) / sizeof(f32); k++) { 
		sc2vccop[k] = scissor2VertCol[k];
	}

	for (int k = 0; k < sizeof(stringVertPos) / sizeof(f32); k++) { 
		stringVertPosCop[k] = stringVertPos[k];
	}

	
	for (int k = 0; k < sizeof(text1VertPos) / sizeof(f32); k++) { 
		text1VertPosCop[k] = text1VertPos[k];
	}

	for (int k = 0; k < sizeof(text2VertPos) / sizeof(f32); k++) { 
		text2VertPosCop[k] = text2VertPos[k];
	}
		
	for (int k = 0; k < sizeof(text3VertPos) / sizeof(f32); k++) { 
		text3VertPosCop[k] = text3VertPos[k];
	}

	// list of animation  and corresponding original arrays and copied arrays
	//  ----------------------------------------------------------------------       -----------------------------------------------------------------------
	// |        name      |   original arrays           |    copy arrays       |    |                            discarded data                             |
	// |__________________|_____________________________|______________________|    |_______________________________________________________________________|
	//                                                                              |  hair animation  |lakeHairRegularAnim          |lakeHairRegularAnimCop|  
	// |  head animation  |lakeHeadAnim                 |lakeHeadAnimCop       |
	// |   eye animation  |lakeEyeAnim                  |lakeEyeAnimCop        |
	// | leftpupil anim   |lakePupilLAnim               |lakePupilLAnimCop     |
	// | rightpupilanim   |lakePupilRAnim               |lakePupilRAnimCop     |
	// | eyebrow anim     |lakeEyebrowAnim              |lakeEyebrowAnimCop    |
	// | body anim        |lakeTorsoAnim                |lakeTorsoAnimCop      |
	// | left hair anim   |lakeHairRipLeftAnim          |lakeHairLeftAnimCop   |
	// | rijt hair anim   |lakeHairRipRightAnim         |lakeHairRightAnimCop  |
	// | scissor 1 anim   |scissor1Anim                 |scissor1AnimCop       |
	// | scissor 2 anim   |scissor2Anim                 |scissor2AnimCop       |
	// | string anim short|stringAnimShort              |stringAnimShortCop    |
	// |lake Hair Clipping|lakeHairCutBitAnim           |lakeClipAnim          | 
	
	// | eh mouth anim    |lakeMouthEhAnim              |lakeMouthEhAnimCop    |
	// |teeth mouth anim  |lakeMouthClosejawShoteefAnim |lakeMouthTeAnimCop    |
	// |close mouth Anim  |lakeMouthCloseAnim           |lakeMouthClAnimCop    |
	// | o mouth anim     |lakeMouthOAnim               |lakeMouthOAnimCop     |
	// | m mouth anim     |lakeMouthMAnim               |lakeMouthMAnimCop     |

	// | camera matricies |cameraAnim                   |camAnimCop            |
	// | cam fov          |cameraFovAnim                |camFovAnimCop         |

	/*f32 *lakeHairRegularAnimCop;
	lakeHairRegularAnimCop = malloc(sizeof(f32) * (sizeof(lakeHairRegularAnim) / sizeof(f32)));*/

	f32 *lakeHeadAnimCop;
	lakeHeadAnimCop = malloc(sizeof(f32) * (sizeof(lakeHeadAnim) / sizeof(f32)));

	f32 *lakeEyeAnimCop;
	lakeEyeAnimCop = malloc(sizeof(f32) * (sizeof(lakeEyeAnim) / sizeof(f32)));

	f32 *lakePupilLAnimCop;
	lakePupilLAnimCop = malloc(sizeof(f32) * (sizeof(lakePupilLAnim) / sizeof(f32)));

	f32 *lakePupilRAnimCop;
	lakePupilRAnimCop = malloc(sizeof(f32) * (sizeof(lakePupilRAnim) / sizeof(f32)));
	
	f32 *lakeEyebrowAnimCop;
	lakeEyebrowAnimCop = malloc(sizeof(f32) * (sizeof(lakeEyebrowAnim) / sizeof(f32)));
	
	f32 *lakeHairLeftAnimCop;
	lakeHairLeftAnimCop = malloc(sizeof(f32) * (sizeof(lakeHairRipLeftAnim) / sizeof(f32)));

	f32 *lakeHairRightAnimCop;
	lakeHairRightAnimCop = malloc(sizeof(f32) * (sizeof(lakeHairRipRightAnim) / sizeof(f32)));

	f32 *lakeTorsoAnimCop;
	lakeTorsoAnimCop = malloc(sizeof(f32) * (sizeof(lakeTorsoAnim) / sizeof(f32)));

	f32 *scissor1AnimCop;
	scissor1AnimCop = malloc(sizeof(f32) * (sizeof(scissor1Anim) / sizeof(f32)));

	f32 *scissor2AnimCop;
	scissor2AnimCop = malloc(sizeof(f32) * (sizeof(scissor2Anim) / sizeof(f32)));

	f32 *lakeMouthEhAnimCop;
	lakeMouthEhAnimCop = malloc(sizeof(f32) * (sizeof(lakeMouthEhAnim) / sizeof(f32)));

	f32 *lakeMouthTeAnimCop;
	lakeMouthTeAnimCop = malloc(sizeof(f32) * (sizeof(lakeMouthClosejawShoteefAnim) / sizeof(f32)));

	f32 *lakeMouthClAnimCop;
	lakeMouthClAnimCop = malloc(sizeof(f32) * (sizeof(lakeMouthCloseAnim) / sizeof(f32)));

	f32 *lakeMouthOAnimCop;
	lakeMouthOAnimCop = malloc(sizeof(f32) * (sizeof(lakeMouthOAnim) / sizeof(f32)));

	f32 *lakeMouthMAnimCop;
	lakeMouthMAnimCop = malloc(sizeof(f32) * (sizeof(lakeMouthMAnim) / sizeof(f32)));

	f32 *camAnimCop;
	camAnimCop = malloc(sizeof(f32) * (sizeof(cameraAnim) / sizeof(f32)));

	f32 *camFovAnimCop;
	camFovAnimCop = malloc(sizeof(f32) * (sizeof(cameraFovAnim) / sizeof(f32)));

	f32 *stringAnimShortCop;
	stringAnimShortCop = malloc(sizeof(f32) * (3285));
	
	f32 *lakeClipAnim;
	lakeClipAnim = malloc(sizeof(f32) * (1620));
	
	/*for (int k = 0; k < sizeof(lakeHairRegularAnim) / sizeof(f32); k++) { 
		lakeHairRegularAnimCop[k] = lakeHairRegularAnim[k];
	}*/

	for (int k = 0; k < sizeof(lakeHeadAnim) / sizeof(f32); k++) { 
		lakeHeadAnimCop[k] = lakeHeadAnim[k];
	}

	for (int k = 0; k < sizeof(lakeEyeAnim) / sizeof(f32); k++) { 
		lakeEyeAnimCop[k] = lakeEyeAnim[k];
	}

	for (int k = 0; k < sizeof(lakePupilLAnim) / sizeof(f32); k++) { 
		lakePupilLAnimCop[k] = lakePupilLAnim[k];
	}

	for (int k = 0; k < sizeof(lakePupilRAnim) / sizeof(f32); k++) { 
		lakePupilRAnimCop[k] = lakePupilRAnim[k];
	}
	
	for (int k = 0; k < sizeof(lakeEyebrowAnim) / sizeof(f32); k++) { 
		lakeEyebrowAnimCop[k] = lakeEyebrowAnim[k];
	}

	for (int k = 0; k < sizeof(lakeTorsoAnim) / sizeof(f32); k++) { 
		lakeTorsoAnimCop[k] = lakeTorsoAnim[k];
	}

	for (int k = 0; k < sizeof(lakeHairRipLeftAnim) / sizeof(f32); k++) {  //
		lakeHairLeftAnimCop[k] = lakeHairRipLeftAnim[k];
	}

	for (int k = 0; k < sizeof(lakeHairRipRightAnim) / sizeof(f32); k++) { 
		lakeHairRightAnimCop[k] = lakeHairRipRightAnim[k];
	}



	for (int k = 0; k < sizeof(scissor1Anim) / sizeof(f32); k++) { 
		scissor1AnimCop[k] = scissor1Anim[k];
	}

	for (int k = 0; k < sizeof(scissor1Anim) / sizeof(f32); k++) { 
		scissor2AnimCop[k] = scissor2Anim[k];
	}



	for (int k = 0; k < sizeof(lakeMouthEhAnim) / sizeof(f32); k++) { 
		lakeMouthEhAnimCop[k] = lakeMouthEhAnim[k];
	}

	for (int k = 0; k < sizeof(lakeMouthClosejawShoteefAnim) / sizeof(f32); k++) { 
		lakeMouthTeAnimCop[k] = lakeMouthClosejawShoteefAnim[k];
	}

	for (int k = 0; k < sizeof(lakeMouthCloseAnim) / sizeof(f32); k++) { 
		lakeMouthClAnimCop[k] = lakeMouthCloseAnim[k];
	}

	for (int k = 0; k < sizeof(lakeMouthOAnim) / sizeof(f32); k++) { 
		lakeMouthOAnimCop[k] = lakeMouthOAnim[k];
	}

	for (int k = 0; k < sizeof(lakeMouthOAnim) / sizeof(f32); k++) { 
		lakeMouthMAnimCop[k] = lakeMouthMAnim[k];
	}



	for (int k = 0; k < sizeof(cameraAnim) / sizeof(f32); k++) { 
		camAnimCop[k] = cameraAnim[k];
	}

	for (int k = 0; k < sizeof(cameraFovAnim) / sizeof(f32); k++) { 
		camFovAnimCop[k] = cameraFovAnim[k];
	}

	for (int k = 0; k < 3285; k++) { 
		stringAnimShortCop[k] = stringAnimShort[k];
	}

	for (int k = 0; k < 1620; k++) { 
		lakeClipAnim[k] = lakeHairCutBitAnim[k];
	}

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



	for (int i = 0; i < sizeof(lakeHairVertCol) / sizeof(f32); i++) { //convert color please
		mdl1VertColk[i] = mdl1VertColk[i] * 0.12f;
		mdl1VertColk[i] = sqrt(mdl1VertColk[i]);
	}
	
	for (int i = 0; i < sizeof(lakeHeadVertCol) / sizeof(f32); i++) { //convert color please
		mdl2VertColk[i] = mdl2VertColk[i] * 0.95f;
		mdl2VertColk[i] = sqrt(mdl2VertColk[i]);
	}

	for (int i = 0; i < sizeof(lakeBodyVertCol) / sizeof(f32); i++) { //convert color please
		mdl5VertColk[i] = mdl5VertColk[i] * 0.12f;
		mdl5VertColk[i] = sqrt(mdl5VertColk[i]);
	}

	for (int i = 0; i < sizeof(lakeHairRipRightVertCol) / sizeof(f32); i++) { //convert color please
		mdl6VertColk[i] = mdl6VertColk[i] * 0.2f;
		mdl6VertColk[i] = sqrt(mdl6VertColk[i]);
	}

	for (int i = 0; i < sizeof(lakeHairRipLeftVertCol) / sizeof(f32); i++) { //convert color please
		mdl7VertColk[i] = mdl7VertColk[i] * 0.2f;
		mdl7VertColk[i] = sqrt(mdl7VertColk[i]);
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
		if ( WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) {
			StopOgg();
			exit(0);
		}		/*
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

		GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
		GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0); 
		

		view[0][0] = camAnimCop[(frame * 12) + 0];
		view[0][1] = camAnimCop[(frame * 12) + 1];
		view[0][2] = camAnimCop[(frame * 12) + 2];
		view[0][3] = camAnimCop[(frame * 12) + 3];	
		view[1][0] = camAnimCop[(frame * 12) + 4];		
		view[1][1] = camAnimCop[(frame * 12) + 5];		
		view[1][2] = camAnimCop[(frame * 12) + 6];		
		view[1][3] = camAnimCop[(frame * 12) + 7];		
		view[2][0] = camAnimCop[(frame * 12) + 8];		
		view[2][1] = camAnimCop[(frame * 12) + 9];		
		view[2][2] = camAnimCop[(frame * 12) + 10];		
		view[2][3] = camAnimCop[(frame * 12) + 11];			
		
		//guLookAt(view, &cam, &up, &look);
		guPerspective(perspective, camFovAnimCop[frame] * 1.6f, aspect, 0.5F, 30.0F);
		GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);

		GX_ClearVtxDesc();
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

		
		if (frame < 1203) {
		guMtxIdentity(model);                                                  //His hair
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakeHeadAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakeHeadAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakeHeadAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakeHeadAnimCop[frame * 9]), lakeHeadAnimCop[(frame * 9) + 1], lakeHeadAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakeHeadAnimCop[(frame * 9) + 6], lakeHeadAnimCop[(frame * 9) + 7], lakeHeadAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 681); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 681; i++) {  //put same number here too
				GX_Position3f32((f32)mdl1VertWork[(i * 3)], (f32)mdl1VertWork[((i * 3) + 1)], (f32)mdl1VertWork[((i * 3) + 2)]);
				//GX_TexCoord2f32(0.0f, 0.0f);
				//GX_Color3f32(1.0f,1.0f,1.0f);
				GX_Color3f32((f32)mdl1VertColk[(i * 3)], (f32)mdl1VertColk[((i * 3) + 1)], (f32)mdl1VertColk[((i * 3) + 2)]);
			}
		}
		GX_End();		
		}		
		
		guMtxIdentity(model);                                                  //His hair , ripped  right
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakeHairRightAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakeHairRightAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakeHairRightAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakeHairRightAnimCop[frame * 9]), lakeHairRightAnimCop[(frame * 9) + 1], lakeHairRightAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakeHairRightAnimCop[(frame * 9) + 6], lakeHairRightAnimCop[(frame * 9) + 7], lakeHairRightAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 579); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 579; i++) {  //put same number here too
				GX_Position3f32((f32)mdl6VertWork[(i * 3)], (f32)mdl6VertWork[((i * 3) + 1)], (f32)mdl6VertWork[((i * 3) + 2)]);

				GX_Color3f32((f32)mdl6VertColk[(i * 3)], (f32)mdl6VertColk[((i * 3) + 1)], (f32)mdl6VertColk[((i * 3) + 2)]);
			}
		}
		GX_End();	

		guMtxIdentity(model);                                                  //His hair , ripped  left
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakeHairLeftAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakeHairLeftAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakeHairLeftAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakeHairLeftAnimCop[frame * 9]), lakeHairLeftAnimCop[(frame * 9) + 1], lakeHairLeftAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakeHairLeftAnimCop[(frame * 9) + 6], lakeHairLeftAnimCop[(frame * 9) + 7], lakeHairLeftAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 162); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 162; i++) {  //put same number here too
				GX_Position3f32((f32)mdl7VertWork[(i * 3)], (f32)mdl7VertWork[((i * 3) + 1)], (f32)mdl7VertWork[((i * 3) + 2)]);

				GX_Color3f32((f32)mdl7VertColk[(i * 3)], (f32)mdl7VertColk[((i * 3) + 1)], (f32)mdl7VertColk[((i * 3) + 2)]);
			}
		}
		GX_End();	

		guMtxIdentity(model);                                                  //His head
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakeHeadAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakeHeadAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakeHeadAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakeHeadAnimCop[frame * 9]), lakeHeadAnimCop[(frame * 9) + 1], lakeHeadAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakeHeadAnimCop[(frame * 9) + 6], lakeHeadAnimCop[(frame * 9) + 7], lakeHeadAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 102); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 102; i++) {  //put same number here too
				GX_Position3f32((f32)mdl2VertWork[(i * 3)], (f32)mdl2VertWork[((i * 3) + 1)], (f32)mdl2VertWork[((i * 3) + 2)]);

				//GX_Color3f32(1.0f,1.0f,1.0f);
				GX_Color3f32((f32)mdl2VertColk[(i * 3)], (f32)mdl2VertColk[((i * 3) + 1)], (f32)mdl2VertColk[((i * 3) + 2)]);
			}
		}
		GX_End();	

		guMtxIdentity(model);                                                  //His eyes
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakeEyeAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakeEyeAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakeEyeAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakeEyeAnimCop[frame * 9]), lakeEyeAnimCop[(frame * 9) + 1], lakeEyeAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakeEyeAnimCop[(frame * 9) + 6], lakeEyeAnimCop[(frame * 9) + 7], lakeEyeAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 180); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 180; i++) {  //put same number here too
				GX_Position3f32((f32)mdl3VertWork[(i * 3)], (f32)mdl3VertWork[((i * 3) + 1)], (f32)mdl3VertWork[((i * 3) + 2)]);

				GX_Color3f32(1.0f,1.0f,1.0f);
			}
		}
		GX_End();	

		guMtxIdentity(model);                                                  //His left pupil
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakePupilLAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakePupilLAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakePupilLAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakePupilLAnimCop[frame * 9]), lakePupilLAnimCop[(frame * 9) + 1], lakePupilLAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakePupilLAnimCop[(frame * 9) + 6], lakePupilLAnimCop[(frame * 9) + 7], lakePupilLAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 6); //amount of faces data were sending ie faces times three

		{
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
		}
		GX_End();	

		guMtxIdentity(model);                                                  //His right pupil
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakePupilRAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakePupilRAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakePupilRAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakePupilRAnimCop[frame * 9]), lakePupilRAnimCop[(frame * 9) + 1], lakePupilRAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakePupilRAnimCop[(frame * 9) + 6], lakePupilRAnimCop[(frame * 9) + 7], lakePupilRAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 6); //amount of faces data were sending ie faces times three

		{
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
		}
		GX_End();	

		guMtxIdentity(model);                                                  //His eyebrows
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakeEyebrowAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakeEyebrowAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakeEyebrowAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakeEyebrowAnimCop[frame * 9]), lakeEyebrowAnimCop[(frame * 9) + 1], lakeEyebrowAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakeEyebrowAnimCop[(frame * 9) + 6], lakeEyebrowAnimCop[(frame * 9) + 7], lakeEyebrowAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 384); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 384; i++) {  //put same number here too
				GX_Position3f32((f32)mdl4VertWork[(i * 3)], (f32)mdl4VertWork[((i * 3) + 1)], (f32)mdl4VertWork[((i * 3) + 2)]);

				GX_Color3f32(1.0f,1.0f,1.0f);
			}
		}
		GX_End();

		guMtxIdentity(model);                                                  //Eh mouth
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakeMouthEhAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakeMouthEhAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakeMouthEhAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakeMouthEhAnimCop[frame * 9]), lakeMouthEhAnimCop[(frame * 9) + 1], lakeMouthEhAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakeMouthEhAnimCop[(frame * 9) + 6], lakeMouthEhAnimCop[(frame * 9) + 7], lakeMouthEhAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 474); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 474; i++) {  //put same number here too
				GX_Position3f32((f32)mouthEhWork[(i * 3)], (f32)mouthEhWork[((i * 3) + 1)], (f32)mouthEhWork[((i * 3) + 2)]);

				GX_Color3f32((f32)mouthEhColk[(i * 3)], (f32)mouthEhColk[((i * 3) + 1)], (f32)mouthEhColk[((i * 3) + 2)]);
			}
		}
		GX_End();	

		guMtxIdentity(model);                                                  //Teeth mouth
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakeMouthTeAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakeMouthTeAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakeMouthTeAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakeMouthTeAnimCop[frame * 9]), lakeMouthTeAnimCop[(frame * 9) + 1], lakeMouthTeAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakeMouthTeAnimCop[(frame * 9) + 6], lakeMouthTeAnimCop[(frame * 9) + 7], lakeMouthTeAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 24); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 24; i++) {  //put same number here too
				GX_Position3f32((f32)mouthTeWork[(i * 3)], (f32)mouthTeWork[((i * 3) + 1)], (f32)mouthTeWork[((i * 3) + 2)]);

				GX_Color3f32((f32)mouthTeColk[(i * 3)], (f32)mouthTeColk[((i * 3) + 1)], (f32)mouthTeColk[((i * 3) + 2)]);
			}
		}
		GX_End();

		guMtxIdentity(model);                                                  //Close mouth
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakeMouthClAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakeMouthClAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakeMouthClAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakeMouthClAnimCop[frame * 9]), lakeMouthClAnimCop[(frame * 9) + 1], lakeMouthClAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakeMouthClAnimCop[(frame * 9) + 6], lakeMouthClAnimCop[(frame * 9) + 7], lakeMouthClAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 12); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 12; i++) {  //put same number here too
				GX_Position3f32((f32)mouthClWork[(i * 3)], (f32)mouthClWork[((i * 3) + 1)], (f32)mouthClWork[((i * 3) + 2)]);
			
				GX_Color3f32(0.027f, 0.027f, 0.027f);
			}
		}
		GX_End();	

		guMtxIdentity(model);                                                  // M mouth
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakeMouthMAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakeMouthMAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakeMouthMAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakeMouthMAnimCop[frame * 9]), lakeMouthMAnimCop[(frame * 9) + 1], lakeMouthMAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakeMouthMAnimCop[(frame * 9) + 6], lakeMouthMAnimCop[(frame * 9) + 7], lakeMouthMAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 24); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 24; i++) {  //put same number here too
				GX_Position3f32((f32)mouthMWork[(i * 3)], (f32)mouthMWork[((i * 3) + 1)], (f32)mouthMWork[((i * 3) + 2)]);
			
				GX_Color3f32(0.027f, 0.027f, 0.027f);
			}
		}
		GX_End();

		guMtxIdentity(model);                                                  // O mouth
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakeMouthOAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakeMouthOAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakeMouthOAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakeMouthOAnimCop[frame * 9]), lakeMouthOAnimCop[(frame * 9) + 1], lakeMouthOAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakeMouthOAnimCop[(frame * 9) + 6], lakeMouthOAnimCop[(frame * 9) + 7], lakeMouthOAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 36); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 36; i++) {  //put same number here too
				GX_Position3f32((f32)mouthOWork[(i * 3)], (f32)mouthOWork[((i * 3) + 1)], (f32)mouthOWork[((i * 3) + 2)]);
			
				GX_Color3f32(0.027f, 0.027f, 0.027f);
			}
		}
		GX_End();	

		guMtxIdentity(model);                                                  //His body
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, lakeTorsoAnimCop[(frame * 9) + 3]);
		guMtxRotAxisRad(mry, &yaxis, lakeTorsoAnimCop[(frame * 9) + 4]);
		guMtxRotAxisRad(mrz, &zaxis, lakeTorsoAnimCop[(frame * 9) + 5]);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxTransApply(model, model, (lakeTorsoAnimCop[frame * 9]), lakeTorsoAnimCop[(frame * 9) + 1], lakeTorsoAnimCop[(frame * 9) + 2]);

		guMtxScaleApply(model2, model2, lakeTorsoAnimCop[(frame * 9) + 6], lakeTorsoAnimCop[(frame * 9) + 7], lakeTorsoAnimCop[(frame * 9) + 8]);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 69); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 69; i++) {  //put same number here too
				GX_Position3f32((f32)mdl5VertWork[(i * 3)], (f32)mdl5VertWork[((i * 3) + 1)], (f32)mdl5VertWork[((i * 3) + 2)]);

				GX_Color3f32((f32)mdl5VertColk[(i * 3)], (f32)mdl5VertColk[((i * 3) + 1)], (f32)mdl5VertColk[((i * 3) + 2)]);
			}
		}
		GX_End();	

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

		guMtxIdentity(model);                                                  //Stage
		guMtxIdentity(model2);
		guMtxIdentity(mrx);
		guMtxIdentity(mry);
		guMtxIdentity(mrz);

		guMtxRotAxisRad(mrx, &xaxis, stage.rx);
		guMtxRotAxisRad(mry, &yaxis, stage.ry);
		guMtxRotAxisRad(mrz, &zaxis, stage.rz);

		guMtxConcat(mrz,mry,mry); 
		guMtxConcat(mry,mrx,model);

		guMtxScaleApply(model2, model2, stage.sx, stage.sy, stage.sz);

		guMtxTransApply(model, model, stage.x, stage.y, stage.z);

		guMtxConcat(model,model2,model);
		guMtxConcat(view,model,modelview);
		// load the modelview matrix into matrix memory
		GX_LoadPosMtxImm(modelview, GX_PNMTX0);

		GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 18); //amount of faces data were sending ie faces times three

		{
			for (int i = 0; i < 18; i++) {  //put same number here too
				GX_Position3f32((f32)stageVertPosCop[(i * 3)], (f32)stageVertPosCop[((i * 3) + 1)], (f32)stageVertPosCop[((i * 3) + 2)]);

				GX_Color3f32((f32)stageVertColCop[(i * 3)], (f32)stageVertColCop[((i * 3) + 1)], (f32)stageVertColCop[((i * 3) + 2)]);
			}
		}
		GX_End();	
	
		if (frame > 165 && frame < 340) {
			guMtxIdentity(model);                                                  //scissor 1
			guMtxIdentity(model2);
			guMtxIdentity(mrx);
			guMtxIdentity(mry);
			guMtxIdentity(mrz);

			guMtxRotAxisRad(mrx, &xaxis, scissor1AnimCop[(frame * 9) + 3]);
			guMtxRotAxisRad(mry, &yaxis, scissor1AnimCop[(frame * 9) + 4]);
			guMtxRotAxisRad(mrz, &zaxis, scissor1AnimCop[(frame * 9) + 5]);

			guMtxConcat(mrz,mry,mry); 
			guMtxConcat(mry,mrx,model);

			guMtxTransApply(model, model, (scissor1AnimCop[frame * 9]), scissor1AnimCop[(frame * 9) + 1], scissor1AnimCop[(frame * 9) + 2]);

			guMtxScaleApply(model2, model2, scissor1AnimCop[(frame * 9) + 6], scissor1AnimCop[(frame * 9) + 7], scissor1AnimCop[(frame * 9) + 8]);

			guMtxConcat(model,model2,model);
			guMtxConcat(view,model,modelview);
			// load the modelview matrix into matrix memory
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 2514); //amount of faces data were sending ie faces times three

			{
				for (int i = 0; i < 2514; i++) {  //put same number here too
					GX_Position3f32((f32)sc1vpcop[(i * 3)], (f32)sc1vpcop[((i * 3) + 1)], (f32)sc1vpcop[((i * 3) + 2)]);
	
					GX_Color3f32((f32)sc1vccop[(i * 3)], (f32)sc1vccop[((i * 3) + 1)], (f32)sc1vccop[((i * 3) + 2)]);
				}
			}
			GX_End();	

			guMtxIdentity(model);                                                  //scissor 2
			guMtxIdentity(model2);
			guMtxIdentity(mrx);
			guMtxIdentity(mry);
			guMtxIdentity(mrz);

			guMtxRotAxisRad(mrx, &xaxis, scissor2AnimCop[(frame * 9) + 3]);
			guMtxRotAxisRad(mry, &yaxis, scissor2AnimCop[(frame * 9) + 4]);
			guMtxRotAxisRad(mrz, &zaxis, scissor2AnimCop[(frame * 9) + 5]);

			guMtxConcat(mrz,mry,mry); 
			guMtxConcat(mry,mrx,model);

			guMtxTransApply(model, model, (scissor2AnimCop[frame * 9]), scissor2AnimCop[(frame * 9) + 1], scissor2AnimCop[(frame * 9) + 2]);

			guMtxScaleApply(model2, model2, scissor2AnimCop[(frame * 9) + 6], scissor2AnimCop[(frame * 9) + 7], scissor2AnimCop[(frame * 9) + 8]);

			guMtxConcat(model,model2,model);
			guMtxConcat(view,model,modelview);
			// load the modelview matrix into matrix memory
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 1452); //amount of faces data were sending ie faces times three

			{
				for (int i = 0; i < 1452; i++) {  //put same number here too
					GX_Position3f32((f32)sc2vpcop[(i * 3)], (f32)sc2vpcop[((i * 3) + 1)], (f32)sc2vpcop[((i * 3) + 2)]);
	
					GX_Color3f32((f32)sc2vccop[(i * 3)], (f32)sc2vccop[((i * 3) + 1)], (f32)sc2vccop[((i * 3) + 2)]);
				}
			}
			GX_End();	
		}

		if (frame > 842 && frame < 1204) {
			guMtxIdentity(model);                                                  //string
			guMtxIdentity(model2);
			guMtxIdentity(mrx);
			guMtxIdentity(mry);
			guMtxIdentity(mrz);

			guMtxRotAxisRad(mrx, &xaxis, stringAnimShortCop[((frame - 839) * 9) + 3]);
			guMtxRotAxisRad(mry, &yaxis, stringAnimShortCop[((frame - 839) * 9) + 4]);
			guMtxRotAxisRad(mrz, &zaxis, stringAnimShortCop[((frame - 839) * 9) + 5]);

			guMtxConcat(mrz,mry,mry); 
			guMtxConcat(mry,mrx,model);

			guMtxTransApply(model, model, (stringAnimShortCop[((frame - 839) * 9) + 0]), (stringAnimShortCop[((frame - 839) * 9) + 1]), stringAnimShortCop[((frame - 839) * 9) + 2]);

			guMtxScaleApply(model2, model2, stringAnimShortCop[((frame - 839) * 9) + 6], stringAnimShortCop[((frame - 839) * 9) + 7], stringAnimShortCop[((frame - 839) * 9) + 8]);

			guMtxConcat(model,model2,model);
			guMtxConcat(view,model,modelview);
			// load the modelview matrix into matrix memory
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 600); //amount of faces data were sending ie faces times three

			{
				for (int i = 0; i < 600; i++) {  //put same number here too
					GX_Position3f32((f32)stringVertPosCop[(i * 3)], (f32)stringVertPosCop[((i * 3) + 1)], (f32)stringVertPosCop[((i * 3) + 2)]);
	
					GX_Color3f32(0.672f, 0.672f, 0.672f);
				}
			}
			GX_End();		
		}

		if (frame > 171 && frame < 349) {
			guMtxIdentity(model);                                                  //hair clipping
			guMtxIdentity(model2);
			guMtxIdentity(mrx);
			guMtxIdentity(mry);
			guMtxIdentity(mrz);

			guMtxRotAxisRad(mrx, &xaxis, lakeClipAnim[((frame - 169) * 9) + 3]);
			guMtxRotAxisRad(mry, &yaxis, lakeClipAnim[((frame - 169) * 9) + 4]);
			guMtxRotAxisRad(mrz, &zaxis, lakeClipAnim[((frame - 169) * 9) + 5]);

			guMtxConcat(mrz,mry,mry); 
			guMtxConcat(mry,mrx,model);

			guMtxTransApply(model, model, (lakeClipAnim[((frame - 169) * 9) + 0]), (lakeClipAnim[((frame - 169) * 9) + 1]), lakeClipAnim[((frame - 169) * 9) + 2]);

			guMtxScaleApply(model2, model2, lakeClipAnim[((frame - 169) * 9) + 6], lakeClipAnim[((frame - 169) * 9) + 7], lakeClipAnim[((frame - 169) * 9) + 8]);

			guMtxConcat(model,model2,model);
			guMtxConcat(view,model,modelview);
			// load the modelview matrix into matrix memory
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
			GX_End();		
		}

		if (frame > 1688 && frame < 1830) {
			guMtxIdentity(model);                                                  //text3

			guMtxConcat(view,model,modelview);
			// load the modelview matrix into matrix memory
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 3789); //amount of faces data were sending ie faces times three

			{
				for (int i = 0; i < 3789; i++) {  //put same number here too
					GX_Position3f32((f32)text3VertPosCop[(i * 3)], (f32)text3VertPosCop[((i * 3) + 1)], (f32)text3VertPosCop[((i * 3) + 2)]);
	
					GX_Color3f32(1.0f, 1.0f, 1.0f);
				}
			}
			GX_End();	
		}

		if (frame > 1538 && frame < 1689) {
			guMtxIdentity(model);                                                  //text1

			guMtxConcat(view,model,modelview);
			// load the modelview matrix into matrix memory
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 5820); //amount of faces data were sending ie faces times three

			{
				for (int i = 0; i < 5820; i++) {  //put same number here too
					GX_Position3f32((f32)text1VertPosCop[(i * 3)], (f32)text1VertPosCop[((i * 3) + 1)], (f32)text1VertPosCop[((i * 3) + 2)]);
	
					GX_Color3f32(1.0f, 1.0f, 1.0f);
				}
			}
			GX_End();	

			guMtxIdentity(model);                                                  //text2

			guMtxConcat(view,model,modelview);
			// load the modelview matrix into matrix memory
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 822); //amount of faces data were sending ie faces times three

			{
				for (int i = 0; i < 822; i++) {  //put same number here too
					GX_Position3f32((f32)text2VertPosCop[(i * 3)], (f32)text2VertPosCop[((i * 3) + 1)], (f32)text2VertPosCop[((i * 3) + 2)]);
	
					GX_Color3f32(0.8f, 0.8f, 0.8f);
				}
			}
			GX_End();		
	


			GX_SetTevOp(GX_TEVSTAGE0,GX_REPLACE);                                 //change settings for the first tev stage to handle textures instead of skipping texture tasks
			GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0); 

			GX_ClearVtxDesc();
			GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);                                  //change vertex descriptors to do texture coords instead of vertex colors
			GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	
			guMtxIdentity(model);                                                  //album art

			guMtxConcat(view,model,modelview);
			// load the modelview matrix into matrix memory
			GX_LoadPosMtxImm(modelview, GX_PNMTX0);

			GX_Begin(GX_TRIANGLES, GX_VTXFMT1, 6); //amount of faces data were sending ie faces times three

			{
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
			GX_End();	
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
