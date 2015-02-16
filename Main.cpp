//----------------------------------------------------------------------------------
//		サンプルビデオフィルタ(フィルタプラグイン)  for AviUtl ver0.99e以降
//----------------------------------------------------------------------------------
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#include <windows.h>

#include "filter.h"
#include "opencv2\opencv.hpp"
#include <string>
using namespace cv;



//---------------------------------------------------------------------
//		フィルタ構造体定義
//---------------------------------------------------------------------
#define	TRACK_N	10														//	トラックバーの数
TCHAR	*track_name[] = { "ThresU", "ThresL", "preBlur","FindAlg", "FillAlg", "SamPx", "HSV", "Speed", "Upscale", "Dilate" };	//	トラックバーの名前
int		track_default[] = { 130, 40, 3, 3, 1, 5, 0, 0, 4, 1 };	//	トラックバーの初期値
int		track_s[] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 };	//	トラックバーの下限値
int		track_e[] = { 255, 255, 20, 4, 1, 50, 3, 90, 4, 10 };	//	トラックバーの上限値


#define	CHECK_N	4														//	チェックボックスの数
TCHAR	*check_name[] = { "Show selection box", "Preset:DEFAULT", "Preset:Fast", "Preset:Thin and Very Pale"};				//	チェックボックスの名前
int		check_default[] = { 1, -1, -1, -1 };				//	チェックボックスの初期値 (値は0か1)


#ifdef __AVX__
string versionInfo = "mtDeSub rev." + (string)__DATE__ + " AVX by Maverick Tse";
#else
string versionInfo = "mtDeSub rev." + (string)__DATE__ + " SSE2 by Maverick Tse";
#endif

FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION | FILTER_FLAG_MAIN_MESSAGE,	//	フィルタのフラグ
	//	FILTER_FLAG_ALWAYS_ACTIVE		: フィルタを常にアクティブにします
	//	FILTER_FLAG_CONFIG_POPUP		: 設定をポップアップメニューにします
	//	FILTER_FLAG_CONFIG_CHECK		: 設定をチェックボックスメニューにします
	//	FILTER_FLAG_CONFIG_RADIO		: 設定をラジオボタンメニューにします
	//	FILTER_FLAG_EX_DATA				: 拡張データを保存出来るようにします。
	//	FILTER_FLAG_PRIORITY_HIGHEST	: フィルタのプライオリティを常に最上位にします
	//	FILTER_FLAG_PRIORITY_LOWEST		: フィルタのプライオリティを常に最下位にします
	//	FILTER_FLAG_WINDOW_THICKFRAME	: サイズ変更可能なウィンドウを作ります
	//	FILTER_FLAG_WINDOW_SIZE			: 設定ウィンドウのサイズを指定出来るようにします
	//	FILTER_FLAG_DISP_FILTER			: 表示フィルタにします
	//	FILTER_FLAG_EX_INFORMATION		: フィルタの拡張情報を設定できるようにします
	//	FILTER_FLAG_NO_CONFIG			: 設定ウィンドウを表示しないようにします
	//	FILTER_FLAG_AUDIO_FILTER		: オーディオフィルタにします
	//	FILTER_FLAG_RADIO_BUTTON		: チェックボックスをラジオボタンにします
	//	FILTER_FLAG_WINDOW_HSCROLL		: 水平スクロールバーを持つウィンドウを作ります
	//	FILTER_FLAG_WINDOW_VSCROLL		: 垂直スクロールバーを持つウィンドウを作ります
	//	FILTER_FLAG_IMPORT				: インポートメニューを作ります
	//	FILTER_FLAG_EXPORT				: エクスポートメニューを作ります
	0, 0,						//	設定ウインドウのサイズ (FILTER_FLAG_WINDOW_SIZEが立っている時に有効)
	"mtDeSub",			//	フィルタの名前
	TRACK_N,					//	トラックバーの数 (0なら名前初期値等もNULLでよい)
	track_name,					//	トラックバーの名前郡へのポインタ
	track_default,				//	トラックバーの初期値郡へのポインタ
	track_s, track_e,			//	トラックバーの数値の下限上限 (NULLなら全て0～256)
	CHECK_N,					//	チェックボックスの数 (0なら名前初期値等もNULLでよい)
	check_name,					//	チェックボックスの名前郡へのポインタ
	check_default,				//	チェックボックスの初期値郡へのポインタ
	func_proc,					//	フィルタ処理関数へのポインタ (NULLなら呼ばれません)
	func_init,						//	開始時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	func_exit,						//	終了時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL,						//	設定が変更されたときに呼ばれる関数へのポインタ (NULLなら呼ばれません)
	func_WndProc,						//	設定ウィンドウにウィンドウメッセージが来た時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL, NULL,					//	システムで使いますので使用しないでください
	NULL,						//  拡張データ領域へのポインタ (FILTER_FLAG_EX_DATAが立っている時に有効)
	NULL,						//  拡張データサイズ (FILTER_FLAG_EX_DATAが立っている時に有効)
	(TCHAR*)versionInfo.c_str(),
	//  フィルタ情報へのポインタ (FILTER_FLAG_EX_INFORMATIONが立っている時に有効)
	NULL,						//	セーブが開始される直前に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL,						//	セーブが終了した直前に呼ばれる関数へのポインタ (NULLなら呼ばれません)
};


