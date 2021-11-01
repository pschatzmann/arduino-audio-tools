#pragma once

#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/Converter.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioCopy.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioHttp/AudioHttp.h"
#include "AudioHttp/Str.h"

#ifdef USE_SDFAT
#include <SPI.h>
#include <SdFat.h>
#endif


// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur. (40?)
#define SPI_CLOCK SD_SCK_MHZ(50)
// Max file name length including directory path
#define MAX_FILE_LEN 256


namespace audio_tools {

	/**
	 * @brief Abstract Audio Data Source which is used by the Audio Players
	 * @author Phil Schatzmann
	 * @copyright GPLv3
	 *
	 */
	class AudioSource {
	public:
		/// Reset actual stream and move to root
		virtual void begin() = 0;

		/// Returns next audio stream
		virtual Stream* nextStream(int offset) = 0;

		/// Returns next audio stream
		virtual Stream* selectStream(int station) = 0;

		/// Returns previous audio stream
		virtual Stream* previousStream(int station) = 0;

		/// Provides the timeout which is triggering to move to the next stream
		virtual int timeoutMs() {
			return 500;
		}

	};

	/**
	 * @brief Callback Audio Data Source which is used by the Audio Players
	 * @author Phil Schatzmann
	 * @copyright GPLv3
	 */
	class AudioSourceCallback : public AudioSource {
	public:
		AudioSourceCallback() {
		}

		AudioSourceCallback(Stream* (*nextStreamCallback)(), void (*onStartCallback)() = nullptr) {
			LOGD(LOG_METHOD);
			this->onStartCallback = onStartCallback;
			this->nextStreamCallback = nextStreamCallback;
		}

		/// Reset actual stream and move to root
		virtual void begin() {
			LOGD(LOG_METHOD);
			if (onStartCallback != nullptr) onStartCallback();
		};

		/// Returns next stream
		virtual Stream* nextStream(int offset) {
			LOGD(LOG_METHOD);
			return nextStreamCallback == nullptr ? nullptr : nextStreamCallback();
		}

		void setCallbackOnStart(void (*callback)()) {
			onStartCallback = callback;
		}

		void setCallbackNextStream(Stream* (*callback)()) {
			nextStreamCallback = callback;
		}

	protected:
		void (*onStartCallback)() = nullptr;
		Stream* (*nextStreamCallback)() = nullptr;

	};

#ifdef USE_SDFAT
#ifdef ARDUINO_ARCH_RP2040
	// RP2040 is using the library with a sdfat namespace
	typedef sdfat::SdSpiConfig SdSpiConfig;
	typedef sdfat::FsFile AudioFile;
	typedef sdfat::SdFs AudioFs;
#else
	typedef FsFile AudioFile;
	typedef SdFs AudioFs;
#endif


	/**
	 * @brief AudioSource using an SD card as data source. This class is based on https://github.com/greiman/SdFat
	 * @author Phil Schatzmann
	 * @copyright GPLv3
	 */
	class AudioSourceSdFat : public AudioSource {
	public:
		AudioSourceSdFat(const char* startFilePath = "/", const char* ext = ".mp3", int chipSelect = PIN_CS, int speedMHz = 2) {
			LOGD(LOG_METHOD);
			LOGI("SD chipSelect: %d", chipSelect);
			LOGI("SD speedMHz: %d", speedMHz);
			if (!sd.begin(SdSpiConfig(chipSelect, DEDICATED_SPI, SD_SCK_MHZ(speedMHz)))) {
				LOGE("SD.begin failed");
			}
			start_path = startFilePath;
			exension = ext;
		}

		virtual void begin() {
			LOGD(LOG_METHOD);
			pos = 0;
		}

		virtual Stream* nextStream(int offset) {
			LOGD(LOG_METHOD);
			// move to next file
			pos += offset;
			file.close();
			file = getFile(start_path, pos);
			file.getName(file_name, MAX_FILE_LEN);
			LOGI("-> nextStream: '%s'", file_name);
			return file ? &file : nullptr;
		}

		/// Defines the regex filter criteria for selecting files. E.g. ".*Bob Dylan.*" 
		void setFileFilter(const char* filter) {
			file_name_pattern = filter;
		}

	protected:
		AudioFile file;
		AudioFs sd;
		size_t pos = 0;
		char file_name[MAX_FILE_LEN];
		const char* exension = nullptr;
		const char* start_path = nullptr;
		const char* file_name_pattern = "*";

