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

static AVFormatContext *pFmtCtx_in = NULL;
//static AVCodecContext *pCodecCtx_Dec = NULL;
//static AVCodecContext *pCodecCtx_Enc = NULL;
typedef struct StreamCodecContext {
    AVCodecContext *pDecCtx;
    AVCodecContext *pEncCtx;
} StreamCodecContext;
static StreamCodecContext *pStreamCodecCtx;

static int open_input_file(const char* filename){
    int ret;
    unsigned i;
    //打开视频文件 读取文件头和相关格式信息存储到AVFormatContext结构体中
    if((ret = avformat_open_input(&pFmtCtx_in, filename, NULL,NULL)) != 0){
    	av_log(NULL, AV_LOG_ERROR, "Can't open input file 123:%s\n", filename);
	return ret;
    }
    //读取文件中的流信息到pFmtCtx->streams中
    if((ret = avformat_find_stream_info(pFmtCtx_in, NULL)) < 0){
    	av_log(NULL, AV_LOG_ERROR, "Can't find stream information\n");
    }
    
    pStreamCodecCtx = av_mallocz_array(pFmtCtx_in->nb_streams, sizeof(*pStreamCodecCtx));
    if(!pStreamCodecCtx)
    	return AVERROR(ENOMEM);
    for(i = 0; i < pFmtCtx_in->nb_streams; i++){
    	AVStream *pStream = pFmtCtx_in->streams[i];
	AVCodec *pDec = avcodec_find_decoder(pStream->codecpar->codec_id);
	AVCodecContext * pCodecCtx;
	if(!pDec){
	    av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", i);
	    return AVERROR_DECODER_NOT_FOUND;
	}
	pCodecCtx = avcodec_alloc_context3(pDec);
	if(!pCodecCtx){
	    av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
	    return AVERROR(ENOMEM);
	}
	ret = avcodec_parameters_to_context(pCodecCtx, pStream->codecpar);
	if(ret < 0){
	    av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context for stream #%u\n", i);
	    return ret;
	}
	if(pCodecCtx->codec_type == AVMEDIA_TYPE_VIDEO || pCodecCtx->codec_type == AVMEDIA_TYPE_AUDIO){
	    if(pCodecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
	    	pCodecCtx->framerate = av_guess_frame_rate(pFmtCtx_in, pStream, NULL);
	    //打开解码器
	    ret = avcodec_open2(pCodecCtx, pDec, NULL);
	    if(ret < 0){
	    	av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
		return ret;
	    }
	}
	pStreamCodecCtx[i].pDecCtx = pCodecCtx;
    }
    //Dump information
    av_dump_format(pFmtCtx_in, 0, filename, 0);
    return 0;
}

void SaveFrame(AVFrame *pFrame, int width, int height, FILE* fp_out){
    logd("FrameSize:%dx%d", width, height);
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


static int decode(AVCodecContext *pDec_Ctx, AVFrame* pFrame, AVPacket *pPkt, FILE* fp_out){
    char buf[1024];
    int ret;
    ret = avcodec_send_packet(pDec_Ctx, pPkt);
    if(ret < 0){
    	av_log(NULL, AV_LOG_ERROR, "Error sending a packet for decoding\n");
	return 1;    
    }
    while(ret >= 0){
    	ret = avcodec_receive_frame(pDec_Ctx, pFrame);
	if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
	    return 0;
	else if(ret < 0){
	    av_log(NULL, AV_LOG_ERROR, "Error during decoding\n");
	    return 1;
	}
	if(ret >= 0)
	    SaveFrame(pFrame, pDec_Ctx->width, pDec_Ctx->height, fp_out);

	//the picture is allocated by the decoder. no need to free it
	snprintf(buf, sizeof(buf), "frame-%d", pDec_Ctx->frame_number);

    }
}

int main(int argc, char* argv[])
{
    int ret;
    AVFrame *pFrame = NULL;
    AVPacket packet = { .data = NULL, .size = 0};
    //enum AVMEDIAType type;
    unsigned int i, streamIdx;
    int gotFrame;
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);

    if(argc != 3){
    	av_log(NULL, AV_LOG_ERROR, "Usage: %s <input file> <outputfile>\n", argv[0]);
	return 1;
    }
    av_register_all();
    
    if((ret = open_input_file(argv[1])) < 0)
    	goto end;
/*
    if((ret = open_output_file(argv[2])) < 0)
    	goto end;
    if((ret = init_filters()) < 0)
    	goto end;
*/
    FILE *fp_out = fopen(argv[2], "wb");
    if(!fp_out){   
    	av_log(NULL, AV_LOG_ERROR, "Fail to open output file %s\n", argv[2]);
    }
    
    //读取所有packets
    while(1){
    	if((ret = av_read_frame(pFmtCtx_in, &packet)) < 0)
	    break;
	streamIdx = packet.stream_index;
	av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n", streamIdx);
	
	//Process:
	pFrame = av_frame_alloc();
	if(!pFrame){
	    ret = AVERROR(ENOMEM);
	    break;
	}

#if 0
	type = pFmtCtx->streams[streamIdx]->codecpar->codec_type;
	dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2() : avcodec_decode_audio4;
	ret = dec_func(pStreamCodecCtx[streamIdx].pDecCtx, pFrame, &gotFrame, &packet);
	if(ret < 0){
	    av_frame_free(&pFrame);
	    av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
	    break;
	}
#else
        ret = decode(pStreamCodecCtx[streamIdx].pDecCtx, pFrame, &packet, fp_out);

#endif
	//av_packet_unref(&packet);
    }
/*
//注册所有的codecs and file formats
    av_register_all();
//Before open codec
    pCodecParserCtx = av_parser_init(pCodecParam->codec_id);
    if(!pCodecParserCtx){
	loge("Failed to allocate video parser context");
	return -1;    }
*/

end:
    av_packet_unref(&packet);
    av_frame_free(&pFrame);
    for(i = 0; i < pFmtCtx_in->nb_streams; i++){
    
        avcodec_free_context(&pStreamCodecCtx[i].pDecCtx);
    }
    av_free(pStreamCodecCtx);
    avformat_close_input(&pFmtCtx_in);
    if(ret < 0)
    	av_log(NULL, AV_LOG_ERROR, "Error occurred:%s\n", av_err2str(ret));
    return ret ? 1:0;
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

