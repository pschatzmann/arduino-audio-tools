#pragma once
#include <string.h>

#include "AudioBasic/StrExt.h"
#include "AudioTools/Buffers.h"

#define WAVE_FORMAT_UNKNOWN 0x0000 /* Microsoft Corporation */
#define WAVE_FORMAT_PCM 0x0001
#define WAVE_FORMAT_ADPCM 0x0002      /* Microsoft Corporation */
#define WAVE_FORMAT_IEEE_FLOAT 0x0003 /* Microsoft Corporation */
#define WAVE_FORMAT_VSELP 0x0004      /* Compaq Computer Corp. */
#define WAVE_FORMAT_IBM_CVSD 0x0005   /* IBM Corporation */
#define WAVE_FORMAT_ALAW 0x0006       /* Microsoft Corporation */
#define WAVE_FORMAT_MULAW 0x0007      /* Microsoft Corporation */
#define WAVE_FORMAT_DTS 0x0008        /* Microsoft Corporation */
#define WAVE_FORMAT_DRM 0x0009        /* Microsoft Corporation */
#define WAVE_FORMAT_WMAVOICE9 0x000A  /* Microsoft Corporation */
#define WAVE_FORMAT_WMAVOICE10 0x000B /* Microsoft Corporation */
#define WAVE_FORMAT_OKI_ADPCM 0x0010  /* OKI */
#define WAVE_FORMAT_DVI_ADPCM 0x0011  /* Intel Corporation */
#define WAVE_FORMAT_IMA_ADPCM (WAVE_FORMAT_DVI_ADPCM) /*  Intel Corporation */
#define WAVE_FORMAT_MEDIASPACE_ADPCM 0x0012           /* Videologic */
#define WAVE_FORMAT_SIERRA_ADPCM 0x0013 /* Sierra Semiconductor Corp */
#define WAVE_FORMAT_G723_ADPCM 0x0014   /* Antex Electronics Corporation */
#define WAVE_FORMAT_DIGISTD 0x0015      /* DSP Solutions, Inc. */
#define WAVE_FORMAT_DIGIFIX 0x0016      /* DSP Solutions, Inc. */
#define WAVE_FORMAT_DIALOGIC_OKI_ADPCM 0x0017 /* Dialogic Corporation */
#define WAVE_FORMAT_MEDIAVISION_ADPCM 0x0018  /* Media Vision, Inc. */
#define WAVE_FORMAT_CU_CODEC 0x0019           /* Hewlett-Packard Company */
#define WAVE_FORMAT_HP_DYN_VOICE 0x001A       /* Hewlett-Packard Company */
#define WAVE_FORMAT_YAMAHA_ADPCM 0x0020 /* Yamaha Corporation of America */
#define WAVE_FORMAT_SONARC 0x0021       /* Speech Compression */
#define WAVE_FORMAT_DSPGROUP_TRUESPEECH 0x0022 /* DSP Group, Inc */
#define WAVE_FORMAT_ECHOSC1 0x0023             /* Echo Speech Corporation */
#define WAVE_FORMAT_AUDIOFILE_AF36 0x0024      /* Virtual Music, Inc. */
#define WAVE_FORMAT_APTX 0x0025                /* Audio Processing Technology */
#define WAVE_FORMAT_AUDIOFILE_AF10 0x0026      /* Virtual Music, Inc. */
#define WAVE_FORMAT_PROSODY_1612 0x0027        /* Aculab plc */
#define WAVE_FORMAT_LRC 0x0028                 /* Merging Technologies S.A. */
#define WAVE_FORMAT_DOLBY_AC2 0x0030           /* Dolby Laboratories */
#define WAVE_FORMAT_GSM610 0x0031              /* Microsoft Corporation */
#define WAVE_FORMAT_MSNAUDIO 0x0032            /* Microsoft Corporation */
#define WAVE_FORMAT_ANTEX_ADPCME 0x0033      /* Antex Electronics Corporation */
#define WAVE_FORMAT_CONTROL_RES_VQLPC 0x0034 /* Control Resources Limited */
#define WAVE_FORMAT_DIGIREAL 0x0035          /* DSP Solutions, Inc. */
#define WAVE_FORMAT_DIGIADPCM 0x0036         /* DSP Solutions, Inc. */
#define WAVE_FORMAT_CONTROL_RES_CR10 0x0037  /* Control Resources Limited */
#define WAVE_FORMAT_NMS_VBXADPCM 0x0038      /* Natural MicroSystems */
#define WAVE_FORMAT_CS_IMAADPCM 0x0039    /* Crystal Semiconductor IMA ADPCM */
#define WAVE_FORMAT_ECHOSC3 0x003A        /* Echo Speech Corporation */
#define WAVE_FORMAT_ROCKWELL_ADPCM 0x003B /* Rockwell International */
#define WAVE_FORMAT_ROCKWELL_DIGITALK 0x003C /* Rockwell International */
#define WAVE_FORMAT_XEBEC 0x003D        /* Xebec Multimedia Solutions Limited */
#define WAVE_FORMAT_G721_ADPCM 0x0040   /* Antex Electronics Corporation */
#define WAVE_FORMAT_G728_CELP 0x0041    /* Antex Electronics Corporation */
#define WAVE_FORMAT_MSG723 0x0042       /* Microsoft Corporation */
#define WAVE_FORMAT_INTEL_G723_1 0x0043 /* Intel Corp. */
#define WAVE_FORMAT_INTEL_G729 0x0044   /* Intel Corp. */
#define WAVE_FORMAT_SHARP_G726 0x0045   /* Sharp */
#define WAVE_FORMAT_MPEG 0x0050         /* Microsoft Corporation */
#define WAVE_FORMAT_RT24 0x0052         /* InSoft, Inc. */
#define WAVE_FORMAT_PAC 0x0053          /* InSoft, Inc. */
#define WAVE_FORMAT_MPEGLAYER3 0x0055   /* ISO/MPEG Layer3 Format Tag */
#define WAVE_FORMAT_LUCENT_G723 0x0059  /* Lucent Technologies */
#define WAVE_FORMAT_CIRRUS 0x0060       /* Cirrus Logic */
#define WAVE_FORMAT_ESPCM 0x0061        /* ESS Technology */
#define WAVE_FORMAT_VOXWARE 0x0062      /* Voxware Inc */
#define WAVE_FORMAT_CANOPUS_ATRAC 0x0063        /* Canopus, co., Ltd. */
#define WAVE_FORMAT_G726_ADPCM 0x0064           /* APICOM */
#define WAVE_FORMAT_G722_ADPCM 0x0065           /* APICOM */
#define WAVE_FORMAT_DSAT 0x0066                 /* Microsoft Corporation */
#define WAVE_FORMAT_DSAT_DISPLAY 0x0067         /* Microsoft Corporation */
#define WAVE_FORMAT_VOXWARE_BYTE_ALIGNED 0x0069 /* Voxware Inc */
#define WAVE_FORMAT_VOXWARE_AC8 0x0070          /* Voxware Inc */
#define WAVE_FORMAT_VOXWARE_AC10 0x0071         /* Voxware Inc */
#define WAVE_FORMAT_VOXWARE_AC16 0x0072         /* Voxware Inc */
#define WAVE_FORMAT_VOXWARE_AC20 0x0073         /* Voxware Inc */
#define WAVE_FORMAT_VOXWARE_RT24 0x0074         /* Voxware Inc */
#define WAVE_FORMAT_VOXWARE_RT29 0x0075         /* Voxware Inc */
#define WAVE_FORMAT_VOXWARE_RT29HW 0x0076       /* Voxware Inc */
#define WAVE_FORMAT_VOXWARE_VR12 0x0077         /* Voxware Inc */
#define WAVE_FORMAT_VOXWARE_VR18 0x0078         /* Voxware Inc */
#define WAVE_FORMAT_VOXWARE_TQ40 0x0079         /* Voxware Inc */
#define WAVE_FORMAT_VOXWARE_SC3 0x007A          /* Voxware Inc */
#define WAVE_FORMAT_VOXWARE_SC3_1 0x007B        /* Voxware Inc */
#define WAVE_FORMAT_SOFTSOUND 0x0080            /* Softsound, Ltd. */
#define WAVE_FORMAT_VOXWARE_TQ60 0x0081         /* Voxware Inc */
#define WAVE_FORMAT_MSRT24 0x0082               /* Microsoft Corporation */
#define WAVE_FORMAT_G729A 0x0083                /* AT&T Labs, Inc. */
#define WAVE_FORMAT_MVI_MVI2 0x0084             /* Motion Pixels */
#define WAVE_FORMAT_DF_G726 0x0085   /* DataFusion Systems (Pty) (Ltd) */
#define WAVE_FORMAT_DF_GSM610 0x0086 /* DataFusion Systems (Pty) (Ltd) */
#define WAVE_FORMAT_ISIAUDIO 0x0088  /* Iterated Systems, Inc. */
#define WAVE_FORMAT_ONLIVE 0x0089    /* OnLive! Technologies, Inc. */
#define WAVE_FORMAT_MULTITUDE_FT_SX20 0x008A      /* Multitude Inc. */
#define WAVE_FORMAT_INFOCOM_ITS_G721_ADPCM 0x008B /* Infocom */
#define WAVE_FORMAT_CONVEDIA_G729 0x008C          /* Convedia Corp. */
#define WAVE_FORMAT_CONGRUENCY 0x008D             /* Congruency Inc. */
#define WAVE_FORMAT_SBC24 0x0091 /* Siemens Business Communications Sys */
#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092    /* Sonic Foundry */
#define WAVE_FORMAT_MEDIASONIC_G723 0x0093    /* MediaSonic */
#define WAVE_FORMAT_PROSODY_8KBPS 0x0094      /* Aculab plc */
#define WAVE_FORMAT_ZYXEL_ADPCM 0x0097        /* ZyXEL Communications, Inc. */
#define WAVE_FORMAT_PHILIPS_LPCBB 0x0098      /* Philips Speech Processing */
#define WAVE_FORMAT_PACKED 0x0099             /* Studer Professional Audio AG */
#define WAVE_FORMAT_MALDEN_PHONYTALK 0x00A0   /* Malden Electronics Ltd. */
#define WAVE_FORMAT_RACAL_RECORDER_GSM 0x00A1 /* Racal recorders */
#define WAVE_FORMAT_RACAL_RECORDER_G720_A 0x00A2      /* Racal recorders */
#define WAVE_FORMAT_RACAL_RECORDER_G723_1 0x00A3      /* Racal recorders */
#define WAVE_FORMAT_RACAL_RECORDER_TETRA_ACELP 0x00A4 /* Racal recorders */
#define WAVE_FORMAT_NEC_AAC 0x00B0                    /* NEC Corp. */
#define WAVE_FORMAT_RAW_AAC1                                                 \
  0x00FF /* For Raw AAC, with format block AudioSpecificConfig() (as defined \
            by MPEG-4), that follows WAVEFORMATEX */