		/// checks if the file is a valid audio file
		bool isValidAudioFile(AudioFile& file) {
			file.getName(file_name, MAX_FILE_LEN);
			Str strFileName(file_name);
			bool result = strFileName.endsWith(exension) && strFileName.matches(file_name_pattern);
			LOGI("-> isValidAudioFile: '%s': %d", file_name, result);
			return result;
		}

		/// Determines the file at the indicated index (starting with 0)
		AudioFile getFile(const char* dirStr, int pos) {
			AudioFile dir;
			AudioFile result;
			dir.open(dirStr);
			size_t count = 0;
			getFileAtIndex(dir, pos, count, result);
			result.getName(file_name, MAX_FILE_LEN);
			LOGI("-> getFile: '%s': %d", file_name, pos);
			dir.close();
			return result;
		}

		/// Recursively walk the directory tree to find the file at the indicated pos.
		void getFileAtIndex(AudioFile dir, size_t pos, size_t& idx, AudioFile& result) {
			LOGD("%s: %d", LOG_METHOD, idx);
			AudioFile file;
			if (idx > pos) return;

			while (file.openNext(&dir, O_READ)) {
				if (idx > pos) return;
				if (!file.isHidden()) {
					if (!file.isDir()) {
						if (isValidAudioFile(file)) {
							idx++;
							if (idx - 1 == pos) {
								LOGI("-> get: '%s'", file_name);
								result = file;
								result.getName(file_name, MAX_FILE_LEN);
								//return;
							}
						}
					}
					else {
						getFileAtIndex(file, pos, idx, result);
					}

					// close all files except result
					char file_name_act[MAX_FILE_LEN];
					file.getName(file_name_act, MAX_FILE_LEN);
					//LOGI("-> %s <-> %s", file_name_act, file_name);
					if (!Str(file_name_act).equals(file_name)) {
						file.getName(file_name, MAX_FILE_LEN);
						file.close();
						LOGI("-> close: '%s'", file_name);
					}
					else {
						return;
					}
				}
			}
		}
	};

#endif


#if defined(ESP32) || defined(ESP8266) || defined(USE_URL_ARDUINO) 

	/**
	 * @brief Audio Source which provides the data via the network from an URL
	 * @author Phil Schatzmann
	 * @copyright GPLv3
	 */
	class AudioSourceURL : public AudioSource {
	public:
		template<typename T, size_t N>
		AudioSourceURL(URLStream& urlStream, T(&urlArray)[N], const char* mime, int startPos = 0) {
			LOGD(LOG_METHOD);
			this->actual_stream = &urlStream;
			this->mime = mime;
			this->urlArray = urlArray;
			this->max = N;
			this->pos = startPos - 1;
		}

		/// Setup Wifi URL
		virtual void begin() {
			LOGD(LOG_METHOD);
			//setPos(-1);
		}

		/// Defines the actual position
		void setPos(int newPos) {
			pos = newPos;
			if (pos >= max || pos < 0) {
				pos = -1;
			}
		}

		/// Opens the next url from the array
		Stream* nextStream(int offset) {
			pos += offset;
			if (pos < 0 || pos >= max) {
				pos = 0;
			}
			LOGI("nextStream: %d -> %s", pos, urlArray[pos]);
			if (offset != 0 || actual_stream == nullptr) {
				if (started) actual_stream->end();
				actual_stream->begin(urlArray[pos], mime);
				started = true;
			}
			return actual_stream;
		}

		/// Opens the selected url from the array
		Stream* selectStream(int Station) {
			pos = Station;
			if (pos < 0 || pos >= max) {
				pos = 0;
			}
			LOGI("selectedStream: %d -> %s", pos, urlArray[pos]);
			if (Station != 0 || actual_stream == nullptr) {
				if (started) actual_stream->end();
				actual_stream->begin(urlArray[pos], mime);
				started = true;
			}
			return actual_stream;
		}

		/// Opens the Selected url from the array
		Stream* previousStream(int offset) {
			pos -= offset;
			if (pos < 0 || pos >= max) {
				pos = max - 1;
			}
			LOGI("previousStream: %d -> %s", pos, urlArray[pos]);
			if (offset != 0 || actual_stream == nullptr) {
				if (started) actual_stream->end();
				actual_stream->begin(urlArray[pos], mime);
				started = true;
			}
			return actual_stream;
		}

		void setTimeoutMs(int millisec) {
			timeout = millisec;
		}

		int timeoutMs() {
			return timeout;
		}

	protected:
		URLStream* actual_stream = nullptr;
		const char** urlArray;
		int pos = 0;
		int max = 0;
		const char* mime = nullptr;
		int timeout = 60000;
		bool started = false;

	};

#endif

	/**
	 * @brief Helper class to debounce user input from a push button
	 *
	 */
	class Debouncer {