//---------------------------------------------------------------------
//		Global Variables
//---------------------------------------------------------------------

//FILTER_PROC_INFO fpi; //a global store for fpip for use by all functions. Do not use if possible.
BOOL isMouseDown = FALSE; //For tracking mouse drag
BOOL isSelectionDefined = FALSE; //track if user has selected an area
int ref_frame = 0; //ref frame #
typedef struct {
	short	x, y;
} SHORTPOINT;
SHORTPOINT mousePos1; //previous mouse position on main window(on_mouse_down)
SHORTPOINT mousePos2; //position for current position (on_mouse_move/up)

Mat OCVRGB; //IMPORTANT: Current image store in OCV's format
Mat img_grey;



//---------------------------------------------------------------------
//		Function Forward Declerations (every function carries FILTER pointer for convenient)
//---------------------------------------------------------------------
//static void Aviutl2OCV(FILTER *fp, FILTER_PROC_INFO *src, Mat &dest); //Convert Aviutl's image to OCV's BGR. Crop away canvas.
//static void Aviutl2OCV(FILTER *src, Mat &dest, void *editp); //Convert Aviutl's image to OCV's BGR. Overloaded function that takes FILTER instead of FILTER_PROC_INFO
static void Aviutl2OCVFast(FILTER *fp, FILTER_PROC_INFO *src, Mat &dest); //MUCH simplified version!
//static void OCV2AviUtl(FILTER *fp, Mat &src, FILTER_PROC_INFO *dest); //Display OCV's image on AviUtl main window. Output must be FILTER_PROC_INFO pointer. Note: OCV image has no canvas area.
static void OCV2AviUtlFast(FILTER *fp, Mat &src, FILTER_PROC_INFO *dest);
static void DrawSelection(FILTER *fp, FILTER_PROC_INFO *dest); //Draw selection and display it
static void fillEdgeImage(Mat edgesIn, Mat& filledEdgesOut); //Stupid way of filling edge
static void imfill(Mat edgesIn, Mat& filledEdgesOut); //Better way to fill edge...
//static void delogo(FILTER *fp); //the actual delogo function
static void delogoF(FILTER *fp); //speed up version

//---------------------------------------------------------------------
//		フィルタ構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTable(void)
{
	return &filter;
}
//	下記のようにすると1つのaufファイルで複数のフィルタ構造体を渡すことが出来ます
/*
FILTER_DLL *filter_list[] = {&filter,&filter2,NULL};
EXTERN_C FILTER_DLL __declspec(dllexport) ** __stdcall GetFilterTableList( void )
{
return (FILTER_DLL **)&filter_list;
}
*/