#define WAVE_FORMAT_RHETOREX_ADPCM 0x0100    /* Rhetorex Inc. */
#define WAVE_FORMAT_IRAT 0x0101              /* BeCubed Software Inc. */
#define WAVE_FORMAT_VIVO_G723 0x0111         /* Vivo Software */
#define WAVE_FORMAT_VIVO_SIREN 0x0112        /* Vivo Software */
#define WAVE_FORMAT_PHILIPS_CELP 0x0120      /* Philips Speech Processing */
#define WAVE_FORMAT_PHILIPS_GRUNDIG 0x0121   /* Philips Speech Processing */
#define WAVE_FORMAT_DIGITAL_G723 0x0123      /* Digital Equipment Corporation */
#define WAVE_FORMAT_SANYO_LD_ADPCM 0x0125    /* Sanyo Electric Co., Ltd. */
#define WAVE_FORMAT_SIPROLAB_ACEPLNET 0x0130 /* Sipro Lab Telecom Inc. */
#define WAVE_FORMAT_SIPROLAB_ACELP4800 0x0131      /* Sipro Lab Telecom Inc. */
#define WAVE_FORMAT_SIPROLAB_ACELP8V3 0x0132       /* Sipro Lab Telecom Inc. */
#define WAVE_FORMAT_SIPROLAB_G729 0x0133           /* Sipro Lab Telecom Inc. */
#define WAVE_FORMAT_SIPROLAB_G729A 0x0134          /* Sipro Lab Telecom Inc. */
#define WAVE_FORMAT_SIPROLAB_KELVIN 0x0135         /* Sipro Lab Telecom Inc. */
#define WAVE_FORMAT_VOICEAGE_AMR 0x0136            /* VoiceAge Corp. */
#define WAVE_FORMAT_G726ADPCM 0x0140               /* Dictaphone Corporation */
#define WAVE_FORMAT_DICTAPHONE_CELP68 0x0141       /* Dictaphone Corporation */
#define WAVE_FORMAT_DICTAPHONE_CELP54 0x0142       /* Dictaphone Corporation */
#define WAVE_FORMAT_QUALCOMM_PUREVOICE 0x0150      /* Qualcomm, Inc. */
#define WAVE_FORMAT_QUALCOMM_HALFRATE 0x0151       /* Qualcomm, Inc. */
#define WAVE_FORMAT_TUBGSM 0x0155                  /* Ring Zero Systems, Inc. */
#define WAVE_FORMAT_MSAUDIO1 0x0160                /* Microsoft Corporation */
#define WAVE_FORMAT_WMAUDIO2 0x0161                /* Microsoft Corporation */
#define WAVE_FORMAT_WMAUDIO3 0x0162                /* Microsoft Corporation */
#define WAVE_FORMAT_WMAUDIO_LOSSLESS 0x0163        /* Microsoft Corporation */
#define WAVE_FORMAT_WMASPDIF 0x0164                /* Microsoft Corporation */
#define WAVE_FORMAT_UNISYS_NAP_ADPCM 0x0170        /* Unisys Corp. */
#define WAVE_FORMAT_UNISYS_NAP_ULAW 0x0171         /* Unisys Corp. */
#define WAVE_FORMAT_UNISYS_NAP_ALAW 0x0172         /* Unisys Corp. */
#define WAVE_FORMAT_UNISYS_NAP_16K 0x0173          /* Unisys Corp. */
#define WAVE_FORMAT_SYCOM_ACM_SYC008 0x0174        /* SyCom Technologies */
#define WAVE_FORMAT_SYCOM_ACM_SYC701_G726L 0x0175  /* SyCom Technologies */
#define WAVE_FORMAT_SYCOM_ACM_SYC701_CELP54 0x0176 /* SyCom Technologies */
#define WAVE_FORMAT_SYCOM_ACM_SYC701_CELP68 0x0177 /* SyCom Technologies */
#define WAVE_FORMAT_KNOWLEDGE_ADVENTURE_ADPCM \
  0x0178 /* Knowledge Adventure, Inc. */