	public:
		Debouncer(uint16_t timeoutMs = 5000, void* ref = nullptr) {
			setDebounceTimeout(timeoutMs);
			p_ref = ref;
		}

		void setDebounceTimeout(uint16_t timeoutMs) {
			ms = timeoutMs;
		}

		/// Prevents that the same method is executed multiple times within the indicated time limit
		bool debounce(void(*cb)(void* ref) = nullptr) {
			bool result = false;
			if (millis() > debounce_ms) {
				LOGI("accpted");
				if (cb != nullptr) cb(p_ref);
				// new time limit
				debounce_ms = millis() + ms;
				result = true;
			}
			else {
				LOGI("rejected");
			}
			return result;
		}

	protected:
		unsigned long debounce_ms = 0; // Debounce sensitive touch
		uint16_t ms;
		void* p_ref = nullptr;

	};

	/**
	 * @brief Implements a simple audio player which supports the following commands:
	 * - begin
	 * - play
	 * - stop
	 * - next
	 * - setVolume
	 * @author Phil Schatzmann
	 * @copyright GPLv3
	 */
	class AudioPlayer : public AudioBaseInfoDependent {

	public:
		/**
		 * @brief Construct a new Audio Player object. The processing chain is
		 * AudioSource -> Stream -copy> EncodedAudioStream -> VolumeOutput -> Print
		 *
		 * @param source
		 * @param output
		 * @param decoder
		 */
		AudioPlayer(AudioSource& source, AudioPrint& output, AudioDecoder& decoder) {
			LOGD(LOG_METHOD);
			this->p_source = &source;
			this->volume_out.begin(output);
			this->p_out_decoding = new EncodedAudioStream(volume_out, decoder);
			this->p_final_print = &output;

			// notification for audio configuration
			decoder.setNotifyAudioChange(*this);
		}

		/**
		 * @brief Construct a new Audio Player object. The processing chain is
		 * AudioSource -> Stream -copy> EncodedAudioStream -> VolumeOutput -> Print
		 *
		 * @param source
		 * @param output
		 * @param decoder
		 * @param notify
		 */
		AudioPlayer(AudioSource& source, Print& output, AudioDecoder& decoder, AudioBaseInfoDependent* notify = nullptr) {
			LOGD(LOG_METHOD);
			this->p_source = &source;
			this->volume_out.begin(output);
			this->p_out_decoding = new EncodedAudioStream(volume_out, decoder);
			this->p_final_notify = notify;

			// notification for audio configuration
			decoder.setNotifyAudioChange(*this);
		}

		/**
		 * @brief Construct a new Audio Player object. The processing chain is
		 * AudioSource -> Stream -copy> EncodedAudioStream -> VolumeOutput -> Print
		 *
		 * @param source
		 * @param output
		 * @param decoder
		 */
		AudioPlayer(AudioSource& source, AudioStream& output, AudioDecoder& decoder) {
			LOGD(LOG_METHOD);
			this->p_source = &source;
			this->volume_out.begin(output);
			this->p_out_decoding = new EncodedAudioStream(volume_out, decoder);
			this->p_final_stream = &output;

			// notification for audio configuration
			decoder.setNotifyAudioChange(*this);
		}

		/// Default destructor
		virtual ~AudioPlayer() {
			if (p_out_decoding != nullptr) {
				delete p_out_decoding;
			}
		}

		/// (Re)Starts the playing of the music (from the beginning)
		virtual bool begin(int index = 0, bool isActive = true) {
			LOGD(LOG_METHOD);
			bool result = false;

			// start dependent objects
			p_out_decoding->begin();
			p_source->begin();
			meta_out.begin();

			p_input_stream = p_source->selectStream(index);
			if (p_input_stream != nullptr) {
				if (meta_active) {
					copier.setCallbackOnWrite(decodeMetaData, this);
				}
				copier.begin(*p_out_decoding, *p_input_stream);
				timeout = millis() + p_source->timeoutMs();
				active = isActive;
				result = true;
			}
			else {
				LOGW("-> begin: no data found");
			}
			return result;
		}

		virtual void end() {
			LOGD(LOG_METHOD);
			p_out_decoding->end();
			meta_out.end();
		}

		/// Updates the audio info in the related objects
		virtual void setAudioInfo(AudioBaseInfo info) {
			LOGD(LOG_METHOD);
			LOGI("sample_rate: %d", info.sample_rate);
			LOGI("bits_per_sample: %d", info.bits_per_sample);
			LOGI("channels: %d", info.channels);
			// notifiy volume
			volume_out.setAudioInfo(info);
			// notifiy final ouput: e.g. i2s
			if (p_final_print != nullptr) p_final_print->setAudioInfo(info);
			if (p_final_stream != nullptr) p_final_stream->setAudioInfo(info);
			if (p_final_notify != nullptr) p_final_notify->setAudioInfo(info);
		};

