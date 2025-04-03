import asyncio
import struct
import wave
import sys
import os
import time
import numpy as np
from datetime import datetime
from bleak import BleakClient, BleakScanner

# For audio processing
import soundfile as sf

# Import faster_whisper without triggering Windows GUI thread issues
os.environ["KMP_DUPLICATE_LIB_OK"] = "TRUE"  # Helps with some Intel MKL issues on Windows
if sys.platform == "win32":
    # Prevent the Windows GUI thread issue
    os.environ["PYTHONEXECUTABLE"] = sys.executable
    os.environ["PYTHONPATH"] = os.pathsep.join(sys.path)

# Import faster_whisper after setting environment variables
from faster_whisper import WhisperModel

# UUIDs matching the ESP32 implementation
AUDIO_SERVICE_UUID = "5F8A9156-41D8-4211-9988-CFF98FE5D4A1"
AUDIO_DATA_CHAR_UUID = "5F8A9157-41D8-4211-9988-CFF98FE5D4A1"
AUDIO_CONTROL_CHAR_UUID = "5F8A9158-41D8-4211-9988-CFF98FE5D4A1"

# Control commands
CONTROL_CMD_START = 0x01
CONTROL_CMD_STOP = 0x02

# Audio parameters (must match ESP32)
SAMPLE_RATE = 16000
BITS_PER_SAMPLE = 16
CHANNELS = 1

# Duration in seconds
RECORDING_DURATION = 15

# Amplification factor for the audio (adjust as needed)
AMPLIFICATION_FACTOR = 5.0