#define WAVE_FORMAT_FRAUNHOFER_IIS_MPEG2_AAC 0x0180 /* Fraunhofer IIS */
#define WAVE_FORMAT_DTS_DS 0x0190         /* Digital Theatre Systems, Inc. */
#define WAVE_FORMAT_CREATIVE_ADPCM 0x0200 /* Creative Labs, Inc */
#define WAVE_FORMAT_CREATIVE_FASTSPEECH8 0x0202  /* Creative Labs, Inc */
#define WAVE_FORMAT_CREATIVE_FASTSPEECH10 0x0203 /* Creative Labs, Inc */
#define WAVE_FORMAT_UHER_ADPCM 0x0210            /* UHER informatic GmbH */
#define WAVE_FORMAT_ULEAD_DV_AUDIO 0x0215        /* Ulead Systems, Inc. */
#define WAVE_FORMAT_ULEAD_DV_AUDIO_1 0x0216      /* Ulead Systems, Inc. */
#define WAVE_FORMAT_QUARTERDECK 0x0220           /* Quarterdeck Corporation */
#define WAVE_FORMAT_ILINK_VC 0x0230              /* I-link Worldwide */
#define WAVE_FORMAT_RAW_SPORT 0x0240             /* Aureal Semiconductor */
#define WAVE_FORMAT_ESST_AC3 0x0241              /* ESS Technology, Inc. */
#define WAVE_FORMAT_GENERIC_PASSTHRU 0x0249
#define WAVE_FORMAT_IPI_HSX 0x0250        /* Interactive Products, Inc. */
#define WAVE_FORMAT_IPI_RPELP 0x0251      /* Interactive Products, Inc. */
#define WAVE_FORMAT_CS2 0x0260            /* Consistent Software */
#define WAVE_FORMAT_SONY_SCX 0x0270       /* Sony Corp. */
#define WAVE_FORMAT_SONY_SCY 0x0271       /* Sony Corp. */
#define WAVE_FORMAT_SONY_ATRAC3 0x0272    /* Sony Corp. */
#define WAVE_FORMAT_SONY_SPC 0x0273       /* Sony Corp. */
#define WAVE_FORMAT_TELUM_AUDIO 0x0280    /* Telum Inc. */
#define WAVE_FORMAT_TELUM_IA_AUDIO 0x0281 /* Telum Inc. */
#define WAVE_FORMAT_NORCOM_VOICE_SYSTEMS_ADPCM \
  0x0285                                /* Norcom Electronics Corp. */
#define WAVE_FORMAT_FM_TOWNS_SND 0x0300 /* Fujitsu Corp. */
#define WAVE_FORMAT_MICRONAS 0x0350     /* Micronas Semiconductors, Inc. */
#define WAVE_FORMAT_MICRONAS_CELP833                                           \
  0x0351                                      /* Micronas Semiconductors, Inc. \
                                               */
