/*
 *  This file is part of vban.
 *  Copyright (c) 2015 by Benoît Quiniou <quiniouben@yahoo.fr>
 *
 *  vban is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  vban is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with vban.  If not, see <http://www.gnu.org/licenses/>.
 */


///////////////////////////////////////////////////////////////////////
//
//      MODIFIED by R. Kinnett, https://github.com/rkinnett, 2020
//
///////////////////////////////////////////////////////////////////////


#ifndef __VBAN_H__
#define __VBAN_H__

#include <inttypes.h>


#define VBAN_HEADER_SIZE            (4 + 1 + 1 + 1 + 1 + 16)
#define VBAN_STREAM_NAME_SIZE       16
#define VBAN_PROTOCOL_MAX_SIZE      1464
#define VBAN_DATA_MAX_SIZE          (VBAN_PROTOCOL_MAX_SIZE - VBAN_HEADER_SIZE)
#define VBAN_CHANNELS_MAX_NB        256
#define VBAN_SAMPLES_MAX_NB         256



#define VBAN_PACKET_NUM_SAMPLES 256  
#define VBAN_PACKET_MAX_SAMPLES 256  
#define VBAN_PACKET_HEADER_BYTES 24  
#define VBAN_PACKET_COUNTER_BYTES 4  
#define VBAN_PACKET_MAX_LEN_BYTES (VBAN_PACKET_HEADER_BYTES + VBAN_PACKET_COUNTER_BYTES + VBAN_PACKET_MAX_SAMPLES*2)


typedef struct
{
    char        preamble[4];                               /* contains 'V' 'B', 'A', 'N' */
    uint8_t     sample_rate;                          /* SR index (see SRList above) */
    uint8_t     num_samples;                         /* nb sample per frame (1 to 256) */
    uint8_t     num_channels;                         /* nb channel (1 to 256) */
    uint8_t     sample_format;                         /* mask = 0x07 (nb Byte integer from 1 to 4) */
    char        stream_name[VBAN_STREAM_NAME_SIZE];  /* stream name */
} VBanHeader;


typedef struct {
  VBanHeader* hdr;
  uint32_t* packet_counter;
  uint8_t* data_frame;
  uint8_t packet[VBAN_PROTOCOL_MAX_SIZE];
  uint16_t packet_data_bytes;
  uint16_t packet_total_bytes;
} VBan;




#define VBAN_SR_MASK                0x1F
#define VBAN_SR_MAXNUMBER           21
static long const VBanSRList[VBAN_SR_MAXNUMBER]=
{
  6000, 12000, 24000, 48000, 96000, 192000, 384000,
  8000, 16000, 32000, 64000, 128000, 256000, 512000,
  11025, 22050, 44100, 88200, 176400, 352800, 705600
};

enum VBanSampleRates
{
  SAMPLE_RATE_6000_HZ,
  SAMPLE_RATE_12000_HZ,
  SAMPLE_RATE_24000_HZ,
  SAMPLE_RATE_48000_HZ,
  SAMPLE_RATE_96000_HZ,
  SAMPLE_RATE_192000_HZ,
  SAMPLE_RATE_384000_HZ,
  SAMPLE_RATE_8000_HZ,
  SAMPLE_RATE_16000_HZ,
  SAMPLE_RATE_32000_HZ,
  SAMPLE_RATE_64000_HZ,
  SAMPLE_RATE_128000_HZ,
  SAMPLE_RATE_256000_HZ,
  SAMPLE_RATE_512000_HZ,
  SAMPLE_RATE_11025_HZ,
  SAMPLE_RATE_22050_HZ,
  SAMPLE_RATE_44100_HZ,
  SAMPLE_RATE_88200_HZ,
  SAMPLE_RATE_176400_HZ,
  SAMPLE_RATE_352800_HZ,
  SAMPLE_RATE_705600_HZ
};


#define VBAN_PROTOCOL_MASK          0xE0
enum VBanProtocol
{
  VBAN_PROTOCOL_AUDIO         =   0x00,
  VBAN_PROTOCOL_SERIAL        =   0x20,
  VBAN_PROTOCOL_TXT           =   0x40,
  VBAN_PROTOCOL_UNDEFINED_1   =   0x80,
  VBAN_PROTOCOL_UNDEFINED_2   =   0xA0,
  VBAN_PROTOCOL_UNDEFINED_3   =   0xC0,
  VBAN_PROTOCOL_UNDEFINED_4   =   0xE0
};