//---------------------------------------------------------------------
//		Initialization
//---------------------------------------------------------------------
BOOL func_init(FILTER *fp)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	mousePos1.x = 0;
	mousePos1.y = 0;
	mousePos2.x = 0;
	mousePos2.y = 0;
	return TRUE;
}

//---------------------------------------------------------------------
//		Before ending
//---------------------------------------------------------------------
BOOL func_exit(FILTER *fp)
{
	//mem_free();
	//_CrtDumpMemoryLeaks();
	return TRUE;
}




//---------------------------------------------------------------------
//		フィルタ処理関数
//---------------------------------------------------------------------
BOOL func_proc(FILTER *fp, FILTER_PROC_INFO *fpip)
{
	//
	//	fp->track[n]			: トラックバーの数値
	//	fp->check[n]			: チェックボックスの数値
	//	fpip->w 				: 実際の画像の横幅
	//	fpip->h 				: 実際の画像の縦幅
	//	fpip->max_w				: 画像領域の横幅
	//	fpip->max_h				: 画像領域の縦幅
	//	fpip->ycp_edit			: 画像領域へのポインタ
	//	fpip->ycp_temp			: テンポラリ領域へのポインタ
	//	fpip->ycp_edit[n].y		: 画素(輝度    )データ (     0 ～ 4096 )
	//	fpip->ycp_edit[n].cb	: 画素(色差(青))データ ( -2048 ～ 2048 )
	//	fpip->ycp_edit[n].cr	: 画素(色差(赤))データ ( -2048 ～ 2048 )
	//
	//  画素データは範囲外に出ていることがあります。
	//  また範囲内に収めなくてもかまいません。
	//
	//	画像サイズを変えたいときは fpip->w や fpip->h を変えます。
	//
	//	テンポラリ領域に処理した画像を格納したいときは
	//	fpip->ycp_edit と fpip->ycp_temp を入れ替えます。
	//
	if (fp->exfunc->is_filter_active(fp) && fp->exfunc->is_editing(fpip->editp))
	{
		//Aviutl2OCV(fp, fpip, OCVRGB);
		Aviutl2OCVFast(fp, fpip, OCVRGB);
		
		if (fp->check[0])
		{
			DrawSelection(fp, fpip);
		}
		
		if (isSelectionDefined)
		{
			delogoF(fp);
		}
		
		//OCV2AviUtl(fp, OCVRGB, fpip);
		OCV2AviUtlFast(fp, OCVRGB, fpip);
		//imshow("Test Image", OCVRGB);

	}
	return TRUE;
}