#define WAVE_FORMAT_BTV_DIGITAL 0x0400        /* Brooktree Corporation */
#define WAVE_FORMAT_INTEL_MUSIC_CODER 0x0401  /* Intel Corp. */
#define WAVE_FORMAT_INDEO_AUDIO 0x0402        /* Ligos */
#define WAVE_FORMAT_QDESIGN_MUSIC 0x0450      /* QDesign Corporation */
#define WAVE_FORMAT_ON2_VP7_AUDIO 0x0500      /* On2 Technologies */
#define WAVE_FORMAT_ON2_VP6_AUDIO 0x0501      /* On2 Technologies */
#define WAVE_FORMAT_VME_VMPCM 0x0680          /* AT&T Labs, Inc. */
#define WAVE_FORMAT_TPC 0x0681                /* AT&T Labs, Inc. */
#define WAVE_FORMAT_LIGHTWAVE_LOSSLESS 0x08AE /* Clearjump */
#define WAVE_FORMAT_OLIGSM 0x1000             /* Ing C. Olivetti & C., S.p.A. */
#define WAVE_FORMAT_OLIADPCM 0x1001           /* Ing C. Olivetti & C., S.p.A. */
#define WAVE_FORMAT_OLICELP 0x1002            /* Ing C. Olivetti & C., S.p.A. */
#define WAVE_FORMAT_OLISBC 0x1003             /* Ing C. Olivetti & C., S.p.A. */
#define WAVE_FORMAT_OLIOPR 0x1004             /* Ing C. Olivetti & C., S.p.A. */
#define WAVE_FORMAT_LH_CODEC 0x1100           /* Lernout & Hauspie */
#define WAVE_FORMAT_LH_CODEC_CELP 0x1101      /* Lernout & Hauspie */
#define WAVE_FORMAT_LH_CODEC_SBC8 0x1102      /* Lernout & Hauspie */
#define WAVE_FORMAT_LH_CODEC_SBC12 0x1103     /* Lernout & Hauspie */
#define WAVE_FORMAT_LH_CODEC_SBC16 0x1104     /* Lernout & Hauspie */
#define WAVE_FORMAT_NORRIS 0x1400             /* Norris Communications, Inc. */
#define WAVE_FORMAT_ISIAUDIO_2 0x1401         /* ISIAudio */
#define WAVE_FORMAT_SOUNDSPACE_MUSICOMPRESS 0x1500 /* AT&T Labs, Inc. */
#define WAVE_FORMAT_MPEG_ADTS_AAC 0x1600           /* Microsoft Corporation */
#define WAVE_FORMAT_MPEG_RAW_AAC 0x1601            /* Microsoft Corporation */
#define WAVE_FORMAT_MPEG_LOAS                                                 \
  0x1602 /* Microsoft Corporation (MPEG-4 Audio Transport Streams (LOAS/LATM) \
          */
#define WAVE_FORMAT_NOKIA_MPEG_ADTS_AAC 0x1608    /* Microsoft Corporation */
#define WAVE_FORMAT_NOKIA_MPEG_RAW_AAC 0x1609     /* Microsoft Corporation */
#define WAVE_FORMAT_VODAFONE_MPEG_ADTS_AAC 0x160A /* Microsoft Corporation */
#define WAVE_FORMAT_VODAFONE_MPEG_RAW_AAC 0x160B  /* Microsoft Corporation */
#define WAVE_FORMAT_MPEG_HEAAC                                               \
  0x1610 /* Microsoft Corporation (MPEG-2 AAC or MPEG-4 HE-AAC v1/v2 streams \
            with any payload (ADTS, ADIF, LOAS/LATM, RAW). Format block      \
            includes MP4 AudioSpecificConfig() -- see HEAACWAVEFORMAT below */
