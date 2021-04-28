from pydub import AudioSegment
sound = AudioSegment.from_wav("hit.wav")
sound = sound.set_channels(1)
sound.export("hit.wav", format="wav")