//---------------------------------------------------------------------
//		WndProc (lparam is actually the mouse coordinate)
//---------------------------------------------------------------------
BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp)
{
	//TODO
	switch (message)
	{
	case WM_FILTER_INIT:
		
		SetWindowText(fp->hwnd, "mtDeSub");
		return TRUE;
	case WM_FILTER_MAIN_MOUSE_DOWN:
		if (fp->exfunc->get_frame_n(editp)>1)
		{
			isMouseDown = TRUE;
			isSelectionDefined = FALSE;
			SetWindowText(fp->hwnd, "Selecting...");
			SHORTPOINT* msp = (SHORTPOINT*)&lparam;
			mousePos1.x = msp->x;
			mousePos1.y = msp->y;
			if (mousePos1.x<0) mousePos1.x = 0;
			if (mousePos1.y<0) mousePos1.y = 0;

			int fw, fh;
			fp->exfunc->get_frame_size(editp, &fw, &fh);

			if (mousePos1.x >= fw) mousePos1.x = fw - 1;
			if (mousePos1.y >= fh) mousePos1.y = fh - 1;
			return FALSE;
		}
		break;
	case WM_FILTER_MAIN_MOUSE_MOVE:
		if (fp->exfunc->get_frame_n(editp)>1 && isMouseDown)
		{
			SHORTPOINT* msp = (SHORTPOINT*)&lparam;
			mousePos2.x = msp->x;
			mousePos2.y = msp->y;
			if (mousePos2.x<0) mousePos2.x = 0;
			if (mousePos2.y<0) mousePos2.y = 0;

			int fw, fh;
			fp->exfunc->get_frame_size(editp, &fw, &fh);

			if (mousePos2.x >= fw) mousePos2.x = fw - 1;
			if (mousePos2.y >= fh) mousePos2.y = fh - 1;
			return TRUE;
		}
		break;
	case WM_FILTER_MAIN_MOUSE_UP:
		if (fp->exfunc->get_frame_n(editp)>1)
		{
			isMouseDown = FALSE;
			isSelectionDefined = TRUE;
			SetWindowText(fp->hwnd, "Selected");
			SHORTPOINT* msp = (SHORTPOINT*)&lparam;
			mousePos2.x = msp->x;
			mousePos2.y = msp->y;
			if (mousePos2.x<0) mousePos2.x = 0;
			if (mousePos2.y<0) mousePos2.y = 0;

			int fw, fh;
			fp->exfunc->get_frame_size(editp, &fw, &fh);

			if (mousePos2.x >= fw) mousePos2.x = fw - 1;
			if (mousePos2.y >= fh) mousePos2.y = fh - 1;
			return TRUE;
		}
		//break;
	case WM_COMMAND:
		switch (wparam)
		{
		case MID_FILTER_BUTTON+1: //Load Defaults
			for (int i = 0; i < TRACK_N; i++)
			{
				fp->track[i] = fp->track_default[i];
			}
			fp->exfunc->filter_window_update(fp);
			return TRUE;
		case MID_FILTER_BUTTON+2: //Load Fast presets
			fp->track[0] = 80;
			fp->track[1] = 40;
			fp->track[2] = 0;
			fp->track[3] = 2;
			fp->track[4] = 0;
			fp->track[5] = 2;
			fp->track[6] = 0;
			fp->track[7] = 50;
			fp->track[8] = 2;
			fp->track[9] = 1;
			fp->exfunc->filter_window_update(fp);
			return TRUE;
		case MID_FILTER_BUTTON+3: //Faint presets
			fp->track[0] = 36;
			fp->track[1] = 10;
			fp->track[2] = 0;
			fp->track[3] = 3;
			fp->track[4] = 1;
			fp->track[5] = 10;
			fp->track[6] = 0;
			fp->track[7] = 0;
			fp->track[8] = 4;
			fp->track[9] = 0;
			fp->exfunc->filter_window_update(fp);
			return TRUE;

		default:
			return FALSE;
		}
	}
	return FALSE;
}


//########################################################################
//     Function Implementations
//########################################################################