#define WAVE_FORMAT_VOXWARE_RT24_SPEECH 0x181C   /* Voxware Inc. */
#define WAVE_FORMAT_SONICFOUNDRY_LOSSLESS 0x1971 /* Sonic Foundry */
#define WAVE_FORMAT_INNINGS_TELECOM_ADPCM 0x1979 /* Innings Telecom Inc. */
#define WAVE_FORMAT_LUCENT_SX8300P 0x1C07        /* Lucent Technologies */
#define WAVE_FORMAT_LUCENT_SX5363S 0x1C0C        /* Lucent Technologies */
#define WAVE_FORMAT_CUSEEME 0x1F03               /* CUSeeMe */
#define WAVE_FORMAT_NTCSOFT_ALF2CM_ACM 0x1FC4    /* NTCSoft */
#define WAVE_FORMAT_DVM 0x2000                   /* FAST Multimedia AG */
#define WAVE_FORMAT_DTS2 0x2001
#define WAVE_FORMAT_MAKEAVIS 0x3313
#define WAVE_FORMAT_DIVIO_MPEG4_AAC 0x4143          /* Divio, Inc. */
#define WAVE_FORMAT_NOKIA_ADAPTIVE_MULTIRATE 0x4201 /* Nokia */
#define WAVE_FORMAT_DIVIO_G726 0x4243               /* Divio, Inc. */
#define WAVE_FORMAT_LEAD_SPEECH 0x434C              /* LEAD Technologies */
#define WAVE_FORMAT_LEAD_VORBIS 0x564C              /* LEAD Technologies */
#define WAVE_FORMAT_WAVPACK_AUDIO 0x5756            /* xiph.org */
#define WAVE_FORMAT_ALAC 0x6C61                     /* Apple Lossless */
#define WAVE_FORMAT_OGG_VORBIS_MODE_1 0x674F        /* Ogg Vorbis */
#define WAVE_FORMAT_OGG_VORBIS_MODE_2 0x6750        /* Ogg Vorbis */
#define WAVE_FORMAT_OGG_VORBIS_MODE_3 0x6751        /* Ogg Vorbis */
#define WAVE_FORMAT_OGG_VORBIS_MODE_1_PLUS 0x676F   /* Ogg Vorbis */
#define WAVE_FORMAT_OGG_VORBIS_MODE_2_PLUS 0x6770   /* Ogg Vorbis */
#define WAVE_FORMAT_OGG_VORBIS_MODE_3_PLUS 0x6771   /* Ogg Vorbis */
#define WAVE_FORMAT_3COM_NBX 0x7000                 /* 3COM Corp. */
#define WAVE_FORMAT_OPUS 0x704F                     /* Opus */
#define WAVE_FORMAT_FAAD_AAC 0x706D
#define WAVE_FORMAT_AMR_NB 0x7361                  /* AMR Narrowband */
#define WAVE_FORMAT_AMR_WB 0x7362                  /* AMR Wideband */
#define WAVE_FORMAT_AMR_WP 0x7363                  /* AMR Wideband Plus */
#define WAVE_FORMAT_GSM_AMR_CBR 0x7A21             /* GSMA/3GPP */
#define WAVE_FORMAT_GSM_AMR_VBR_SID 0x7A22         /* GSMA/3GPP */
#define WAVE_FORMAT_COMVERSE_INFOSYS_G723_1 0xA100 /* Comverse Infosys */
#define WAVE_FORMAT_COMVERSE_INFOSYS_AVQSBC 0xA101 /* Comverse Infosys */
#define WAVE_FORMAT_COMVERSE_INFOSYS_SBC 0xA102    /* Comverse Infosys */
#define WAVE_FORMAT_SYMBOL_G729_A 0xA103           /* Symbol Technologies */
#define WAVE_FORMAT_VOICEAGE_AMR_WB 0xA104         /* VoiceAge Corp. */
#define WAVE_FORMAT_INGENIENT_G726 0xA105 /* Ingenient Technologies, Inc. */
#define WAVE_FORMAT_MPEG4_AAC 0xA106      /* ISO/MPEG-4 */
#define WAVE_FORMAT_ENCORE_G726 0xA107    /* Encore Software */
#define WAVE_FORMAT_ZOLL_ASAO 0xA108      /* ZOLL Medical Corp. */
#define WAVE_FORMAT_SPEEX_VOICE 0xA109    /* xiph.org */
#define WAVE_FORMAT_VIANIX_MASC 0xA10A    /* Vianix LLC */
#define WAVE_FORMAT_WM9_SPECTRUM_ANALYZER 0xA10B /* Microsoft */
#define WAVE_FORMAT_WMF_SPECTRUM_ANAYZER 0xA10C  /* Microsoft */
#define WAVE_FORMAT_GSM_610 0xA10D
#define WAVE_FORMAT_GSM_620 0xA10E
#define WAVE_FORMAT_GSM_660 0xA10F
#define WAVE_FORMAT_GSM_690 0xA110
#define WAVE_FORMAT_GSM_ADAPTIVE_MULTIRATE_WB 0xA111
#define WAVE_FORMAT_POLYCOM_G722 0xA112               /* Polycom */
#define WAVE_FORMAT_POLYCOM_G728 0xA113               /* Polycom */
#define WAVE_FORMAT_POLYCOM_G729_A 0xA114             /* Polycom */
#define WAVE_FORMAT_POLYCOM_SIREN 0xA115              /* Polycom */
#define WAVE_FORMAT_GLOBAL_IP_ILBC 0xA116             /* Global IP */
#define WAVE_FORMAT_RADIOTIME_TIME_SHIFT_RADIO 0xA117 /* RadioTime */
#define WAVE_FORMAT_NICE_ACA 0xA118                   /* Nice Systems */
#define WAVE_FORMAT_NICE_ADPCM 0xA119                 /* Nice Systems */
#define WAVE_FORMAT_VOCORD_G721 0xA11A                /* Vocord Telecom */
#define WAVE_FORMAT_VOCORD_G726 0xA11B                /* Vocord Telecom */
#define WAVE_FORMAT_VOCORD_G722_1 0xA11C              /* Vocord Telecom */
#define WAVE_FORMAT_VOCORD_G728 0xA11D                /* Vocord Telecom */
#define WAVE_FORMAT_VOCORD_G729 0xA11E                /* Vocord Telecom */
#define WAVE_FORMAT_VOCORD_G729_A 0xA11F              /* Vocord Telecom */
#define WAVE_FORMAT_VOCORD_G723_1 0xA120              /* Vocord Telecom */
#define WAVE_FORMAT_VOCORD_LBC 0xA121                 /* Vocord Telecom */
#define WAVE_FORMAT_NICE_G728 0xA122                  /* Nice Systems */
#define WAVE_FORMAT_FRACE_TELECOM_G729 0xA123         /* France Telecom */
#define WAVE_FORMAT_CODIAN 0xA124                     /* CODIAN */
#define WAVE_FORMAT_FLAC 0xF1AC                       /* flac.sourceforge.net */

#define LIST_HEADER_SIZE 12
#define CHUNK_HEADER_SIZE 8

namespace audio_tools {

class VideoOutput : public AudioOutput {
 public:
  virtual void beginFrame(int size) = 0;
  virtual void endFrame() = 0;
};

class ParseBuffer {
 public:
  size_t writeArray(uint8_t *data, size_t len) {
    int to_write = min(availableToWrite(), (int)len);
    memmove(vector.data() + available_byte_count, data, to_write);
    available_byte_count += to_write;
    return to_write;
  }
  void consume(int size) {
    memmove(vector.data(), &vector[size], available_byte_count - size);
    available_byte_count -= size;
    // memset(&vector[size], 0, size);
  }
  void resize(int size) { vector.resize(size + 4); }

  uint8_t *data() { return vector.data(); }

  int availableToWrite() { return size() - available_byte_count; }

  int available() { return available_byte_count; }

  void clear() {
    available_byte_count = 0;
    memset(vector.data(), 0, vector.size());
  }

  bool isEmpty() { return available_byte_count == 0; }

  int size() { return vector.size(); }

  int indexOf(const char *str) {
    uint8_t *ptr = (uint8_t *)memmem(vector.data(), available_byte_count, str,
                                     strlen(str));
    return ptr == nullptr ? -1 : ptr - vector.data();
  }

