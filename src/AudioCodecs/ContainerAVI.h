#pragma once
#include <string.h>

#include "AudioBasic/StrExt.h"
#include "AudioTools/Buffers.h"


#define LIST_HEADER_SIZE 12
#define CHUNK_HEADER_SIZE 8

namespace audio_tools {


/// Audio Formats
enum class AudioFormat : uint16_t {
  UNKNOWN = 0x0000, /* Microsoft Corporation */
  PCM = 0x0001,
  ADPCM = 0x0002,      /* Microsoft Corporation */
  IEEE_FLOAT = 0x0003, /* Microsoft Corporation */
  // VSELP = 0x0004 /* Compaq Computer Corp. */
  // IBM_CVSD = 0x0005 /* IBM Corporation */
  ALAW = 0x0006,  /* Microsoft Corporation */
  MULAW = 0x0007, /* Microsoft Corporation */
  // DTS = 0x0008 /* Microsoft Corporation */
  // DRM = 0x0009 /* Microsoft Corporation */
  // WMAVOICE9 = 0x000A /* Microsoft Corporation */
  // WMAVOICE10 = 0x000B /* Microsoft Corporation */
  // OKI_ADPCM = 0x0010 /* OKI */
  // DVI_ADPCM = 0x0011 /* Intel Corporation */
  // IMA_ADPCM(DVI_ADPCM) /*  Intel Corporation */
  // MEDIASPACE_ADPCM = 0x0012 /* Videologic */
  // SIERRA_ADPCM = 0x0013 /* Sierra Semiconductor Corp */
  // G723_ADPCM = 0x0014 /* Antex Electronics Corporation */
  // DIGISTD = 0x0015 /* DSP Solutions, Inc. */
  // DIGIFIX = 0x0016 /* DSP Solutions, Inc. */
  // DIALOGIC_OKI_ADPCM = 0x0017 /* Dialogic Corporation */
  // MEDIAVISION_ADPCM = 0x0018 /* Media Vision, Inc. */
  // CU_CODEC = 0x0019 /* Hewlett-Packard Company */
  // HP_DYN_VOICE = 0x001A /* Hewlett-Packard Company */
  // YAMAHA_ADPCM = 0x0020 /* Yamaha Corporation of America */
  // SONARC = 0x0021 /* Speech Compression */
  // DSPGROUP_TRUESPEECH = 0x0022 /* DSP Group, Inc */
  // ECHOSC1 = 0x0023 /* Echo Speech Corporation */
  // AUDIOFILE_AF36 = 0x0024 /* Virtual Music, Inc. */
  // APTX = 0x0025 /* Audio Processing Technology */
  // AUDIOFILE_AF10 = 0x0026 /* Virtual Music, Inc. */
  // PROSODY_1612 = 0x0027 /* Aculab plc */
  // LRC = 0x0028 /* Merging Technologies S.A. */
  // DOLBY_AC2 = 0x0030 /* Dolby Laboratories */
  // GSM610 = 0x0031 /* Microsoft Corporation */
  // MSNAUDIO = 0x0032 /* Microsoft Corporation */
  // ANTEX_ADPCME = 0x0033 /* Antex Electronics Corporation */
  // CONTROL_RES_VQLPC = 0x0034 /* Control Resources Limited */
  // DIGIREAL = 0x0035 /* DSP Solutions, Inc. */
  // DIGIADPCM = 0x0036 /* DSP Solutions, Inc. */
  // CONTROL_RES_CR10 = 0x0037 /* Control Resources Limited */
  // NMS_VBXADPCM = 0x0038 /* Natural MicroSystems */
  // CS_IMAADPCM = 0x0039 /* Crystal Semiconductor IMA ADPCM */
  // ECHOSC3 = 0x003A /* Echo Speech Corporation */
  // ROCKWELL_ADPCM = 0x003B /* Rockwell International */
  // ROCKWELL_DIGITALK = 0x003C /* Rockwell International */
  // XEBEC = 0x003D /* Xebec Multimedia Solutions Limited */
  // G721_ADPCM = 0x0040 /* Antex Electronics Corporation */
  // G728_CELP = 0x0041 /* Antex Electronics Corporation */
  // MSG723 = 0x0042 /* Microsoft Corporation */
  // INTEL_G723_1 = 0x0043 /* Intel Corp. */
  // INTEL_G729 = 0x0044 /* Intel Corp. */
  // SHARP_G726 = 0x0045 /* Sharp */
  // MPEG = 0x0050 /* Microsoft Corporation */
  // RT24 = 0x0052 /* InSoft, Inc. */
  // PAC = 0x0053 /* InSoft, Inc. */
  // MPEGLAYER3 = 0x0055 /* ISO/MPEG Layer3 Format Tag */
  // LUCENT_G723 = 0x0059 /* Lucent Technologies */
  // CIRRUS = 0x0060 /* Cirrus Logic */
  // ESPCM = 0x0061 /* ESS Technology */
  // VOXWARE = 0x0062 /* Voxware Inc */
  // CANOPUS_ATRAC = 0x0063 /* Canopus, co., Ltd. */
  // G726_ADPCM = 0x0064 /* APICOM */
  // G722_ADPCM = 0x0065 /* APICOM */
  // DSAT = 0x0066 /* Microsoft Corporation */
  // DSAT_DISPLAY = 0x0067 /* Microsoft Corporation */
  // VOXWARE_BYTE_ALIGNED = 0x0069 /* Voxware Inc */
  // VOXWARE_AC8 = 0x0070 /* Voxware Inc */
  // VOXWARE_AC10 = 0x0071 /* Voxware Inc */
  // VOXWARE_AC16 = 0x0072 /* Voxware Inc */
  // VOXWARE_AC20 = 0x0073 /* Voxware Inc */
  // VOXWARE_RT24 = 0x0074 /* Voxware Inc */
  // VOXWARE_RT29 = 0x0075 /* Voxware Inc */
  // VOXWARE_RT29HW = 0x0076 /* Voxware Inc */
  // VOXWARE_VR12 = 0x0077 /* Voxware Inc */
  // VOXWARE_VR18 = 0x0078 /* Voxware Inc */
  // VOXWARE_TQ40 = 0x0079 /* Voxware Inc */
  // VOXWARE_SC3 = 0x007A /* Voxware Inc */
  // VOXWARE_SC3_1 = 0x007B /* Voxware Inc */
  // SOFTSOUND = 0x0080 /* Softsound, Ltd. */
  // VOXWARE_TQ60 = 0x0081 /* Voxware Inc */
  // MSRT24 = 0x0082 /* Microsoft Corporation */
  // G729A = 0x0083 /* AT&T Labs, Inc. */
  // MVI_MVI2 = 0x0084 /* Motion Pixels */
  // DF_G726 = 0x0085 /* DataFusion Systems (Pty) (Ltd) */
  // DF_GSM610 = 0x0086 /* DataFusion Systems (Pty) (Ltd) */
  // ISIAUDIO = 0x0088 /* Iterated Systems, Inc. */
  // ONLIVE = 0x0089 /* OnLive! Technologies, Inc. */
  // MULTITUDE_FT_SX20 = 0x008A /* Multitude Inc. */
  // INFOCOM_ITS_G721_ADPCM = 0x008B /* Infocom */
  // CONVEDIA_G729 = 0x008C /* Convedia Corp. */
  // CONGRUENCY = 0x008D /* Congruency Inc. */
  // SBC24 = 0x0091 /* Siemens Business Communications Sys */
  // DOLBY_AC3_SPDIF = 0x0092 /* Sonic Foundry */
  // MEDIASONIC_G723 = 0x0093 /* MediaSonic */
  // PROSODY_8KBPS = 0x0094 /* Aculab plc */
  // ZYXEL_ADPCM = 0x0097 /* ZyXEL Communications, Inc. */
  // PHILIPS_LPCBB = 0x0098 /* Philips Speech Processing */
  // PACKED = 0x0099 /* Studer Professional Audio AG */
  // MALDEN_PHONYTALK = 0x00A0 /* Malden Electronics Ltd. */
  // RACAL_RECORDER_GSM = 0x00A1 /* Racal recorders */
  // RACAL_RECORDER_G720_A = 0x00A2 /* Racal recorders */
  // RACAL_RECORDER_G723_1 = 0x00A3 /* Racal recorders */
  // RACAL_RECORDER_TETRA_ACELP = 0x00A4 /* Racal recorders */
  // NEC_AAC = 0x00B0 /* NEC Corp. */
  // RAW_AAC1 = 0x00FF /* For Raw AAC, with format block
  // AudioSpecificConfig() (as
  //                    defined \ by MPEG-4), that follows WAVEFORMATEX */
  // RHETOREX_ADPCM = 0x0100 /* Rhetorex Inc. */
  // IRAT = 0x0101 /* BeCubed Software Inc. */
  // VIVO_G723 = 0x0111 /* Vivo Software */
  // VIVO_SIREN = 0x0112 /* Vivo Software */
  // PHILIPS_CELP = 0x0120 /* Philips Speech Processing */
  // PHILIPS_GRUNDIG = 0x0121 /* Philips Speech Processing */
  // DIGITAL_G723 = 0x0123 /* Digital Equipment Corporation */
  // SANYO_LD_ADPCM = 0x0125 /* Sanyo Electric Co., Ltd. */
  // SIPROLAB_ACEPLNET = 0x0130 /* Sipro Lab Telecom Inc. */
  // SIPROLAB_ACELP4800 = 0x0131 /* Sipro Lab Telecom Inc. */
  // SIPROLAB_ACELP8V3 = 0x0132 /* Sipro Lab Telecom Inc. */
  // SIPROLAB_G729 = 0x0133 /* Sipro Lab Telecom Inc. */
  // SIPROLAB_G729A = 0x0134 /* Sipro Lab Telecom Inc. */
  // SIPROLAB_KELVIN = 0x0135 /* Sipro Lab Telecom Inc. */
  // VOICEAGE_AMR = 0x0136 /* VoiceAge Corp. */
  // G726ADPCM = 0x0140 /* Dictaphone Corporation */
  // DICTAPHONE_CELP68 = 0x0141 /* Dictaphone Corporation */
  // DICTAPHONE_CELP54 = 0x0142 /* Dictaphone Corporation */
  // QUALCOMM_PUREVOICE = 0x0150 /* Qualcomm, Inc. */
  // QUALCOMM_HALFRATE = 0x0151 /* Qualcomm, Inc. */
  // TUBGSM = 0x0155 /* Ring Zero Systems, Inc. */
  // MSAUDIO1 = 0x0160 /* Microsoft Corporation */
  // WMAUDIO2 = 0x0161 /* Microsoft Corporation */
  // WMAUDIO3 = 0x0162 /* Microsoft Corporation */
  // WMAUDIO_LOSSLESS = 0x0163 /* Microsoft Corporation */
  // WMASPDIF = 0x0164 /* Microsoft Corporation */
  // UNISYS_NAP_ADPCM = 0x0170 /* Unisys Corp. */
  // UNISYS_NAP_ULAW = 0x0171 /* Unisys Corp. */
  // UNISYS_NAP_ALAW = 0x0172 /* Unisys Corp. */
  // UNISYS_NAP_16K = 0x0173 /* Unisys Corp. */
  // SYCOM_ACM_SYC008 = 0x0174 /* SyCom Technologies */
  // SYCOM_ACM_SYC701_G726L = 0x0175 /* SyCom Technologies */
  // SYCOM_ACM_SYC701_CELP54 = 0x0176 /* SyCom Technologies */
  // SYCOM_ACM_SYC701_CELP68 = 0x0177 /* SyCom Technologies */
  // KNOWLEDGE_ADVENTURE_ADPCM = 0x0178 /* Knowledge Adventure, Inc.
  // */
  // FRAUNHOFER_IIS_MPEG2_AAC = 0x0180 /* Fraunhofer IIS */
  // DTS_DS = 0x0190 /* Digital Theatre Systems, Inc. */
  // CREATIVE_ADPCM = 0x0200 /* Creative Labs, Inc */
  // CREATIVE_FASTSPEECH8 = 0x0202 /* Creative Labs, Inc */
  // CREATIVE_FASTSPEECH10 = 0x0203 /* Creative Labs, Inc */
  // UHER_ADPCM = 0x0210 /* UHER informatic GmbH */
  // ULEAD_DV_AUDIO = 0x0215 /* Ulead Systems, Inc. */
  // ULEAD_DV_AUDIO_1 = 0x0216 /* Ulead Systems, Inc. */
  // QUARTERDECK = 0x0220 /* Quarterdeck Corporation */
  // ILINK_VC = 0x0230 /* I-link Worldwide */
  // RAW_SPORT = 0x0240 /* Aureal Semiconductor */
  // ESST_AC3 = 0x0241 /* ESS Technology, Inc. */
  // GENERIC_PASSTHRU = 0x0249,
  // IPI_HSX = 0x0250 /* Interactive Products, Inc. */
  // IPI_RPELP = 0x0251 /* Interactive Products, Inc. */
  // CS2 = 0x0260 /* Consistent Software */
  // SONY_SCX = 0x0270 /* Sony Corp. */
  // SONY_SCY = 0x0271 /* Sony Corp. */
  // SONY_ATRAC3 = 0x0272 /* Sony Corp. */
  // SONY_SPC = 0x0273 /* Sony Corp. */
  // TELUM_AUDIO = 0x0280 /* Telum Inc. */
  // TELUM_IA_AUDIO = 0x0281 /* Telum Inc. */
  // NORCOM_VOICE_SYSTEMS_ADPCM = 0x0285 /* Norcom Electronics Corp.*/
  // FM_TOWNS_SND = 0x0300 /* Fujitsu Corp. */
  // MICRONAS = 0x0350 /* Micronas Semiconductors, Inc. */
  // MICRONAS_CELP833 = 0x0351 /* Micronas Semiconductors, Inc. */
  // BTV_DIGITAL = 0x0400 /* Brooktree Corporation */
  // INTEL_MUSIC_CODER = 0x0401 /* Intel Corp. */
  // INDEO_AUDIO = 0x0402 /* Ligo */
  // QDESIGN_MUSIC = 0x0450 /* QDeign Corporation */
  // ON2_VP7_AUDIO = 0x0500 /* On2 echnologies */
  // ON2_VP6_AUDIO = 0x0501 /* On2 Tchnologies */
  // VME_VMPCM = 0x0680 /* AT&T Labs,Inc. */
  // TPC = 0x0681 /* AT&T Labs, Inc. *
  // LIGHTWAVE_LOSSLESS = 0x08AE /* Clerjump */
  // OLIGSM = 0x1000 /* Ing C. Olivetti  C., S.p.A. */
  // OLIADPCM = 0x1001 /* Ing C. Olivetti& C., S.p.A.*/
  // OLICELP = 0x1002 /* Ing C. Olivetti &C., S.p.A. */
  // OLISBC = 0x1003 /* Ing C. Olivetti & C, S.p.A. */
  // OLIOPR = 0x1004 /* Ing C. Olivetti & C. S.p.A. */
  // LH_CODEC = 0x1100 /* Lernout & Hauspie *
  // LH_CODEC_CELP = 0x1101 /* Lernout & Hauspie *
  // LH_CODEC_SBC8 = 0x1102 /* Lernout & Hauspie */
  // LH_CODEC_SBC12 = 0x1103 /* Lernout & Hauspie */
  // LH_CODEC_SBC16 = 0x1104 /* Lernout & Hauspie */
  // NORRIS = 0x1400 /* Norris Communications, Inc. */
  // ISIAUDIO_2 = 0x1401 /* ISIAudio */
  // SOUNDSPACE_MUSICOMPRESS = 0x1500 /* AT&T Labs, Inc. */
  // MPEG_ADTS_AAC = 0x1600 /* Microsoft Corporation */
  // MPEG_RAW_AAC = 0x1601 /* Microsoft Corporation */
  // MPEG_LOAS = 0x1602 /* Microsoft Corporation (MPEG-4 Audio
  //                     * Transport Streams (LOAS/LATM) \
  //                     */
  // NOKIA_MPEG_ADTS_AAC = 0x1608 /* Microsoft Corporation */
  // NOKIA_MPEG_RAW_AAC = 0x1609 /* Microsoft Corporation */
  // VODAFONE_MPEG_ADTS_AAC = 0x160A /* Microsoft Corporation */
  // VODAFONE_MPEG_RAW_AAC = 0x160B /* Microsoft Corporation */
  // MPEG_HEAAC =
  //     0x1610 /* Microsoft Corporation (MPEG-2 AAC or MPEG-4 HE-AAC
  //     v1/v2 streams
  //             \
  //             with any payload (ADTS, ADIF, LOAS/LATM, RAW). Format block \
  //             icludes MP4 AudioSpecificConfig() -- see HEAACWAVEFORMAT below
  //             */
  // VOXWARE_RT24_SPEECH = 0x181C /* Voxware Inc. */
  // SONICFOUNDRY_LOSSLESS = 0x1971 /* Sonic Foundry */
  // INNINGS_TELECOM_ADPCM = 0x1979 /* Innings Telecom Inc. */
  // LUCENT_SX8300P = 0x1C07 /* Lucent Technologies */
  // LUCENT_SX5363S = 0x1C0C /* Lucent Technologies */
  // CUSEEME = 0x1F03 /* CUSeeMe */
  // NTCSOFT_ALF2CM_ACM = 0x1FC4 /* NTCSoft */
  // DVM = 0x2000 /* FAST Multimedia AG */
  // DTS2 = 0x2001,
  // MAKEAVIS = 0x3313
  // DIVIO_MPEG4_AAC = 0x4143 /* Divio, Inc. */
  // NOKIA_ADAPTIVE_MULTIRATE = 0x4201 /* Nokia */
  // DIVIO_G726 = 0x4243 /* Divio, Inc. */
  // LEAD_SPEECH = 0x434C /* LEAD Technologies */
  // LEAD_VORBIS = 0x564C /* LEAD Technologies */
  // WAVPACK_AUDIO = 0x5756 /* xiph.org */
  // ALAC = 0x6C61 /* Apple Lossless */
  // OGG_VORBIS_MODE_1 = 0x674F /* Ogg Vorbis */
  // OGG_VORBIS_MODE_2 = 0x6750 /* Ogg Vorbis */
  // OGG_VORBIS_MODE_3 = 0x6751 /* Ogg Vorbis */
  // OGG_VORBIS_MODE_1_PLUS = 0x676F /* Ogg Vorbis */
  // OGG_VORBIS_MODE_2_PLUS = 0x6770 /* Ogg Vorbis */
  // OGG_VORBIS_MODE_3_PLUS = 0x6771 /* Ogg Vorbis */
  // F3COM_NBX = 0x7000 /* 3COM Corp. */
  // OPUS = 0x704F /* Opus */
  // FAAD_AAC = 0x706D
  // AMR_NB = 0x7361 /* AMR Narrowband */
  // AMR_WB = 0x7362 /* AMR Wideband */
  // AMR_WP = 0x7363 /* AMR Wideband Plus */
  // GSM_AMR_CBR = 0x7A21 /* GSMA/3GPP */
  // GSM_AMR_VBR_SID = 0x7A22 /* GSMA/3GPP */
  // COMVERSE_INFOSYS_G723_1 = 0xA100 /* Comverse Infosys */
  // COMVERSE_INFOSYS_AVQSBC = 0xA101 /* Comverse Infosys */
  // COMVERSE_INFOSYS_SBC = 0xA102 /* Comverse Infosys */
  // SYMBOL_G729_A = 0xA103 /* Symbol Technologies */
  // VOICEAGE_AMR_WB = 0xA104 /* VoiceAge Corp. */
  // INGENIENT_G726 = 0xA105 /* Ingenient Technologies, Inc. */
  // MPEG4_AAC = 0xA106 /* ISO/MPEG-4 */
  // ENCORE_G726 = 0xA107 /* Encore Software */
  // ZOLL_ASAO = 0xA108 /* ZOLL Medical Corp. */
  // SPEEX_VOICE = 0xA109 /* xiph.org */
  // VIANIX_MASC = 0xA10A /* Vianix LLC */
  // WM9_SPECTRUM_ANALYZER = 0xA10B /* Microsoft */
  // WMF_SPECTRUM_ANAYZER = 0xA10C /* Microsoft */
  // GSM_610 = 0xA0D,
  // GSM_620 = 0xA1E,
  // GSM_660 = 0xA10,
  // GSM_690 = 0xA110
  // GSM_ADAPTIVE_MULTIRATE_WB = 0xA111
  // POLYCOM_G722 = 0xA112 /* Polycom */
  // POLYCOM_G728 = 0xA113 /* Polycom */
  // POLYCOM_G729_A = 0xA114 /* Polycom */
  // POLYCOM_SIREN = 0xA115 /* Polycom */
  // GLOBAL_IP_ILBC = 0xA116 /* Global IP */
  // RADIOTIME_TIME_SHIFT_RADIO = 0xA117 /* RadioTime */
  // NICE_ACA = 0xA118 /* Nice Systems */
  // NICE_ADPCM = 0xA119 /* Nice Systems */
  // VOCORD_G721 = 0xA11A /* Vocord Telecom */
  // VOCORD_G726 = 0xA11B /* Vocord Telecom */
  // VOCORD_G722_1 = 0xA11C /* Vocord Telecom */
  // VOCORD_G728 = 0xA11D /* Vocord Telecom */
  // VOCORD_G729 = 0xA11E /* Vocord Telecom */
  // VOCORD_G729_A = 0xA11F /* Vocord Telecom */
  // VOCORD_G723_1 = 0xA120 /* Vocord Telecom */
  // VOCORD_LBC = 0xA121 /* Vocord Telecom */
  // NICE_G728 = 0xA122 /* Nice Systems */
  // FRACE_TELECOM_G729 = 0xA123 /* France Telecom */
  // CODIAN = 0xA124 /* CODIAN */
  // FLAC = 0xF1AC /* flac.sourceforge.net */
};


class VideoOutput : public AudioOutput {
 public:
  virtual void beginFrame(size_t size) = 0;
  virtual size_t write(const uint8_t *data, size_t byteCount) = 0;
  virtual void endFrame() = 0;
};

class ParseBuffer {
 public:
  size_t writeArray(uint8_t *data, size_t len) {
    int to_write = min(availableToWrite(), (size_t)len);
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

  size_t availableToWrite() { return size() - available_byte_count; }

  size_t available() { return available_byte_count; }

  void clear() {
    available_byte_count = 0;
    memset(vector.data(), 0, vector.size());
  }

  bool isEmpty() { return available_byte_count == 0; }

  size_t size() { return vector.size(); }

  long indexOf(const char *str) {
    uint8_t *ptr = (uint8_t *)memmem(vector.data(), available_byte_count, str,
                                     strlen(str));
    return ptr == nullptr ? -1l : ptr - vector.data();
  }

 protected:
  Vector<uint8_t> vector{0};
  size_t available_byte_count = 0;
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
  AudioFormat wFormatTag;
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

/// @brief Represents a LIST or a CHUNK
class ParseObject {
 public:
  void set(size_t currentPos, Str id, size_t size, ParseObjectType type) {
    set(currentPos, id.c_str(), size, type);
  }

  void set(size_t currentPos, const char *id, size_t size,
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
  size_t size() { return data_size; }

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

  size_t open;
  size_t end_pos;
  size_t start_pos;
  size_t data_size;

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
  AVIDecoder(AudioDecoder *audioDecoder, VideoOutput *videoOut=nullptr, int bufferSize = 1024) {
    parse_buffer.resize(bufferSize);
    p_decoder = audioDecoder;
    p_output_audio = new EncodedAudioOutput(audioDecoder);
    if (videoOut!=nullptr){
      setOutputVideoStream(*videoOut);
    }
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
  BitmapInfoHeader aviVideoInfo() { return video_info; };

  const char *videoFormat() { return video_format; }

  /// Provides the audio information
  WAVFormatX aviAudioInfo() { return audio_info; }

  /// Provides the  audio_info.wFormatTag
  AudioFormat audioFormat() { return audio_info.wFormatTag; }

  /// Returns true if all metadata has been parsed and is available
  bool isMetadataReady() { return is_metadata_ready; }
  /// Register a validation callback which is called after parsing just before
  /// playing the audio
  void setValidationCallback(bool (*cb)(AVIDecoder &avi)) {
    validation_cb = cb;
  }

  /// Provide the length of the video in seconds
  int videoSeconds() {return video_seconds;}

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
  int video_seconds = 0;

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
          LOGI("audioFormat: %d", (int)audioFormat());
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
          LOGI("video:[%d]->[%d]",(int)current_stream_data.start_pos, (int)current_stream_data.end_pos );
          if (p_output_video != nullptr)
            p_output_video->beginFrame(current_stream_data.open);
        } else if (current_stream_data.isAudio()){
          LOGI("audio:[%d]->[%d]", (int)current_stream_data.start_pos,(int)current_stream_data.end_pos );
        } else {
          LOGW("unknown subchunk at %d", (int)current_pos);
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
    AVIStreamHeader *vh = &stream_header[stream_header_idx];
    if (vh->dwScale <= 0) {
      vh->dwScale = 1;
    }
    int rate = vh->dwRate / vh->dwScale;
    video_seconds = rate <= 0 ? 0 : vh->dwLength / rate;
    LOGI("videoSeconds: %d seconds", video_seconds);
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