/*
//Convert  Au image struct to OCV Mat using fp and fpip
void Aviutl2OCV(FILTER *fp, FILTER_PROC_INFO *src, Mat &dest)
{
	//Test:OK
	if (src) //if fpip is not a null pointer, do the main work
	{
		int fw = src->w;
		int fh = src->h;
		int aubufsize = fw*fh;
		PIXEL* rgb = new PIXEL[aubufsize];
		PIXEL_YC* yc = new PIXEL_YC[aubufsize];
		//PIXEL_YC* yc_base = yc;
		//PIXEL_YC* yctemp_base = src->ycp_edit;
		Mat mBuffer(fh, fw, CV_8UC3);

		//int canvasOffset = (src->max_w) - (src->w);
		int src_idx, dest_idx = 0;
		//maka copy of YC image removing canvas
		for (int row = 0; row< fh; ++row)
		{
			for (int col = 0; col< fw; ++col)
			{
				src_idx = col + row*src->max_w;
				dest_idx = col + row*fw;
				yc[dest_idx].y = src->ycp_edit[src_idx].y;
				yc[dest_idx].cb = src->ycp_edit[src_idx].cb;
				yc[dest_idx].cr = src->ycp_edit[src_idx].cr;

				//yc->cb = yctemp_base->cb;
				//yc->cr = yctemp_base->cr;
				//++yc;
				//++yctemp_base;
			}
			//yctemp_base += canvasOffset; //skip canvas
		}
		//restore pointer
		//yc = yc_base;


		BOOL rgbOK = fp->exfunc->yc2rgb(rgb, yc, aubufsize); //color conversion
		src_idx = 0;
		if (rgbOK)
		{

			//PIXEL* rgbDataCopy = rgb; // going to work on a copy of the pointer
			for (int row = 0; row< src->h; ++row) //loop through each row
			{
				for (int col = 0; col< src->w; ++col) //and each column
				{
					src_idx = col + row*fw;
					
					mBuffer.at<Vec3b>(row, col)[0] = rgb[src_idx].b; //Copy Blue value
					mBuffer.at<Vec3b>(row, col)[1] = rgb[src_idx].g; //Copy Green value
					mBuffer.at<Vec3b>(row, col)[2] = rgb[src_idx].r; //Copy Red value
					
					//mBuffer.at<Vec3b>(row, col)[0] = rgb->b; //Copy Blue value
					//mBuffer.at<Vec3b>(row, col)[1] = rgb->g; //Copy Green value
					//mBuffer.at<Vec3b>(row, col)[2] = rgb->r; //Copy Red value
					//++rgb; //move to next pixel
				}

			}
			//rgb = rgbDataCopy;
		}
		else
		{
			SetWindowText(fp->hwnd, "YC2RGB failed!");
		}
		mBuffer.copyTo(dest);
		delete[] rgb;
		delete[] yc;
		//cvtColor(mBuffer, dest, CV_RGB2BGR);
		//mBuffer.copyTo(dest);
		//dest = mBuffer;
	}
	else //or throw error
	{
		SetWindowText(fp->hwnd, "fpip is NULL!");
	}

}

*/
/*
//Convert  Au image struct to OCV Mat using fp and editp
void Aviutl2OCV(FILTER *src, Mat &dest, void *editp)
{
	
	int srcw, srch;
	src->exfunc->get_frame_size(editp, &srcw, &srch);

	
	if (src) //if fp is not a null pointer, do the main work
	{
		PIXEL* rgb = new PIXEL[srcw*srch];
		BOOL gotRGB = src->exfunc->get_pixel_filtered(editp, src->exfunc->get_frame(editp), rgb, &srcw, &srch);
		if (gotRGB)
		{
			Mat Buffer(srch, srcw, CV_8UC3);
			dest.create(srch, srcw, CV_8UC3); //allocate space for OCV image
			PIXEL* rgbDataCopy = rgb; // going to work on a copy of the pointer
			for (int row = 0; row< srch; ++row) //loop through each row
			{
				for (int col = 0; col< srcw; ++col) //and each column
				{
					Buffer.at<Vec3b>(row, col)[0] = rgbDataCopy->b; //Copy Blue value
					Buffer.at<Vec3b>(row, col)[1] = rgbDataCopy->g; //Copy Green value
					Buffer.at<Vec3b>(row, col)[2] = rgbDataCopy->r; //Copy Red value
					++rgbDataCopy; //move to next pixel
				}
			}
			flip(Buffer, dest, 0);
		}
		else
		{
			SetWindowText(src->hwnd, "Failed to get RGB image");
		}

	}
}

*/