 protected:
  Vector<uint8_t> vector{0};
  int available_byte_count = 0;
};

using FOURCC = char[4];

struct AVIMainHeader {
  //  FOURCC fcc;
  //  uint32_t cb;
  uint32_t dwMicroSecPerFrame;
  uint32_t dwMaxBytesPerSec;
  uint32_t dwPaddingGranularity;
  uint32_t dwFlags;
  uint32_t dwTotalFrames;
  uint32_t dwInitialFrames;
  uint32_t dwStreams;
  uint32_t dwSuggestedBufferSize;
  uint32_t dwWidth;
  uint32_t dwHeight;
  uint32_t dwReserved[4];
};

struct RECT {
  uint32_t dwWidth;
  uint32_t dwHeight;
};

struct AVIStreamHeader {
  FOURCC fccType;
  FOURCC fccHandler;
  uint32_t dwFlags;
  uint16_t wPriority;
  uint16_t wLanguage;
  uint32_t dwInitialFrames;
  uint32_t dwScale;
  uint32_t dwRate;
  uint32_t dwStart;
  uint32_t dwLength;
  uint32_t dwSuggestedBufferSize;
  uint32_t dwQuality;
  uint32_t dwSampleSize;
  RECT rcFrame;
};

struct BitmapInfoHeader {
  uint32_t biSize;
  uint64_t biWidth;
  uint64_t biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  uint64_t biXPelsPerMeter;
  uint64_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
};

struct WAVFormatX {
  uint16_t wFormatTag;
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
  uint16_t wBitsPerSample;
  uint16_t cbSize;
};

struct WAVFormat {
  uint16_t wFormatTag;
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
};

enum StreamContentType { Audio, Video };

enum ParseObjectType { AVIList, AVIChunk, AVIStreamData };

enum ParseState {
  ParseHeader,
  ParseHdrl,
  ParseAvih,
  ParseStrl,
  SubChunkContinue,
  SubChunk,
  ParseRec,
  ParseStrf,
  AfterStrf,
  ParseMovi,
  ParseIgnore,
};

// http://bass.radio42.com/help/html/56c44e65-9b99-fa0d-d74a-3d9de3b01e89.htm
enum AVIAudioFormat { PCM = 1, ADPCM = 2, IEEE_FLOAT = 3, AAC = 255 };

/// @brief Represents a LIST or a CHUNK
class ParseObject {
 public:
  void set(long currentPos, Str id, uint32_t size, ParseObjectType type) {
    set(currentPos, id.c_str(), size, type);
  }

  void set(long currentPos, const char *id, uint32_t size,
           ParseObjectType type) {
    object_type = type;
    data_size = size;
    start_pos = currentPos;
    // allign on word
    if (size % 2 != 0) {
      data_size++;
    }
    end_pos = currentPos + data_size + 4;
    // save FOURCC
    if (id != nullptr) {
      memcpy(chunk_id, id, 4);
      chunk_id[5] = 0;
    }
    open = data_size;
  }
  const char *id() { return chunk_id; }
  int size() { return data_size; }

  ParseObjectType type() { return object_type; }
  bool isValid() {
    switch (object_type) {
      case AVIStreamData:
        return isAudio() || isVideo();
      case AVIChunk:
        return open > 0;
      case AVIList:
        return true;
    }
    return false;
  }

  // for Chunk
  AVIMainHeader *asAVIMainHeader(void *ptr) { return (AVIMainHeader *)ptr; }
  AVIStreamHeader *asAVIStreamHeader(void *ptr) {
    return (AVIStreamHeader *)ptr;
  }
  WAVFormatX *asAVIAudioFormat(void *ptr) { return (WAVFormatX *)ptr; }
  BitmapInfoHeader *asAVIVideoFormat(void *ptr) {
    return (BitmapInfoHeader *)ptr;
  }

  uint32_t open;
  size_t end_pos;
  size_t start_pos;
  uint32_t data_size;

  // for AVIStreamData
  int streamNumber() {
    return object_type == AVIStreamData ? chunk_id[1] << 8 + chunk_id[0] : 0;
  }
  bool isAudio() {
    return object_type == AVIStreamData
               ? chunk_id[2] == 'w' && chunk_id[3] == 'b'
               : false;
  }
  bool isVideoUncompressed() {
    return object_type == AVIStreamData
               ? chunk_id[2] == 'd' && chunk_id[3] == 'b'
               : false;
  }
  bool isVideoCompressed() {
    return object_type == AVIStreamData
               ? chunk_id[2] == 'd' && chunk_id[3] == 'c'
               : false;
  }
  bool isVideo() { return isVideoCompressed() || isVideoUncompressed(); }