		/// starts / resumes the playing of a matching song
		virtual void play() {
			LOGD(LOG_METHOD);
			active = true;
		}

		/// halts the playing
		virtual void stop() {
			LOGD(LOG_METHOD);
			active = false;
		}

		/// moves to next file
		virtual void next() {
			LOGD(LOG_METHOD);
			active = startNextStream();
		}

		/// moves to Selected file
		virtual void Select(int station) {
			LOGD(LOG_METHOD);
			active = startSelectedStream(station);
			//startSelectedStream(station);
		}

		/// moves to previous file
		virtual void previous() {
			LOGD(LOG_METHOD);
			active = startPreviousStream();
		}

		/// determines if the player is active
		virtual bool isActive() {
			return active;
		}

		/// sets the volume - values need to be between 0.0 and 1.0
		virtual void setVolume(float volume) {
			if (volume >= 0 && volume <= 1.0) {
				if (abs(volume - current_volume) > 0.01) {
					LOGI("setVolume(%f)", volume);
					volume_out.setVolume(volume);
					current_volume = volume;
				}
			}
			else {
				LOGE("setVolume value '%f' out of range (0.0 -1.0)", volume);
			}
		}

		/// Determines the actual volume
		virtual float volume() {
			return volume_out.volume();
		}

		/// Call this method in the loop. 
		virtual void copy() {
			if (active) {
				LOGD(LOG_METHOD);
				// handle sound
				if (copier.copy() || timeout == 0) {
					// reset timeout
					timeout = millis() + p_source->timeoutMs();
				}

				// move to next stream after timeout
				if (millis() > timeout) {
					LOGW("-> timeout - moving to next stream");
					// open next stream
					if (!startNextStream()) {
						LOGD("stream is null");
					}
				}
			}
		}

		/// Defines the medatadata callback
		virtual void setCallbackMetadata(void (*callback)(MetaDataType type, const char* str, int len)) {
			LOGD(LOG_METHOD);
			meta_active = true;
			meta_out.setCallback(callback);
		}

		/// Change the VolumeControl implementation
		virtual void setVolumeControl(VolumeControl& vc) {
			volume_out.setVolumeControl(vc);
		}


	protected:
		bool active = false;
		AudioSource* p_source = nullptr;
		//AudioDecoder *p_decoder = nullptr;
		VolumeOutput volume_out; // Volume control
		MetaDataID3 meta_out; // Metadata parser
		EncodedAudioStream* p_out_decoding = nullptr; // Decoding stream
		Stream* p_input_stream = nullptr;
		AudioPrint* p_final_print = nullptr;
		AudioStream* p_final_stream = nullptr;
		AudioBaseInfoDependent* p_final_notify = nullptr;
		StreamCopy copier; // copies sound into i2s
		bool meta_active = false;
		uint32_t timeout = 0;
		float current_volume = -1; // illegal value which will trigger an update


		/// start next stream
		virtual bool startNextStream() {
			end();
			p_out_decoding->begin();
			p_source->begin();
			p_input_stream = p_source->nextStream(+1);
			if (p_input_stream != nullptr) {
				LOGD("open next stream");
				meta_out.begin();
				copier.begin(*p_out_decoding, *p_input_stream);
			}
			return p_input_stream != nullptr;
		}
		/// start selected stream
		virtual bool startSelectedStream(int station) {
			end();
			p_out_decoding->begin();
			p_source->begin();
			p_input_stream = p_source->selectStream(station);
			if (p_input_stream != nullptr) {
				LOGD("open selected stream");
				meta_out.begin();
				copier.begin(*p_out_decoding, *p_input_stream);
			}
			return p_input_stream != nullptr;
		}
		/// start previous stream
		virtual bool startPreviousStream() {
			end();
			p_out_decoding->begin();
			p_source->begin();
			p_input_stream = p_source->previousStream(1);
			if (p_input_stream != nullptr) {
				LOGD("open previous stream");
				meta_out.begin();
				copier.begin(*p_out_decoding, *p_input_stream);
			}
			return p_input_stream != nullptr;
		}
		/// Callback implementation which writes to metadata
		static void decodeMetaData(void* obj, void* data, size_t len) {
			LOGD(LOG_METHOD);
			AudioPlayer* p = (AudioPlayer*)obj;
			if (p->meta_active) {
				p->meta_out.write((const uint8_t*)data, len);
			}
		}

	};

}