/*
//Display an OCV image on Aviutl main window
void OCV2AviUtl(FILTER *fp, Mat &src, FILTER_PROC_INFO *dest)
{
	//TEST OK
	if (dest && !src.empty()) //if fpip is not null and src has image
	{
		PIXEL *RGBTEMP = new PIXEL[dest->max_w*dest->max_h]; //allocate rgb buffer
		//PIXEL *rgbPointerBase = RGBTEMP;
		int dest_idx = 0;

		
			for (int row = 0; row < dest->h; ++row)
			{
				
				for (int col = 0; col < dest->w; ++col)
				{
					dest_idx = col + row* dest->max_w;
					RGBTEMP[dest_idx].b = src.at<Vec3b>(row, col)[0]; //Blue
					RGBTEMP[dest_idx].g = src.at<Vec3b>(row, col)[1]; //Green
					RGBTEMP[dest_idx].r = src.at<Vec3b>(row, col)[2]; //Red

					//RGBTEMP->b = src.at<Vec3b>(row, col)[0]; //Blue
					//RGBTEMP->g = src.at<Vec3b>(row, col)[1]; //Green
					//RGBTEMP->r = src.at<Vec3b>(row, col)[2]; //Red

					//++RGBTEMP; //to next pixel
				}
				//RGBTEMP += dest->max_w - dest->w; //add canvas area
			}
		
		//RGBTEMP = rgbPointerBase; //restore pointer base
		BOOL ycOK = fp->exfunc->rgb2yc(dest->ycp_temp, RGBTEMP, dest->max_w*dest->max_h); //rgb to YC
		if (ycOK)
		{
			PIXEL_YC *YCTEMP = dest->ycp_edit; //Swap display and buffer when done.
			dest->ycp_edit = dest->ycp_temp;
			dest->ycp_temp = YCTEMP;
		}
		else
		{
			SetWindowText(fp->hwnd, "RGB2YC failed!");
		}
		delete[] RGBTEMP;
	}
	else
	{
		SetWindowText(fp->hwnd, "dest is null or src is empty");
	}
}

*/

void DrawSelection(FILTER *fp, FILTER_PROC_INFO *dest)
{
	//TEST OK
	if (!OCVRGB.empty())
	{
		rectangle(OCVRGB, Point(mousePos1.x, mousePos1.y), Point(mousePos2.x, mousePos2.y), CV_RGB(200, 200, 200), 1);
	}
	else
	{
		SetWindowText(fp->hwnd, "No image");
	}
}

void fillEdgeImage(Mat edgesIn, Mat& filledEdgesOut)
{
	Mat edgesNeg = edgesIn.clone();
	
	//Floodfill along width
	for (int i = 0; i < edgesIn.cols; i++)
	{
		floodFill(edgesNeg, Point(i, 0), CV_RGB(255, 255, 255));
		floodFill(edgesNeg, Point(i, edgesIn.rows-1), CV_RGB(255, 255, 255));
	}

	//Floodfill along height
	for (int i = 0; i < edgesIn.rows; i++)
	{
		floodFill(edgesNeg, Point(0, i), CV_RGB(255, 255, 255));
		floodFill(edgesNeg, Point(edgesIn.cols-1, i), CV_RGB(255, 255, 255));
	}

	//Mat element = getStructuringElement(MORPH_RECT,
	//	Size(2, 2),
	//	Point(1, 1));
	//dilate(edgesNeg, edgesNeg, element);
	bitwise_not(edgesNeg, edgesNeg);
	filledEdgesOut = (edgesNeg | edgesIn);

	return;
}

void imfill(Mat edgesIn, Mat& filledEdgesOut)
{
	Mat edgesNeg(edgesIn.rows+10, edgesIn.cols+10, edgesIn.type()); //add 5pixels to each side
	edgesNeg = Mat::zeros(edgesNeg.size(), edgesNeg.type());
	Rect usr_content(5, 5, edgesIn.cols, edgesIn.rows); //define where the input should copy to
	edgesIn.copyTo(edgesNeg(usr_content)); //now we have the BW image with black border
	floodFill(edgesNeg, Point(0, 0), CV_RGB(255, 255, 255)); //now we only need to fill from the origin
	Mat edgeDone(edgesNeg, usr_content); //trim it back to original size for bitwise ops afterwards
	bitwise_not(edgeDone, edgeDone); //invert -> holes are white
	filledEdgesOut = (edgeDone | edgesIn); //fill holes and store to output
	
}