class AudioReceiver:
    def __init__(self):
        self.client = None
        self.audio_data = bytearray()
        self.recording = False
        self.output_file = None
        self.amplified_output_file = None
        self.data_count = 0
        self.last_progress_update = 0

        # Add these new attributes
        self.buffer_size = SAMPLE_RATE * 3  # 3-second buffer for processing
        self.processing_buffer = bytearray()
        self.last_processed_size = 0
        self.transcript_segments = []
        self.transcription_task = None
        self.is_processing = False
        
        # Initialize the transcription model
        print("Loading Whisper model, please wait...")
        # try:
        #     # Use the tiny model for fastest transcription
        #     self.whisper_model = WhisperModel("base", device="cpu", compute_type="int8")
        #     print("Whisper model loaded successfully")
        # except Exception as e:
        #     print(f"Error loading Whisper model: {e}")
        #     self.whisper_model = None

        # Check if CUDA (GPU) is available
        try:
            import torch
            
            print(f"PyTorch version: {torch.__version__}")
            print(f"CUDA available: {torch.cuda.is_available()}")
            if torch.cuda.is_available():
                print(f"CUDA device: {torch.cuda.get_device_name(0)}")
                print(f"CUDA version: {torch.version.cuda}")

            device = "cuda" if torch.cuda.is_available() else "cpu"
            compute_type = "float16" if device == "cuda" else "int8"
            print(f"Using device: {device} with compute type: {compute_type}")
            self.whisper_model = WhisperModel("base", device=device, compute_type=compute_type)
        except ImportError:
            # Fallback if torch is not installed
            print("PyTorch not found, using CPU only")
            self.whisper_model = WhisperModel("base", device="cpu", compute_type="int8")
        
    async def scan_for_device(self):
        print("Scanning for PebbleAudio device...")
        devices = await BleakScanner.discover()
        for device in devices:
            if device.name and ("PebbleAudio" in device.name):
                print(f"Found ESP32 device: {device.name} at {device.address}")
                return device.address
        return None
        
    async def connect_to_device(self, address):
        try:
            self.client = BleakClient(address)
            await self.client.connect()
            print(f"Connected to {address}")
            
            # Only print minimal service info for verification
            services = await self.client.get_services()
            if not services:
                print("No services found! Device isn't advertising any GATT services.")
                return False
            
            return True
        except Exception as e:
            print(f"Error connecting to device: {e}")
            return False
            
    def audio_notification_handler(self, sender, data):
        if self.recording:
            self.audio_data.extend(data)
            self.processing_buffer.extend(data)
            self.data_count += len(data)
            
            # Regular progress update
            current_time = datetime.now().timestamp()
            if self.data_count - self.last_progress_update > 4096 or current_time - getattr(self, 'last_time_update', 0) > 1.0:
                sys.stdout.write(f'\rReceived {self.data_count // 1024} KB')
                sys.stdout.flush()
                self.last_progress_update = self.data_count
                self.last_time_update = current_time
            
            # Check if we have enough new data for real-time processing
            if len(self.processing_buffer) - self.last_processed_size >= self.buffer_size and not self.is_processing:
                # Schedule transcription in the background
                asyncio.create_task(self.process_audio_chunk())

    async def process_audio_chunk(self):
        """Process the latest audio chunk for real-time transcription"""
        if self.is_processing or not self.whisper_model:
            return
        
        try:
            self.is_processing = True
            
            # Create a copy of the current buffer for processing
            # This ensures we don't modify the buffer while it's being updated
            current_buffer = self.processing_buffer.copy()
            buffer_size = len(current_buffer)
            
            # Update last processed size
            self.last_processed_size = buffer_size
            
            # Process the chunk in a separate thread
            loop = asyncio.get_event_loop()
            segment_text = await loop.run_in_executor(
                None, 
                self.transcribe_audio_chunk, 
                current_buffer
            )
            
            if segment_text and segment_text.strip():
                print(f"\nTranscript so far: {segment_text}")
                self.transcript_segments.append(segment_text)
        
        except Exception as e:
            print(f"\nError in real-time processing: {e}")
        finally:
            self.is_processing = False
    
    def transcribe_audio_chunk(self, audio_bytes):
        """Convert bytes to audio and transcribe with Whisper"""
        try:
            # Convert bytes to numpy array
            audio_array = np.frombuffer(audio_bytes, dtype=np.int16)
            
            # Convert to float and normalize
            audio_float = audio_array.astype(np.float32) / 32768.0
            
            # Apply amplification
            amplified_audio = np.clip(audio_float * AMPLIFICATION_FACTOR, -1.0, 1.0)
            
            # Save to temporary file (Faster Whisper works better with files)
            temp_file = "temp_chunk.wav"
            sf.write(temp_file, amplified_audio, SAMPLE_RATE)
            
            # Transcribe the chunk
            segments, info = self.whisper_model.transcribe(
                temp_file, 
                beam_size=1,
                language="en",  # Force English for consistency
                condition_on_previous_text=True,  # Maintain context
                no_speech_threshold=0.6  # Reduce false positives
            )
            
            # Get the transcript text
            transcript = " ".join(segment.text for segment in segments)
            
            # Clean up
            if os.path.exists(temp_file):
                os.remove(temp_file)
                
            return transcript
        
        except Exception as e:
            print(f"\nTranscription chunk error: {e}")
            return ""
            
    async def start_recording(self):
        if not self.client or not self.client.is_connected:
            print("Not connected to any device")
            return False
        
        # Clear previous data
        self.audio_data = bytearray()
        self.data_count = 0
        self.last_progress_update = 0
        
        try:
            # In newer Bleak versions, MTU exchange is either automatic or handled differently
            # Check if the method exists before trying to use it
            if hasattr(self.client, 'exchange_mtu'):
                await self.client.exchange_mtu(512)
            else:
                # For newer Bleak versions, MTU might be automatically negotiated
                print("Note: MTU negotiation skipped (automatically handled by Bleak)")
            
            # Subscribe to audio data notifications
            await self.client.start_notify(AUDIO_DATA_CHAR_UUID, self.audio_notification_handler)
            
            # Send start command
            await self.client.write_gatt_char(AUDIO_CONTROL_CHAR_UUID, bytes([CONTROL_CMD_START]))
            
            self.recording = True
            print("Recording started...")
            
            # Create output file name based on timestamp
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            self.output_file = f"audio_recording_{timestamp}.wav"
            self.amplified_output_file = f"audio_recording_{timestamp}_amplified.wav"
            
            # Reset real-time processing state
            self.processing_buffer = bytearray()
            self.last_processed_size = 0
            self.transcript_segments = []
            self.is_processing = False

            return True
        
        except Exception as e:
            print(f"Error starting recording: {e}")
            return False
       
    async def stop_recording(self):
        if not self.recording:
            print("Not currently recording")
            return False
        
        try:
            # Send stop command
            await self.client.write_gatt_char(AUDIO_CONTROL_CHAR_UUID, bytes([CONTROL_CMD_STOP]))
            
            # Stop notifications
            await self.client.stop_notify(AUDIO_DATA_CHAR_UUID)
            
            self.recording = False
            print("\nRecording stopped")

            # Save the complete transcript
            if hasattr(self, 'transcript_segments') and self.transcript_segments:
                transcript_file = os.path.splitext(self.output_file)[0] + "_realtime.txt"
                full_transcript = " ".join(self.transcript_segments)
                
                with open(transcript_file, "w", encoding="utf-8") as f:
                    f.write(full_transcript)
                
                print(f"Complete transcript saved to {transcript_file}")
                print(f"Full transcript: {full_transcript}")
            
            # Save the audio data to a WAV file
            if self.save_wav_file():
                # Process the audio (amplify and transcribe)
                await self.process_audio()
                return True
            return False
        
        except Exception as e:
            print(f"Error stopping recording: {e}")
            self.recording = False
            return False
        
    def save_wav_file(self):
        if not self.audio_data:
            print("No audio data to save")
            return False
            
        try:
            print(f"Saving {len(self.audio_data)} bytes of audio data")
            
            # Create WAV file
            with wave.open(self.output_file, 'wb') as wf:
                wf.setnchannels(CHANNELS)
                wf.setsampwidth(BITS_PER_SAMPLE // 8)
                wf.setframerate(SAMPLE_RATE)
                wf.writeframes(self.audio_data)
                
            print(f"Saved original audio to {self.output_file}")
            return True
            
        except Exception as e:
            print(f"Error saving WAV file: {e}")
            return False
    
    async def process_audio(self):
        """Amplify the audio and transcribe it"""
        try:
            # Convert byte data to numpy array for processing
            print("Processing audio...")
            audio_array = np.frombuffer(self.audio_data, dtype=np.int16)
            
            # Normalize and amplify
            print(f"Amplifying audio by factor of {AMPLIFICATION_FACTOR}...")
            # Convert to float for processing
            audio_float = audio_array.astype(np.float32) / 32768.0
            
            # Apply amplification with clipping prevention
            amplified_audio = np.clip(audio_float * AMPLIFICATION_FACTOR, -1.0, 1.0)
            
            # Convert back to int16 for saving
            amplified_audio_int = (amplified_audio * 32767).astype(np.int16)
            
            # Save amplified audio
            sf.write(self.amplified_output_file, amplified_audio_int, SAMPLE_RATE)
            print(f"Saved amplified audio to {self.amplified_output_file}")
            
            # Transcribe the audio if model is loaded
            if self.whisper_model:
                print("Transcribing audio with Faster Whisper...")
                
                # Run transcription in a separate thread to avoid blocking the event loop
                loop = asyncio.get_event_loop()
                transcript = await loop.run_in_executor(None, self.transcribe_audio, self.amplified_output_file)
                
                # Save transcript to file
                transcript_file = os.path.splitext(self.amplified_output_file)[0] + ".txt"
                with open(transcript_file, "w", encoding="utf-8") as f:
                    f.write(transcript)
                
                print(f"Transcription saved to {transcript_file}")
                print(f"Transcript: {transcript}")
            
            return True
        
        except Exception as e:
            print(f"Error processing audio: {e}")
            return False
    
    def transcribe_audio(self, audio_file):
        """Transcribe audio using Faster Whisper"""
        try:
            # Transcribe with the tiny model
            segments, info = self.whisper_model.transcribe(audio_file, beam_size=1)
            
            # Combine all segments into one string
            transcript = " ".join(segment.text for segment in segments)
            
            return transcript
        except Exception as e:
            print(f"Transcription error: {e}")
            return "Transcription failed"
            
    async def disconnect(self):
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print("Disconnected from device")

async def main():
    receiver = AudioReceiver()
    
    # Scan for the device
    device_address = await receiver.scan_for_device()
    if not device_address:
        print("No PebbleAudio device found. Make sure the device is powered and in range.")
        return
    
    # Connect to the device
    if not await receiver.connect_to_device(device_address):
        return
    
    try:
        # Wait for user input
        print("\nCommands:")
        print("1: Start recording")
        print("2: Stop recording")
        print("3: Record for specified duration")
        print("q: Quit")
        
        while True:
            cmd = input("\nEnter command: ")
            
            if cmd == "1":
                await receiver.start_recording()
                print(f"Recording for {RECORDING_DURATION} seconds...")
                await asyncio.sleep(RECORDING_DURATION * 1.25)  # Add 25% buffer to ensure complete recording
                await receiver.stop_recording()
                
            elif cmd == "2":
                await receiver.stop_recording()
                
            elif cmd == "3":
                duration = input("Enter recording duration in seconds (default 15): ")
                try:
                    duration = int(duration) if duration else RECORDING_DURATION
                except ValueError:
                    duration = RECORDING_DURATION
                
                # Add a small buffer to ensure complete recording
                adjusted_duration = duration * 1.25
                
                await receiver.start_recording()
                print(f"Recording for {duration} seconds...")
                await asyncio.sleep(adjusted_duration)
                await receiver.stop_recording()
                
            elif cmd.lower() == "q":
                break
                
            else:
                print("Unknown command")
                
    finally:
        # Ensure we disconnect properly
        await receiver.disconnect()

if __name__ == "__main__":
    # Check dependencies
    required_packages = ["bleak", "numpy", "faster-whisper", "soundfile"]
    missing_packages = []
    
    for package in required_packages:
        try:
            __import__(package.replace("-", "_"))
        except ImportError:
            missing_packages.append(package)
    
    if missing_packages:
        print("Missing dependencies. Please install required packages:")
        print(f"pip install {' '.join(missing_packages)}")
        sys.exit(1)
    
    # Run the main function
    try:
        # For Windows, use a different event loop policy
        if sys.platform == "win32":
            # Use the default event loop policy on Windows
            # This avoids the GUI callback issues
            asyncio.run(main())
        else:
            asyncio.run(main())
    except Exception as e:
        print(f"Error in main execution: {e}")