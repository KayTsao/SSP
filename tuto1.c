#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
//#include <inttypes.h>

#include "include/libavformat/avformat.h"
#include "include/libavcodec/avcodec.h"
#include "include/libavutil/avutil.h"
#include "include/libavutil/hwcontext.h"
#include "include/libswscale/swscale.h"
static void loge(const char *fmt, ...);
static void logd(const char *fmt, ...);

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame, FILE* fp_out){
    logd("Frame:%d\tsize:%dx%d", iFrame, width, height);
    for(int i = 0; i < height; i++){
	fwrite(pFrame->data[0] + i * pFrame->linesize[0], 1, width, fp_out);	
    }
    for(int i = 0; i < height/2; i++){
    	fwrite(pFrame->data[1] + i * pFrame->linesize[1], 1, width/2, fp_out);
    }
    for(int i = 0; i < height/2; i++){
    	fwrite(pFrame->data[2] + i * pFrame->linesize[2], 1, width/2, fp_out);
    }
    return;
}


int main(int argc, char* argv[])
{
    if(!argv[1] || !argv[2]){
    	loge("Please input as: ./test [input_file_name] [output_file_name]");
	return -1;
    }
    char* file_in = argv[1];
    char* file_out = argv[2];
    AVFormatContext *pFmtCtx_in = NULL;
    //AVFormatContext *pFmtCtx_out = NULL;
    AVCodec *pDec;
    AVCodecParameters *pCodecParam = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVCodecParserContext *pCodecParserCtx = NULL;
    FILE *fp_in;
    FILE *fp_out;
    AVFrame *pFrame;
    AVPacket packet;
    int i, frameFinished, videoStreamIdx;


    //注册所有的codecs and file formats
    av_register_all();

    //打开视频文件 读取文件头和相关格式信息存储到AVFormatContext结构体中
    if(avformat_open_input(&pFmtCtx_in, file_in, NULL, NULL) != 0){
   	loge("Can't open file %s", file_in); 
    	return -1;
    }

    //读取文件中的流信息到pFmtCtx->streams中
    if(avformat_find_stream_info(pFmtCtx_in, NULL) < 0){
    	loge("Can't find stream info");
	return -1;
    }
    //Dump information
    av_dump_format(pFmtCtx_in, 0, file_in, 0);
    //遍历streams直到找到视频流
    videoStreamIdx = -1; 
    for(i = 0; i < pFmtCtx_in->nb_streams; i++){
	if(pFmtCtx_in->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
	    videoStreamIdx = i;
	    break;
	}
    }
    if(videoStreamIdx == -1){
    	loge("can't find video stream");
	return -1;
    }
    //pCodecParam指针指向这个流所用到的全部编解码信息
    pCodecParam = pFmtCtx_in->streams[videoStreamIdx]->codecpar;
    //找到所用的codec并打开Decoder
    pDec = avcodec_find_decoder(pCodecParam->codec_id);
    if(!pDec){
    	loge("can't find decoder");
	return -1;
    }

    pCodecCtx = avcodec_alloc_context3(pDec);// NULL;
    if(pCodecCtx == NULL){
	loge("Failed to allocate the decoder context fot stream %d", videoStreamIdx);
	return -1;
    }

    int ret = avcodec_parameters_to_context(pCodecCtx, pCodecParam);
    if(ret < 0){
	loge("Failed to copy decoder params to input decoder context");
	return -1;
    }

    pCodecParserCtx = av_parser_init(pCodecParam->codec_id);
    if(!pCodecParserCtx){
	loge("Failed to allocate video parser context");
	return -1;
    }

    //打开codec
    if(avcodec_open2(pCodecCtx, pDec, NULL) < 0){
    	loge("Can't open codec");
	return -1;
    }
    //输入文件
    fp_in = fopen(file_in, "rb");
    if(!fp_in){
        loge("can't open input file");
	return -1;
    }
    fp_out = fopen(file_out, "wb");
    if(!fp_out){
        loge("failed to open output file");
	return -1;
    }
    //初始化AVFrame&AVPacket
    //Allocate video frame
    pFrame = av_frame_alloc();
    i = 0;
    //Read frames and save first five frames to disk
    while(av_read_frame(pFmtCtx_in, &packet) >= 0){
    	// Is this a packet from the video stream?
	if(packet.stream_index == videoStreamIdx){
	    //Decode video frame
#if 0
	    avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
#else
	    if(pCodecCtx->codec_type == AVMEDIA_TYPE_VIDEO ||
	    pCodecCtx->codec_type == AVMEDIA_TYPE_AUDIO){
		int used = avcodec_send_packet(pCodecCtx, &packet);
		if(used < 0 && used != AVERROR(EAGAIN) && used != AVERROR_EOF){
		//do noting	
		}
		else{
		    if(used >= 0)
		        packet.size = 0;
		    used = avcodec_receive_frame(pCodecCtx, pFrame);
		    if(used >= 0)
		    	frameFinished = 1;
		    if(used == AVERROR(EAGAIN) || used == AVERROR_EOF)
		    	used = 0;
		}
	    }
#endif
	    //Did we get a video frame?
	    if(frameFinished){
		SaveFrame(pFrame, pCodecCtx->width, pCodecCtx->height, ++i, fp_out);
	    }
	}
	//Free the packet that was allocated by av_read_frame
	av_packet_unref(&packet);
    //logd("end one loop?");
    }
    //logd("end read?");
    //Free the YUV frame
    av_frame_free(&pFrame);
    //Close the codecs
    avcodec_close(pCodecCtx);
    //Close the video file
//    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFmtCtx_in);


    return 0;
}

static void loge(const char *fmt, ...)
{
    va_list args;
    fprintf( stderr, "ERROR: " );
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
    fprintf( stderr, "\n" );
}
static void logd(const char *fmt, ...)
{
    va_list args;
    fprintf( stderr, "DEBUG: " );
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
    fprintf( stderr, "\n" );
}