#define VBAN_BIT_RESOLUTION_MASK    0x07
enum VBanBitResolution
{
  VBAN_BITFMT_8_INT = 0,
  VBAN_BITFMT_16_INT,
  VBAN_BITFMT_24_INT,
  VBAN_BITFMT_32_INT,
  VBAN_BITFMT_32_FLOAT,
  VBAN_BITFMT_64_FLOAT,
  VBAN_BITFMT_12_INT,
  VBAN_BITFMT_10_INT,
  VBAN_BIT_RESOLUTION_MAX
};

static int const VBanBitResolutionSize[VBAN_BIT_RESOLUTION_MAX] =
{
  1, 2, 3, 4, 4, 8, 
};

#define VBAN_RESERVED_MASK          0x08

#define VBAN_CODEC_MASK             0xF0
enum VBanCodec
{
  VBAN_CODEC_PCM              =   0x00,
  VBAN_CODEC_VBCA             =   0x10,
  VBAN_CODEC_VBCV             =   0x20,
  VBAN_CODEC_UNDEFINED_3      =   0x30,
  VBAN_CODEC_UNDEFINED_4      =   0x40,
  VBAN_CODEC_UNDEFINED_5      =   0x50,
  VBAN_CODEC_UNDEFINED_6      =   0x60,
  VBAN_CODEC_UNDEFINED_7      =   0x70,
  VBAN_CODEC_UNDEFINED_8      =   0x80,
  VBAN_CODEC_UNDEFINED_9      =   0x90,
  VBAN_CODEC_UNDEFINED_10     =   0xA0,
  VBAN_CODEC_UNDEFINED_11     =   0xB0,
  VBAN_CODEC_UNDEFINED_12     =   0xC0,
  VBAN_CODEC_UNDEFINED_13     =   0xD0,
  VBAN_CODEC_UNDEFINED_14     =   0xE0,
  VBAN_CODEC_USER             =   0xF0
};


/********************************************************
 *              TEXT SUB PROTOCOL                       *
 ********************************************************/

#define VBAN_BPS_MASK           0xE0
#define VBAN_BPS_MAXNUMBER      25
static long const VBanBPSList[VBAN_BPS_MAXNUMBER] =
{
    0,      110,    150,    300,    600,
    1200,   2400,   4800,   9600,   14400,
    19200,  31250,  38400,  57600,  115200,
    128000, 230400, 250000, 256000, 460800,
    921600,1000000,1500000,2000000, 3000000
};

#define VBAN_DATATYPE_MASK          0x07
#define VBAN_DATATYPE_MAXNUMBER     1
enum VBanDataTypeList
{
    VBAN_DATATYPE_8BITS = 0
};

#define VBAN_STREAMTYPE_MASK        0xF0
enum VBanStreamType
{
    VBAN_TXT_ASCII          =   0x00,
    VBAN_TXT_UTF8           =   0x10,
    VBAN_TXT_WCHAR          =   0x20,
    VBAN_TXT_UNDEFINED_3    =   0x30,
    VBAN_TXT_UNDEFINED_4    =   0x40,
    VBAN_TXT_UNDEFINED_5    =   0x50,
    VBAN_TXT_UNDEFINED_6    =   0x60,
    VBAN_TXT_UNDEFINED_7    =   0x70,
    VBAN_TXT_UNDEFINED_8    =   0x80,
    VBAN_TXT_UNDEFINED_9    =   0x90,
    VBAN_TXT_UNDEFINED_10   =   0xA0,
    VBAN_TXT_UNDEFINED_11   =   0xB0,
    VBAN_TXT_UNDEFINED_12   =   0xC0,
    VBAN_TXT_UNDEFINED_13   =   0xD0,
    VBAN_TXT_UNDEFINED_14   =   0xE0,
    VBAN_TXT_USER           =   0xF0
};

#endif /*__VBAN_H__*/