/*
void delogo(FILTER *fp)
{
	Rect roi(Point(mousePos1.x, mousePos1.y), Point(mousePos2.x, mousePos2.y));
	//Determine on how to make the greyscale image
	if (fp->track[6] == 0)
	{
		cvtColor(OCVRGB, img_grey, CV_BGR2GRAY);
	}
	else
	{
		Mat hsv(OCVRGB.size(), CV_8UC3);
		vector<Mat> channels;
		cvtColor(OCVRGB, hsv, CV_BGR2HSV_FULL);
		split(hsv, channels);
		if (fp->track[6] == 1)
		{
			img_grey = channels[0];
		}
		else if (fp->track[6] == 2)
		{
			img_grey = channels[1];
		}
		else
		{
			img_grey = channels[2];
		}
	}
	//crop the image to selected area
	Mat roiImage(img_grey, roi);

	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;
	blur(roiImage, roiImage, Size(fp->track[2], fp->track[2])); //pre-blur to reduce noise
	Canny(roiImage, roiImage, fp->track[0], fp->track[1]);
	//dialate image
	Mat element = getStructuringElement(MORPH_RECT,
		Size(fp->track[2] + 2, fp->track[2] + 2),
		Point(fp->track[2], fp->track[2]));
	dilate(roiImage, roiImage, element);
	
	//track counter
	findContours(roiImage, contours, hierarchy, CV_RETR_EXTERNAL, fp->track[3]);
	Mat filled_mask = Mat::zeros(roiImage.size(), CV_8UC1);
	//vector<Point> approxShape;
	for (UINT i = 0; i < contours.size(); i++)
	{
		//approxPolyDP(contours[i], approxShape, arcLength(Mat(contours[i]), true)*0.04, true);
		drawContours(filled_mask, contours, i, 255, 2, 8, hierarchy, 0, Point());
	}
	dilate(filled_mask, filled_mask, element);
	fillEdgeImage(filled_mask, filled_mask); //fill holes
	//fillEdgeImage(roiImage, roiImage); //fill holes
	Mat final_mask = Mat::zeros(img_grey.size(), CV_8UC1);
	filled_mask.copyTo(final_mask(roi));
	//roiImage.copyTo(final_mask(roi));
	if (!fp->track[7]) //no speed boost
	{
		inpaint(OCVRGB, final_mask, OCVRGB, fp->track[5], fp->track[4]);
	}
	else //with speed boost
	{
		//transform boost to scaling ratio
		double scale = static_cast<double>(100 - fp->track[7]) / 100.0;
		Mat sMask, sImg, eImg;
		resize(final_mask, sMask, Size(0, 0), scale, scale, INTER_AREA); //shrink the mask
		resize(OCVRGB, sImg, Size(0, 0), scale, scale, INTER_AREA); //shrink the frame image
		inpaint(sImg, sMask, sImg, fp->track[5], fp->track[4]); //inpaint the shrinked image
		resize(sImg, eImg, OCVRGB.size(), 0, 0, INTER_LANCZOS4); //Upscale the fixed image
		Mat mImg(eImg, roi); //extracts the subtitle area
		mImg.copyTo(OCVRGB(roi)); //overwrite the subtitle area
	}
}
*/
void delogoF(FILTER *fp)
{
	Rect roi(Point(mousePos1.x, mousePos1.y), Point(mousePos2.x, mousePos2.y));
	Mat sOCVRGB(OCVRGB, roi);
	double scale = static_cast<double>(100 - fp->track[7]) / 100.0;
	double scaledX, scaledY;
	scaledX = sOCVRGB.size().width * scale;
	scaledY = sOCVRGB.size().height * scale;
	if (scaledX < 4 || scaledY < 4) //check if too small
	{
		scale = 100.0; //ignore scale if too small
	}
	resize(sOCVRGB, sOCVRGB, Size(0, 0), scale, scale, INTER_AREA);
	Mat roiImage;
	//Determine on how to make the greyscale image
	if (fp->track[6] == 0)
	{
		cvtColor(sOCVRGB, roiImage, COLOR_BGR2GRAY);
	}
	else
	{
		Mat hsv(sOCVRGB.size(), CV_8UC3);
		std::vector<Mat> channels;
		cvtColor(sOCVRGB, hsv, COLOR_BGR2HSV_FULL);
		split(hsv, channels);
		if (fp->track[6] == 1)
		{
			roiImage = channels[0];
		}
		else if (fp->track[6] == 2)
		{
			roiImage = channels[1];
		}
		else
		{
			roiImage = channels[2];
		}
	}
	//crop the image to selected area
	//Mat roiImage(img_grey, roi);

	std::vector<std::vector<Point> > contours;
	std::vector<Vec4i> hierarchy;
	if (fp->track[2] > 0) //blur with size 0,0 will crash
	{
		blur(roiImage, roiImage, Size(fp->track[2], fp->track[2])); //pre-blur to reduce noise
	}
	Canny(roiImage, roiImage, fp->track[0], fp->track[1]);
	//dialate image
	if (fp->track[9] > 0) //Use boundary dilation?
	{
		Mat element = getStructuringElement(MORPH_RECT,
			Size(2+fp->track[9], 2+fp->track[9])); //for MORPH_RECT, need at least 3,3 to have visible effect
		dilate(roiImage, roiImage, element);
	}
	//track counter
	findContours(roiImage, contours, hierarchy, RETR_EXTERNAL, fp->track[3]);//External outline only
	Mat filled_mask = Mat::zeros(roiImage.size(), CV_8UC1);
	//vector<Point> approxShape;
	#pragma omp parallel for
	for (int i = 0; i < contours.size(); i++)
	{
		//approxPolyDP(contours[i], approxShape, arcLength(Mat(contours[i]), true)*0.04, true);
		//drawContours(filled_mask, contours, i, 255, 2, 8, hierarchy, 0, Point());
		drawContours(filled_mask, contours, i, 255, 2, 8, hierarchy, 0, Point());
	}
	if (fp->track[9] > 0)// Dilation after counter drawing
	{
		Mat element = getStructuringElement(MORPH_RECT,
			Size(2+fp->track[9], 2+fp->track[9]));
		dilate(filled_mask, filled_mask, element);
	}
	//fillEdgeImage(filled_mask, filled_mask); //fill holes, stupid way.
	imfill(filled_mask, filled_mask);
	//dilate(filled_mask, filled_mask, element);
	Mat final_mask = Mat::zeros(Size(static_cast<int>(OCVRGB.cols*scale), static_cast<int>(OCVRGB.rows*scale)), CV_8UC1);

	int new_tlx = static_cast<int>(roi.tl().x*scale);
	int new_tly = static_cast<int>(roi.tl().y*scale);
	//int new_brx = static_cast<int>(roi.br().x*scale);
	//int new_bry = static_cast<int>(roi.br().y*scale);

	Rect new_roi(Point(new_tlx, new_tly), filled_mask.size());

	filled_mask.copyTo(final_mask(new_roi));
	

	Mat sImg, eImg;
	resize(OCVRGB, sImg, final_mask.size(), 0, 0, INTER_AREA);
	inpaint(sImg, final_mask, sImg, fp->track[5], fp->track[4]);
	
	Mat cropSmallSub(sImg, new_roi);
	resize(cropSmallSub, eImg, roi.size(), 0, 0, fp->track[8]);
	eImg.copyTo(OCVRGB(roi));

}

void Aviutl2OCVFast(FILTER *fp, FILTER_PROC_INFO *src, Mat &dest)
{
	Mat mbuffer(src->h, src->w, CV_8UC3, fp->exfunc->get_pixelp(src->editp, src->frame));
	//cvtColor(mbuffer, mbuffer, CV_RGB2BGR);
	flip(mbuffer, mbuffer, 0);
	mbuffer.copyTo(dest);
}

void OCV2AviUtlFast(FILTER *fp, Mat &src, FILTER_PROC_INFO *dest)
{
	int pxCount = dest->max_w * dest->max_h;
	
	PIXEL* AuRGB = new PIXEL[pxCount];
	Mat canvas = Mat::zeros(Size(dest->max_w, dest->max_h), CV_8UC3);
	src.copyTo(canvas(Rect(0, 0, dest->w, dest->h)));
	memcpy_s(AuRGB, sizeof(PIXEL)*pxCount, canvas.data, pxCount*3);
	fp->exfunc->rgb2yc(dest->ycp_edit, AuRGB, pxCount);
	delete[] AuRGB;
	
	
}
//*******************************************************************************

