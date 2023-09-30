#pragma once

#include "AudioCodecs/AudioEncoded.h"

namespace audio_tools {

#include "AudioCodecs/AudioEncoded.h"
#include "tsdemux.h"
#include "stdlib.h"

#ifndef MTS_PRINT_PIDS_LEN
#  define MTS_PRINT_PIDS_LEN (16)
#endif

#ifndef MTS_UNDERFLOW_LIMIT
#  define MTS_UNDERFLOW_LIMIT 200
#endif

#ifndef MTS_WRITE_BUFFER_SIZE
#  define MTS_WRITE_BUFFER_SIZE 2000
#endif

#ifndef ALLOC_MEM_INIT
#  define ALLOC_MEM_INIT 0
#endif

struct AllocSize {
   void *data = nullptr;
   size_t size = 0;

  AllocSize() = default;
   AllocSize(void*data, size_t size){
    this->data = data;
    this->size = size;
   }
};

/**
 * @brief MPEG-TS (MTS) decoder. Extracts the AAC audio data from a MPEG-TS (MTS) data stream. You can
 * define the relevant stream types via the API.
 * Required dependency: https://github.com/pschatzmann/arduino-tsdemux
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class MTSDecoder : public AudioDecoder {
 public:
  MTSDecoder() { 
    self = this; 
  };

  void begin() override {
    TRACED();
    // automatically close when called multiple times
    if (is_active) {
      end();
    }

    is_active = true;

    // create the pids we plan on printing
    memset(print_pids, 0, sizeof(print_pids));

    // set default values onto the context.
    if (tsd_context_init(&ctx)!=TSD_OK){
      TRACEE();
      is_active = false;
    }

    // log memory allocations ?
    if (is_alloc_active){
      ctx.malloc = log_malloc;
      ctx.realloc = log_realloc;
      ctx.calloc = log_calloc;
      ctx.free = log_free;
    }

    // default supported stream types
    if (stream_types.empty()){
      addStreamType(TSD_PMT_STREAM_TYPE_PES_METADATA);
      addStreamType(TSD_PMT_STREAM_TYPE_AUDIO_AAC);
    }

    // add a callback.
    // the callback is used to determine which PIDs contain the data we want
    // to demux. We also receive PES data for any PIDs that we register later
    // on.
    if (tsd_set_event_callback(&ctx, event_cb)!=TSD_OK){
      TRACEE();
      is_active = false;
    }
  }

  void end() override {
    TRACED();
    // finally end the demux process which will flush any remaining PES data.
    tsd_demux_end(&ctx);

    // destroy context
    tsd_context_destroy(&ctx);

    is_active = false;
  }

  virtual operator bool() override { return is_active; }

  const char *mime() { return "video/MP2T"; }

  size_t write(const void *in_ptr, size_t in_size) override {
    if (!is_active) return 0;
    LOGD("MTSDecoder::write: %d", (int)in_size);
    size_t result = buffer.writeArray((uint8_t*)in_ptr, in_size);
    // demux
    demux(underflowLimit);
    return result;
  }

  void flush(){
    demux(0);
  }

  void clearStreamTypes(){
    TRACED();
    stream_types.clear();
  }

  void addStreamType(TSDPMTStreamType type){
    TRACED();
    stream_types.push_back(type);
  }

  bool isStreamTypeActive(TSDPMTStreamType type){
    for (int j=0;j<stream_types.size();j++){
      if (stream_types[j]==type) return true;
    }
    return false;
  }

  /// Set a new write buffer size (default is 2000)
  void resizeBuffer(int size){
    buffer.resize(size);
  }

  /// Activate logging for memory allocations
  void setMemoryAllocationLogging(bool flag){
    is_alloc_active = flag;
  }

 protected:
  static MTSDecoder *self;
  int underflowLimit = MTS_UNDERFLOW_LIMIT;
  bool is_active = false;
  bool is_write_active = false;
  bool is_alloc_active = false;
  TSDemuxContext ctx;
  uint16_t print_pids[MTS_PRINT_PIDS_LEN] = {0};
  SingleBuffer<uint8_t> buffer{MTS_WRITE_BUFFER_SIZE};
  Vector<TSDPMTStreamType> stream_types;
  Vector<AllocSize> alloc_vector;

  void set_write_active(bool flag){
    //LOGD("is_write_active: %s", flag ? "true":"false");
    is_write_active = flag;
  }

  /// Determines if we are at the beginning of a new file
  bool is_new_file(uint8_t* data){
    bool payloadUnitStartIndicator = (data[1] & 0x40) >> 6;
    bool result = data[0]==0x47 && payloadUnitStartIndicator;
    return result;
  }

  void demux(int limit){
    TRACED();
    TSDCode res = TSD_OK;
    int count = 0;
    while (res == TSD_OK && buffer.available() > limit) {
      // Unfortunatly we need to reset the demux after each file
      if (is_new_file(buffer.data())){
        LOGD("parsing new file");
        begin();
      }
      size_t len = tsd_demux(&ctx, (void *)buffer.data(), buffer.available(), &res);
      // remove processed bytes
      buffer.clearArray(len);
      // get next bytes
      count++;
      if (res != TSD_OK) logResult(res);
    }
    LOGD("Number of demux calls: %d", count);
  }

  void logResult(TSDCode code) {
    switch (code) {
      case TSD_OK:
        LOGD("TSD_OK");
        break;
      case TSD_INVALID_SYNC_BYTE:
        LOGW("TSD_INVALID_SYNC_BYTE");
        break;
      case TSD_INVALID_CONTEXT:
        LOGW("TSD_INVALID_CONTEXT");
        break;
      case TSD_INVALID_DATA:
        LOGW("TSD_INVALID_DATA");
        break;
      case TSD_INVALID_DATA_SIZE:
        LOGW("TSD_INVALID_DATA_SIZE");
        break;
      case TSD_INVALID_ARGUMENT:
        LOGW("TSD_INVALID_ARGUMENT");
        break;
      case TSD_INVALID_START_CODE_PREFIX:
        LOGW("TSD_INVALID_START_CODE_PREFIX");
        break;
      case TSD_OUT_OF_MEMORY:
        LOGW("TSD_OUT_OF_MEMORY");
        break;
      case TSD_INCOMPLETE_TABLE:
        LOGW("TSD_INCOMPLETE_TABLE");
        break;
      case TSD_NOT_A_TABLE_PACKET:
        LOGW("TSD_NOT_A_TABLE_PACKET");
        break;
      case TSD_PARSE_ERROR:
        LOGW("TSD_PARSE_ERROR");
        break;
      case TSD_PID_ALREADY_REGISTERED:
        LOGW("TSD_PID_ALREADY_REGISTERED");
        break;
      case TSD_TSD_MAX_PID_REGS_REACHED:
        LOGW("TSD_TSD_MAX_PID_REGS_REACHED");
        break;
      case TSD_PID_NOT_FOUND:
        LOGW("TSD_PID_NOT_FOUND");
        break;
      case TSD_INVALID_POINTER_FIELD:
        LOGW("TSD_INVALID_POINTER_FIELD");
        break;
    }
  }

  // event callback
  static void event_cb(TSDemuxContext *ctx, uint16_t pid, TSDEventId event_id,
                       void *data) {
    TRACED();
    if (MTSDecoder::self != nullptr) {
      MTSDecoder::self->event_cb_local(ctx, pid, event_id, data);
    }
  }

  void event_cb_local(TSDemuxContext *ctx, uint16_t pid, TSDEventId event_id,
                      void *data) {
    if (event_id == TSD_EVENT_PAT) {
      set_write_active(false);
      print_pat(ctx, data);
    } else if (event_id == TSD_EVENT_PMT) {
      set_write_active(false);
      print_pmt(ctx, data);
    } else if (event_id == TSD_EVENT_PES) {
      TSDPESPacket *pes = (TSDPESPacket *)data;
      // This is where we write the PES data into our buffer.
      LOGD("====================");
      LOGD("PID %x PES Packet, Size: %ld, stream_id=%u, pts=%lu, dts=%lu", pid,
           pes->data_bytes_length, pes->stream_id, pes->pts, pes->dts);
      // print out the PES Packet data if it's in our print list
      int i;
      AudioLogger logger = AudioLogger::instance();
      for (i = 0; i < MTS_PRINT_PIDS_LEN; ++i) {
        if (print_pids[i] == pid) {
          // log data
          if (logger.isLogging(AudioLogger::Debug)) {
            logger.print("    PES data");
            logger.print(is_write_active? "active:":"inactive:");
            int j = 0;
            while (j < pes->data_bytes_length) {
              char n = pes->data_bytes[j];
              logger.printCharHex(n);
              ++j;
            }
            logger.printChar('\n');
          }
          // output data
          if (p_print != nullptr) {
            //size_t eff = p_print->write(pes->data_bytes, pes->data_bytes_length);
            size_t eff = writeSamples<uint8_t>(p_print,(uint8_t*) pes->data_bytes, pes->data_bytes_length);
            if(eff!=pes->data_bytes_length){
              // we should not get here
              TRACEE();
            }
          }
        }
      }

    } else if (event_id == TSD_EVENT_ADAP_FIELD_PRV_DATA) {
      set_write_active(false);
      // we're only watching for SCTE Adaptions Field Private Data,
      // so we know that we must parse it as a list of descritors.
      TSDAdaptationField *adap_field = (TSDAdaptationField *)data;
      TSDDescriptor *descriptors = NULL;
      size_t descriptors_length = 0;
      tsd_descriptor_extract(ctx, adap_field->private_data_bytes,
                             adap_field->transport_private_data_length,
                             &descriptors, &descriptors_length);

      LOGD("====================");
      LOGD("Descriptors - Adaptation Fields");
      int i = 0;
      for (; i < descriptors_length; ++i) {
        TSDDescriptor *des = &descriptors[i];
        LOGD("  %d) tag: (0x%04X) %s", i, des->tag,
             descriptor_tag_to_str(des->tag));
        LOGD("      length: %d", des->length);
        print_descriptor_info(des);
      }
    }
  }

  void print_pat(TSDemuxContext *ctx, void *data) {
    LOGD("====================");
    TSDPATData *pat = (TSDPATData *)data;
    size_t len = pat->length;
    size_t i;
    LOGD("PAT, Length %d", (int)pat->length);

    if (len > 1) {
      LOGD("number of progs: %d", (int)len);
    }
    for (i = 0; i < len; ++i) {
      LOGD("  %d) prog num: 0x%X, pid: 0x%X", (int)i, pat->program_number[i],
           pat->pid[i]);
    }
  }

  void print_pmt(TSDemuxContext *ctx, void *data) {
    LOGD("====================");
    LOGD("PMT");
    TSDPMTData *pmt = (TSDPMTData *)data;
    LOGD("PCR PID: 0x%04X", pmt->pcr_pid);
    LOGD("program info length: %d", (int)pmt->program_info_length);
    LOGD("descriptors length: %d", (int)pmt->descriptors_length);
    size_t i;

    for (i = 0; i < pmt->descriptors_length; ++i) {
      TSDDescriptor *des = &pmt->descriptors[i];
      LOGD("  %d) tag: (0x%04X) %s", (int)i, des->tag,
           descriptor_tag_to_str(des->tag));
      LOGD("     length: %d", des->length);
      print_descriptor_info(des);
    }

    LOGD("program elements length: %d", (int)pmt->program_elements_length);
    for (i = 0; i < pmt->program_elements_length; ++i) {
      TSDProgramElement *prog = &pmt->program_elements[i];
      LOGD("  -----Program #%d", (int)i);
      LOGD("  stream type: (0x%04X)  %s", prog->stream_type,
           stream_type_to_str((TSDPESStreamId)(prog->stream_type)));
      LOGD("  elementary pid: 0x%04X", prog->elementary_pid);
      LOGD("  es info length: %d", prog->es_info_length);
      LOGD("  descriptors length: %d", (int)prog->descriptors_length);

      if (isStreamTypeActive((TSDPMTStreamType)prog->stream_type)){
        set_write_active(true);
      }

      // keep track of metadata pids, we'll print the data for these
      for(int j=0;j<stream_types.size();j++){
        add_print_pid(prog, stream_types[j]);
      }

      // we'll register to listen to the PES data for this program.
      int reg_types = TSD_REG_PES;

      size_t j;
      for (j = 0; j < prog->descriptors_length; ++j) {
        TSDDescriptor *des = &prog->descriptors[j];
        LOGD("    %d) tag: (0x%04X) %s", (int)j, des->tag,
             descriptor_tag_to_str(des->tag));
        LOGD("         length: %d", des->length);
        print_descriptor_info(des);

        // if this tag is the SCTE Adaption field private data descriptor,
        // we'll also register for the Adaptation Field Privae Data.
        if (des->tag == 0x97) {
          reg_types |= TSD_REG_ADAPTATION_FIELD;
        }
      }

      // register all the PIDs we come across.
      tsd_register_pid(ctx, prog->elementary_pid, reg_types);
    }
  }

  void add_print_pid(TSDProgramElement *prog, TSDPMTStreamType type) {
    if (prog->stream_type == type) {
      int k;
      for (k = 0; k < MTS_PRINT_PIDS_LEN; ++k) {
        // find a spare slot in the pids
        if (print_pids[k] == 0) {
          print_pids[k] = prog->elementary_pid;
          break;
        }
      }
    }
  }

  const char *stream_type_to_str(TSDPESStreamId stream_id) {
    if (stream_id >= 0x1C && stream_id <= 0x7F && stream_id != 0x24 &&
        stream_id != 0x42) {
      stream_id = (TSDPESStreamId)0x1C;
    } else if (stream_id >= 0x8A && stream_id <= 0x8F) {
      stream_id = (TSDPESStreamId)0x8A;
    } else if (stream_id >= 0x93 && stream_id <= 0x94) {
      stream_id = (TSDPESStreamId)0x93;
    } else if (stream_id >= 0x96 && stream_id <= 0x9F) {
      stream_id = (TSDPESStreamId)0x96;
    } else if (stream_id >= 0xA1 && stream_id <= 0xBF) {
      stream_id = (TSDPESStreamId)0xA1;
    } else if (stream_id >= 0xC4 && stream_id <= 0xE9) {
      stream_id = (TSDPESStreamId)0xC4;
    } else if (stream_id >= 0xEB && stream_id <= 0xFF) {
      stream_id = (TSDPESStreamId)0xEB;
    }

    switch (stream_id) {
      case 0x00:
        return "ITU-T | ISO/IEC Reserved";
      case 0x01:
        return "ISO/IEC 11172 Video";
      case 0x02:
        return "ITU-T Rec. H.262 | ISO/IEC 13818-2 Video";
      case 0x03:
        return "ISO/IEC 11172 Audio";
      case 0x04:
        return "ISO/IEC 13818-3 Audio";
      case 0x05:
        return "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private sections";
      case 0x06:
        return "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 PES packets containing "
               "private data";
      case 0x07:
        return "ISO/IEC 13522 MHEG";
      case 0x08:
        return "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 DSM-CC";
      case 0x09:
        return "ITU-T Rec. H.222.0 | ISO/IEC 13818-1/11172-1 auxiliary";
      case 0x0A:
        return "ISO/IEC 13818-6 Multi-protocol Encapsulation";
      case 0x0B:
        return "ISO/IEC 13818-6 DSM-CC U-N Messages";
      case 0x0C:
        return "ISO/IEC 13818-6 Stream Descriptors";
      case 0x0D:
        return "ISO/IEC 13818-6 Sections (any type, including private data)";
      case 0x0E:
        return "ISO/IEC 13818-1 auxiliary";
      case 0x0F:
        return "ISO/IEC 13818-7 Audio (AAC) with ADTS transport";
      case 0x10:
        return "ISO/IEC 14496-2 Visual";
      case 0x11:
        return "ISO/IEC 14496-3 Audio with the LATM transport syntax as "
               "defined in ISO/IEC 14496-3";
      case 0x12:
        return "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried "
               "in PES packets";
      case 0x13:
        return "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried "
               "in ISO/IEC 14496_sections";
      case 0x14:
        return "ISO/IEC 13818-6 DSM-CC Synchronized Download Protocol";
      case 0x15:
        return "Metadata carried in PES packets";
      case 0x16:
        return "Metadata carried in metadata_sections";
      case 0x17:
        return "Metadata carried in ISO/IEC 13818-6 Data Carousel";
      case 0x18:
        return "Metadata carried in ISO/IEC 13818-6 Object Carousel";
      case 0x19:
        return "Metadata carried in ISO/IEC 13818-6 Synchronized Download "
               "Protocol";
      case 0x1A:
        return "IPMP stream (defined in ISO/IEC 13818-11, MPEG-2 IPMP)";
      case 0X1B:
        return "AVC video stream as defined in ITU-T Rec. H.264 | ISO/IEC "
               "14496-10 Video";
      case 0x1C:
        return "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved";
      case 0x24:
        return "ITU-T Rec. H.265 and ISO/IEC 23008-2 (Ultra HD video) in a "
               "packetized stream";
      case 0x42:
        return "Chinese Video Standard in a packetized stream";
      case 0x80:
        return "DigiCipher® II video | Identical to ITU-T Rec. H.262 | ISO/IEC "
               "13818-2 Video";
      case 0x81:
        return "ATSC A/53 audio [2] | AC-3 audio";
      case 0x82:
        return "SCTE Standard Subtitle";
      case 0x83:
        return "SCTE Isochronous Data | Reserved";
      case 0x84:
        return "ATSC/SCTE reserved";
      case 0x85:
        return "ATSC Program Identifier , SCTE Reserved";
      case 0x86:
        return "SCTE 35 splice_information_table | [Cueing]";
      case 0x87:
        return "E-AC-3";
      case 0x88:
        return "DTS HD Audio";
      case 0x89:
        return "ATSC Reserved";
      case 0x8A:
        return "ATSC Reserved";
      case 0x90:
        return "DVB stream_type value for Time Slicing / MPE-FEC";
      case 0x91:
        return "IETF Unidirectional Link Encapsulation (ULE)";
      case 0x92:
        return "VEI stream_type";
      case 0x93:
        return "ATSC Reserved";
      case 0x95:
        return "ATSC Data Service Table, Network Resources Table";
      case 0x96:
        return "ATSC Reserved";
      case 0xA0:
        return "SCTE [IP Data] | ATSC Reserved";
      case 0xA1:
        return "ATSC Reserved";
      case 0xC0:
        return "DCII (DigiCipher®) Text";
      case 0xC1:
        return "ATSC Reserved";
      case 0xC2:
        return "ATSC synchronous data stream | [Isochronous Data]";
      case 0xC3:
        return "SCTE Asynchronous Data";
      case 0xC4:
        return "ATSC User Private Program Elements";
      case 0xEA:
        return "VC-1 Elementary Stream per RP227";
      case 0xEB:
        return "ATSC User Private Program Elements";
    }
    return "Unknown";
  }

  const char *descriptor_tag_to_str(uint8_t tag) {
    if (tag >= 0x24 && tag <= 0x27) {
      tag = 0x24;
    } else if (tag >= 0x29 && tag <= 0x35) {
      tag = 0x29;
    } else if (tag >= 0x3A && tag <= 0x3F) {
      tag = 0x3A;
    } else if (tag >= 0x40 && tag <= 0x51) {
      tag = 0x40;
    } else if (tag >= 0x98 && tag <= 0x9F) {
      tag = 0x98;
    }

    switch (tag) {
      case 0x00:
      case 0x01:
        return "ISO/IEC 13818 Reserved";
      case 0x02:
        return "video_stream_descriptor";
      case 0x03:
        return "audio_stream_descriptor";
      case 0x04:
        return "hierarchy_descriptor";
      case 0x05:
        return "registration_descriptor";
      case 0x06:
        return "data_stream_alignment_descriptor";
      case 0x07:
        return "target_background_grid_descriptor";
      case 0x08:
        return "video_window_descriptor";
      case 0x09:
        return "CA_descriptor";
      case 0x0A:
        return "ISO_639_language_descriptor";
      case 0x0B:
        return "system_clock_descriptor";
      case 0x0C:
        return "multiplex_buffer_utilization_descriptor";
      case 0x0D:
        return "copyright_descriptor";
      case 0x0E:
        return "Maximum_bitrate_descriptor";
      case 0x0F:
        return "Private_data_indicator_descriptor";
      case 0x10:
        return "smoothing_buffer_descriptor";
      case 0x11:
        return "STD_descriptor";
      case 0x12:
        return "IBP descriptor";
      case 0x13:
        return "DSM-CC carousel_identifier_descriptor";
      case 0x14:
        return "DSM-CC association_tag_descriptor";
      case 0x15:
        return "DSM-CC deferred_association_tags_descriptor";
      case 0x16:
        return "ISO/IEC 13818-6 reserved";
      case 0x17:
        return "NPT Reference descriptor";
      case 0x18:
        return "NPT Endpoint descriptor";
      case 0x19:
        return "Stream Mode descriptor";
      case 0x1A:
        return "Stream Event descriptor";
      case 0x1B:
        return "MPEG-4_video_descriptor";
      case 0x1C:
        return "MPEG-4_audio_descriptor";
      case 0x1D:
        return "IOD_descriptor";
      case 0x1E:
        return "SL_descriptor";
      case 0x1F:
        return "FMC_descriptor";
      case 0x20:
        return "External_ES_ID_descriptor";
      case 0x21:
        return "MuxCode_descriptor";
      case 0x22:
        return "FmxBufferSize_descriptor";
      case 0x23:
        return "MultiplexBuffer_descriptor";
      case 0x24:
        return "Reserved for ISO/IEC 13818-1 use";
      case 0x28:
        return "AVC_video_descriptor()";
      case 0x29:
        return "Reserved for ISO/IEC 13818-1 use";
      case 0x36:
        return "content_labeling_descriptor";
      case 0x37:
        return "Metadata_location_descriptor";
      case 0x3A:
        return "ISO/IEC 13818 Reserved";
      case 0x40:
        return "User Private";
      case 0x52:
        return "SCTE 35 Stream Identifier Descriptor";
      case 0x60:
        return "ACAP-X Application Descriptor";
      case 0x61:
        return "ACAP-X Application Location Descriptor";
      case 0x62:
        return "ACAP-X Application Boundary Descriptor";
      case 0x80:
        return "Stuffing_descriptor";
      case 0x81:
        return "AC3_audio_descriptor";
      case 0x82:
        return "SCTE Frame_rate_descriptor";
      case 0x83:
        return "SCTE Extended_video_descriptor";
      case 0x84:
        return "SCTE Component_name_descriptor";
      case 0x85:
        return "ATSC program_identifier";
      case 0x86:
        return "Caption_service_descriptor";
      case 0x87:
        return "Content_advisory_descriptor";
      case 0x88:
        return "ATSC CA_descriptor";
      case 0x89:
        return "ATSC Descriptor_tag";
      case 0x8A:
        return "SCTE 35 cue identifier descriptor";
      case 0x8B:
        return "ATSC/SCTE Reserved";
      case 0x8C:
        return "TimeStampDescriptor";
      case 0x8D:
        return "parameterized_service_descriptor() ";
      case 0x8E:
        return "Interactive Services Filtering Criteria descriptor";
      case 0x8F:
        return "Interactive Services NRT Services Summary descriptor";
      case 0x90:
        return "SCTE Frequency_spec_descriptor";
      case 0x91:
        return "SCTE Modulation_params_descriptor";
      case 0x92:
        return "SCTE Transport_stream_id_descriptor";
      case 0x93:
        return "SCTE Revision detection descriptor";
      case 0x94:
        return "SCTE Two part channel number descriptor";
      case 0x95:
        return "SCTE Channel properties descriptor";
      case 0x96:
        return "SCTE Daylight Savings Time Descriptor";
      case 0x97:
        return "SCTE_adaptation_field_data_descriptor()";
      case 0x98:
        return "SCTE Reserved";
      case 0xA0:
        return "extended_channel_name_descriptor";
      case 0xA1:
        return "ATSC service_location_descriptor";
      case 0xA2:
        return "time_shifted_service_descriptor";
      case 0xA3:
        return "component_name_descriptor";
      case 0xA4:
        return "ATSC data_service_descriptor";
      case 0xA5:
        return "ATSC PID Count descriptor";
      case 0xA6:
        return "ATSC Download descriptor";
      case 0xA7:
        return "ATSC Multiprotocol Encapsulation descriptor";
      case 0xA8:
        return "ATSC dcc_departing_request_descriptor";
      case 0xA9:
        return "ATSC dcc_arriving_request_descriptor";
      case 0xAA:
        return "ATSC rc_descriptor";
      case 0xAB:
        return "ATSC Genre descriptor";
      case 0xAC:
        return "SCTE MAC Address List";
      case 0xAD:
        return "ATSC private information descriptor";
      case 0xAE:
        return "ATSC compatibility wrapper descriptor";
      case 0xAF:
        return "ATSC broadcaster policy descriptor";
      case 0xB0:
        return "ATSC service name descriptor";
      case 0xB1:
        return "ATSC URI descriptor";
      case 0xB2:
        return "ATSC enhanced signaling descriptor";
      case 0xB3:
        return "ATSC M/H string mapping descriptor";
      case 0xB4:
        return "ATSC Module Link descriptor";
      case 0xB5:
        return "ATSC CRC32 descriptor";
      case 0xB6:
        return "ATSC Content Identifier Descriptor";
      case 0xB7:
        return "ModuleInfoDescriptor";
      case 0xB8:
        return "ATSC Group Link descriptor";
      case 0xB9:
        return "ATSC Time Stamp descriptor";
      case 0xBA:
        return "ScheduleDescriptor";
      case 0xBB:
        return "Component list descriptor";
      case 0xBC:
        return "ATSC M/H component descriptor";
      case 0xBD:
        return "ATSC M/H rights issuer descriptor";
      case 0xBE:
        return "ATSC M/H current program descriptor";
      case 0xBF:
        return "ATSC M/H original service identification descriptor";
      case 0xC0:
        return "protection_descriptor";
      case 0xC1:
        return "MH_SG_bootstrap_descriptor";
      case 0xC2:
        return "Service ID descriptor";
      case 0xC3:
        return "Protocol Version descriptor";
      case 0xC4:
        return "NRT Service descriptor";
      case 0xC5:
        return "Capabilities descriptor";
      case 0xC6:
        return "Icon descriptor";
      case 0xC7:
        return "Receiver Targeting descriptor";
      case 0xC8:
        return "Time Slot descriptor";
      case 0xC9:
        return "Internet Location Descriptor";
      case 0xCA:
        return "Associated Service descriptor";
      case 0xCB:
        return "Eye Identification Descriptor tag";
      case 0xCC:
        return "E-AC-3 descriptor (A/52 Annex G)";
      case 0xCD:
        return "2D 3D Corresponding Content Descriptor";
      case 0xCE:
        return "Multimedia EPG Linkage Descriptor";
      case 0xE0:
        return "etv_application_information_descriptor()";
      case 0xE1:
        return "etv_media_time_descriptor()";
      case 0xE2:
        return "etv_stream_event_descriptor()";
      case 0xE3:
        return "etv_application_descriptor()";
      case 0xE4:
        return "RBI_signaling_descriptor()";
      case 0xE5:
        return "etv_application_metadata_descriptor()";
      case 0xE6:
        return "etv_bif_platform_descriptor()";
      case 0xE7:
        return "etv_integrated_signaling_descriptor()";
      case 0xE8:
        return "3d_MPEG2_descriptor()";
      case 0XE9:
        return "ebp_descriptor()";
      case 0xEA:
        return "MPEG_AAC_descriptor";
      case 0xEB:
        return "IC3D_event_info_descriptor";
      case 0xEC:
        return "MDTV hybrid stereoscopic service descriptor";
    }
    return "Unknown";
  }

  void print_descriptor_info(TSDDescriptor *desc) {
    // print out some interesting descriptor data
    switch (desc->tag) {
      case 0x05:  // Registration descriptor
      {
        TSDDescriptorRegistration res;
        if (TSD_OK == tsd_parse_descriptor_registration(
                          desc->data, desc->data_length, &res)) {
          LOGD("\n  format identififer: 0x%08X", res.format_identifier);
        }
      } break;
      case 0x0A:  // ISO 639 Language descriptor
      {
        TSDDescriptorISO639Language res;
        if (TSD_OK == tsd_parse_descriptor_iso639_language(
                          desc->data, desc->data_length, &res)) {
          LOGD("\n");
          int i = 0;
          for (; i < res.language_length; ++i) {
            LOGD(" ISO Language Code: 0x%08X, audio type: 0x%02x",
                 res.iso_language_code[i], res.audio_type[i]);
          }
          LOGD("\n");
        }
      } break;
      case 0x0E:  // Maximum bitrate descriptor
      {
        TSDDescriptorMaxBitrate res;
        if (TSD_OK == tsd_parse_descriptor_max_bitrate(
                          desc->data, desc->data_length, &res)) {
          LOGD(" Maximum Bitrate: %d x 50 bytes/second", res.max_bitrate);
        }
      } break;
      default: {
        LOGW(" Unknown Descriptor: 0x%x ", desc->tag);
      } break;
    }
  }

  static void* log_malloc (size_t size) {
      void *result = malloc(size);
      LOGI("malloc(%d) -> %p %s", (int)size,result, result!=NULL?"OK":"ERROR");
      return result;
  }

  static void* log_calloc(size_t num, size_t size){
      void *result = calloc(num, size);
      LOGI("calloc(%d) -> %p %s", (int)(num*size),result, result!=NULL?"OK":"ERROR");
      return result;
  }

  static void* log_realloc(void *ptr, size_t size){
      void *result = realloc(ptr, size);
      LOGI("realloc(%d) -> %p %s", (int)size, result, result!=NULL?"OK":"ERROR");
      return result;
  }

  static void log_free (void *mem){
      LOGD("free(%p)", mem);
      free(mem);
  }

  //   // store allocated size in first bytes
  // static void* log_malloc (size_t size) {
  //     void *result = malloc(size);
  //     memset(result, 0, size);
  //     AllocSize entry{result, size};
  //     self->alloc_vector.push_back(entry);
  //     assert(find_size(result)>=0);
  //     LOGI("malloc(%d) -> %p %s\n", (int)size,result, result!=NULL?"OK":"ERROR");
  //     return result;
  // }

  // static void* log_calloc(size_t num, size_t size){
  //   return log_malloc(num*size);
  // }

  // static int find_size(void *ptr){
  //   for (int j=0;j<self->alloc_vector.size();j++){
  //     if (self->alloc_vector[j].data==ptr) return j;
  //   }
  //   return -1;
  // }

  // static void* log_realloc(void *ptr, size_t size){
  //     int pos = find_size(ptr);
  //     void *result = nullptr;
  //     if (pos>=0){
  //       result =  realloc(ptr, size);
  //       // store size in header
  //       size_t old_size = self->alloc_vector[pos].size;
  //       memset(result+old_size, 0, size-old_size);
  //       self->alloc_vector[pos].size = size;
  //     } else {
  //       LOGE("realloc of unallocatd memory %p", ptr);
  //       result =  realloc(ptr, size);
  //       AllocSize entry{result, size};
  //       self->alloc_vector.push_back(entry);
  //       assert(find_size(result)>=0);
  //     }

  //     LOGI("realloc(%d) -> %p %s\n", (int)size, result, result!=NULL?"OK":"ERROR");
  //     return result;
  // }

  // static void log_free (void *mem){
  //     LOGD("free(%p)\n", mem);
  //     free(mem);
  //     int pos = find_size(mem);
  //     if (pos>=0){
  //       self->alloc_vector.erase(pos);
  //       assert(find_size(mem)==-1);

  //     } else {
  //       LOGE("free of unallocatd memory %p", mem);
  //     }
  // }


};
// init static variable
MTSDecoder *MTSDecoder::self = nullptr;

}  // namespace audio_tools