 protected:
  // ParseBuffer data_buffer;
  char chunk_id[5] = {};
  ParseObjectType object_type;
};

/**
 * @brief AVI Container Decoder which can be fed with small chunks of data. The
 * minimum length must be bigger then the header size! The file structure is
 * documented at
 * https://learn.microsoft.com/en-us/windows/win32/directshow/avi-riff-file-reference
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AVIDecoder : public AudioDecoder {
 public:
  AVIDecoder(int bufferSize = 1024) {
    parse_buffer.resize(bufferSize);
    p_decoder = &copy_decoder;
    p_output_audio = new EncodedAudioOutput(&copy_decoder);
  }
  AVIDecoder(AudioDecoder *audioDecoder, int bufferSize = 1024) {
    parse_buffer.resize(bufferSize);
    p_decoder = audioDecoder;
    p_output_audio = new EncodedAudioOutput(audioDecoder);
  }
  ~AVIDecoder() {
    if (p_output_audio != nullptr) delete p_output_audio;
  }

  void begin() {
    parse_state = ParseHeader;
    header_is_avi = false;
    is_parsing_active = true;
    current_pos = 0;
    header_is_avi = false;
    stream_header_idx = -1;
    is_metadata_ready = false;
  }

  virtual void setOutputStream(Print &out_stream) {
    // p_output_audio = &out_stream;
    p_output_audio->setOutput(&out_stream);
  }

  virtual void setOutputVideoStream(VideoOutput &out_stream) {
    p_output_video = &out_stream;
  }

  virtual size_t write(const void *data, size_t length) {
    LOGD("write: %d", (int)length);
    int result = parse_buffer.writeArray((uint8_t *)data, length);
    if (is_parsing_active) {
      // we expect the first parse to succeed
      if (parse()) {
        // if so we process the parse_buffer
        while (parse_buffer.available() > 4) {
          if (!parse()) break;
        }
      } else {
        LOGD("Parse Error");
        parse_buffer.clear();
        result = length;
        is_parsing_active = false;
      }
    }
    return result;
  }

  operator bool() override { return is_parsing_active; }

  void end() override { is_parsing_active = false; };

  /// Provides the information from the main header chunk
  AVIMainHeader mainHeader() { return main_header; }

  /// Provides the information from the stream header chunks
  AVIStreamHeader streamHeader(int idx) { return stream_header[idx]; }

  /// Provides the video information
  BitmapInfoHeader videoInfoExt() { return video_info; };

  const char *videoFormat() { return video_format; }

  /// Provides the audio information
  WAVFormatX audioInfoExt() { return audio_info; }

  /// Provides the  audio_info.wFormatTag
  int audioFormat() { return audio_info.wFormatTag; }

  /// Returns true if all metadata has been parsed and is available
  bool isMetadataReady() { return is_metadata_ready; }
  /// Register a validation callback which is called after parsing just before
  /// playing the audio
  void setValidationCallback(bool (*cb)(AVIDecoder &avi)) {
    validation_cb = cb;
  }

 protected:
  bool header_is_avi = false;
  bool is_parsing_active = true;
  ParseState parse_state = ParseHeader;
  ParseBuffer parse_buffer;
  AVIMainHeader main_header;
  int stream_header_idx = -1;
  Vector<AVIStreamHeader> stream_header;
  BitmapInfoHeader video_info;
  WAVFormatX audio_info;
  Vector<StreamContentType> content_types;
  Stack<ParseObject> object_stack;
  ParseObject current_stream_data;
  EncodedAudioOutput *p_output_audio = nullptr;
  VideoOutput *p_output_video = nullptr;
  long open_subchunk_len = 0;
  long current_pos = 0;
  long movi_end_pos = 0;
  StrExt spaces;
  StrExt str;
  char video_format[5] = {0};
  bool is_metadata_ready = false;
  bool (*validation_cb)(AVIDecoder &avi) = nullptr;
  CopyDecoder copy_decoder;
  AudioDecoder *p_decoder = nullptr;

  bool isCurrentStreamAudio() {
    return strncmp(stream_header[stream_header_idx].fccType, "auds", 4) == 0;
  }

  bool isCurrentStreamVideo() {
    return strncmp(stream_header[stream_header_idx].fccType, "vids", 4) == 0;
  }

  // we return true if at least one parse step was successful
  bool parse() {
    bool result = true;
    switch (parse_state) {
      case ParseHeader: {
        result = parseHeader();
        if (result) parse_state = ParseHdrl;
      } break;

      case ParseHdrl: {
        ParseObject hdrl = parseList("hdrl");
        result = hdrl.isValid();
        if (result) {
          parse_state = ParseAvih;
        }
      } break;

      case ParseAvih: {
        ParseObject avih = parseChunk("avih");
        result = avih.isValid();
        if (result) {
          main_header = *(avih.asAVIMainHeader(parse_buffer.data()));
          stream_header.resize(main_header.dwStreams);
          consume(avih.size());
          parse_state = ParseStrl;
        }
      } break;

      case ParseStrl: {
        ParseObject strl = parseList("strl");
        ParseObject strh = parseChunk("strh");
        stream_header[++stream_header_idx] =
            *(strh.asAVIStreamHeader(parse_buffer.data()));
        consume(strh.size());
        parse_state = ParseStrf;
      } break;

      case ParseStrf: {
        ParseObject strf = parseChunk("strf");
        if (isCurrentStreamAudio()) {
          audio_info = *(strf.asAVIAudioFormat(parse_buffer.data()));
          setupAudioInfo();
          LOGI("audioFormat: %d", audioFormat());
          content_types.push_back(Audio);
          consume(strf.size());
        } else if (isCurrentStreamVideo()) {
          video_info = *(strf.asAVIVideoFormat(parse_buffer.data()));
          setupVideoInfo();
          LOGI("videoFormat: %s", videoFormat());
          content_types.push_back(Video);
          video_format[4] = 0;
          consume(strf.size());
        } else {
          result = false;
        }
        parse_state = AfterStrf;
      } break;

      case AfterStrf: {
        // ignore all data until we find a new List
        int pos = parse_buffer.indexOf("LIST");
        if (pos >= 0) {
          consume(pos);
          ParseObject tmp = tryParseList();
          if (Str(tmp.id()).equals("strl")) {
            parse_state = ParseStrl;
          } else if (Str(tmp.id()).equals("movi")) {
            parse_state = ParseMovi;
          } else {
            // e.g. ignore info
            consume(tmp.size() + LIST_HEADER_SIZE);
          }
        } else {
          // no valid data, so throw it away, we keep the last 4 digits in case
          // if it contains the beginning of a LIST
          cleanupStack();
          consume(parse_buffer.available() - 4);
        }
      } break;

      case ParseMovi: {
        ParseObject movi = tryParseList();
        if (Str(movi.id()).equals("movi")) {
          consume(LIST_HEADER_SIZE);
          is_metadata_ready = true;
          if (validation_cb) is_parsing_active = (validation_cb(*this));
          processStack(movi);
          movi_end_pos = movi.end_pos;
          parse_state = SubChunk;
          // trigger new write
          result = false;
        }
      } break;

      case SubChunk: {
        // rec is optinal
        ParseObject hdrl = tryParseList();
        if (Str(hdrl.id()).equals("rec")) {
          consume(CHUNK_HEADER_SIZE);
          processStack(hdrl);
        }

        current_stream_data = parseAVIStreamData();
        parse_state = SubChunkContinue;
        open_subchunk_len = current_stream_data.open;
        if (current_stream_data.isVideo()) {
          if (p_output_video != nullptr)
            p_output_video->beginFrame(hdrl.size());
        }

      } break;

      case SubChunkContinue: {
        writeData();
        if (open_subchunk_len == 0) {
          if (current_stream_data.isVideo() && p_output_video != nullptr)
            p_output_video->endFrame();
          if (tryParseChunk("idx").isValid()) {
            parse_state = ParseIgnore;
          } else if (tryParseList("rec").isValid()) {
            parse_state = ParseRec;
          } else {
            if (current_pos >= movi_end_pos) {
              parse_state = ParseIgnore;
            } else {
              parse_state = SubChunk;
            }
          }
        }
      } break;

      case ParseIgnore: {
        LOGD("ParseIgnore");
        parse_buffer.clear();
      } break;

      default:
        result = false;
        break;
    }
    return result;
  }

  void setupAudioInfo() {
    info.channels = audio_info.nChannels;
    info.bits_per_sample = audio_info.wBitsPerSample;
    info.sample_rate = audio_info.nSamplesPerSec;
    info.logInfo();
    // adjust the audio info if necessary
    if (p_decoder != nullptr) {
      p_decoder->setAudioInfo(info);
      info = p_decoder->audioInfo();
    }
    if (p_notify) {
      p_notify->setAudioInfo(info);
    }
  }

  void setupVideoInfo() {
    memcpy(video_format, stream_header[stream_header_idx].fccHandler, 4);
  }

  void writeData() {
    long to_write = min((long)parse_buffer.available(), open_subchunk_len);
    if (current_stream_data.isAudio()) {
      LOGD("audio %d", (int)to_write);
      p_output_audio->write(parse_buffer.data(), to_write);
      open_subchunk_len -= to_write;
      cleanupStack();
      consume(to_write);
    } else if (current_stream_data.isVideo()) {
      LOGD("video %d", (int)to_write);
      if (p_output_video != nullptr)
        p_output_video->write(parse_buffer.data(), to_write);
      open_subchunk_len -= to_write;
      cleanupStack();
      consume(to_write);
    }
  }

  // 'RIFF' fileSize fileType (data)
  bool parseHeader() {
    bool header_is_avi = false;
    int headerSize = 12;
    if (getStr(0, 4).equals("RIFF")) {
      ParseObject result;
      uint32_t header_file_size = getInt(4);
      header_is_avi = getStr(8, 4).equals("AVI ");
      result.set(current_pos, "AVI ", header_file_size, AVIChunk);
      processStack(result);
      consume(headerSize);

    } else {
      LOGE("parseHeader");
    }
    return header_is_avi;
  }

  /// We parse a chunk and  provide the FOURCC id and size: No content data is
  /// stored
  ParseObject tryParseChunk() {
    ParseObject result;
    result.set(current_pos, getStr(0, 4), 0, AVIChunk);
    return result;
  }

  /// We try to parse the indicated chunk and determine the size: No content
  /// data is stored
  ParseObject tryParseChunk(const char *id) {
    ParseObject result;
    if (getStr(0, 4).equals(id)) {
      result.set(current_pos, id, 0, AVIChunk);
    }
    return result;
  }

  ParseObject tryParseList(const char *id) {
    ParseObject result;
    Str &list_id = getStr(8, 4);
    if (list_id.equals(id) && getStr(0, 3).equals("LIST")) {
      result.set(current_pos, getStr(8, 4), getInt(4), AVIList);
    }
    return result;
  }

  /// We try to parse the actual state for any list
  ParseObject tryParseList() {
    ParseObject result;
    if (getStr(0, 4).equals("LIST")) {
      result.set(current_pos, getStr(8, 4), getInt(4), AVIList);
    }
    return result;
  }

  /// We load the indicated chunk from the current data
  ParseObject parseChunk(const char *id) {
    ParseObject result;
    int chunk_size = getInt(4);
    if (getStr(0, 4).equals(id) && parse_buffer.size() >= chunk_size) {
      result.set(current_pos, id, chunk_size, AVIChunk);
      processStack(result);
      consume(CHUNK_HEADER_SIZE);
    }
    return result;
  }

  /// We load the indicated list from the current data
  ParseObject parseList(const char *id) {
    ParseObject result;
    if (getStr(0, 4).equals("LIST") && getStr(8, 4).equals(id)) {
      int size = getInt(4);
      result.set(current_pos, id, size, AVIList);
      processStack(result);
      consume(LIST_HEADER_SIZE);
    }
    return result;
  }

  ParseObject parseAVIStreamData() {
    ParseObject result;
    int size = getInt(4);
    result.set(current_pos, getStr(0, 4), size, AVIStreamData);
    if (result.isValid()) {
      processStack(result);
      consume(8);
    }
    return result;
  }

  void processStack(ParseObject &result) {
    cleanupStack();
    object_stack.push(result);
    spaces.setChars(' ', object_stack.size());
    LOGD("%s - %s (%d-%d) size:%d", spaces.c_str(), result.id(),
         (int)result.start_pos, (int)result.end_pos, (int)result.data_size);
  }

  void cleanupStack() {
    ParseObject current;
    // make sure that we remove the object from the stack of we past the end
    object_stack.peek(current);
    while (current.end_pos <= current_pos) {
      object_stack.pop(current);
      object_stack.peek(current);
    }
  }

  /// Provides the string at the indicated byte offset with the indicated length
  Str &getStr(int offset, int len) {
    str.setCapacity(len + 1);
    const char *data = (const char *)parse_buffer.data();
    str.copyFrom((data + offset), len, 5);

    return str;
  }

  /// Provides the int32 at the indicated byte offset
  uint32_t getInt(int offset) {
    uint32_t *result = (uint32_t *)(parse_buffer.data() + offset);
    return *result;
  }

  /// We remove the processed bytes from the beginning of the buffer
  void consume(int len) {
    current_pos += len;
    parse_buffer.consume(len);
  }
};

}  // namespace audio_tools