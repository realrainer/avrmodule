
#ifndef __tools_h__
#define __tools_h__

//internal codecId definitions
#define AVR_CODEC_ID_UNDEFINED    0
#define AVR_CODEC_ID_H264         1
#define AVR_CODEC_ID_MJPEG        2

#define CODEC_MAXLENGTH           32

extern unsigned char const h264_start_code[4];
int h264_getVopType( const void *p, int len );

#endif
